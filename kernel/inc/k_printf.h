#ifndef __K_PRINTF_H__
#define __K_PRINTF_H__
#include "stdint.h"
void k_printf(const char* format, ...);
void set_cursor(uint16_t offset);
void clean_line(uint8_t begin_,uint8_t line);
#endif