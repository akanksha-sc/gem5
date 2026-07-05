# Import helper libs
import argparse
import pathlib
import shlex

from l1l2_cache_pm.l1l2_cache_with_pm import PrivateL1SharedL2CacheHierarchy

# Import PM Classes
from mcpat_power_model.inorder_mcpat_power_model.inorder_mcpat_cpu_power_model import (
    InorderMcPATCpuPowerModel,
)
from mcpat_power_model.o3_mcpat_power_model.o3_mcpat_cpu_power_model import (
    O3McPATCpuPowerModel,
)
from mcpat_power_model.static_regression import (
    INORDER_REGRESSION,
    O3_REGRESSION,
)
from power_thermal_runtime import (
    get_on_state,
    remove_trace_files,
    sample_power_now,
    start_power_sampling,
    stop_power_sampling,
)
from thermal_backends.hotspot_runtime_adapter import HotSpotRuntimeAdapter
from thermal_helper import (
    collect_cpu_cache_power_bindings,
    create_thermal_network,
)

# Import m5 objects
import m5
from m5.objects import (
    BranchPredictor,
    Root,
    TournamentBP,
)

# Import gem5 components:
from gem5.components.boards.simple_board import SimpleBoard
from gem5.components.memory.single_channel import SingleChannelLPDDR3_1600
from gem5.components.processors.cpu_types import CPUTypes
from gem5.components.processors.simple_processor import SimpleProcessor
from gem5.isas import ISA

# Import gem5 workload handler
from gem5.resources.resource import (
    BinaryResource,
    obtain_resource,
)
from gem5.simulate.exit_handler import ExitHandler
from gem5.simulate.simulator import Simulator

_thermal_model = None
_hotspot_backend = None


def _set_power_trace_labels(board):
    for label, _simobj, pm in collect_cpu_cache_power_bindings(board):
        try:
            on = get_on_state(pm)
        except Exception:
            continue
        if hasattr(on, "trace_label") and not getattr(on, "trace_label", ""):
            on.trace_label = label


def _get_power_models():
    board = Root.getInstance().board
    return [
        pm for _label, _simobj, pm in collect_cpu_cache_power_bindings(board)
    ]


def _set_pm_sampling(enable):
    pms = _get_power_models()
    if _trace_debug_enabled():
        print(
            f"_set_pm_sampling(enable={enable}) tick={m5.curTick()} "
            f"num_models={len(pms)}"
        )

    if enable:
        start_power_sampling(pms)
    else:
        stop_power_sampling(pms)


def _set_thermal_stepping(enable):
    """Start or stop ROI-controlled thermal stepping."""
    if _thermal_model is None:
        return
    if enable:
        _thermal_model.getCCObject().startStepping()
    else:
        _thermal_model.getCCObject().stopStepping()


def _sample_pm_now():
    sample_power_now(_get_power_models())


def _flush_thermal_now():
    if _thermal_model is not None:
        _thermal_model.getCCObject().flushStepNow()


class BeginExitHandler(ExitHandler, hypercall_num=1999):
    def _process(self, simulator):
        if _trace_debug_enabled():
            print(
                "BeginExitHandler, resetting stats and starting ROI sampling"
            )

        remove_trace_files(
            m5.options.outdir, "power_trace.csv", "thermal_trace.csv"
        )

        m5.stats.reset()
        _set_pm_sampling(True)
        _set_thermal_stepping(True)

    def _exit_simulation(self):
        return False


class EndExitHandler(ExitHandler, hypercall_num=2000):
    def _process(self, simulator):
        if _trace_debug_enabled():
            print("EndExitHandler, stopping ROI sampling and dumping stats")

        _sample_pm_now()
        _flush_thermal_now()
        _set_thermal_stepping(False)
        _set_pm_sampling(False)
        m5.stats.dump()

    def _exit_simulation(self):
        return True


def _trace_debug_enabled():
    return getattr(args, "power_trace_debug", False) or getattr(
        args, "thermal_trace_debug", False
    )


cpus = {
    "timing": CPUTypes.TIMING,
    "minor": CPUTypes.MINOR,
    "o3": CPUTypes.O3,
}


