#!/bin/bash
set -e
export FORCE_UNSAFE_CONFIGURE=1

cd /mnt/data3t/builddata/src/mt7621/mtk-openwrt-lede-4.2.1.0

echo "=== 1. Sync config ==="
cp -f autobuild/mt7621da-5port-switch/.config .config
cp -f autobuild/mt7621da-5port-switch/config-4.4 target/linux/ramips/mt7621/config-4.4

make defconfig

echo "=== 2. Compiling OpenWrt (8 cores) ==="
make -j8 V=s

echo "=== 3. Target Images ==="
ls -lh bin/targets/ramips/mt7621/*5port* || ls -lh bin/targets/ramips/mt7621/
