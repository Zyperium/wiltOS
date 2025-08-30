#include <stdint.h>
#include "../src/Exec/app_api.h"

static inline int is_print(int ch){ return ch>=32 && ch<127; }

int app_main(const struct app_api* api, const char* arg){
    char buf[1024]; int n=0;
    const char* outpath = (arg && *arg) ? arg : "/edit.txt";
    api->puts("[edit] type, Enter to save, Esc to quit\n");
    api->puts("path: "); api->puts(outpath); api->putc('\n');
    for(;;){
        int ch = api->getch();
        if (ch < 0){ __asm__ __volatile__("hlt"); continue; }
        if (ch == 27){ api->puts("\n[edit] abort\n"); return 1; }
        if (ch == '\r' || ch == '\n'){ api->putc('\n'); break; }
        if ((ch==8 || ch==127) && n){ n--; api->puts("\b \b"); continue; }
        if (is_print(ch) && n < (int)sizeof(buf)-1){ buf[n++] = (char)ch; api->putc((char)ch); }
    }
    buf[n]=0;
    int rc = api->write_file(outpath, (const uint8_t*)buf, (uint64_t)n);
    if (rc==0){ api->puts("[edit] saved\n"); return 0; }
    api->puts("[edit] save failed\n"); return 2;
}
