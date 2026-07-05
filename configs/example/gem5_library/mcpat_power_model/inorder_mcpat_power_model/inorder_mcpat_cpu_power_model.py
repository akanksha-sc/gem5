import m5
from m5.objects import (
    BaseCPU,
    PowerModel,
    PowerModelPyFunc,
)

from ..static_regression import (
    INORDER_REGRESSION,
    leakage_w,
)
from .inorder_mcpat_exec_power_model import InorderMcPATExecutePower
from .inorder_mcpat_fetch_power_model import InorderMcPATFetchPower
from .inorder_mcpat_lsu_power_model import InorderMcPATLsuPower
from .inorder_mcpat_mmu_power_model import InorderMcPATMmuPower


class InorderMcPATCpuPowerOn(PowerModelPyFunc):
    def __init__(
        self,
        cpu: BaseCPU,
        act_energies,
        interval=0,
        interval_ticks=0,
        trace_debug=False,
    ):
        """core must be a BaseCPU core"""
        super().__init__()
        self._interval = interval
        self._interval_ticks = interval_ticks
        self._sampling_enabled = interval > 0 or interval_ticks > 0
        self._trace_debug = trace_debug
        self._fetch = InorderMcPATFetchPower(
            cpu, act_energies, 1.0, 0.9, interval, interval_ticks
        )
        self._lsu = InorderMcPATLsuPower(
            cpu, act_energies, 1.0, 0.71, interval, interval_ticks
        )
        self._mmu = InorderMcPATMmuPower(
            cpu, act_energies, 1.0, 0.71, interval, interval_ticks
        )
        self._exec = InorderMcPATExecutePower(
            cpu, act_energies, 1.0, 0.76, interval, interval_ticks
        )

        if self._sampling_enabled:
            self.pwr_interval = interval
            self.pwr_interval_ticks = interval_ticks
            self.enable_trace = trace_debug
            if trace_debug:
                self.trace_file = f"{m5.options.outdir}/power_trace.csv"

        self.reset = self.clear_sample_state
        self.dyn = self.dynamic_power
        self.st = self.static_power

    def _set_sample_mode(self, enable: bool):
        self._fetch.set_sample_mode(enable)
        self._lsu.set_sample_mode(enable)
        self._mmu.set_sample_mode(enable)
        self._exec.set_sample_mode(enable)

    def clear_sample_state(self):
        for sub in [self._fetch, self._lsu, self._mmu, self._exec]:
            if hasattr(sub, "clear_sample_state"):
                sub.clear_sample_state()
            else:
                sub.reset_stats_dict()

    def static_power(self):
        return leakage_w(
            INORDER_REGRESSION, "Core", self.getTemperatureKelvin()
        )

    def dynamic_power(self):
        sample_mode = self._sampling_enabled and self.inPowerAtInterval()
        self._set_sample_mode(sample_mode)

        total = (
            self._fetch.dynamic_power()
            + self._lsu.dynamic_power()
            + self._mmu.dynamic_power()
            + self._exec.dynamic_power()
        )

        if not self._sampling_enabled:
            self.print_mcpat(6, total)

        if sample_mode:
            self.reset_stats_dict()

        return total

    def reset_stats_dict(self):
        self._fetch.reset_stats_dict()
        self._lsu.reset_stats_dict()
        self._mmu.reset_stats_dict()
        self._exec.reset_stats_dict()

    def print_mcpat(self, indent, total):
        print("*" * 80)
        print("Core:")
        print(" " * indent + f"Runtime Dynamic = {total}\n")
        self._fetch.print_mcpat(indent)
        self._lsu.print_mcpat(indent)
        self._mmu.print_mcpat(indent)
        self._exec.print_mcpat(indent)
        print("*" * 80)


class InorderMcPATCpuPowerOff(PowerModelPyFunc):
    def __init__(self):
        super().__init__()
        self.dyn = lambda: 0.0
        self.st = lambda: 0.0


class InorderMcPATCpuPowerModel(PowerModel):
    def __init__(
        self,
        core,
        act_energies,
        interval=0,
        interval_ticks=0,
        trace_debug=False,
    ):
        super().__init__()
        # Choose a power model for every power state
        self.pm = [
            InorderMcPATCpuPowerOn(
                core, act_energies, interval, interval_ticks, trace_debug
            ),  # ON
            InorderMcPATCpuPowerOff(),  # CLK_GATED
            InorderMcPATCpuPowerOff(),  # SRAM_RETENTION
            InorderMcPATCpuPowerOff(),  # OFF
        ]
