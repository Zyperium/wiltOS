# wiltOS
OS loaded by limine and written in C.

# Prerequisites
Limine, x86_64-elf-gcc, x86_64-elf-ld, any virtual machine (I use qemu)

# Status/Known bugs
1) --API write to disk doesn't actually write to disk
2) using new dls commands fail and may cause an error
3) may be unstable

# Commands
1) help
2) run <dir> [obsolete]
3) echo <arg>
4) cat <dir>
5) ls <dir>
6) cd <dir>
7) pwd <dir>
8) dls <dir> [broken]
9) dcd <dir> [broken]
   
# API
For whatever reason, if you want to write an executable for the os, make a new C file in 'apps' and add the .h "../src/Exec/app_api.h" the api supplies a few functions that can be called from your program.
```C
void (*puts)(const char*); // put string

void (*putc)(char); // put char

int (*getch)(void); // get char (keyboard input)

void (*hex64)(uint64_t); // write hex64

int (*write_file)(const char*, const uint8_t*, uint64_t); // "fake" writing to files will be fixed
```
once you've included the app_api.h, create your entry:
```C
int app_main(const struct app_api* api, const char* arg) {
    // arg has the paramaters the file was run with
   // api is how you'll interface the OS
}
```
when you want the OS to take over, use the api:
```C
api->puts("example text printed to console");
// or
api->putc('\n');
```
Use run.sh to rebuild your apps. (Note, even though you can `make apps`, it won't add the app to disk.img and therefore if you want to use your compiled binary you've got to either build it manually or use run.sh) 
```bash
./run.sh
```
