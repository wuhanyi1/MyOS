#ifndef __LIB_KERNEL_BITMAP_H
#define __LIB_KERNEL_BITMAP_H
#include "global.h"
#define BITMAP_MASK 1
struct bitmap {
   uint32_t btmp_bytes_len;//位图所占的字节数
/* 在遍历位图时,整体上以字节为单位,细节上是以位为单位,所以此处位图的指针必须是单字节 */
   uint8_t* bits;
   bitmap();
   void init();
   bool bitmap_test(uint32_t bit_idx);
   int bitmap_alloc(uint32_t cnt);
   void bitmap_set(uint32_t bit_idx, uint8_t value);
};


#endif
