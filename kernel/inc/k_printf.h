#ifndef __K_PRINTF_H__
#define __K_PRINTF_H__
#include "stdint.h"
void k_printf(const char* format, ...);
void set_cursor(uint16_t offset);
void vsprintf(char* buf,const char* format,uint32_t* args);
void sprintf(char* buf,const char* format,...);
void clean_line(uint8_t begin_,uint8_t line);
void printfk(const char* format, ...);//互斥版本的
#endif