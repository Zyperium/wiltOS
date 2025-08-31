#pragma once
int  kbd_getch(void);
void kbd_init(void);
void kbd_isr_poll(void);
void kbd_ps2_drain(void);
