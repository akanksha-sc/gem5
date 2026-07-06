"""Wire SALAMTickEngine objects for legacy generated accelerator configs."""

from m5.objects import SALAMTickEngine
from m5.proxy import isproxy


def _salam_spm_clk_domain(system):
    return getattr(
        system,
        "acc_spm_clk_domain",
        getattr(
            system,
            "acc_mem_clk_domain",
            getattr(system, "acc_clk_domain", system.clk_domain),
        ),
    )


def _salam_mem_clk_domain(system):
    return getattr(
        system,
        "acc_mem_clk_domain",
        getattr(system, "acc_clk_domain", system.clk_domain),
    )


def wire_missing_salam_tick_engines(system):
    """Assign tick engines for ScratchpadMemory/RegisterBank at Parent.any."""
    for obj in system.descendants():
        kind = type(obj).__name__
        if kind not in ("ScratchpadMemory", "RegisterBank"):
            continue

        tick_engine = getattr(obj, "tick_engine", None)
        if tick_engine is not None and not isproxy(tick_engine):
            continue

        clk_domain = (
            _salam_spm_clk_domain(system)
            if kind == "ScratchpadMemory"
            else _salam_mem_clk_domain(system)
        )
        engine = SALAMTickEngine(clk_domain=clk_domain)
        obj.engine = engine
        obj.tick_engine = engine
        if kind == "ScratchpadMemory":
            obj.access_latency_cycles = 1
