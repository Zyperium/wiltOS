#!/usr/bin/env bash
set -euo pipefail

export MTOOLS_SKIP_CHECK=1

KERNEL=bin/wiltOS
BOOT_IMG=image.hdd
DATA_IMG=disk.img
BOOT_SIZE_M=64
DATA_SIZE_M=64
SECTOR=512
START_LBA=2048
OFFSET_BYTES=$((START_LBA * SECTOR))

# ---- Run QEMU ----
qemu-system-x86_64 \
  -drive file="$BOOT_IMG",format=raw,if=ide,cache=writeback \
  -drive file="$DATA_IMG",format=raw,if=ide,cache=writeback,snapshot=off \
  -m 512M -serial stdio