def init_act_energies(args):
    act_energies = {
        "IntAlu": 6.22113e-12,
        "FpAlu": 1.86634e-11,
        "ComplexAlu": 1.24423e-11,
        "BTB": {"Read": 5.62534e-12, "Write": 8.68454e-12},
        "InstBuffer": {"Read": 6.69479e-12, "Write": 7.269e-12},
        "IDInst": 4.90024e-12,
        "IDOp": 4.89659e-12,
        "IDMisc": 4.90566e-12,
        "LoadStoreQueue": {
            "Read": 2.49139e-12,
            "Write": 2.57072e-12,
            "Search": 2.5361e-12,
        },
        "ITLB": {
            "Read": 1.4039e-12,
            "Write": 1.77965e-12,
            "Search": 3.49054e-12,
        },
        "DTLB": {
            "Read": 1.4039e-12,
            "Write": 1.77965e-12,
            "Search": 3.49054e-12,
        },
        "GlobalPred": {"Read": 2.81993e-12, "Write": 1.50057e-12, "Search": 0},
        "L1LocalPred": {
            "Read": 1.59669e-13,
            "Write": 2.99057e-13,
            "Search": 0,
        },
        "L2LocalPred": {
            "Read": 1.22289e-13,
            "Write": 1.96004e-13,
            "Search": 0,
        },
        "ChooserPred": {
            "Read": 2.81993e-12,
            "Write": 1.50057e-12,
            "Search": 0,
        },
        "RAS": {"Read": 3.36208e-13, "Write": 4.51353e-13, "Search": 0},
    }

    if args.cpu_type != "o3":
        act_energies["IntRegFile"] = {
            "Read": 1.93199e-12,
            "Write": 2.55776e-12,
        }
        act_energies["FpRegFile"] = {"Read": 1.3027e-12, "Write": 1.66805e-12}
        act_energies["IntBypass"] = 4.99873e-12
        act_energies["IntTagBypass"] = 1.24968e-12
        act_energies["FpBypass"] = 1.18181e-11
        act_energies["FpTagBypass"] = 2.08847e-12
        act_energies["MulBypass"] = 1.09294e-11
        act_energies["MulTagBypass"] = 1.82156e-12
        act_energies["Pipeline"] = 1.17338e-11
    else:
        act_energies["IntRegFile"] = {
            "Read": 2.14779e-12,
            "Write": 3.49231e-12,
        }
        act_energies["FpRegFile"] = {"Read": 1.44241e-12, "Write": 2.26658e-12}
        act_energies["IntBypass"] = 6.87115e-12
        act_energies["IntTagBypass"] = 1.56691e-12
        act_energies["FpBypass"] = 8.74963e-12
        act_energies["FpTagBypass"] = 1.88238e-12
        act_energies["MulBypass"] = 1.00743e-11
        act_energies["MulTagBypass"] = 2.16751e-12
        act_energies["IntInstWindow"] = {
            "Read": 1.50873e-12,
            "Write": 1.83695e-12,
            "Search": 2.67858e-12,
        }
        act_energies["FpInstWindow"] = {
            "Read": 1.35879e-12,
            "Write": 1.38643e-12,
            "Search": 1.61929e-12,
        }
        act_energies["IntFreeList"] = {
            "Read": 2.97125e-13,
            "Write": 3.43681e-13,
        }
        act_energies["FpFreeList"] = {"Read": 1.767e-13, "Write": 3.1542e-13}
        act_energies["IntFRAT"] = {
            "Read": 4.6752e-13,
            "Write": 6.88901e-13,
            "Search": 2.31626e-12,
        }
        act_energies["FpFRAT"] = {
            "Read": 3.31586e-13,
            "Write": 4.81804e-13,
            "Search": 1.83071e-12,
        }
        act_energies["IntDCL"] = 8.6783e-13
        act_energies["FpDCL"] = 8.6783e-13
        act_energies["SelLogic"] = 2.44692e-12
        act_energies["Pipeline"] = 6.34086e-12

    return act_energies


