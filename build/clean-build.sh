#!/bin/sh
cd /extra2/projects/home/rgds-kernel/src || exit 1
export PATH=/extra2/projects/home/rgds-kernel/toolchain/clang-r487747c/bin:$PATH
make ARCH=arm64 LLVM=1 mrproper >/dev/null
sh /extra2/projects/home/rgds-kernel/build-ack.sh
