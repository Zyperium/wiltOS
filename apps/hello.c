#include <stdint.h>
#include "../src/Exec/app_api.h"

int app_main(const struct app_api* api, const char* arg){
    api->puts("[hello] start\n");
    if (arg && *arg){ api->puts("arg="); api->puts(arg); api->putc('\n'); }
    api->puts("[hello] press any key to exit\n");
    for(;;){
        int ch = api->getch();
        if (ch >= 0) break;
        __asm__ __volatile__("hlt");
    }
    api->puts("[hello] bye\n");
    return 0;
}
