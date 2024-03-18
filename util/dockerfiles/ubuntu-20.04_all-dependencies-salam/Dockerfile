FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt -y update
RUN apt -y upgrade
RUN apt -y install build-essential git m4 scons zlib1g zlib1g-dev \
                   libprotobuf-dev protobuf-compiler libprotoc-dev libgoogle-perftools-dev \
                   python3-dev python3-pip python-is-python3 doxygen libboost-all-dev \
                   libhdf5-serial-dev python3-pydot libpng-dev libelf-dev pkg-config \
                   llvm-12 llvm-12-tools llvm-12-dev clang-12 python3-venv
RUN apt clean
RUN rm -rf /var/lib/apt/lists/*

# Determine the correct name of llvm-config executable and create a symbolic link
RUN update-alternatives --install /usr/bin/llvm-config llvm-config /usr/bin/llvm-config-12 100

RUN pip install --upgrade pip
RUN pip install mypy
