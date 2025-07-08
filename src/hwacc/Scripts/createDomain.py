# Copyright (c) 2025 Akanksha Chaudhari, Matt Sinclair
# All rights reserved.
#
# This file contains modifications and/or code derived from:
# gem5-SALAM: https://github.com/TeCSAR-UNCC/gem5-SALAM
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
# this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation
# and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its
# contributors may be used to endorse or promote products derived from this
# software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

# Script varies parameters to create design domain
import os
import sys
from argparse import ArgumentParser

try:
    from configparser import ConfigParser
except ImportError:
    from ConfigParser import ConfigParser

parser = ArgumentParser()
parser.add_argument(
    "-f", "--config", dest="configFile", help="Config file location"
)
parser.add_argument(
    "-m", "--memports", dest="memPorts", help="Read/Write ports"
)
parser.add_argument(
    "-t", "--timing", dest="nsTime", help="Transistor timing in ns"
)
parser.add_argument(
    "-c", "--counter", dest="counters", help="Set number of counter units"
)
parser.add_argument(
    "-ia", "--intadd", dest="intAdders", help="Set number of int adder units"
)
parser.add_argument(
    "-im", "--intmul", dest="intMuls", help="Set number of int mul units"
)
parser.add_argument(
    "-s", "--shifter", dest="shifters", help="Set number of shifter units"
)
parser.add_argument(
    "-b", "--bitwise", dest="bitWise", help="Set number of bitwise units"
)
parser.add_argument(
    "-fa",
    "--floatadd",
    dest="floatAdd",
    help="Set number of float adder units",
)
parser.add_argument(
    "-da", "--doubadd", dest="doubAdd", help="Set number of double adder units"
)
parser.add_argument(
    "-fm", "--floatmul", dest="floatMul", help="Set number of float mul units"
)
parser.add_argument(
    "-dm", "--doubmul", dest="doubMul", help="Set number of double mul units"
)
parser.add_argument(
    "-z", "--zero", dest="zeroCyl", help="Set number of zero cycle units"
)
parser.add_argument(
    "-g", "--gep", dest="gepSet", help="Set number of GEPs per cycle"
)
parser.add_argument(
    "-cv", "--conv", dest="conversion", help="Set number of conversion units"
)
args = parser.parse_args()


config = ConfigParser()
config.read(args.configFile)
config.set("Memory", "read_ports", args.memPorts)
config.set("Memory", "write_ports", args.memPorts)
config.set("Scheduler", "fu_clock_period", args.nsTime)
config.set("FunctionalUnits", "fp_sp_add", args.floatAdd)
config.set("FunctionalUnits", "fp_dp_add", args.doubAdd)
config.set("FunctionalUnits", "fp_sp_mul", args.floatMul)
config.set("FunctionalUnits", "fp_dp_mul", args.doubMul)
config.set("FunctionalUnits", "fu_int_add", args.intAdders)
config.set("FunctionalUnits", "fu_int_mul", args.intMuls)
config.set("FunctionalUnits", "fu_int_bit", args.bitWise)
config.set("FunctionalUnits", "fu_int_shift", args.shifters)
config.set("FunctionalUnits", "fu_counter", args.counters)
config.set("FunctionalUnits", "fu_gep", args.gepSet)
config.set("FunctionalUnits", "fu_compare", args.zeroCyl)
config.set("FunctionalUnits", "fu_conversion", args.conversion)


with open(args.configFile, "w") as configfile:
    config.write(configfile)
