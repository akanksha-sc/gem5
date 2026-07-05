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

"""
NOTE: THIS MODELS A *SHARED* L2 Cache. You should see the L1I/L1D models
for a private cache.
"""


class L2PowerOn(PowerModelPyFunc, McPATPowerModel):
    def __init__(
        self,
        l2cache,
        writeback,
        interval=0,
        interval_ticks=0,
        trace_debug=False,
        static_regression=None,
    ):
        super().__init__()
        self._simobj = l2cache
        self._interval = interval
        self._interval_ticks = interval_ticks
        self._sampling_enabled = interval > 0 or interval_ticks > 0
        self._stats = {}
        self._trace_debug = trace_debug
        self._static_regression = static_regression or {}

        # The SimObject that contains the stats we need.
        self._l2cache = l2cache
        self._writeback = writeback

        self._l2cache_data_ae = 1.52711e-10
        self._l2cache_tag_read_ae = 7.0062e-12
        self._l2cache_tag_write_ae = 2.10746e-11
        self._l2cache_read_ae = (
            self._l2cache_data_ae + self._l2cache_tag_read_ae
        )
        self._l2cache_write_ae = 1.80156e-10

        self._l2cache_mb_read_ae = 7.44104e-12
        self._l2cache_mb_write_ae = 7.67662e-12
        self._l2cache_mb_search_ae = 7.79179e-12

        self._l2cache_ifb_read_ae = 3.72729e-11
        self._l2cache_ifb_write_ae = 3.7854e-11
        self._l2cache_ifb_search_ae = 3.5083e-11

        self._l2cache_pfb_read_ae = 3.72729e-11
        self._l2cache_pfb_write_ae = 3.7854e-11
        self._l2cache_pfb_search_ae = 3.5083e-11

        self._l2cache_wbb_read_ae = 3.72729e-11
        self._l2cache_wbb_write_ae = 3.7854e-11
        self._l2cache_wbb_search_ae = 3.5083e-11

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
            "L2",
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

        total_energy = self.l2cache_energy()
        if self._trace_debug:
            print(f"L2cache energy: {total_energy}")

        total_energy += self.miss_buffer_energy()
        if self._trace_debug:
            print(f"\t+ mb energy: {total_energy}")

        total_energy += self.inst_fill_buffer_energy()
        if self._trace_debug:
            print(f"\t+ ifb energy: {total_energy}")

        total_energy += self.prefetch_buffer_energy()
        if self._trace_debug:
            print(f"\t+ prefetch energy: {total_energy}")

        total_energy += self.writeback_buffer_energy()
        power = self.convert_to_watts(total_energy)

        if self._trace_debug:
            print(f"\t+ wbb energy: {total_energy}")
            print(f"L2 power: {power}")

        if getattr(self, "_sample_mode", False):
            self.reset_stats_dict()

        return power

    def l2cache_energy(self):
        # Writeback is ALWAYS assumed in shared caches, according to McPAT
        read_accesses = self.get_stat("ReadExReq.accesses")
        write_accesses = self.get_stat("overallAccesses") + self.get_stat(
            "WritebackClean.accesses"
        )

        read_misses = self.get_stat("ReadExReq.misses")
        write_misses = self.get_stat("overallMisses") - read_misses

        read_hits = read_accesses - read_misses
        write_hits = write_accesses - write_misses
        return (
            read_hits * self._l2cache_read_ae
            + read_misses * self._l2cache_tag_read_ae
            + write_misses * self._l2cache_tag_write_ae
            + write_accesses * self._l2cache_write_ae
        )

    def miss_buffer_energy(self):
        # by default, there is WB in caches (to my knowledge)
        read_accesses = write_accesses = self.get_stat(
            "overallMisses"
        ) - self.get_stat("ReadExReq.misses")

        return (
            read_accesses * self._l2cache_mb_search_ae  # CAM Energy
            + write_accesses * self._l2cache_mb_write_ae  # Miss Energy
        )

    def inst_fill_buffer_energy(self):
        read_accesses = write_accesses = self.get_stat(
            "overallMisses"
        ) - self.get_stat("ReadExReq.misses")
        return (
            read_accesses * self._l2cache_ifb_search_ae
            + write_accesses * self._l2cache_ifb_write_ae
        )

    def prefetch_buffer_energy(self):
        read_accesses = write_accesses = self.get_stat(
            "overallMisses"
        ) - self.get_stat("ReadExReq.misses")
        return (
            read_accesses * self._l2cache_pfb_search_ae
            + write_accesses * self._l2cache_pfb_write_ae
        )

    def writeback_buffer_energy(self):
        read_accesses = write_accesses = self.get_stat(
            "overallMisses"
        ) - self.get_stat("ReadExReq.misses")
        return (
            read_accesses * self._l2cache_wbb_search_ae
            + write_accesses * self._l2cache_wbb_write_ae
        )
