TOOLCHAIN_PREFIX ?= x86_64-elf-
CC := $(TOOLCHAIN_PREFIX)gcc
LD := $(TOOLCHAIN_PREFIX)ld

CFLAGS := -std=gnu11 -ffreestanding -fno-stack-protector -fno-stack-check \
          -fno-lto -fno-pic -fno-plt -fno-pie -m64 -march=x86-64 -mabi=sysv \
          -mno-80387 -mno-mmx -mno-sse -mno-sse2 -mno-red-zone -mcmodel=kernel
LDFLAGS := -T linker.lds -nostdlib -z max-page-size=0x1000

# collect all .c files recursively
SRC := $(shell find src -type f -name '*.c')

# turn src/%.c into obj/%.o
OBJ := $(patsubst src/%.c,obj/%.o,$(SRC))

# generic compile rule (mirrors folder structure and makes dirs)
obj/%.o: src/%.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -I src -c $< -o $@

OUT := bin/wiltOS

all: $(OUT)

$(OUT): $(OBJ) linker.lds
	mkdir -p $(@D)
	$(LD) $(LDFLAGS) $(OBJ) -o $@

INITRD := build/initrd.tar
$(INITRD):
	mkdir -p build
	tar --format=ustar -C initrd -cf $(INITRD) .

obj/initrd.o: $(INITRD)
	mkdir -p $(@D)
	$(LD) -r -b binary -o $@ $(INITRD)

# add obj/initrd.o to link
$(OUT): obj/initrd.o $(OBJ) linker.lds
	mkdir -p $(@D)
	$(LD) $(LDFLAGS) obj/initrd.o $(OBJ) -o $@



clean:
	rm -rf obj bin image.hdd limine
