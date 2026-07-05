# configs/example/gem5_library/salam/check_salam_pm_domains.py

"""
Dry-run SALAM stdlib power-domain discovery.

This script does not run the benchmark. It:
  - builds the stdlib SALAM board,
  - attaches generated SALAM accelerator topology,
  - scans board.descendants(),
  - attaches dummy PowerModel objects to accelerator LLVMInterface objects,
  - prints candidate SALAM power/thermal domains.

Use this before wiring the real SALAM McPAT/CACTI power model.
"""

import argparse
import importlib
import os
import sys
from pathlib import Path
from types import SimpleNamespace

from m5.objects import (
    PowerModel,
    PowerModelPyFunc,
    VExpress_GEM5_V1,
)
from m5.util import fatal

from gem5.components.boards.salam_arm_board import (
    SALAMArmBoard,
    SALAMClockConfig,
)
from gem5.components.cachehierarchies.classic.private_l1_private_l2_cache_hierarchy import (
    PrivateL1PrivateL2CacheHierarchy,
)
from gem5.components.memory import DualChannelDDR4_2400
from gem5.components.processors.cpu_types import CPUTypes
from gem5.components.processors.simple_processor import SimpleProcessor
from gem5.isas import ISA
from gem5.utils.requires import requires

requires(isa_required=ISA.ARM)


class DummySALAMPowerOn(PowerModelPyFunc):
    """
    Dummy ON-state power model.

    This intentionally returns small nonzero values so we can verify that a
    PowerModel can be attached to SALAM accelerator execution objects.
    """

    def __init__(self, label: str, pwr_interval_ticks: int = 0):
        super().__init__()

        self.trace_label = label
        self.pwr_interval_ticks = pwr_interval_ticks

        self.dyn = self.dynamic_power
        self.st = self.static_power

    def dynamic_power(self):
        return 0.001

    def static_power(self):
        return 0.0001


class DummyZeroPower(PowerModelPyFunc):
    """Dummy inactive-state model."""

    def __init__(self, label: str):
        super().__init__()

        self.trace_label = f"{label}.zero"
        self.dyn = lambda: 0.0
        self.st = lambda: 0.0


class DummySALAMPowerModel(PowerModel):
    """
    gem5 PowerModel wrapper.

    gem5 PowerModel expects one PowerModelState per power state. The first
    state is the ON state; the others are dummy zero-power states.
    """

    def __init__(self, label: str, pwr_interval_ticks: int = 0):
        super().__init__()

        on = DummySALAMPowerOn(
            label=label,
            pwr_interval_ticks=pwr_interval_ticks,
        )
        off0 = DummyZeroPower(label)
        off1 = DummyZeroPower(label)
        off2 = DummyZeroPower(label)

        self.pm = [on, off0, off1, off2]


def parse_args():
    p = argparse.ArgumentParser()

    p.add_argument("--m5-path", required=True)
    p.add_argument("--acc-bench-path", required=True)
    p.add_argument("--bench", required=True)
    p.add_argument("--bench-path", required=True)
    p.add_argument("--config-name", default="config.yml")
    p.add_argument("--kernel", required=True)

    p.add_argument("--sys-clock", default="1GHz")
    p.add_argument("--sys-voltage", default="1.0V")
    p.add_argument("--mem-size", default="16GB")

    p.add_argument("--acc-clock", default=None)
    p.add_argument("--acc-voltage", default=None)
    p.add_argument("--acc-compute-clock", default=None)
    p.add_argument("--acc-compute-voltage", default=None)
    p.add_argument("--acc-mem-clock", default=None)
    p.add_argument("--acc-mem-voltage", default=None)
    p.add_argument("--acc-dma-clock", default=None)
    p.add_argument("--acc-dma-voltage", default=None)
    p.add_argument("--acc-localbus-clock", default=None)
    p.add_argument("--acc-localbus-voltage", default=None)

    p.add_argument(
        "--pm-interval-ticks",
        type=int,
        default=0,
        help=(
            "Dummy power sampling interval in global ticks. "
            "Use 0 to only attach and list models."
        ),
    )

    return p.parse_args()


def make_salam_options(args):
    return SimpleNamespace(
        acc_cache=False,
        l2cache=True,
        accpath=os.path.join(args.acc_bench_path, args.bench_path),
        accbench=args.bench,
        acc_clock=args.acc_clock,
        acc_voltage=args.acc_voltage,
        acc_compute_clock=args.acc_compute_clock,
        acc_compute_voltage=args.acc_compute_voltage,
        acc_mem_clock=args.acc_mem_clock,
        acc_mem_voltage=args.acc_mem_voltage,
        acc_dma_clock=args.acc_dma_clock,
        acc_dma_voltage=args.acc_dma_voltage,
        acc_localbus_clock=args.acc_localbus_clock,
        acc_localbus_voltage=args.acc_localbus_voltage,
    )


def make_salam_clocks(args):
    compute_clock = args.acc_compute_clock or args.acc_clock or args.sys_clock
    compute_voltage = (
        args.acc_compute_voltage or args.acc_voltage or args.sys_voltage
    )

    mem_clock = args.acc_mem_clock or compute_clock
    mem_voltage = args.acc_mem_voltage or compute_voltage

    dma_clock = args.acc_dma_clock or mem_clock
    dma_voltage = args.acc_dma_voltage or mem_voltage

    localbus_clock = args.acc_localbus_clock or mem_clock
    localbus_voltage = args.acc_localbus_voltage or mem_voltage

    return SALAMClockConfig(
        compute_clock=compute_clock,
        compute_voltage=compute_voltage,
        mem_clock=mem_clock,
        mem_voltage=mem_voltage,
        dma_clock=dma_clock,
        dma_voltage=dma_voltage,
        localbus_clock=localbus_clock,
        localbus_voltage=localbus_voltage,
    )


