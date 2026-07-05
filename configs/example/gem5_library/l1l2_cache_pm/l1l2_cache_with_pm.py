from m5.objects import (
    BadAddr,
    BaseXBar,
    L2XBar,
    Port,
    SystemXBar,
)
from m5.stats.gem5stats import get_simstat

from gem5.components.boards.abstract_board import AbstractBoard
from gem5.components.cachehierarchies.classic.abstract_classic_cache_hierarchy import (
    AbstractClassicCacheHierarchy,
)

from .l1cache import L1Cache
from .l2cache import L2Cache
from .power_model_l1d import L1DPowerModel
from .power_model_l1i import L1IPowerModel
from .power_model_l2 import L2PowerModel


class PrivateL1SharedL2CacheHierarchy(AbstractClassicCacheHierarchy):
    def __init__(
        self,
        l1i_size: str,
        l1d_size: str,
        l2_size: str,
        power_interval: int = 0,
        power_interval_ticks: int = 0,
        trace_debug: bool = False,
        static_regression=None,
    ) -> None:
        AbstractClassicCacheHierarchy.__init__(self=self)
        self.membus = SystemXBar(width=64)
        self._l1i_size = l1i_size
        self._l1d_size = l1d_size
        self._l2_size = l2_size
        self._power_interval = power_interval
        self._power_interval_ticks = power_interval_ticks
        self._trace_debug = trace_debug
        self._static_regression = static_regression or {}

    def get_mem_side_port(self) -> Port:
        return self.membus.mem_side_ports

    def get_cpu_side_port(self) -> Port:
        return self.membus.cpu_side_ports

    def incorporate_cache(self, board: AbstractBoard) -> None:
        # Set up the system port for functional access from the simulator.
        board.connect_system_port(self.membus.cpu_side_ports)

        for cntr in board.get_memory().get_memory_controllers():
            cntr.port = self.membus.mem_side_ports

        self.l1icaches = [
            L1Cache(size=self._l1i_size)
            for i in range(board.get_processor().get_num_cores())
        ]

        self.l1dcaches = [
            L1Cache(size=self._l1d_size)
            for i in range(board.get_processor().get_num_cores())
        ]

        self.l2cache = L2Cache(size=self._l2_size)

        self.l2XBar = L2XBar()

        for l1i in self.l1icaches:
            l1i.power_state.default_state = "ON"

            l1i.power_model = L1IPowerModel(
                l1i,
                True,
                interval=self._power_interval,
                interval_ticks=self._power_interval_ticks,
                trace_debug=self._trace_debug,
                static_regression=self._static_regression,
            )

        for l1d in self.l1dcaches:
            l1d.power_state.default_state = "ON"

            l1d.power_model = L1DPowerModel(
                l1d,
                True,
                interval=self._power_interval,
                interval_ticks=self._power_interval_ticks,
                trace_debug=self._trace_debug,
                static_regression=self._static_regression,
            )

        self.l2cache.power_state.default_state = "ON"

        self.l2cache.power_model = L2PowerModel(
            self.l2cache,
            True,
            interval=self._power_interval,
            interval_ticks=self._power_interval_ticks,
            trace_debug=self._trace_debug,
            static_regression=self._static_regression,
        )

        for i, cpu in enumerate(board.get_processor().get_cores()):
            cpu.connect_icache(self.l1icaches[i].cpu_side)
            cpu.connect_dcache(self.l1dcaches[i].cpu_side)

            self.l1icaches[i].mem_side = self.l2XBar.cpu_side_ports
            self.l1dcaches[i].mem_side = self.l2XBar.cpu_side_ports

            int_req_port = self.membus.mem_side_ports
            int_resp_port = self.membus.cpu_side_ports
            cpu.connect_interrupt(int_req_port, int_resp_port)

        self.l2XBar.mem_side_ports = self.l2cache.cpu_side

        self.membus.cpu_side_ports = self.l2cache.mem_side
