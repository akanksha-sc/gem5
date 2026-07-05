import pathlib

import m5
from m5.objects import (
    ThermalDomain,
    ThermalModel,
    ThermalModelPyFunc,
    ThermalNode,
)


def normalize_power_model(pm):
    """Unwrap gem5 vector-like power_model wrappers."""
    try:
        return pm[0]
    except (TypeError, IndexError, KeyError):
        return pm


def get_on_state(pm):
    pm = normalize_power_model(pm)
    return pm.pm[0]


def _set_trace_label_if_possible(pm, label):
    try:
        on = get_on_state(pm)
    except Exception:
        return

    if hasattr(on, "trace_label") and not getattr(on, "trace_label", ""):
        on.trace_label = label


def _set_power_auto_start_if_possible(pm, enable):
    try:
        on = get_on_state(pm)
    except Exception:
        return

    if hasattr(on, "auto_start"):
        on.auto_start = bool(enable)


def set_power_sampling_auto_start(power_models, enable):
    for pm in power_models:
        _set_power_auto_start_if_possible(pm, enable)


def _safe_attr_label(label: str) -> str:
    out = []
    for ch in label:
        out.append(ch if ch.isalnum() or ch == "_" else "_")
    return "".join(out)


def start_power_sampling(power_models):
    for pm in power_models:
        on = get_on_state(pm)

        cycle_interval = int(getattr(on, "pwr_interval", 0))
        tick_interval = int(getattr(on, "pwr_interval_ticks", 0))
        if cycle_interval > 0 or tick_interval > 0:
            on.startSampling()


def stop_power_sampling(power_models):
    for pm in power_models:
        on = get_on_state(pm)
        cycle_interval = int(getattr(on, "pwr_interval", 0))
        tick_interval = int(getattr(on, "pwr_interval_ticks", 0))
        if cycle_interval > 0 or tick_interval > 0:
            on.stopSampling()


def sample_power_now(power_models):
    for pm in power_models:
        on = get_on_state(pm)
        cycle_interval = int(getattr(on, "pwr_interval", 0))
        tick_interval = int(getattr(on, "pwr_interval_ticks", 0))
        if cycle_interval > 0 or tick_interval > 0:
            if hasattr(on, "isSamplingActive") and not on.isSamplingActive():
                continue
            on.sampleNow()


def create_thermal_network_from_bindings(
    parent,
    bindings,
    thermal_interval=0,
    thermal_interval_ticks=0,
    thermal_post_power_delay_ticks=1,
    sample_wait_timeout_ticks=0,
    trace_debug=False,
    trace_file=None,
    solver_callback=None,
    solver_reset_callback=None,
    auto_start_power=False,
    auto_start_thermal=False,
):
    """
    bindings: iterable of (label, simobj, power_model)

    Returns (thermal_model, domain_list), where domain_list entries are:
        (label, ThermalDomain, ThermalNode, PowerModel)
    """
    bindings = list(bindings)

    labels = [label for label, _simobj, _pm in bindings]
    if len(labels) != len(set(labels)):
        raise ValueError(f"Duplicate thermal labels: {labels}")

    for label, _simobj, pm in bindings:
        if pm is None:
            raise ValueError(f"Thermal binding {label} has no power model")
        try:
            on = get_on_state(pm)
        except Exception as exc:
            raise ValueError(
                f"Thermal binding {label} has invalid power model"
            ) from exc
        if on is None:
            raise ValueError(
                f"Thermal binding {label} has invalid power model"
            )

    thermal_model = ThermalModel()
    thermal_model.step = 0.01
    thermal_model.thermal_interval = thermal_interval
    thermal_model.thermal_interval_ticks = thermal_interval_ticks
    thermal_model.thermal_post_power_delay_ticks = (
        thermal_post_power_delay_ticks
    )
    thermal_model.sample_wait_timeout_ticks = sample_wait_timeout_ticks
    thermal_model.auto_start = bool(auto_start_thermal)
    thermal_model.enable_trace = trace_debug

    if trace_file is None and trace_debug:
        trace_file = f"{m5.options.outdir}/thermal_trace.csv"
    if trace_file:
        thermal_model.trace_file = trace_file

    if solver_callback is None or solver_reset_callback is None:
        raise ValueError(
            "Thermal stepping requires an explicit thermal solver backend. "
            "Use arm_a9_with_mcpat_and_hotspot.py or provide solver_callback "
            "and solver_reset_callback."
        )

    solver_obj = ThermalModelPyFunc()
    solver_obj.solve = solver_callback
    solver_obj.reset = solver_reset_callback
    thermal_model.solver = solver_obj

    domain_list = []
    for label, _simobj, pm in bindings:
        pm = normalize_power_model(pm)
        _set_trace_label_if_possible(pm, label)
        _set_power_auto_start_if_possible(pm, auto_start_power)

        domain = ThermalDomain()
        domain.initial_temperature = "26.85C"  # 300 K
        domain.power_model = pm
        domain.label = label

        node = ThermalNode()
        domain_list.append((label, domain, node, pm))

    parent.thermal_model = thermal_model

    used_attrs = set()
    for label, domain, node, _pm in domain_list:
        safe = _safe_attr_label(label)
        if safe in used_attrs:
            raise ValueError(
                f"Duplicate sanitized thermal label: {label} -> {safe}"
            )
        used_attrs.add(safe)

        setattr(parent, f"thermal_domain_{safe}", domain)
        setattr(parent, f"thermal_node_{safe}", node)

    if not hasattr(parent, "thermal_components"):
        parent.thermal_components = []

    for _label, domain, node, _pm in domain_list:
        thermal_model.addDomain(domain, node)

    return thermal_model, domain_list


def remove_trace_files(outdir, *names):
    outdir = pathlib.Path(outdir)
    for name in names:
        path = outdir / name
        if path.exists():
            path.unlink()
