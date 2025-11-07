from configparser import ConfigParser

from HWAccConfig import *

import m5
from m5.objects import *
from m5.util import *


class L1Cache(Cache):
    assoc = 2
    tag_latency = 2
    data_latency = 2
    response_latency = 2
    mshrs = 4
    tgts_per_mshr = 20


def __init__(self, size, options=None):
    self.size = size
    super(L1Cache, self).__init__()
    pass


def buildbfs_clstr(options, system, clstr):

    local_low = 0x2F000000
    local_high = 0x2F004AC0
    local_range = AddrRange(local_low, local_high)
    external_range = [
        AddrRange(0x00000000, local_low - 1),
        AddrRange(local_high + 1, 0xFFFFFFFF),
    ]
    system.iobus.mem_side_ports = clstr.local_bus.cpu_side_ports
    clstr._connect_caches(system, options, l2coherent=False)
    gic = system.realview.gic

    # Noncoherent DMA
    clstr.dma = NoncoherentDma(
        pio_addr=0x2F000000, pio_size=21, gic=gic, int_num=95
    )
    clstr.dma.cluster_dma = clstr.local_bus.cpu_side_ports
    clstr.dma.max_req_size = 128
    clstr.dma.buffer_size = 256
    clstr.dma.dma = clstr.coherency_bus.cpu_side_ports
    clstr.local_bus.mem_side_ports = clstr.dma.pio

    # top Definition
    acc = "top"
    ir = (
        "/nobackup/akankshac/research/benchmarks/sys_validation/bfs//hw/top.ll"
    )
    hw_config = (
        "/nobackup/akankshac/research/benchmarks/sys_validation/bfs/config.yml"
    )
    clstr.top = CommInterface(
        devicename=acc, gic=gic, pio_addr=0x2F000040, pio_size=64, int_num=68
    )
    AccConfig(clstr.top, ir, hw_config)

    # bfs Definition
    acc = "bfs"
    ir = (
        "/nobackup/akankshac/research/benchmarks/sys_validation/bfs//hw/bfs.ll"
    )
    hw_config = (
        "/nobackup/akankshac/research/benchmarks/sys_validation/bfs/config.yml"
    )
    clstr.bfs = CommInterface(
        devicename=acc, gic=gic, pio_addr=0x2F000080, pio_size=64
    )
    AccConfig(clstr.bfs, ir, hw_config)

    # top Config
    clstr.top.local = clstr.local_bus.cpu_side_ports
    clstr.top.pio = clstr.local_bus.mem_side_ports
    clstr.top.enable_debug_msgs = False

    # bfs Config
    clstr.bfs.pio = clstr.local_bus.mem_side_ports
    clstr.bfs.enable_debug_msgs = False

    # NODES (Variable)
    addr = 0x2F0000C0
    regRange = AddrRange(addr, addr + 0x800)
    clstr.nodes = RegisterBank(range=regRange)
    clstr.nodes.load_port = clstr.local_bus.mem_side_ports

    # Connecting NODES to bfs
    clstr.bfs.reg = clstr.nodes.reg_port

    # EDGES (Variable)
    addr = 0x2F000900
    regRange = AddrRange(addr, addr + 0x4000)
    clstr.edges = RegisterBank(range=regRange)
    clstr.edges.load_port = clstr.local_bus.mem_side_ports

    # Connecting EDGES to bfs
    clstr.bfs.reg = clstr.edges.reg_port

    # LEVELS (Variable)
    addr = 0x2F004940
    regRange = AddrRange(addr, addr + 0x100)
    clstr.levels = RegisterBank(range=regRange)
    clstr.levels.load_port = clstr.local_bus.mem_side_ports

    # Connecting LEVELS to bfs
    clstr.bfs.reg = clstr.levels.reg_port

    # LEVELCOUNTS (Variable)
    addr = 0x2F004A80
    regRange = AddrRange(addr, addr + 0x28)
    clstr.levelcounts = RegisterBank(range=regRange)
    clstr.levelcounts.load_port = clstr.local_bus.mem_side_ports

    # Connecting LEVELCOUNTS to bfs
    clstr.bfs.reg = clstr.levelcounts.reg_port


def makeHWAcc(args, system):

    system.bfs_clstr = AccCluster()
    buildbfs_clstr(args, system, system.bfs_clstr)
