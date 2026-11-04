#!/bin/bash
set -e
export PATH=/opt/buildroot-gcc492_mips_glibc/usr/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
export CROSS_COMPILE=mipsel-linux-

cd /mnt/data3t/builddata/src/mt7621/u-boot-mt7621-2018.09-20210902-git41e9b526

echo "=== 1. Checking compiler ==="
which mipsel-linux-gcc
mipsel-linux-gcc -v 2>&1 | tail -n 2

echo "=== 2. Cleaning ==="
make distclean

echo "=== 3. Defconfig ==="
make mt7621_5port_defconfig

echo "=== 4. Compiling ==="
make -j8

echo "=== 5. Finished artifacts ==="
ls -lh u-boot-mt7621.bin
stat -c '%n: %s bytes' u-boot-mt7621.bin
sha256sum u-boot-mt7621.bin
