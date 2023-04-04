#ifndef __LIB__STDIO_H
#define __LIB__STDIO_H
#include "stdint.h"

typedef void* va_list;
uint32_t vsprintf(char* str,const char* format,va_list ap);
uint32_t sprintf(char* buf,const char* format, ...);
uint32_t printf(const char* str, ...);

#endif

