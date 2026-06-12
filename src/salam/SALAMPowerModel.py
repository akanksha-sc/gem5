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

from m5.params import *
from m5.proxy import *
from m5.SimObject import *


class SALAMPowerModel(SimObject):
    type = "SALAMPowerModel"
    cxx_header = "salam/HWModeling/salam_power_model.hh"

    cxx_exports = [
        PyBindMethod("getDynamicPower"),
        PyBindMethod("getStaticPower"),
        PyBindMethod("getArea"),
    ]

    half_adder_area_cap = Param.UInt32(
        0, "Cap compare+GEP half-adder instances for synthesis area/leakage"
    )
    half_adder_dynamic = Param.UInt32(0, "HalfAdderDynamicMode enum")
    integer_mul_dynamic = Param.UInt32(0, "IntMulDynamicMode enum")
    fp_add_dynamic = Param.UInt32(0, "FpAddDynamicMode enum")
    fp_mul_dynamic = Param.UInt32(
        1, "FpMulDynamicMode enum (default SHARED_MACRO)"
    )
    dynamic_activity_scale = Param.Float(
        1.0, "RTL activity multiplier on dynamic energy"
    )
    static_synthesis_floor = Param.Bool(
        False, "Charge static CDFG FU counts every cycle (FFT-class RTL floor)"
    )
