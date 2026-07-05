import m5
from m5.objects import PowerModelPyFunc

from .salam_stat_reader import SalamStatReader
from .static_regression import (
    SALAM_REGRESSION,
    leakage_w,
)

COMPONENT_INDEX = {
    "fuDynamic": 0,
    "fuStatic": 1,
    "regDynamic": 2,
    "regStatic": 3,
    "spmReadDynamic": 4,
    "spmWriteDynamic": 5,
    "spmStatic": 6,
}

BLOCK_DYNAMIC_COMPONENTS = {
    "datapath": ["fuDynamic"],
    "registers": ["regDynamic"],
    "spm": ["spmReadDynamic", "spmWriteDynamic"],
}

BLOCK_STATIC_COMPONENT = {
    "datapath": "Datapath",
    "registers": "Register",
    "spm": "SPM",
}


class SalamBlockPowerOn(PowerModelPyFunc):
    """Interval power model for one SALAM major block (datapath/reg/SPM)."""

    def __init__(
        self,
        stat_source,
        block,
        label,
        interval=0,
        interval_ticks=0,
        trace_debug=False,
    ):
        super().__init__()

        if block not in BLOCK_DYNAMIC_COMPONENTS:
            raise ValueError(f"Unknown SALAM power block: {block}")

        self._reader = SalamStatReader(stat_source, interval, interval_ticks)
        self._block = block
        self._dynamic_components = BLOCK_DYNAMIC_COMPONENTS[block]
        self._static_component = BLOCK_STATIC_COMPONENT[block]
        self._sampling_enabled = interval > 0 or interval_ticks > 0

        self.trace_label = label
        if self._sampling_enabled:
            self.pwr_interval = interval
            self.pwr_interval_ticks = interval_ticks
            self.enable_trace = trace_debug
            if trace_debug:
                self.trace_file = f"{m5.options.outdir}/power_trace.csv"

        self.reset = self.clear_sample_state
        self.dyn = self.dynamic_power
        self.st = self.static_power

    def clear_sample_state(self):
        self._reader.clear_sample_state()

    def _sample_mode_active(self):
        return self._sampling_enabled and self.inPowerAtInterval()

    def _cycle_delta(self):
        stat = self._reader.get_stat("power.accCycles")
        return float(stat.total)

    def _component_energy_mw_cycles(self):
        stat_info = self._reader.get_stat("power.componentEnergy")
        values = stat_info.value
        if not isinstance(values, list):
            values = [values]

        total = 0.0
        for comp in self._dynamic_components:
            total += float(values[COMPONENT_INDEX[comp]])
        return total

    def _dynamic_mw(self):
        self._reader.set_sample_mode(self._sample_mode_active())

        energy_mw_cycles = self._component_energy_mw_cycles()

        cycles = self._cycle_delta()
        if cycles <= 0.0:
            return 0.0
        return energy_mw_cycles / cycles

    def dynamic_power(self):
        dyn_w = self._dynamic_mw() * 1e-3
        if self._sample_mode_active():
            self._reader.reset_stats_dict()
        return dyn_w

    def static_power(self):
        return leakage_w(
            SALAM_REGRESSION,
            self._static_component,
            self.getTemperatureKelvin(),
        )


class SalamBlockPowerOff(PowerModelPyFunc):
    def __init__(self, label):
        super().__init__()
        self.trace_label = f"{label}.off"
        self.dyn = lambda: 0.0
        self.st = lambda: 0.0
