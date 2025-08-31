# wiltOS
OS loaded by limine and written in C.

# Prerequisites
Limine, x86_64-elf-gcc, x86_64-elf-ld, any virtual machine (I use qemu)

# Usage right now?
Not very useful. It can run ELF programs, but you'd need to write your own.
The api is very lacking right now, and you can't write to the disk, only 
memory (changes are reset on reboot). However, this will change (soon?)

# Commands
1) help
2) run <dir> (planned to become obsolete, run programs by simply typing the exe name)
3) echo <arg>
4) cat <dir>
5) ls <dir>
6) cd <dir>
7) pwd <dir>

# API
For whatever reason, if you want to write an executable for the os, make a new C
file in 'apps' and add the .h "../src/Exec/app_api.h" the things you can get the
os to do are:
void (*puts)(const char*); // put string
void (*putc)(char); // put char
int  (*getch)(void); // get char (keyboard input)
void (*hex64)(uint64_t); // write hex64
int  (*write_file)(const char*, const uint8_t*, uint64_t); // "fake" writing to files

once you've got the app_api.h, create your entry:
```C
int app_main(const struct app_api* api, const char* arg) {
    // code here
}
```
when you want to do something with your code, simply call:
```C
api->puts("example text printed to console");
// or
api->putc('\n');
```

and rebuild apps with
```
make apps
```