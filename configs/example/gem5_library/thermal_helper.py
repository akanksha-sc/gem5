from power_thermal_runtime import (
    create_thermal_network_from_bindings,
    normalize_power_model,
)


def collect_cpu_cache_power_bindings(board):
    bindings = []

    for i, core in enumerate(board.processor.get_cores()):
        bindings.append(
            (
                f"cpu{i}",
                core.core,
                normalize_power_model(core.core.power_model),
            )
        )

    ch = board.cache_hierarchy

    for i, l1i in enumerate(getattr(ch, "l1icaches", [])):
        bindings.append(
            (f"l1i{i}", l1i, normalize_power_model(l1i.power_model))
        )

    for i, l1d in enumerate(getattr(ch, "l1dcaches", [])):
        bindings.append(
            (f"l1d{i}", l1d, normalize_power_model(l1d.power_model))
        )

    l2 = getattr(ch, "l2cache", None)
    if l2 is not None:
        bindings.append(("l2", l2, normalize_power_model(l2.power_model)))

    return bindings


def create_thermal_network(
    board,
    thermal_interval,
    thermal_interval_ticks=0,
    thermal_post_power_delay_ticks=1,
    sample_wait_timeout_ticks=0,
    trace_debug=False,
    solver_callback=None,
    solver_reset_callback=None,
    auto_start_power=False,
    auto_start_thermal=False,
):
    if solver_callback is None or solver_reset_callback is None:
        raise ValueError(
            "create_thermal_network requires a real thermal solver. "
            "Pass HotSpotRuntimeAdapter.solve and HotSpotRuntimeAdapter.reset."
        )

    bindings = collect_cpu_cache_power_bindings(board)
    return create_thermal_network_from_bindings(
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
