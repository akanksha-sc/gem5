# Copyright (c) 2025 Akanksha Chaudhari, Matt Sinclair
# Copyright (c) 2021-2025 The Regents of the University of California
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""
SALAM stdlib launcher: imports generated ``configs/SALAM/<bench>.py`` and
attaches accelerators via ``SALAMArmBoard`` + ``makeHWAcc(args, system)``.
"""

import argparse
import importlib
import os
import sys
from pathlib import Path
from types import SimpleNamespace

from m5.objects import VExpress_GEM5_V1
from m5.util import (
    fatal,
    warn,
)

from gem5.components.boards.salam_arm_board import (
    SALAMArmBoard,
    SALAMClockConfig,
)
from gem5.components.cachehierarchies.classic.private_l1_private_l2_cache_hierarchy import (
    PrivateL1PrivateL2CacheHierarchy,
)
from gem5.components.memory import DualChannelDDR4_2400
from gem5.components.processors.cpu_types import CPUTypes
from gem5.components.processors.simple_processor import SimpleProcessor
from gem5.isas import ISA
from gem5.simulate.simulator import Simulator
from gem5.utils.requires import requires

requires(isa_required=ISA.ARM)


def parse_args():
    # SALAM stdlib mode runs bare-metal ARM ELFs. It intentionally does not
    # accept --disk-image or use set_kernel_disk_workload().
    p = argparse.ArgumentParser()

    p.add_argument("--m5-path", required=True)
    p.add_argument("--acc-bench-path", required=True)
    p.add_argument("--bench", required=True)
    p.add_argument("--bench-path", required=True)
    p.add_argument("--config-name", default="config.yml")

    p.add_argument("--kernel", required=True)

    p.add_argument(
        "--sys-clock",
        default="1GHz",
        help=(
            "Board/system clock used to construct the stdlib ArmBoard. "
            "The current SALAM stdlib runner does not expose a separate "
            "per-CPU clock option."
        ),
    )
    p.add_argument(
        "--sys-voltage",
        default="1.0V",
        help=(
            "Default accelerator voltage fallback when per-domain voltages "
            "are not set. Board voltage-domain wiring is not changed by this "
            "value alone."
        ),
    )
    p.add_argument("--mem-size", default="16GB")

    p.add_argument("--acc-cache", action="store_true")

    p.add_argument(
        "--l2cache",
        action="store_true",
        default=False,
        help=(
            "Legacy SALAM option passed to generated configs. "
            "This stdlib runner does not implement true L2-coherent "
            "accelerator attachment yet."
        ),
    )

    p.add_argument("--acc-clock", default=None)
    p.add_argument("--acc-voltage", default=None)
    p.add_argument("--acc-compute-clock", default=None)
    p.add_argument("--acc-compute-voltage", default=None)
    p.add_argument("--acc-mem-clock", default=None)
    p.add_argument("--acc-mem-voltage", default=None)
    p.add_argument("--acc-dma-clock", default=None)
    p.add_argument("--acc-dma-voltage", default=None)
    p.add_argument("--acc-localbus-clock", default=None)
    p.add_argument("--acc-localbus-voltage", default=None)
    p.add_argument("--acc-spm-clock", default=None)
    p.add_argument("--acc-spm-voltage", default=None)

    p.add_argument("--dry-run", action="store_true")

    return p.parse_args()


def make_salam_options(args):
    return SimpleNamespace(
        acc_cache=args.acc_cache,
        l2cache=args.l2cache,
        accpath=os.path.join(args.acc_bench_path, args.bench_path),
        accbench=args.bench,
        acc_clock=args.acc_clock,
        acc_voltage=args.acc_voltage,
        acc_compute_clock=args.acc_compute_clock,
        acc_compute_voltage=args.acc_compute_voltage,
        acc_mem_clock=args.acc_mem_clock,
        acc_mem_voltage=args.acc_mem_voltage,
        acc_dma_clock=args.acc_dma_clock,
        acc_dma_voltage=args.acc_dma_voltage,
        acc_localbus_clock=args.acc_localbus_clock,
        acc_localbus_voltage=args.acc_localbus_voltage,
    )


def make_salam_clocks(args):
    compute_clock = args.acc_compute_clock or args.acc_clock or args.sys_clock
    compute_voltage = (
        args.acc_compute_voltage or args.acc_voltage or args.sys_voltage
    )

    mem_clock = args.acc_mem_clock or compute_clock
    mem_voltage = args.acc_mem_voltage or compute_voltage

    dma_clock = args.acc_dma_clock or mem_clock
    dma_voltage = args.acc_dma_voltage or mem_voltage

    localbus_clock = args.acc_localbus_clock or mem_clock
    localbus_voltage = args.acc_localbus_voltage or mem_voltage

    # Only create a dedicated SPM domain when explicitly requested (e.g. via
    # --acc-spm-clock). Otherwise leave it None so SPM tick engines fall back
    # to the accelerator memory domain.
    spm_clock = args.acc_spm_clock
    spm_voltage = args.acc_spm_voltage or mem_voltage if spm_clock else None

    return SALAMClockConfig(
        compute_clock=compute_clock,
        compute_voltage=compute_voltage,
        mem_clock=mem_clock,
        mem_voltage=mem_voltage,
        dma_clock=dma_clock,
        dma_voltage=dma_voltage,
        localbus_clock=localbus_clock,
        localbus_voltage=localbus_voltage,
        spm_clock=spm_clock,
        spm_voltage=spm_voltage,
    )


def import_generated_salam_module(args):
    salam_config_dir = Path(args.m5_path) / "configs" / "SALAM"
    generated = salam_config_dir / f"{args.bench}.py"

    if not generated.is_file():
        fatal(
            f"Generated SALAM config not found: {generated}. "
            "Run systembuilder.py or run_system_stdlib.sh first."
        )

    p = str(salam_config_dir)
    if p not in sys.path:
        sys.path.insert(0, p)
    return importlib.import_module(args.bench)


def main():
    args = parse_args()

    if not Path(args.kernel).is_file():
        fatal(f"Kernel/bare-metal ELF not found: {args.kernel}")

    if args.l2cache:
        warn(
            "--l2cache is accepted as a legacy generated-config option, "
            "but this stdlib runner currently aliases tol2bus to membus and "
            "does not implement true L2-coherent accelerator attachment."
        )

    if args.sys_voltage != "1.0V":
        warn(
            "--sys-voltage is used as the default accelerator voltage fallback "
            "only; it does not independently reconfigure the stdlib board "
            "voltage domain."
        )

    salam_module = import_generated_salam_module(args)
    salam_options = make_salam_options(args)
    salam_clocks = make_salam_clocks(args)

    processor = SimpleProcessor(
        cpu_type=CPUTypes.O3,
        isa=ISA.ARM,
        num_cores=1,
    )

    # The first stdlib SALAM path intentionally uses a classic cache
    # hierarchy. Generated SALAM configs connect accelerator memory traffic
    # through system.membus / tol2bus compatibility aliases. Ruby is not
    # supported here yet.
    cache_hierarchy = PrivateL1PrivateL2CacheHierarchy(
        l1d_size="32kB",
        l1i_size="32kB",
        l2_size="256kB",
    )

    memory = DualChannelDDR4_2400(size=args.mem_size)

    board = SALAMArmBoard(
        clk_freq=args.sys_clock,
        processor=processor,
        memory=memory,
        cache_hierarchy=cache_hierarchy,
        salam_module=salam_module,
        salam_options=salam_options,
        salam_clocks=salam_clocks,
        baremetal_binary=args.kernel,
        platform=VExpress_GEM5_V1(),
    )

    if args.dry_run:
        board._pre_instantiate(full_system=True)
        print("SALAM stdlib dry-run attach complete.")
        print("Validated:")
        print("  - stdlib ARM board setup")
        print("  - generated SALAM module import")
        print("  - SALAM compatibility aliases")
        print("  - makeHWAcc(args, board) attachment")
        print("Not validated:")
        print("  - guest ELF execution")
        print("  - accelerator MMIO traffic")
        print("  - DMA / memory behavior")
        print("  - benchmark correctness")
        print("Clock domains:")
        print(f"  system: {board.clk_domain.clock}")
        print(f"  acc_compute: {board.acc_compute_clk_domain.clock}")
        print(f"  acc_mem: {board.acc_mem_clk_domain.clock}")
        print(f"  acc_dma: {board.acc_dma_clk_domain.clock}")
        print(f"  acc_localbus: {board.acc_localbus_clk_domain.clock}")
        if hasattr(board, "acc_spm_clk_domain"):
            print(f"  acc_spm: {board.acc_spm_clk_domain.clock}")
        return

    simulator = Simulator(board=board)
    simulator.run()


if __name__ == "__m5_main__":
    main()
