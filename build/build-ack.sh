#!/bin/sh
set -x
cd /extra2/projects/home/rgds-kernel/src || exit 1
export PATH=/extra2/projects/home/rgds-kernel/toolchain/clang-r487747c/bin:$PATH
export KBUILD_BUILD_USER=builder1 KBUILD_BUILD_HOST=dell-PowerEdge-R730XD KCFLAGS=-D__ANDROID_COMMON_KERNEL__ KBUILD_BUILD_VERSION=31
export KBUILD_BUILD_TIMESTAMP="Tue Sep 29 16:47:09 CST 2026"
K=/extra2/projects/home/rgds-kernel/stock/stock-6.1.141.config
clang --version | head -1
cp $K .config && make ARCH=arm64 LLVM=1 olddefconfig || exit 2
./scripts/diffconfig $K .config > ../config-diff-r8c.txt
echo "DIFFCONFIG DONE"
make -k -j32 ARCH=arm64 LLVM=1 Image dtbs
echo "BUILD EXIT $?"
