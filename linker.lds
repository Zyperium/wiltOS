OUTPUT_FORMAT(elf64-x86-64)
ENTRY(kmain)

PHDRS {
  limine_requests PT_LOAD FLAGS(4);
  text            PT_LOAD FLAGS(5);
  rodata          PT_LOAD FLAGS(4);
  data            PT_LOAD FLAGS(6);
}

SECTIONS {
  . = 0xffffffff80000000;
  __kernel_start = .;

  .limine_requests : {
    KEEP(*(.limine_requests_start))
    KEEP(*(.limine_requests))
    KEEP(*(.limine_requests_end))
  } :limine_requests

  . = ALIGN(0x1000);
  .text : { *(.text .text.*) } :text

  . = ALIGN(0x1000);
  .rodata : { *(.rodata .rodata.*) *(.note.gnu.build-id) } :rodata

  . = ALIGN(0x1000);
  .data : { *(.data .data.*) } :data

  .bss : { *(.bss .bss.*) *(COMMON) } :data

  __kernel_end = .;

  /DISCARD/ : {
    *(.eh_frame*)
    *(.note .note.*)
    *(.interp) *(.dynamic) *(.dynsym) *(.dynstr) *(.gnu.*) *(.hash) *(.gnu.hash)
    *(.plt*) *(.iplt) *(.rel*) *(.rela*) *(.igot*) *(.got*) *(.comment)
  }
}
