docker run -it --name pke_mirror \
  --mount type=bind,source=~/Developer/HustWorkfield/riscv-pke,target=/app/riscv-pke \
  ubuntu:latest /bin/bash
