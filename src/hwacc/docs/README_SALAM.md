# gem5-SALAM #

gem5-SALAM (System Architecture for LLVM-based Accelerator Modeling), is a novel system architecture designed to enable LLVM-based modeling and simulation of custom hardware accelerators.

# Requirements

- gem5 dependencies
- LLVM-9 or newer
- Frontend LLVM compiler for preferred development language (eg. clang for C)

# gem5-SALAM Setup

## All Required Dependencies for gem5-SALAM (Ubuntu 20.04)

```bash
sudo apt install build-essential git m4 scons zlib1g zlib1g-dev \
    libprotobuf-dev protobuf-compiler libprotoc-dev libgoogle-perftools-dev \
    python3-dev python-is-python3 libboost-all-dev pkg-config
```

## LLVM/Clang Setup

For a quick start, one can simply run the following to install LLVM and Clang on Ubuntu 20.04.
```bash
sudo apt install llvm-12 llvm-12-tools clang-12
```
After installing these specific libraries, simply run the [update alternatives](https://github.com/TeCSAR-UNCC/gem5-SALAM/blob/main/docs/update-alternatives.sh) script in docs/.

Alternatively, you can install the latest version of LLVM via your system package manager or build from source found at https://github.com/llvm/llvm-project.

# Building gem5-SALAM

Once you have successfully installed all of the necessary dependencies, you can go ahead and clone the gem5-SALAM repository to begin working with it.

```bash
git clone https://github.com/TeCSAR-UNCC/gem5-SALAM
```

Before building gem5-SALAM, you must generate source and header files for hardware modeling using the provided script:

```bash
$M5_PATH/tools/hw_generator/generate_hw.sh
```

This script generates necessary functional unit and instruction configuration files required for successful compilation. Be sure to run it before invoking scons.

When building gem5-SALAM, there are multiple different binary types that can be created. Just like in gem5 the options are debug, opt, fast, prof, and perf. We recommend that users either use the opt or debug builds, as these are the build types we develop and test on.

Below are the bash commands you would use to build the opt or debug binary.

```bash
scons build/ARM/gem5.opt -j`nproc`
```

```bash
scons build/ARM/gem5.debug -j`nproc`
```

For more information regarding the binary types, and other build information refer to the gem5 build documentation [here](http://learning.gem5.org/book/part1/building.html).

# Building with docker
You can use the Dockerfile given in the `docker/` directory to build the project and run the benchmarks. To build the project use the following command:
```bash
docker build . --file docker/Dockerfile --build-arg BUILD_TYPE="opt"
```

The `BUILD_TYPE` argument sets the the building option for the project and can be `opt` or `debug`.

# Using gem5-SALAM

To use gem5-SALAM you need to define the computation model of you accelerator in your language of choice,and compile it to LLVM IR. Any control and dataflow graph optimization (eg. loop unrolling) should be handled by the compiler. You can construct accelerators by associating their LLVM IR with an LLVMInterface and connecting it to the desired CommInterface in the gem5 memory map.

Below are some resources in the gem5-SALAM directory that can be used when getting started:

- Examples for system-level configuration can be found in **configs/common/HWAcc.py**.
- Accelerator benchmarks and examples can be found in the **benchmarks** directory.
- The **benchmarks/common** directory contains basic drivers and syscalls for baremetal simulation.
- **benchmarks/sys_validation** contains examples for configuring and using gem5-SALAM with different algorithms.

## System Validation Examples

The system validation examples under **benchmarks/sys_validation** are good examples for how you interface with the gem5-SALAM simulation objects.

In order to use the system validation benchmarks, it is required to have the ARM GCC cross-compiler installed. If you didn't already install it when you setup the dependencies, you can install it in Ubuntu by running the below command:

```bash
sudo apt-get install gcc-multilib gcc-arm-none-eabi
```

**run_system.sh** requires environment variables named **M5_PATH** and **ACC_BENCH_PATH** to be set. You will want to point them to your gem5 and benchmark root paths (respectively) as shown below.

```bash
export M5_PATH=/path/to/gem5 root
```

```bash
export ACC_BENCH_PATH=/path/to/benchmarks root
```

All paths passed at runtime as arguments would be relative to this benchmark root.

Next, compile your desired example.

```bash
cd $ACC_BENCH_PATH/benchmarks/sys_validation/[benchmark]
make
```

Finally, you can run any of the benchmarks you have compiled by running the run system script.

```bash
$M5_PATH/tools/run_system.sh --bench bfs --bench-path benchmarks/sys_validation/bfs
```

If you would like to see the gem5-SALAM command created by the shell file you would just need to inspect the **RUN_SCRIPT** variable in the shell file.

## Using Custom Hardware Profiles

The gem5-SALAM toolchain also allows you to run benchmarks with custom hardware profiles to be specified in YAML files. The hardware generator in **tools/hw_generator** auto-generates functional unit and instruction files using the specified YAML profile.

To utilize this functionality, the following script must be run to generate source code for functional unit and instruction timing models before building and running gem5.

```bash
python3 tools/hw_generator/HWProfileGenerator.py -b <benchmark_name>
```

## Power Modeling using cacti-SALAM

The cacti-SALAM toolchain is a mini-suite of Python scripts to drive CACTI analyses on SALAM scratchpad memories based on YAML accelerator config, aggregating power/area/delay results.

Start by running the setup script:

```bash
cd tools/cacti_salam
./setup_cacti_salam.py
```

Next, in `$ACC_BENCH_PATH/benchmarks.list`, prepare a list of lines with the following fields:

```
path/to/config.yml <benchmark name> <config name>
```

Finally, run cacti-SALAM using the following script to generate the lookup table:

```bash
python3 ./run_cacti_salam.py --bench-list $ACC_BENCH_PATH/benchmarks.list --delay 1.0
```

Check `tools/cacti-SALAM/results/SALAM-out.csv` for the consolidated table of `Benchmark,Config,Acc,…<CACTI columns>`, which will be used by the simulation to generate power/area/delay results.

# Resources

## gem5 Documentation

https://www.gem5.org/documentation/

## gem5 Tutorial

The gem5 documentation has a [tutorial for working with gem5](http://learning.gem5.org/book/index.html#) that will help get you started with the basics of creating your own sim objects.

## Building and Integrating Accelerators in gem5-SALAM

We have written a guide on how to create the GEMM system validation example. This will help you get started with creating your own benchmarks and systems. It can be viewed [here](https://github.com/TeCSAR-UNCC/gem5-SALAM/blob/master/docs/Building_and_Integrating_Accelerators.md).

## SALAM Object Overview

The [SALAM Object Overview](https://github.com/TeCSAR-UNCC/gem5-SALAM/blob/master/docs/SALAM_Object_Overview.md) covers what various Sim Objects in gem5-SALAM are and their purpose.

## Full-system OS Simulation ##

Please download the latest version of the Linux Kernel for ARM from the [gem5 ARM kernel page](http://gem5.org/ARM_Kernel).
You will also need the [ARM disk images](http://www.gem5.org/dist/current/arm/) for full system simulation.
Devices operate in the physical memory address space.
