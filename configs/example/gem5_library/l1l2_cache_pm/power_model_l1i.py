from mcpat_power_model.static_regression import leakage_w

import m5
from m5.objects import (
    L2XBar,
    Port,
    PowerModel,
    PowerModelPyFunc,
    SystemXBar,
)
from m5.stats import *

from .mcpat_power_model import McPATPowerModel


class L1IPowerOn(PowerModelPyFunc, McPATPowerModel):
    def __init__(
        self,
        l1icache,
        writeback,
        interval=0,
        interval_ticks=0,
        trace_debug=False,
        static_regression=None,
    ):
        super().__init__()
        self._simobj = l1icache
        self._interval = interval
        self._interval_ticks = interval_ticks
        self._sampling_enabled = interval > 0 or interval_ticks > 0
        self._stats = {}
        self._trace_debug = trace_debug
        self._static_regression = static_regression or {}

        # The SimObject that contains the stats we need.
        self._l1icache = l1icache
        self._writeback = writeback

        self._icache_data_ae = 7.80998e-12
        self._icache_tag_ae = 1.57793e-12

        self._icache_read_ae = self._icache_data_ae + self._icache_tag_ae
        self._icache_write_ae = 1.77257e-11

        self._icache_mb_read_ae = 6.13188e-12
        self._icache_mb_write_ae = 6.03686e-12
        self._icache_mb_search_ae = 5.31524e-12

        self._icache_ifb_read_ae = 3.19263e-12
        self._icache_ifb_write_ae = 3.17392e-12
        self._icache_ifb_search_ae = 2.93613e-12

        self._icache_pfb_read_ae = 3.19263e-12
        self._icache_pfb_write_ae = 3.17392e-12
        self._icache_pfb_search_ae = 2.93613e-12

        self._icache_wbb_read_ae = 3.19263e-12
        self._icache_wbb_write_ae = 3.17392e-12
        self._icache_wbb_search_ae = 2.93613e-12

        if self._sampling_enabled:
            self.pwr_interval = interval
            self.pwr_interval_ticks = interval_ticks
            self.enable_trace = trace_debug
            if trace_debug:
                self.trace_file = f"{m5.options.outdir}/power_trace.csv"

        self.reset = self.clear_sample_state
        self.dyn = lambda: self.dynamic_power()
        self.st = lambda: self.static_power()

    def static_power(self):
        return leakage_w(
            self._static_regression,
            "Instruction Cache",
            self.getTemperatureKelvin(),
        )

    def clear_sample_state(self):
        self._stats.clear()
        self._sample_stats_prepared = False
        self._sample_mode = False

    def dynamic_power(self):
        """Returns dynamic power in Watts"""
        self.set_sample_mode(
            self._sampling_enabled and self.inPowerAtInterval()
        )

        energy = self.icache_energy()
        energy += self.miss_buffer_energy()
        energy += self.inst_fill_buffer_energy()
        energy += self.prefetch_buffer_energy()
        power = self.convert_to_watts(energy)

        if self._trace_debug:
            print(f"L1I power: {power}")

        if getattr(self, "_sample_mode", False):
            self.reset_stats_dict()

        return power

    def icache_energy(self):
        read_accesses = self.get_stat("ReadReq.accesses")
        read_misses = self.get_stat("ReadReq.misses")
        read_hits = read_accesses - read_misses

        return (
            read_hits * self._icache_read_ae
            + read_misses * self._icache_read_ae
            + read_misses * self._icache_write_ae
        )

    def miss_buffer_energy(self):
        read_accesses = write_accesses = self.get_stat("ReadReq.misses")

        return (
            read_accesses * self._icache_mb_search_ae  # CAM Energy
            + write_accesses * self._icache_mb_write_ae  # Miss Energy
        )

    def inst_fill_buffer_energy(self):
        read_accesses = write_accesses = self.get_stat("ReadReq.misses")
        return (
            read_accesses * self._icache_ifb_search_ae
            + write_accesses * self._icache_ifb_write_ae
        )

    def prefetch_buffer_energy(self):
        read_accesses = write_accesses = self.get_stat("ReadReq.misses")
        return (
            read_accesses * self._icache_pfb_search_ae
            + write_accesses * self._icache_pfb_write_ae
        )


class L1IPowerOff(PowerModelPyFunc):
    def __init__(self):
        super().__init__()
        self.dyn = lambda: 0.0
        self.st = lambda: 0.0


class L1IPowerModel(PowerModel):
    def __init__(
        self,
        l1icache,
        writeback,
        interval=0,
        interval_ticks=0,
        trace_debug=False,
        static_regression=None,
    ):
        super().__init__()
        # Choose a power model for every power state
        self.pm = [
            L1IPowerOn(
                l1icache,
                writeback,
                interval,
                interval_ticks,
                trace_debug,
                static_regression,
            ),  # ON
            L1IPowerOff(),  # CLK_GATED
            L1IPowerOff(),  # SRAM_RETENTION
            L1IPowerOff(),  # OFF
        ]
