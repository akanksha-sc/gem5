import os
from configparser import ConfigParser
from pathlib import Path

import yaml

import m5
from m5.objects import *
from m5.util import *


def AccConfig(acc, bench_file, config_file):
    # Initialize LLVMInterface Objects
    acc.llvm_interface = LLVMInterface()

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

    # TODO: Auto generate the functional unit list

    # Initialize HWInterface Objects
    acc.hw_interface = HWInterface()
    # Define HW Counts
    acc.hw_interface.cycle_counts = CycleCounts()
    # acc.hw_interface.cycle_counts

    if benchPath[m5PathLen + 1] == "mobilenetv2":
        fu_yaml = open(config_file)
        for yaml_inst_list in yaml.safe_load_all(fu_yaml):
            document = yaml_inst_list["hw_config"]
            current_acc = yaml_inst_list["hw_config"]["name"] + "_" + benchname
            if benchPath[9] == current_acc:
                print(current_acc + " Profile Loaded")
                print(yaml_inst_list["hw_config"][benchname])
                inst_list = yaml_inst_list["hw_config"][current_acc][
                    "instructions"
                ].keys()
                for instruction in inst_list:
                    setattr(
                        acc.hw_interface.cycle_counts,
                        instruction,
                        yaml_inst_list["hw_config"][current_acc][
                            "instructions"
                        ][instruction]["runtime_cycles"],
                    )
        fu_yaml.close()

    else:
        fu_yaml = open(config_file)
        yaml_inst_list = yaml.safe_load(fu_yaml)
        if yaml_inst_list["hw_config"][benchname] is not None:
            inst_list = yaml_inst_list["hw_config"][benchname][
                "instructions"
            ].keys()
            for instruction in inst_list:
                setattr(
                    acc.hw_interface.cycle_counts,
                    instruction,
                    yaml_inst_list["hw_config"][benchname]["instructions"][
                        instruction
                    ]["runtime_cycles"],
                )
        fu_yaml.close()

    #  Functional Units
    acc.hw_interface.functional_units = FunctionalUnits()

    # tech-node, lat, profile
    tech_model = "40nm_model"
    lat_ns = "5ns"
    profile = "default_profile"
    qc = Path(Path(bench_file).parent.parent, "configs/quick_config.yml")
    if qc.exists():
        qyml = yaml.safe_load(qc.read_text())
        tech_model = qyml.get("tech_model", tech_model)
        lat_ns = qyml.get("clock_period", lat_ns)
        profile = qyml.get("profile", profile)

    fu_yaml_root = Path(
        Path(bench_file).parent.parent,
        "configs/hw_interface/functional_units",
        tech_model,
        lat_ns,
        profile,
    )

    for fu_dir in fu_yaml_root.iterdir():
        alias = fu_dir.name
        clsname = "".join(w.capitalize() for w in alias.split("_"))
        SimObj = getattr(m5.objects, clsname)
        setattr(acc.hw_interface.functional_units, alias, SimObj())

    #  Instructions
    acc.hw_interface.inst_config = InstConfig()

    inst_list_path = Path(
        Path(bench_file).parent.parent,
        "configs/hw_interface/instructions/inst_list.yml",
    )
    inst_yaml = yaml.safe_load(inst_list_path.read_text())

    for opcode in sorted(inst_yaml["instructions"].keys()):
        clsname = "".join(w.capitalize() for w in opcode.split("_"))
        SimObj = getattr(m5.objects, clsname)
        setattr(acc.hw_interface.inst_config, opcode, SimObj())

    acc.hw_interface.salam_power_model = SALAMPowerModel()
    acc.hw_interface.hw_statistics = HWStatistics()
    acc.hw_interface.simulator_config = SimulatorConfig()
    acc.hw_interface.opcodes = InstOpCodes()
