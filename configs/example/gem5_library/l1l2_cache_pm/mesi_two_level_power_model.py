import xml.etree.ElementTree as ET

from m5.objects import (
    DMASequencer,
    RubyPortProxy,
    RubySequencer,
    RubySystem,
)

from gem5.coherence_protocol import CoherenceProtocol
from gem5.components.boards.abstract_board import AbstractBoard
from gem5.components.cachehierarchies.abstract_two_level_cache_hierarchy import (
    AbstractTwoLevelCacheHierarchy,
)
from gem5.components.cachehierarchies.ruby.abstract_ruby_cache_hierarchy import (
    AbstractRubyCacheHierarchy,
)
from gem5.components.cachehierarchies.ruby.caches.mesi_two_level.directory import (
    Directory,
)
from gem5.components.cachehierarchies.ruby.caches.mesi_two_level.dma_controller import (
    DMAController,
)
from gem5.components.cachehierarchies.ruby.caches.mesi_two_level.l1_cache import (
    L1Cache,
)
from gem5.components.cachehierarchies.ruby.caches.mesi_two_level.l2_cache import (
    L2Cache,
)
from gem5.components.cachehierarchies.ruby.topologies.simple_pt2pt import (
    SimplePt2Pt,
)
from gem5.isas import ISA
from gem5.utils.requires import requires

from .power_model_l1d import L1DPowerModel
from .power_model_l1i import L1IPowerModel
from .power_model_l2 import L2PowerModel


class MESITwoLevelCHPowerModel(
    AbstractRubyCacheHierarchy, AbstractTwoLevelCacheHierarchy
):
    """A two level private L1 shared L2 MESI hierarchy.

    In addition to the normal two level parameters, you can also change the
    number of L2 banks in this protocol.

    The on-chip network is a point-to-point all-to-all simple network.
    """

    def __init__(
        self,
        l1i_size: str,
        l1i_assoc: str,
        l1d_size: str,
        l1d_assoc: str,
        l2_size: str,
        l2_assoc: str,
        num_l2_banks: int,
        xml_tree,
    ):
        AbstractRubyCacheHierarchy.__init__(self=self)
        AbstractTwoLevelCacheHierarchy.__init__(
            self,
            l1i_size=l1i_size,
            l1i_assoc=l1i_assoc,
            l1d_size=l1d_size,
            l1d_assoc=l1d_assoc,
            l2_size=l2_size,
            l2_assoc=l2_assoc,
        )

        self._num_l2_banks = num_l2_banks
        self._xml_tree = xml_tree

    def incorporate_cache(self, board: AbstractBoard) -> None:
        requires(coherence_protocol_required=CoherenceProtocol.MESI_TWO_LEVEL)

        cache_line_size = board.get_cache_line_size()

        self.ruby_system = RubySystem()

        # MESI_Two_Level needs 3 virtual networks
        self.ruby_system.number_of_virtual_networks = 3

        self.ruby_system.network = SimplePt2Pt(self.ruby_system)
        self.ruby_system.network.number_of_virtual_networks = 3

        self._l1_controllers = []
        for i, core in enumerate(board.get_processor().get_cores()):
            cache = L1Cache(
                self._l1i_size,
                self._l1i_assoc,
                self._l1d_size,
                self._l1d_assoc,
                self.ruby_system.network,
                core,
                self._num_l2_banks,
                cache_line_size,
                board.processor.get_isa(),
                board.get_clock_domain(),
            )

            cache.sequencer = RubySequencer(
                version=i, dcache=cache.L1Dcache, clk_domain=cache.clk_domain
            )

            if board.has_io_bus():
                cache.sequencer.connectIOPorts(board.get_io_bus())

            cache.ruby_system = self.ruby_system

            core.connect_icache(cache.sequencer.in_ports)
            core.connect_dcache(cache.sequencer.in_ports)

            core.connect_walker_ports(
                cache.sequencer.in_ports, cache.sequencer.in_ports
            )

            # Connect the interrupt ports
            if board.get_processor().get_isa() == ISA.X86:
                int_req_port = cache.sequencer.interrupt_out_port
                int_resp_port = cache.sequencer.in_ports
                core.connect_interrupt(int_req_port, int_resp_port)
            else:
                core.connect_interrupt()

            self._l1_controllers.append(cache)

        self._l2_controllers = [
            L2Cache(
                self._l2_size,
                self._l2_assoc,
                self.ruby_system.network,
                self._num_l2_banks,
                cache_line_size,
            )
            for _ in range(self._num_l2_banks)
        ]
        # TODO: Make this prettier: The problem is not being able to proxy
        # the ruby system correctly
        for cache in self._l2_controllers:
            cache.ruby_system = self.ruby_system

        self._directory_controllers = [
            Directory(self.ruby_system.network, cache_line_size, range, port)
            for range, port in board.get_mem_ports()
        ]
        # TODO: Make this prettier: The problem is not being able to proxy
        # the ruby system correctly
        for dir in self._directory_controllers:
            dir.ruby_system = self.ruby_system

        self._dma_controllers = []
        if board.has_dma_ports():
            dma_ports = board.get_dma_ports()
            for i, port in enumerate(dma_ports):
                ctrl = DMAController(self.ruby_system.network, cache_line_size)
                ctrl.dma_sequencer = DMASequencer(version=i, in_ports=port)
                self._dma_controllers.append(ctrl)
                ctrl.ruby_system = self.ruby_system

        self.ruby_system.num_of_sequencers = len(self._l1_controllers) + len(
            self._dma_controllers
        )
        self.ruby_system.l1_controllers = self._l1_controllers
        self.ruby_system.l2_controllers = self._l2_controllers
        self.ruby_system.directory_controllers = self._directory_controllers

        if len(self._dma_controllers) != 0:
            self.ruby_system.dma_controllers = self._dma_controllers

        # Create the network and connect the controllers.
        self.ruby_system.network.connectControllers(
            self._l1_controllers
            + self._l2_controllers
            + self._directory_controllers
            + self._dma_controllers
        )
        self.ruby_system.network.setup_buffers()

        # Set up a proxy port for the system_port. Used for load binaries and
        # other functional-only things.
        self.ruby_system.sys_port_proxy = RubyPortProxy()
        board.connect_system_port(self.ruby_system.sys_port_proxy.in_ports)
