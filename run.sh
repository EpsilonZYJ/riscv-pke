#!/bin/bash
echo "=============================Start Compile================================="
make clean
make
echo "============================Compile Complete==============================="
spike -p2 ./obj/riscv-pke ./obj/app0 ./obj/app1
