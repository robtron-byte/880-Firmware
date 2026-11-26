/*
 * ui.h
 * 880 Perform: key handling (EDIT / PERFORM / PATTERN CLEAR) and LED rendering.
 */
#ifndef UI_H
#define UI_H

#include <stdint.h>

extern volatile uint16_t ui_ticks; //10 ms ticks (Timer4)

void ui_init(void);
void ui_service(void); //call once per refresh, before spi_read_write()

#endif