def _apply_pm(
    simobj,
    power_model_fn,
    so_type,
    act_energies,
    interval=0,
    interval_ticks=0,
    trace_debug=False,
):
    for desc in simobj.descendants():
        if not isinstance(desc, so_type):
            continue
        desc.power_state.default_state = "ON"
        desc.power_model = power_model_fn(
            desc,
            act_energies,
            interval,
            interval_ticks,
            trace_debug=trace_debug,
        )


def _build_hotspot_backend(args):
    """
    Build a small HotSpot block-model backend for the same four sources that the
    runtime thermal interface exports:
        cpu0, l1i0, l1d0, l2
    """
    floorplan = [
        {
            "name": "cpu0",
            "width_m": 0.0040,
            "height_m": 0.0040,
            "x_m": 0.0000,
            "y_m": 0.0000,
        },
        {
            "name": "l1i0",
            "width_m": 0.0010,
            "height_m": 0.0010,
            "x_m": 0.0040,
            "y_m": 0.0000,
        },
        {
            "name": "l1d0",
            "width_m": 0.0010,
            "height_m": 0.0010,
            "x_m": 0.0040,
            "y_m": 0.0010,
        },
        {
            "name": "l2",
            "width_m": 0.0050,
            "height_m": 0.0020,
            "x_m": 0.0000,
            "y_m": 0.0040,
        },
    ]

    layers = [
        {
            "has_lateral": True,
            "has_power": True,
            "k_W_mK": 100.0,
            "thickness_m": 0.15e-3,
            "vol_heat_cap_J_m3K": 1.75e6,
        },
        {
            "has_lateral": True,
            "has_power": False,
            "k_W_mK": 4.0,
            "thickness_m": 20e-6,
            "vol_heat_cap_J_m3K": 4.0e6,
        },
    ]

    config = {
        "model_type": args.hotspot_model_type,
        "chip_width_m": 0.0050,
        "chip_height_m": 0.0060,
        "grid_rows": args.hotspot_grid_rows,
        "grid_cols": args.hotspot_grid_cols,
        "spreader_width_m": 0.03,
        "spreader_height_m": 0.03,
        "spreader_thickness_m": 1e-3,
        "spreader_k_W_mK": 400.0,
        "spreader_vol_heat_cap_J_m3K": 3.55e6,
        "heatsink_width_m": 0.06,
        "heatsink_height_m": 0.06,
        "heatsink_thickness_m": 6.9e-3,
        "heatsink_k_W_mK": 400.0,
        "heatsink_vol_heat_cap_J_m3K": 3.55e6,
        "r_convec_K_per_W": 0.1,
        "c_convec_J_per_K": 140.4,
        "ambient_temp_K": args.hotspot_ambient_temp_k,
        "initial_temp_K": args.hotspot_initial_temp_k,
        "model_secondary": args.hotspot_model_secondary,
        "detailed_3D_used": False,
        "grid_map_mode": "center",
    }

    return HotSpotRuntimeAdapter(
        floorplan=floorplan,
        layers=layers,
        config=config,
        method=args.hotspot_method,
        max_external_step_s=args.hotspot_max_external_step_s,
    )


def _validate_sampling_intervals(args):
    thermal_enabled = (
        args.thermal_interval > 0 or args.thermal_interval_ticks > 0
    )

    if args.power_interval > 0 and args.power_interval_ticks > 0:
        raise ValueError(
            "--power_interval and --power_interval_ticks are mutually exclusive"
        )

    if args.thermal_interval > 0 and args.thermal_interval_ticks > 0:
        raise ValueError(
            "--thermal_interval and --thermal_interval_ticks are "
            "mutually exclusive"
        )

    if not thermal_enabled:
        return

    if args.power_interval > 0 or args.thermal_interval > 0:
        raise ValueError(
            "Thermal-coupled sampling is tick-only. "
            "Use --power_interval_ticks and --thermal_interval_ticks."
        )

    if args.power_interval_ticks <= 0:
        raise ValueError(
            "Thermal-coupled sampling requires --power_interval_ticks > 0"
        )

    if args.thermal_interval_ticks <= 0:
        raise ValueError(
            "Thermal-coupled sampling requires --thermal_interval_ticks > 0"
        )

    if args.thermal_interval_ticks % args.power_interval_ticks != 0:
        raise ValueError(
            "--thermal_interval_ticks must be an integer multiple of "
            "--power_interval_ticks"
        )

    if args.thermal_post_power_delay_ticks < 1:
        raise ValueError("--thermal_post_power_delay_ticks must be >= 1")


