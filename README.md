# elf-obj

A simple tool to list the objects of an ELF file in size order.

## Building

    gcc main.c -o elf-obj

## Building and installing with CMake

    cmake . -Bbuild
    cmake --build build --target install

## Usage

    elf-obj <binary>

To demangle C++ symbols, pipe through `c++filt`

    elf-obj <binary> | c++filt
