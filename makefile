TOOLCHAIN_PREFIX ?= x86_64-elf-
CC := $(TOOLCHAIN_PREFIX)gcc
LD := $(TOOLCHAIN_PREFIX)ld

CFLAGS := -std=gnu11 -ffreestanding -fno-stack-protector -fno-stack-check \
          -fno-lto -fno-pic -fno-plt -fno-pie -m64 -march=x86-64 -mabi=sysv \
          -mno-80387 -mno-mmx -mno-sse -mno-sse2 -mno-red-zone -mcmodel=kernel
LDFLAGS := -T linker.lds -nostdlib -z max-page-size=0x1000

SRC := $(shell find src -type f -name '*.c')
OBJ := $(patsubst src/%.c,obj/%.o,$(SRC))
OUT := bin/wiltOS

APPS := edit hello
APP_CC := $(TOOLCHAIN_PREFIX)gcc
APP_CFLAGS := -std=gnu11 -ffreestanding -fpie -fno-stack-protector -fno-stack-check \
              -fno-plt -m64 -march=x86-64 -mno-red-zone -mno-sse -mno-sse2
APP_LDFLAGS := -nostdlib -Wl,-pie -Wl,-e,app_main -Wl,-z,max-page-size=0x1000
APP_ELFS := $(addprefix build/,$(addsuffix .elf,$(APPS)))
INITRD_BINS := $(addprefix initrd/bin/,$(APPS))
INITRD_DIR := initrd
INITRD := build/initrd.tar
INITRD_SRC := $(shell find $(INITRD_DIR) -type f -o -type l 2>/dev/null)

.PHONY: all os apps clean

all: os apps

os: $(OUT)

apps: $(INITRD) obj/initrd.o

$(OUT): obj/initrd.o $(OBJ) linker.lds
	mkdir -p $(@D)
	$(LD) $(LDFLAGS) obj/initrd.o $(OBJ) -o $@

obj/%.o: src/%.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -I src -c $< -o $@

build/%.elf: apps/%.c src/Exec/app_api.h
	mkdir -p $(@D)
	$(APP_CC) $(APP_CFLAGS) $< -o $@ $(APP_LDFLAGS)

initrd/bin/%: build/%.elf
	mkdir -p $(@D)
	install -m 0755 $< $@

$(INITRD): $(INITRD_BINS) $(INITRD_SRC)
	mkdir -p $(@D)
	tar --format=ustar -C $(INITRD_DIR) -cf $@ .

obj/initrd.o: $(INITRD)
	mkdir -p $(@D)
	$(LD) -r -b binary -o $@ $<

clean:
	rm -rf obj bin build $(OUT) $(INITRD)