def _resolve_thermal_wait_timeout_ticks(args):
    if args.thermal_sample_wait_timeout_ticks > 0:
        return args.thermal_sample_wait_timeout_ticks

    return max(args.power_interval_ticks, args.thermal_interval_ticks)


def simulation_main(args):
    global _thermal_model
    global _hotspot_backend

    _validate_sampling_intervals(args)

    act_energies = init_act_energies(args)
    cache_static_regression = (
        O3_REGRESSION if args.cpu_type == "o3" else INORDER_REGRESSION
    )
    workload_args = []
    for item in args.workload_args:
        workload_args.extend(shlex.split(str(item)))

    cache_hierarchy = PrivateL1SharedL2CacheHierarchy(
        l1d_size="32kB",
        l1i_size="32kB",
        l2_size="1MB",
        power_interval=args.power_interval,
        power_interval_ticks=args.power_interval_ticks,
        trace_debug=args.power_trace_debug,
        static_regression=cache_static_regression,
    )
    mem = SingleChannelLPDDR3_1600("1GiB")
    processor = SimpleProcessor(
        cpu_type=cpus[args.cpu_type], num_cores=1, isa=ISA.ARM
    )
    board = SimpleBoard(
        clk_freq="2GHz",
        processor=processor,
        memory=mem,
        cache_hierarchy=cache_hierarchy,
    )

    for core in processor.get_cores():
        core.core.branchPred = BranchPredictor()
        core.core.branchPred.conditionalBranchPred = TournamentBP()

    if args.cpu_type == "o3":
        for core in processor.get_cores():
            _apply_pm(
                core,
                O3McPATCpuPowerModel,
                m5.objects.BaseO3CPU,
                act_energies,
                args.power_interval,
                args.power_interval_ticks,
                trace_debug=args.power_trace_debug,
            )
    else:
        for core in processor.get_cores():
            _apply_pm(
                core,
                InorderMcPATCpuPowerModel,
                m5.objects.BaseCPU,
                act_energies,
                args.power_interval,
                args.power_interval_ticks,
                trace_debug=args.power_trace_debug,
            )

    # Workload must be set before we force early wiring.
    if args.workload == "hello-world":
        board.set_se_binary_workload(
            binary=obtain_resource("arm-hello64-static"),
            arguments=workload_args,
        )
    else:
        this_dir = pathlib.Path(__file__).parent.absolute()
        workload_dir = (
            pathlib.Path(args.workload_dir)
            if args.workload_dir
            else this_dir / "workloads"
        )
        workload_path = str(workload_dir / args.workload)
        board.set_se_binary_workload(
            binary=BinaryResource(local_path=workload_path),
            arguments=workload_args,
        )

    # Force board/cache construction once so l1icaches/l1dcaches/l2cache exist
    # before thermal network creation.
    orig_connect_things = board._connect_things

    def _connect_things_once():
        if getattr(board, "_thermal_network_wired", False):
            return
        orig_connect_things()
        board._thermal_network_wired = True

    board._connect_things = _connect_things_once
    board._connect_things()

    # Stable labels so power-only and thermal traces both
    # use cpu0/l1i0/l1d0/l2.
    _set_power_trace_labels(board)

    if args.thermal_interval > 0 or args.thermal_interval_ticks > 0:
        _hotspot_backend = _build_hotspot_backend(args)
        board._hotspot_backend = _hotspot_backend  # keep Python object alive

        _thermal_model, _ = create_thermal_network(
            board,
            args.thermal_interval,
            thermal_interval_ticks=args.thermal_interval_ticks,
            thermal_post_power_delay_ticks=args.thermal_post_power_delay_ticks,
            sample_wait_timeout_ticks=_resolve_thermal_wait_timeout_ticks(
                args
            ),
            trace_debug=args.thermal_trace_debug,
            solver_callback=_hotspot_backend.solve,
            solver_reset_callback=_hotspot_backend.reset,
        )

    simulator = Simulator(board=board)
    simulator.run()


