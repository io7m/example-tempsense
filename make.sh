#!/bin/sh -ex

mkdir -p build

clang-format -i main.cpp
clang-format -i uart.h
clang-format -i uart.c

./c-compile build/uart.o uart.c
./cxx-compile build/main.o main.cpp
./cxx-link build/main build/main.o build/uart.o
./hex build/main.hex build/main

