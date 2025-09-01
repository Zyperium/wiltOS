#include <stdint.h>
#include "../src/Exec/app_api.h"

static inline int is_print(int ch){ return ch>=32 && ch<127; }

int app_main(const struct app_api* api, const char* arg){
    char buf[4096]; int n=0;
    api->puts("[edit] path required, ESC aborts\n");
    if (!arg || !*arg){ api->puts("usage: edit /disk/NAME.TXT\n"); return 1; }
    for(;;){
        int ch = api->getch();
        if (ch < 0){ __asm__ __volatile__("hlt"); continue; }
        if (ch == 27){ api->puts("\n[edit] abort\n"); return 2; }
        if (ch == '\r' || ch == '\n'){ api->putc('\n'); break; }
        if ((ch==8 || ch==127) && n){ n--; api->puts("\b \b"); continue; }
        if (is_print(ch) && n < (int)sizeof(buf)-1){ buf[n++] = (char)ch; api->putc((char)ch); }
    }
    buf[n]=0;
    int rc = api->write_file(arg, (const uint8_t*)buf, (uint32_t)n);
    if (rc==0){ api->puts("[edit] saved\n"); return 0; }
    api->puts("[edit] save failed\n"); return 3;
}
