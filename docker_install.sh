cd ~

apt-get install autoconf automake autotools-dev curl libmpc-dev libmpfr-dev libgmp-dev gawk build-essential bison flex texinfo gperf libtool patchutils bc zlib1g-dev libexpat-dev device-tree-compiler

git clone https://github.com/riscv/riscv-isa-sim.git

cd riscv-isa-sim

./configure --prefix=$RISCV

make

make install
