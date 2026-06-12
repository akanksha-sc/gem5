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

import os
from configparser import ConfigParser
from pathlib import Path

import yaml

import m5
from m5.objects import *
from m5.util import *


def _apply_fu_hardware_limits(acc, fu_limits):
    """Apply per-benchmark synthesized FU instance caps."""
    fu_map = {
        "integer_adder": acc.hw_interface.functional_units.integer_adder,
        "integer_multiplier": acc.hw_interface.functional_units.integer_multiplier,
        "bit_shifter": acc.hw_interface.functional_units.bit_shifter,
        "bitwise_operations": acc.hw_interface.functional_units.bitwise_operations,
        "float_adder": acc.hw_interface.functional_units.float_adder,
        "float_multiplier": acc.hw_interface.functional_units.float_multiplier,
        "double_adder": acc.hw_interface.functional_units.double_adder,
        "double_multiplier": acc.hw_interface.functional_units.double_multiplier,
    }
    pm = acc.hw_interface.salam_power_model
    for fu_name, limit in fu_limits.items():
        if fu_name == "half_adder":
            pm.half_adder_area_cap = limit
        elif fu_name in fu_map:
            fu_map[fu_name].limit = limit


def _apply_power_calibration(acc, calibration):
    """Apply per-kernel power model modes from config.yml."""
    if not calibration:
        return
    pm = acc.hw_interface.salam_power_model
    if "half_adder_dynamic" in calibration:
        pm.half_adder_dynamic = calibration["half_adder_dynamic"]
    if "integer_mul_dynamic" in calibration:
        pm.integer_mul_dynamic = calibration["integer_mul_dynamic"]
    if "fp_add_dynamic" in calibration:
        pm.fp_add_dynamic = calibration["fp_add_dynamic"]
    if "fp_mul_dynamic" in calibration:
        pm.fp_mul_dynamic = calibration["fp_mul_dynamic"]
    if "dynamic_activity_scale" in calibration:
        pm.dynamic_activity_scale = calibration["dynamic_activity_scale"]
    if calibration.get("static_synthesis_floor"):
        pm.static_synthesis_floor = True


def _load_accel_hw_profile(config_file, benchname, benchPath, m5PathLen):
    """Return hw_config profile dict for this accelerator, or None."""
    with open(config_file) as fu_yaml:
        if benchPath[m5PathLen + 1] == "mobilenetv2":
            for yaml_inst_list in yaml.safe_load_all(fu_yaml):
                current_acc = (
                    yaml_inst_list["hw_config"]["name"] + "_" + benchname
                )
                if benchPath[9] == current_acc:
                    return yaml_inst_list["hw_config"][current_acc]
            return None
        yaml_inst_list = yaml.safe_load(fu_yaml)
        return yaml_inst_list["hw_config"].get(benchname)


