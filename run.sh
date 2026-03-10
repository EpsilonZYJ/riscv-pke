#!/bin/bash
echo "=============================Start Compile================================="
make clean
make
rm -rf output.txt
echo "============================Compile Complete==============================="
spike -p2 obj/riscv-pke /bin/zsh
