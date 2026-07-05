import csv
import pathlib

import m5
from m5.objects import (
    BaseO3CPU,
    PowerModel,
    PowerModelPyFunc,
)

from ..static_regression import (
    O3_REGRESSION,
    leakage_w,
)
from .o3_mcpat_exec_power_model import O3McPATExecutePower
from .o3_mcpat_fetch_power_model import O3McPATFetchPower
from .o3_mcpat_lsu_power_model import O3McPATLsuPower
from .o3_mcpat_mmu_power_model import O3McPATMmuPower
from .o3_mcpat_renaming_unit_power_model import O3McPATRenamingUnitPower


class O3McPATCpuPowerOn(PowerModelPyFunc):
    def __init__(
        self,
        cpu: BaseO3CPU,
        act_energies,
        interval=0,
        interval_ticks=0,
        trace_debug=False,
    ):
        """core must be a BaseO3CPU core"""
        super().__init__()
        self._interval = interval
        self._interval_ticks = interval_ticks
        self._sampling_enabled = interval > 0 or interval_ticks > 0
        self._trace_debug = trace_debug
        self._trace_prefix = "board.processor.cores.core"
        self._fetch = O3McPATFetchPower(
            cpu, act_energies, 1.0, 0.9, interval, interval_ticks
        )
        self._rnu = O3McPATRenamingUnitPower(
            cpu, act_energies, 1.0, interval, interval_ticks
        )
        self._lsu = O3McPATLsuPower(
            cpu, act_energies, 1.0, 0.71, interval, interval_ticks
        )
        self._mmu = O3McPATMmuPower(
            cpu, act_energies, 1.0, 0.71, interval, interval_ticks
        )
        self._exec = O3McPATExecutePower(
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
        self._rnu.set_sample_mode(enable)
        self._lsu.set_sample_mode(enable)
        self._mmu.set_sample_mode(enable)
        self._exec.set_sample_mode(enable)

    def clear_sample_state(self):
        for sub in [self._fetch, self._rnu, self._lsu, self._mmu, self._exec]:
            if hasattr(sub, "clear_sample_state"):
                sub.clear_sample_state()
            else:
                sub.reset_stats_dict()

    def static_power(self):
        return leakage_w(O3_REGRESSION, "Core", self.getTemperatureKelvin())

    def dynamic_power(self):
        sample_mode = self._sampling_enabled and self.inPowerAtInterval()
        self._set_sample_mode(sample_mode)

        fetch = self._fetch.dynamic_power()
        rnu = self._rnu.dynamic_power()
        lsu = self._lsu.dynamic_power()
        mmu = self._mmu.dynamic_power()
        exu = self._exec.dynamic_power()

        total = fetch + rnu + lsu + mmu + exu

        if sample_mode and self._trace_debug:
            self._append_subcomponent_trace(
                [
                    (f"{self._trace_prefix}.fetch", fetch),
                    (f"{self._trace_prefix}.rename", rnu),
                    (f"{self._trace_prefix}.lsu", lsu),
                    (f"{self._trace_prefix}.mmu", mmu),
                    (f"{self._trace_prefix}.execute", exu),
                ]
            )
        elif not self._sampling_enabled:
            self.print_mcpat(6, total)

        if sample_mode:
            self.reset_stats_dict()

        return total

    def reset_stats_dict(self):
        self._fetch.reset_stats_dict()
        self._rnu.reset_stats_dict()
        self._lsu.reset_stats_dict()
        self._mmu.reset_stats_dict()
        self._exec.reset_stats_dict()

    def print_mcpat(self, indent, total):
        print("*" * 80)
        print("Core:")
        print(" " * indent + f"Runtime Dynamic = {total}\n")
        self._fetch.print_mcpat(indent)
        self._rnu.print_mcpat(indent)
        self._lsu.print_mcpat(indent)
        self._mmu.print_mcpat(indent)
        self._exec.print_mcpat(indent)
        print("*" * 80)

    def _append_subcomponent_trace(self, rows):
        if (
            not self._sampling_enabled
            or not self._trace_debug
            or not self.inPowerAtInterval()
        ):
            return

        trace_file = getattr(self, "trace_file", "")
        if not trace_file:
            return

        trace_path = pathlib.Path(trace_file)
        file_exists = trace_path.exists()

        with trace_path.open("a", newline="") as fp:
            writer = csv.writer(fp)
            if not file_exists:
                writer.writerow(
                    [
                        "tick",
                        "label",
                        "dynamic_power_w",
                        "static_power_w",
                        "total_power_w",
                        "sample_duration_ticks",
                        "sample_duration_s",
                        "temp_k",
                        "sample_kind",
                    ]
                )

            temp_k = self.getTemperatureKelvin()
            dur_s = self.getSampleDurationSeconds()
            dur_ticks = int(dur_s * 1e12) if dur_s > 0 else 0

            for name, dyn in rows:
                writer.writerow(
                    [
                        m5.curTick(),
                        name,
                        f"{dyn:.17g}",
                        f"{0.0:.17g}",
                        f"{dyn:.17g}",
                        dur_ticks,
                        f"{dur_s:.17g}",
                        f"{temp_k:.17g}",
                        "interval",
                    ]
                )


class O3McPATCpuPowerOff(PowerModelPyFunc):
    def __init__(self):
        super().__init__()
        self.dyn = lambda: 0.0
        self.st = lambda: 0.0


class O3McPATCpuPowerModel(PowerModel):
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
            O3McPATCpuPowerOn(
                core, act_energies, interval, interval_ticks, trace_debug
            ),  # ON
            O3McPATCpuPowerOff(),  # CLK_GATED
            O3McPATCpuPowerOff(),  # SRAM_RETENTION
            O3McPATCpuPowerOff(),  # OFF
        ]