def parse_cli_args(_parser):
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "--cpu_type",
        type=str,
        choices=list(cpus.keys()),
        default="minor",
        help="CPU simulation mode. Default: %(default)s",
    )
    parser.add_argument(
        "--workload",
        type=str,
        choices=["hello-world", "iaxpy", "daxpy", "iax", "sax", "saxpy"],
        default="hello-world",
        help="Workload to run.",
    )
    parser.add_argument(
        "--workload_args",
        "--workload-args",
        nargs="*",
        default=[],
        dest="workload_args",
        help=(
            "Arguments passed to the SE workload binary. Supports either "
            "--workload-args 524288 2000 or --workload_args '524288 2000'."
        ),
    )
    parser.add_argument(
        "--workload_dir",
        type=str,
        default="",
        help="Directory containing SE workload binaries (default: ./workloads)",
    )
    parser.add_argument(
        "--power_interval",
        type=int,
        default=0,
        help="Sample top-level CPU/cache power every N cycles inside ROI."
        " 0 disables sampling.",
    )
    parser.add_argument(
        "--power_interval_ticks",
        type=int,
        default=0,
        help="Sample power every N simulated ticks. "
        "Overrides --power_interval.",
    )
    parser.add_argument(
        "--power_trace_debug",
        action="store_true",
        help="Enable debug CSV/console trace output for sampled power.",
    )

    parser.add_argument(
        "--thermal_interval",
        type=int,
        default=0,
        help="Step thermal model every N cycles inside ROI. "
        "For current runtime path this should equal --power_interval.",
    )
    parser.add_argument(
        "--thermal_interval_ticks",
        type=int,
        default=0,
        help="Step thermal model every N simulated ticks. "
        "Overrides --thermal_interval.",
    )
    parser.add_argument(
        "--thermal_post_power_delay_ticks",
        "--thermal-post-power-delay-ticks",
        type=int,
        default=1,
        dest="thermal_post_power_delay_ticks",
        help=(
            "Ticks to delay thermal consumption after expected power sample "
            "tick alignment."
        ),
    )
    parser.add_argument(
        "--thermal_sample_wait_timeout_ticks",
        type=int,
        default=0,
        help=(
            "Thermal wait timeout in ticks. 0 means auto; auto resolves to "
            "max(power_interval_ticks, thermal_interval_ticks)."
        ),
    )
    parser.add_argument(
        "--thermal_trace_debug",
        action="store_true",
        help="Enable temperature trace CSV output.",
    )

    parser.add_argument(
        "--hotspot_max_external_step_s",
        type=float,
        default=1e-6,
        help="Maximum HotSpot backend step size in seconds.",
    )
    parser.add_argument(
        "--hotspot_method",
        type=str,
        choices=["rk4", "euler"],
        default="rk4",
        help="Transient HotSpot integration method.",
    )
    parser.add_argument(
        "--hotspot_model_type",
        type=str,
        choices=["block", "grid"],
        default="block",
        help="HotSpot backend model type. "
        "'block' is the best default for runtime use.",
    )
    parser.add_argument(
        "--hotspot_grid_rows",
        type=int,
        default=64,
        help="HotSpot grid rows if --hotspot_model_type=grid.",
    )
    parser.add_argument(
        "--hotspot_grid_cols",
        type=int,
        default=64,
        help="HotSpot grid cols if --hotspot_model_type=grid.",
    )
    parser.add_argument(
        "--hotspot_ambient_temp_k",
        type=float,
        default=300.0,
        help="Ambient temperature (Kelvin) for the HotSpot backend. "
        "Default 300.0 matches thermal domain initial temp in "
        "thermal_helper.",
    )
    parser.add_argument(
        "--hotspot_initial_temp_k",
        type=float,
        default=300.0,
        help="Initial temperature (Kelvin) for the HotSpot backend. "
        "Default 300.0 matches thermal domain initial temp in "
        "thermal_helper.",
    )
    parser.add_argument(
        "--hotspot_model_secondary",
        action="store_true",
        help="Enable HotSpot secondary path (substrate/solder/PCB).",
    )

    return parser.parse_args()


parser = argparse.ArgumentParser()
args = parse_cli_args(parser)
simulation_main(args)
