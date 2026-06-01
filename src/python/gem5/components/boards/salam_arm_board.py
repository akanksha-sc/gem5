# Copyright (c) 2025 Arm Limited
# All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause

"""Stdlib ARM board adapter for generated gem5-SALAM accelerator configs."""

from types import SimpleNamespace
from typing import (
    TYPE_CHECKING,
    NamedTuple,
    Optional,
)

from m5.objects import (
    ArmDefaultRelease,
    ArmFsWorkload,
    ArmRelease,
    Root,
    SrcClockDomain,
    VExpress_GEM5_Base,
    VExpress_GEM5_V1,
    VoltageDomain,
)
from m5.util import fatal

from gem5.components.boards.abstract_board import AbstractBoard
from gem5.components.boards.arm_board import ArmBoard

if TYPE_CHECKING:
    from gem5.components.cachehierarchies.abstract_cache_hierarchy import (
        AbstractCacheHierarchy,
    )
    from gem5.components.memory.abstract_memory_system import (
        AbstractMemorySystem,
    )
    from gem5.components.processors.abstract_processor import AbstractProcessor


class _SalamArmBaremetalWorkload(ArmFsWorkload):
    """Bare-metal ELF; mirrors configs/example/arm/workloads.ArmBaremetal."""

    dtb_addr = 0

    def __init__(self, object_file: str, system, **kwargs):
        # Keep the same constructor shape as the legacy
        # ArmBaremetal(object_file, system) path; system is unused here.
        _ = system
        super().__init__(**kwargs)
        self.object_file = object_file
        self.addr_check = False


class SALAMClockConfig(NamedTuple):
    compute_clock: str
    compute_voltage: str
    mem_clock: str
    mem_voltage: str
    dma_clock: str
    dma_voltage: str
    localbus_clock: str
    localbus_voltage: str


