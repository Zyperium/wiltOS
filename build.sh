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

make clean
make all

truncate -s ${BOOT_SIZE_M}M "$BOOT_IMG"
sgdisk -Z "$BOOT_IMG"
sgdisk -o "$BOOT_IMG"
sgdisk -n 1:${START_LBA}:0 -t 1:ef00 -c 1:"EFI System" "$BOOT_IMG"
limine bios-install "$BOOT_IMG"

mformat -i "$BOOT_IMG@@${OFFSET_BYTES}" ::
mmd     -i "$BOOT_IMG@@${OFFSET_BYTES}" ::/EFI ::/EFI/BOOT ::/boot ::/boot/limine
mcopy   -i "$BOOT_IMG@@${OFFSET_BYTES}" "$KERNEL"                          ::/boot
mcopy   -i "$BOOT_IMG@@${OFFSET_BYTES}" limine.conf                        ::/boot/limine
mcopy   -i "$BOOT_IMG@@${OFFSET_BYTES}" /usr/share/limine/limine-bios.sys  ::/boot/limine
mcopy   -i "$BOOT_IMG@@${OFFSET_BYTES}" /usr/share/limine/BOOTX64.EFI      ::/EFI/BOOT

qemu-img create -f raw "$DATA_IMG" ${DATA_SIZE_M}M
sfdisk "$DATA_IMG" <<'EOF'
label: dos
, , c, *
EOF

mformat -i "$DATA_IMG@@${OFFSET_BYTES}" -F -n 32 -h 64 -t 64 -H 2048 -c 1 -v DATA ::
mmd     -i "$DATA_IMG@@${OFFSET_BYTES}" ::/BIN
mcopy   -i "$DATA_IMG@@${OFFSET_BYTES}" initrd/bin/hello ::/BIN/HELLO.ELF
mcopy   -i "$DATA_IMG@@${OFFSET_BYTES}" initrd/bin/edit  ::/BIN/EDIT.ELF

head -c 102400 /dev/urandom > sample.bin
mcopy -i "$DATA_IMG@@${OFFSET_BYTES}" sample.bin ::/SAMPLE.BIN
rm -f sample.bin

qemu-system-x86_64 \
  -drive file="$BOOT_IMG",format=raw,if=ide,cache=writeback \
  -drive file="$DATA_IMG",format=raw,if=ide,cache=writeback,snapshot=off \
  -m 512M -serial stdio
