#!/bin/sh
cd /extra2/projects/home/rgds-kernel/src || exit 1
export PATH=/extra2/projects/home/rgds-kernel/toolchain/clang-r487747c/bin:$PATH
export KCFLAGS=-D__ANDROID_COMMON_KERNEL__
make -k -j32 ARCH=arm64 LLVM=1 "$1"
echo "DIR EXIT $?"
