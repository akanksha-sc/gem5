# Copyright (c) 2006-2007 The Regents of The University of Michigan
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

from os import environ as env

from common.SysPaths import (
    binary,
    disk,
    script,
)

from m5.defines import buildEnv


class SysConfig:
    def __init__(
        self, script=None, mem=None, disks=None, rootdev=None, os_type="linux"
    ):
        self.scriptname = script
        self.disknames = disks
        self.memsize = mem
        self.root = rootdev
        self.ostype = os_type

    def script(self):
        if self.scriptname:
            return script(self.scriptname)
        else:
            return ""

    def mem(self):
        if self.memsize:
            return self.memsize
        else:
            return "128MiB"

    def disks(self):
        if self.disknames:
            return [disk(diskname) for diskname in self.disknames]
        else:
            return []

    def rootdev(self):
        if self.root:
            return self.root
        else:
            return "/dev/sda1"

    def os_type(self):
        return self.ostype


# Benchmarks are defined as a key in a dict which is a list of SysConfigs
# The first defined machine is the test system, the others are driving systems

Benchmarks = {
    "PovrayBench": [SysConfig("povray-bench.rcS", "512MiB", ["povray.img"])],
    "PovrayAutumn": [SysConfig("povray-autumn.rcS", "512MiB", ["povray.img"])],
    "NetperfStream": [
        SysConfig("netperf-stream-client.rcS"),
        SysConfig("netperf-server.rcS"),
    ],
    "NetperfStreamUdp": [
        SysConfig("netperf-stream-udp-client.rcS"),
        SysConfig("netperf-server.rcS"),
    ],
    "NetperfUdpLocal": [SysConfig("netperf-stream-udp-local.rcS")],
    "NetperfStreamNT": [
        SysConfig("netperf-stream-nt-client.rcS"),
        SysConfig("netperf-server.rcS"),
    ],
    "NetperfMaerts": [
        SysConfig("netperf-maerts-client.rcS"),
        SysConfig("netperf-server.rcS"),
    ],
    "SurgeStandard": [
        SysConfig("surge-server.rcS", "512MiB"),
        SysConfig("surge-client.rcS", "256MiB"),
    ],
    "SurgeSpecweb": [
        SysConfig("spec-surge-server.rcS", "512MiB"),
        SysConfig("spec-surge-client.rcS", "256MiB"),
    ],
    "Nhfsstone": [
        SysConfig("nfs-server-nhfsstone.rcS", "512MiB"),
        SysConfig("nfs-client-nhfsstone.rcS"),
    ],
    "Nfs": [
        SysConfig("nfs-server.rcS", "900MiB"),
        SysConfig("nfs-client-dbench.rcS"),
    ],
    "NfsTcp": [
        SysConfig("nfs-server.rcS", "900MiB"),
        SysConfig("nfs-client-tcp.rcS"),
    ],
    "IScsiInitiator": [
        SysConfig("iscsi-client.rcS", "512MiB"),
        SysConfig("iscsi-server.rcS", "512MiB"),
    ],
    "IScsiTarget": [
        SysConfig("iscsi-server.rcS", "512MiB"),
        SysConfig("iscsi-client.rcS", "512MiB"),
    ],
    "Validation": [
        SysConfig("iscsi-server.rcS", "512MiB"),
        SysConfig("iscsi-client.rcS", "512MiB"),
    ],
    "Ping": [SysConfig("ping-server.rcS"), SysConfig("ping-client.rcS")],
    "ValAccDelay": [SysConfig("devtime.rcS", "512MiB")],
    "ValAccDelay2": [SysConfig("devtimewmr.rcS", "512MiB")],
    "ValMemLat": [SysConfig("micro_memlat.rcS", "512MiB")],
    "ValMemLat2MB": [SysConfig("micro_memlat2mb.rcS", "512MiB")],
    "ValMemLat8MB": [SysConfig("micro_memlat8mb.rcS", "512MiB")],
    "ValMemLat": [SysConfig("micro_memlat8.rcS", "512MiB")],
    "ValTlbLat": [SysConfig("micro_tlblat.rcS", "512MiB")],
    "ValSysLat": [SysConfig("micro_syscall.rcS", "512MiB")],
    "ValCtxLat": [SysConfig("micro_ctx.rcS", "512MiB")],
    "ValStream": [SysConfig("micro_stream.rcS", "512MiB")],
    "ValStreamScale": [SysConfig("micro_streamscale.rcS", "512MiB")],
    "ValStreamCopy": [SysConfig("micro_streamcopy.rcS", "512MiB")],
    "MutexTest": [SysConfig("mutex-test.rcS", "128MiB")],
    "ArmAndroid-GB": [
        SysConfig(
            "null.rcS",
            "256MiB",
            ["ARMv7a-Gingerbread-Android.SMP.mouse.nolock.clean.img"],
            None,
            "android-gingerbread",
        )
    ],
    "bbench-gb": [
        SysConfig(
            "bbench-gb.rcS",
            "256MiB",
            ["ARMv7a-Gingerbread-Android.SMP.mouse.nolock.img"],
            None,
            "android-gingerbread",
        )
    ],
    "ArmAndroid-ICS": [
        SysConfig(
            "null.rcS",
            "256MiB",
            ["ARMv7a-ICS-Android.SMP.nolock.clean.img"],
            None,
            "android-ics",
        )
    ],
    "bbench-ics": [
        SysConfig(
            "bbench-ics.rcS",
            "256MiB",
            ["ARMv7a-ICS-Android.SMP.nolock.img"],
            None,
            "android-ics",
        )
    ],
}

benchs = list(Benchmarks.keys())
benchs.sort()
DefinedBenchmarks = ", ".join(benchs)
