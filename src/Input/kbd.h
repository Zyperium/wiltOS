#pragma once
#include <stdint.h>
void kbd_init(void);
void kbd_irq(void);
int  kbd_getch(void);