def import_generated_salam_module(args):
    salam_config_dir = Path(args.m5_path) / "configs" / "SALAM"
    generated = salam_config_dir / f"{args.bench}.py"

    if not generated.is_file():
        fatal(
            f"Generated SALAM config not found: {generated}. "
            "Run systembuilder.py or run_system_stdlib.sh first."
        )

    sys.path.insert(0, str(salam_config_dir))
    return importlib.import_module(args.bench)


def obj_type_name(obj):
    return obj.__class__.__name__


def safe_name(obj):
    try:
        return obj.name()
    except Exception:
        return repr(obj)


def safe_devname(obj):
    try:
        return str(getattr(obj, "devicename"))
    except Exception:
        return safe_name(obj)


def safe_descendants(obj):
    try:
        return list(obj.descendants())
    except Exception:
        return []


def is_comm_interface(obj):
    return obj_type_name(obj) == "CommInterface"


def has_llvm_interface(obj):
    return hasattr(obj, "llvm_interface")


def find_fu_like_objects(root):
    """
    Heuristic FU discovery.

    This is intentionally loose because this is only a check script.
    The real model should use the known SALAM HWInterface/FunctionalUnits
    structure directly.
    """
    out = []

    for obj in safe_descendants(root):
        has_energy = hasattr(obj, "dynamic_energy")
        has_leakage = hasattr(obj, "leakage_power")
        has_area = hasattr(obj, "area")
        type_name = obj_type_name(obj).lower()

        type_looks_fu = any(
            token in type_name
            for token in [
                "adder",
                "multiplier",
                "divider",
                "shifter",
                "bitwise",
                "register",
            ]
        )

        if has_energy or has_leakage or has_area or type_looks_fu:
            out.append(obj)

    return out


def apply_dummy_salam_power_models(board, interval_ticks=0):
    """
    Attach one dummy PowerModel per accelerator LLVMInterface.

    Returns:
        list of (label, comm_interface, llvm_interface, power_model)
    """
    bindings = []

    for obj in board.descendants():
        if not is_comm_interface(obj):
            continue
        if not has_llvm_interface(obj):
            continue

        comm = obj
        llvm = comm.llvm_interface

        label = safe_devname(comm)
        if not label:
            label = safe_name(llvm)

        llvm.power_state.default_state = "ON"
        llvm.power_model = DummySALAMPowerModel(
            label=label,
            pwr_interval_ticks=interval_ticks,
        )

        bindings.append((label, comm, llvm, llvm.power_model))

    return bindings


def print_salam_power_domains(bindings):
    print()
    print("Discovered SALAM accelerator power domains")
    print("=========================================")

    if not bindings:
        print("No CommInterface objects with llvm_interface were found.")
        print(
            "This usually means makeHWAcc() has not run yet, or the generated "
            "SALAM topology did not attach correctly."
        )
        return

    for idx, (label, comm, llvm, pm) in enumerate(bindings):
        print(f"[{idx}] domain label: {label}")
        print(f"    CommInterface: {safe_name(comm)}")
        print(f"    LLVMInterface: {safe_name(llvm)}")
        print(f"    PowerModel:    {safe_name(pm)}")

        fus = find_fu_like_objects(llvm)
        print(f"    FU-like descendants found under LLVMInterface: {len(fus)}")

        for fu in fus[:20]:
            pieces = [f"type={obj_type_name(fu)}", f"name={safe_name(fu)}"]

            for attr in ["dynamic_energy", "leakage_power", "area"]:
                if hasattr(fu, attr):
                    try:
                        pieces.append(f"{attr}={getattr(fu, attr)}")
                    except Exception:
                        pieces.append(f"{attr}=<unresolved>")

            print("      - " + ", ".join(pieces))

        if len(fus) > 20:
            print(f"      ... {len(fus) - 20} more FU-like objects omitted")

    print()


def main():
    args = parse_args()

    if not Path(args.kernel).is_file():
        fatal(f"Bare-metal ELF not found: {args.kernel}")

    salam_module = import_generated_salam_module(args)
    salam_options = make_salam_options(args)
    salam_clocks = make_salam_clocks(args)

    processor = SimpleProcessor(
        cpu_type=CPUTypes.O3,
        isa=ISA.ARM,
        num_cores=1,
    )

    cache_hierarchy = PrivateL1PrivateL2CacheHierarchy(
        l1d_size="32kB",
        l1i_size="32kB",
        l2_size="256kB",
    )

    memory = DualChannelDDR4_2400(size=args.mem_size)

    board = SALAMArmBoard(
        clk_freq=args.sys_clock,
        processor=processor,
        memory=memory,
        cache_hierarchy=cache_hierarchy,
        salam_module=salam_module,
        salam_options=salam_options,
        salam_clocks=salam_clocks,
        baremetal_binary=args.kernel,
        platform=VExpress_GEM5_V1(),
    )

    # This forces stdlib board setup and generated makeHWAcc() attachment.
    # We intentionally do not call m5.instantiate() or run the benchmark.
    board._pre_instantiate(full_system=True)

    bindings = apply_dummy_salam_power_models(
        board,
        interval_ticks=args.pm_interval_ticks,
    )

    print_salam_power_domains(bindings)

    print("Dummy SALAM PM discovery complete.")
    print("This checked attachment points only; it did not run the benchmark.")


if __name__ == "__m5_main__":
    main()
