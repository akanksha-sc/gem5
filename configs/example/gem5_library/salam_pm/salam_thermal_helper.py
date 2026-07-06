"""SALAM interval power sampling and thermal domain wiring."""

from power_thermal_runtime import (
    create_thermal_network_from_bindings,
    normalize_power_model,
)

from .salam_block_power_model import SalamBlockPowerModel
from .salam_hotspot_backend import build_lazy_salam_hotspot_backend

SALAM_BLOCKS = ("datapath", "registers", "spm")


def _normalize_board_prefix(path: str) -> str:
    return path.replace("<orphan SALAMArmBoard>", "board")


def _block_prefix_from_stat_source(stat_source) -> str:
    return _normalize_board_prefix(
        stat_source.path().rsplit(".hw_interface.salam_power_model", 1)[0]
    )


def _safe_attr_label(label: str) -> str:
    out = []
    for ch in label:
        out.append(ch if ch.isalnum() or ch == "_" else "_")
    return "".join(out)


def _iter_salam_stat_sources(board):
    for obj in board.descendants():
        if type(obj).__name__ != "HWInterface":
            continue
        stat_source = getattr(obj, "salam_power_model", None)
        if stat_source is None:
            continue
        prefix = _block_prefix_from_stat_source(stat_source)
        yield prefix, stat_source


def _llvm_for_hw_interface(hw_interface):
    parent = getattr(hw_interface, "_parent", None)
    if parent is None:
        return None
    return getattr(parent, "llvm_interface", None)


def collect_salam_power_bindings(
    board,
    interval=0,
    interval_ticks=0,
    trace_debug=False,
):
    bindings = []
    used_attrs = set()

    for obj in board.descendants():
        if type(obj).__name__ != "HWInterface":
            continue
        stat_source = getattr(obj, "salam_power_model", None)
        if stat_source is None:
            continue
        prefix = _block_prefix_from_stat_source(stat_source)
        llvm = _llvm_for_hw_interface(obj)

        for block in SALAM_BLOCKS:
            label = f"{prefix}.{block}"
            pm = SalamBlockPowerModel(
                stat_source,
                block,
                label,
                interval,
                interval_ticks,
                trace_debug,
            )
            safe = _safe_attr_label(label)
            attr = f"salam_pm_{safe}"
            if attr in used_attrs:
                raise ValueError(f"Duplicate SALAM PM attribute: {attr}")
            used_attrs.add(attr)
            setattr(board, attr, pm)
            if llvm is not None:
                llvm.power_state.default_state = "ON"
                llvm.power_model = list(getattr(llvm, "power_model", [])) + [
                    pm
                ]
            bindings.append((label, stat_source, pm))

    return bindings


class SimpleSalamThermalSolver:
    """Lightweight RC-style solver for bring-up and smoke tests."""

    def __init__(self, labels, ambient_temp_k=300.0, thermal_resistance=50.0):
        self._labels = list(labels)
        self._ambient = float(ambient_temp_k)
        self._r_th = float(thermal_resistance)
        self._temps = {label: self._ambient for label in self._labels}

    def reset(self, initial_temp_k):
        initial_temp_k = float(initial_temp_k)
        self._temps = {label: initial_temp_k for label in self._labels}

    def solve(self, domain_data, dt_seconds):
        dt_seconds = float(dt_seconds)
        out = {}
        for label, power_w, current_temp_k in domain_data:
            t = float(current_temp_k)
            p = float(power_w)
            t_new = t + (p * self._r_th * dt_seconds)
            t_new = max(self._ambient, t_new)
            self._temps[label] = t_new
            out[label] = t_new
        return out


def create_salam_thermal_network(
    board,
    interval=0,
    interval_ticks=0,
    thermal_interval=0,
    thermal_interval_ticks=0,
    thermal_post_power_delay_ticks=1,
    sample_wait_timeout_ticks=0,
    trace_debug=False,
    ambient_temp_k=300.0,
    thermal_solver="hotspot",
    hotspot_args=None,
    floorplan_geometry="area",
    auto_start_power=False,
    auto_start_thermal=False,
):
    bindings = collect_salam_power_bindings(
        board,
        interval=interval,
        interval_ticks=interval_ticks,
        trace_debug=trace_debug,
    )

    if not bindings:
        return None, []

    labels = [label for label, _src, _pm in bindings]
    if thermal_solver == "hotspot":
        if hotspot_args is None:
            raise ValueError(
                "HotSpot thermal solver requires hotspot_args namespace"
            )
        solver_backend = build_lazy_salam_hotspot_backend(
            labels,
            bindings,
            hotspot_args,
            geometry_mode=floorplan_geometry,
            trace_debug=trace_debug,
        )
    elif thermal_solver == "simple":
        solver_backend = SimpleSalamThermalSolver(
            labels, ambient_temp_k=ambient_temp_k
        )
    else:
        raise ValueError(f"Unknown SALAM thermal solver: {thermal_solver}")

    def solver_callback(domain_data, dt_seconds):
        return solver_backend.solve(domain_data, dt_seconds)

    def solver_reset_callback(initial_temp_k):
        solver_backend.reset(initial_temp_k)

    thermal_model, _domain_list = create_thermal_network_from_bindings(
        board,
        bindings,
        thermal_interval=thermal_interval,
        thermal_interval_ticks=thermal_interval_ticks,
        thermal_post_power_delay_ticks=thermal_post_power_delay_ticks,
        sample_wait_timeout_ticks=sample_wait_timeout_ticks,
        trace_debug=trace_debug,
        solver_callback=solver_callback,
        solver_reset_callback=solver_reset_callback,
        auto_start_power=auto_start_power,
        auto_start_thermal=auto_start_thermal,
    )

    return thermal_model, bindings


def get_salam_power_models(board):
    out = []
    for obj in board.descendants():
        if type(obj).__name__ != "PowerModel":
            continue
        leaf = obj.path().rsplit(".", 1)[-1]
        if not leaf.startswith("salam_pm_"):
            continue
        out.append(normalize_power_model(obj))
    return out
