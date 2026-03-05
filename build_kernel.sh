#!/bin/bash

# Configuration
ARCH=arm64
SUBARCH=arm64
DEFCONFIG=cupida_defconfig
TOOLCHAIN=/home/captain/Projects/toolchain/proton-clang
KBUILD_BUILD_USER=xCaptaiN09
KBUILD_BUILD_HOST=NetHunter

# Force minimal PATH for Step 1
SAFE_PATH="/usr/bin:/bin:/usr/sbin:/sbin"

# Step 1: Clean out directory
echo "Cleaning out directory..."
rm -rf out

# Step 2: Configure and build host tools
echo "Step 1: Configuring and building scripts..."
make ARCH=$ARCH O=out \
    HOSTCC=/usr/bin/gcc \
    HOSTCXX=/usr/bin/g++ \
    CC=/usr/bin/gcc \
    CROSS_COMPILE=aarch64-linux-gnu- \
    $DEFCONFIG

make ARCH=$ARCH O=out \
    HOSTCC=/usr/bin/gcc \
    HOSTCXX=/usr/bin/g++ \
    CC=/usr/bin/gcc \
    CROSS_COMPILE=aarch64-linux-gnu- \
    scripts -j$(nproc --all)

# Step 3: Build the kernel using Toolchain
echo "Step 2: Compiling kernel with Proton Clang..."
# Cross tools variables using absolute paths
CLANG=$TOOLCHAIN/bin/clang
LD=$TOOLCHAIN/bin/ld.lld
AR=$TOOLCHAIN/bin/llvm-ar
NM=$TOOLCHAIN/bin/llvm-nm
OBJCOPY=$TOOLCHAIN/bin/llvm-objcopy
OBJDUMP=$TOOLCHAIN/bin/llvm-objdump
STRIP=$TOOLCHAIN/bin/llvm-strip

# Build actual kernel. 
# Note: We do NOT put toolchain in PATH. We pass absolute paths to MAKE instead.
# This prevents host tools from ever "finding" toolchain bins.
make ARCH=$ARCH SUBARCH=$SUBARCH O=out \
    CC="$CLANG" \
    LD="$LD" \
    AR="$AR" \
    NM="$NM" \
    OBJCOPY="$OBJCOPY" \
    OBJDUMP="$OBJDUMP" \
    STRIP="$STRIP" \
    CROSS_COMPILE=aarch64-linux-gnu- \
    CROSS_COMPILE_ARM32=arm-linux-gnueabi- \
    HOSTCC=/usr/bin/gcc \
    HOSTCXX=/usr/bin/g++ \
    HOSTLD=/usr/bin/ld \
    HOSTAS=/usr/bin/as \
    -j$(nproc --all) 2>&1 | tee build.log

if [ -f "out/arch/arm64/boot/Image.gz-dtb" ]; then
    echo "--- Build Success ---"
    echo "Output: out/arch/arm64/boot/Image.gz-dtb"
else
    echo "--- Build Failed ---"
    exit 1
fi
