#!/bin/bash
echo "=============================Start Compile================================="
make clean
make
#rm -rf output.txt
echo "============================Compile Complete==============================="
spike obj/riscv-pke /bin/app_shell
