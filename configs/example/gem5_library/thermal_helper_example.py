import m5
from m5.objects import (
    ThermalDomain,
    ThermalModel,
    ThermalModelPyFunc,
    ThermalNode,
)


def _fake_solver(domain_data, dt_seconds):
    """Validation solver: T_new = T_old + total_power"""
    new_temps = []
    for entry in domain_data:
        name, power, current_temp_k = entry[0], entry[1], entry[2]
        new_temps.append(current_temp_k + power)
    return new_temps


def _norm_pm(pm):
    """Unwrap gem5 vector-like power model wrapper."""
    try:
        pm = pm[0]
    except (TypeError, IndexError, KeyError):
        pass
    return pm


def create_thermal_network(
    board,
    thermal_interval,
    trace_debug=False,
    solver_callback=None,
):
    """Create thermal domains and model for ROI-controlled stepping.

    Returns (thermal_model, domain_list) where domain_list is a list of
    (tag, ThermalDomain, ThermalNode, PowerModel) tuples.

    The thermal core remains solver-agnostic. Any runtime backend may be
    injected via solver_callback(domain_data, dt_seconds).
    """
    bindings = []

    for i, core in enumerate(board.processor.get_cores()):
        bindings.append(
            (f"cpu{i}", core.core, _norm_pm(core.core.power_model))
        )

    ch = board.cache_hierarchy

    for i, l1i in enumerate(getattr(ch, "l1icaches", [])):
        bindings.append((f"l1i{i}", l1i, _norm_pm(l1i.power_model)))

    for i, l1d in enumerate(getattr(ch, "l1dcaches", [])):
        bindings.append((f"l1d{i}", l1d, _norm_pm(l1d.power_model)))

    l2 = getattr(ch, "l2cache", None)
    if l2 is not None:
        bindings.append(("l2", l2, _norm_pm(l2.power_model)))

    thermal_model = ThermalModel()
    thermal_model.step = 0.01
    thermal_model.thermal_interval = thermal_interval
    thermal_model.enable_trace = trace_debug
    if trace_debug:
        thermal_model.trace_file = f"{m5.options.outdir}/thermal_trace.csv"

    solver_obj = ThermalModelPyFunc()
    solver_obj.solve = (
        solver_callback if solver_callback is not None else _fake_solver
    )

    thermal_model.solver = solver_obj

    domain_list = []
    for tag, simobj, pm in bindings:
        domain = ThermalDomain()
        domain.initial_temperature = "26.85C"  # 300 K
        domain.power_model = pm
        domain.label = tag

        node = ThermalNode()
        domain_list.append((tag, domain, node, pm))

    board.thermal_model = thermal_model
    board.thermal_solver = solver_obj

    for tag, domain, node, _ in domain_list:
        setattr(board, f"thermal_domain_{tag}", domain)
        setattr(board, f"thermal_node_{tag}", node)

    for tag, domain, node, pm in domain_list:
        thermal_model.addDomain(domain, node)

    return thermal_model, domain_list
