#!/bin/bash
echo "=============================Start Compile================================="
make clean
make
echo "============================Compile Complete==============================="
spike -p2 ./obj/riscv-pke ./obj/app_alloc0 ./obj/app_alloc1
