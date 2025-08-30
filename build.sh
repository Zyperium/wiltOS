rm -rf bin/wiltOS
rm -rf build/

make

dd if=/dev/zero bs=1M count=0 seek=64 of=image.hdd
sgdisk image.hdd -n 1:2048 -t 1:ef00 -m 1

limine bios-install image.hdd

mformat -i image.hdd@@1M ::
mmd -i image.hdd@@1M ::/EFI ::/EFI/BOOT ::/boot ::/boot/limine
mcopy -i image.hdd@@1M bin/wiltOS ::/boot
mcopy -i image.hdd@@1M limine.conf ::/boot/limine
mcopy -i image.hdd@@1M /usr/share/limine/limine-bios.sys ::/boot/limine
mcopy -i image.hdd@@1M /usr/share/limine/BOOTX64.EFI ::/EFI/BOOT

qemu-system-x86_64 -drive file=image.hdd,format=raw -m 512M -serial stdio
