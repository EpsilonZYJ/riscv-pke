#!/bin/bash
echo "=============================Start Compile================================="
make clean
make
echo "============================Compile Complete==============================="
spike ./obj/riscv-pke ./obj/app_relativepath