def AccConfig(acc, bench_file, config_file):
    # Initialize LLVMInterface Objects
    acc.llvm_interface = LLVMInterface()

    # Bind the LLVM execution engine to the parent accelerator's clock
    # domain so callers do not need to patch the child separately after
    # AccConfig() runs. This assumes acc.clk_domain has already been assigned.
    acc.llvm_interface.clk_domain = acc.clk_domain

    # Benchmark path
    acc.llvm_interface.in_file = bench_file

    M5_Path = os.getenv("ACC_BENCH_PATH")
    benchname = os.path.splitext(os.path.basename(bench_file))[0]

    # lenet config launcher custom stuff
    benchPath = Path(bench_file).parts
    m5PathLen = len(Path(M5_Path).parts)

    # Set scheduling constraints
    # acc.llvm_interface.sched_threshold =
    #     ConfigSectionMap("Scheduler")['sched_threshold']
    # acc.llvm_interface.clock_period =
    #     ConfigSectionMap("AccConfig")['clock_period']
    # acc.llvm_interface.lockstep_mode =
    #     Config.getboolean("Scheduler", 'lockstep_mode')

    # Initialize HWInterface Objects
    acc.hw_interface = HWInterface()
    # Define HW Counts
    acc.hw_interface.cycle_counts = CycleCounts()
    # acc.hw_interface.cycle_counts

    def _load_instruction_overrides():
        if benchPath[m5PathLen + 1] == "mobilenetv2":
            with open(config_file) as fu_yaml:
                for yaml_inst_list in yaml.safe_load_all(fu_yaml):
                    current_acc = (
                        yaml_inst_list["hw_config"]["name"] + "_" + benchname
                    )
                    if benchPath[9] == current_acc:
                        return yaml_inst_list["hw_config"][current_acc][
                            "instructions"
                        ]
            return {}
        else:
            with open(config_file) as fu_yaml:
                yaml_inst_list = yaml.safe_load(fu_yaml)
            bench_cfg = yaml_inst_list["hw_config"].get(benchname)
            if bench_cfg is None:
                return {}
            return bench_cfg["instructions"]

    inst_overrides = _load_instruction_overrides()
    accel_hw_profile = _load_accel_hw_profile(
        config_file, benchname, benchPath, m5PathLen
    )

    #  Functional Units
    acc.hw_interface.functional_units = FunctionalUnits()
    acc.hw_interface.functional_units.double_multiplier = DoubleMultiplier()
    acc.hw_interface.functional_units.bit_register = BitRegister()
    acc.hw_interface.functional_units.bitwise_operations = BitwiseOperations()
    acc.hw_interface.functional_units.double_adder = DoubleAdder()
    acc.hw_interface.functional_units.float_divider = FloatDivider()
    acc.hw_interface.functional_units.bit_shifter = BitShifter()
    acc.hw_interface.functional_units.integer_multiplier = IntegerMultiplier()
    acc.hw_interface.functional_units.integer_adder = IntegerAdder()
    acc.hw_interface.functional_units.double_divider = DoubleDivider()
    acc.hw_interface.functional_units.float_adder = FloatAdder()
    acc.hw_interface.functional_units.float_multiplier = FloatMultiplier()

    fu_enum_map = {}
    for fu_name in [
        "double_multiplier",
        "bit_register",
        "bitwise_operations",
        "double_adder",
        "float_divider",
        "bit_shifter",
        "integer_multiplier",
        "integer_adder",
        "double_divider",
        "float_adder",
        "float_multiplier",
    ]:
        fu_obj = getattr(acc.hw_interface.functional_units, fu_name, None)
        if fu_obj is not None and hasattr(fu_obj, "enum_value"):
            fu_enum_map[int(fu_obj.enum_value)] = fu_obj

    #  Instructions
    acc.hw_interface.inst_config = InstConfig()
    acc.hw_interface.inst_config.add = Add()
    acc.hw_interface.inst_config.addrspacecast = Addrspacecast()
    acc.hw_interface.inst_config.alloca = Alloca()
    acc.hw_interface.inst_config.and_inst = AndInst()
    acc.hw_interface.inst_config.ashr = Ashr()
    acc.hw_interface.inst_config.bitcast = Bitcast()
    acc.hw_interface.inst_config.br = Br()
    acc.hw_interface.inst_config.call = Call()
    acc.hw_interface.inst_config.fadd = Fadd()
    acc.hw_interface.inst_config.fcmp = Fcmp()
    acc.hw_interface.inst_config.fdiv = Fdiv()
    acc.hw_interface.inst_config.fence = Fence()
    acc.hw_interface.inst_config.fmul = Fmul()
    acc.hw_interface.inst_config.fmuladd = Fmuladd()
    acc.hw_interface.inst_config.fneg = Fneg()
    acc.hw_interface.inst_config.fpext = Fpext()
    acc.hw_interface.inst_config.fptosi = Fptosi()
    acc.hw_interface.inst_config.fptoui = Fptoui()
    acc.hw_interface.inst_config.fptrunc = Fptrunc()
    acc.hw_interface.inst_config.frem = Frem()
    acc.hw_interface.inst_config.fsub = Fsub()
    acc.hw_interface.inst_config.gep = Gep()
    acc.hw_interface.inst_config.icmp = Icmp()
    acc.hw_interface.inst_config.indirectbr = Indirectbr()
    acc.hw_interface.inst_config.inttoptr = Inttoptr()
    acc.hw_interface.inst_config.invoke = Invoke()
    acc.hw_interface.inst_config.landingpad = Landingpad()
    acc.hw_interface.inst_config.load = Load()
    acc.hw_interface.inst_config.lshr = Lshr()
    acc.hw_interface.inst_config.mul = Mul()
    acc.hw_interface.inst_config.or_inst = OrInst()
    acc.hw_interface.inst_config.phi = Phi()
    acc.hw_interface.inst_config.ptrtoint = Ptrtoint()
    acc.hw_interface.inst_config.resume = Resume()
    acc.hw_interface.inst_config.ret = Ret()
    acc.hw_interface.inst_config.sdiv = Sdiv()
    acc.hw_interface.inst_config.select = Select()
    acc.hw_interface.inst_config.sext = Sext()
    acc.hw_interface.inst_config.shl = Shl()
    acc.hw_interface.inst_config.srem = Srem()
    acc.hw_interface.inst_config.store = Store()
    acc.hw_interface.inst_config.sub = Sub()
    acc.hw_interface.inst_config.switch_inst = SwitchInst()
    acc.hw_interface.inst_config.trunc = Trunc()
    acc.hw_interface.inst_config.udiv = Udiv()
    acc.hw_interface.inst_config.uitofp = Uitofp()
    acc.hw_interface.inst_config.unreachable = Unreachable()
    acc.hw_interface.inst_config.urem = Urem()
    acc.hw_interface.inst_config.vaarg = Vaarg()
    acc.hw_interface.inst_config.xor_inst = XorInst()
    acc.hw_interface.inst_config.zext = Zext()

    # Apply instruction-level overrides from hw_config
    for inst_name, inst_data in inst_overrides.items():
        if "runtime_cycles" in inst_data and hasattr(
            acc.hw_interface.cycle_counts, inst_name
        ):
            setattr(
                acc.hw_interface.cycle_counts,
                inst_name,
                int(inst_data["runtime_cycles"]),
            )

        inst_obj = getattr(acc.hw_interface.inst_config, inst_name, None)
        if inst_obj is None:
            continue

        if "functional_unit" in inst_data:
            inst_obj.functional_unit = int(inst_data["functional_unit"])
        if "functional_unit_limit" in inst_data:
            inst_obj.functional_unit_limit = int(
                inst_data["functional_unit_limit"]
            )
        if "opcode_num" in inst_data:
            inst_obj.opcode_num = int(inst_data["opcode_num"])
        if "runtime_cycles" in inst_data:
            inst_obj.runtime_cycles = int(inst_data["runtime_cycles"])

    # Apply explicit FU capacities from hw_config.
    #   functional_unit_limit == 0 => keep legacy/unbounded behavior
    #   functional_unit_limit > 0  => set explicit capacity
    for inst_name, inst_data in inst_overrides.items():
        fu_enum = int(inst_data.get("functional_unit", 0))
        fu_limit = int(inst_data.get("functional_unit_limit", 0))
        if fu_limit > 0 and fu_enum in fu_enum_map:
            fu_obj = fu_enum_map[fu_enum]
            fu_obj.limit = max(int(getattr(fu_obj, "limit", 0)), fu_limit)

    acc.hw_interface.salam_power_model = SALAMPowerModel()
    acc.hw_interface.hw_statistics = HWStatistics(
        cycle_tracking=True,
        debug=False,
        stat_buffer_size=10000,
        stat_buffer_predefine=2,
    )
    acc.hw_interface.simulator_config = SimulatorConfig()
    acc.hw_interface.opcodes = InstOpCodes()

    if accel_hw_profile is not None:
        calibration = accel_hw_profile.get("power_calibration")
        if calibration:
            _apply_power_calibration(acc, calibration)
        fu_limits = accel_hw_profile.get("fu_hardware_limits")
        if fu_limits:
            _apply_fu_hardware_limits(acc, fu_limits)