class SALAMArmBoard(ArmBoard):
    """
    ARM stdlib board adapter for generated gem5-SALAM accelerator configs.
    * ARM full-system board.
    * Bare-metal ELF workload.
    * Classic cache hierarchy.
    * Generated ``configs/SALAM/<bench>.py`` attachment through
      ``makeHWAcc(args, system)``.
    * Compatibility aliases for legacy generated SALAM configs.
    """

    def __init__(
        self,
        clk_freq: str,
        processor: "AbstractProcessor",
        memory: "AbstractMemorySystem",
        cache_hierarchy: "AbstractCacheHierarchy",
        *,
        salam_module,
        salam_options: SimpleNamespace,
        salam_clocks: SALAMClockConfig,
        baremetal_binary: str,
        platform: Optional[VExpress_GEM5_Base] = None,
        release: Optional[ArmRelease] = None,
    ) -> None:
        self._salam_module = salam_module
        self._salam_options = salam_options
        self._salam_clocks = salam_clocks
        self._baremetal_binary = baremetal_binary
        self._salam_attached = False

        if platform is None:
            platform = VExpress_GEM5_V1()
        if release is None:
            release = ArmDefaultRelease()

        super().__init__(
            clk_freq=clk_freq,
            processor=processor,
            memory=memory,
            cache_hierarchy=cache_hierarchy,
            platform=platform,
            release=release,
        )

        # Force stdlib full-system board setup so ArmBoard creates the RealView
        # platform, GIC, iobus, memory ranges, and system clock domain before
        # generated SALAM configs are attached.
        #
        # SALAM needs these objects but does not use the Linux kernel/disk
        # workload helper.
        self._set_fullsystem(True)
        self._setup_salam_baremetal_workload()
        self.set_is_workload_set(True)

    def _setup_salam_baremetal_workload(self) -> None:
        # SALAM benchmarks are bare-metal ARM ELFs. The legacy flow used:
        #   --bare-metal --dtb-file=none --kernel sw/main.elf
        #
        # Do not use set_kernel_disk_workload(); that API is for a Linux
        # kernel plus disk-image workload.
        self.auto_reset_addr = True
        self.highest_el_is_64 = True

        if hasattr(self.realview.gic, "gicv4"):
            self.realview.gic.gicv4 = False

        self.workload = _SalamArmBaremetalWorkload(
            self._baremetal_binary, self
        )

    def _setup_salam_clock_domains(self) -> None:
        # These domains are consumed by generated configs/SALAM/<bench>.py.
        # The board owns the domains; generated SALAM code assigns them to
        # CommInterface, LLVMInterface, DMA, local bus, and memory tick
        # engines.
        #
        # System clock is the stdlib board clock domain, created from
        # clk_freq in AbstractBoard / ArmBoard. Accelerator subdomains are
        # separate SALAM-specific domains.
        self.acc_compute_voltage_domain = VoltageDomain(
            voltage=self._salam_clocks.compute_voltage
        )
        self.acc_compute_clk_domain = SrcClockDomain(
            clock=self._salam_clocks.compute_clock,
            voltage_domain=self.acc_compute_voltage_domain,
        )

        self.acc_mem_voltage_domain = VoltageDomain(
            voltage=self._salam_clocks.mem_voltage
        )
        self.acc_mem_clk_domain = SrcClockDomain(
            clock=self._salam_clocks.mem_clock,
            voltage_domain=self.acc_mem_voltage_domain,
        )

        self.acc_dma_voltage_domain = VoltageDomain(
            voltage=self._salam_clocks.dma_voltage
        )
        self.acc_dma_clk_domain = SrcClockDomain(
            clock=self._salam_clocks.dma_clock,
            voltage_domain=self.acc_dma_voltage_domain,
        )

        self.acc_localbus_voltage_domain = VoltageDomain(
            voltage=self._salam_clocks.localbus_voltage
        )
        self.acc_localbus_clk_domain = SrcClockDomain(
            clock=self._salam_clocks.localbus_clock,
            voltage_domain=self.acc_localbus_voltage_domain,
        )

        # Legacy alias names. Do not use normal SimObject assignment:
        #   self.acc_clk_domain = self.acc_compute_clk_domain
        #
        # Normal assignment would try to re-parent the compute clock domain
        # as a child parameter of this board. Generated configs only need
        # Python-level aliases acc_clk_domain / acc_voltage_domain.
        object.__setattr__(
            self, "acc_voltage_domain", self.acc_compute_voltage_domain
        )
        object.__setattr__(self, "acc_clk_domain", self.acc_compute_clk_domain)

    def _setup_salam_legacy_aliases(self) -> None:
        cache_hierarchy = self.get_cache_hierarchy()

        if not hasattr(cache_hierarchy, "membus"):
            fatal(
                "SALAM stdlib path currently requires a classic cache hierarchy "
                "that exposes cache_hierarchy.membus. Ruby is not supported "
                "here yet."
            )

        membus = cache_hierarchy.membus

        # tol2bus aliases membus because current generated SALAM configs call
        # clstr._connect_caches(..., l2coherent=False) and connect through
        # system.membus.
        #
        # Do not use normal SimObject assignment:
        #   self.membus = cache_hierarchy.membus
        #
        # membus is already owned by the stdlib cache hierarchy. Normal
        # assignment would attempt to re-parent it under this board.
        # Generated SALAM configs only need system.membus as a Python-level
        # compatibility alias.
        if "membus" not in self.__dict__:
            object.__setattr__(self, "membus", membus)
        if "tol2bus" not in self.__dict__:
            object.__setattr__(self, "tol2bus", membus)

    def _attach_salam(self) -> None:
        # Generated SALAM configs expect the system object to expose:
        #   system.iobus
        #   system.membus
        #   system.tol2bus
        #   system.realview.gic
        #   system.clk_domain
        #   system.acc_compute_clk_domain
        #   system.acc_mem_clk_domain
        #   system.acc_dma_clk_domain
        #   system.acc_localbus_clk_domain
        #
        # SALAMArmBoard provides this compatibility surface before calling
        # generated makeHWAcc(args, system).
        if self._salam_attached:
            return

        self._setup_salam_clock_domains()
        self._setup_salam_legacy_aliases()

        self._salam_module.makeHWAcc(self._salam_options, self)
        self._salam_attached = True

    def _pre_instantiate(self, full_system: Optional[bool] = None) -> Root:
        # Intentionally bypass ArmBoard._pre_instantiate()
        # as it generates a DTB and configure bootloader / kernel-disk state.
        # SALAM's ARM flow is bare-metal and uses no DTB.
        # Use AbstractBoard's generic pre-instantiate path and attach SALAM
        # before m5.instantiate().
        if full_system is False:
            fatal("SALAMArmBoard requires full_system=True.")

        root = AbstractBoard._pre_instantiate(self, full_system=True)
        self._attach_salam()
        return root
