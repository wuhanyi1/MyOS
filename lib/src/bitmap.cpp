#include "bitmap.h"
#include "stdint.h"
#include "string.h"
#include "k_printf.h"
#include "interrupt.h"
#include "debug.h"

/* 将位图btmp初始化 */
bitmap::bitmap():btmp_bytes_len(0),bits((uint8_t*)0) {}

//初始化位图真正的内容为0
void bitmap::init() {
   memset(bits, 0, btmp_bytes_len);   
}

/* 判断bit_idx位是否为1,若为1则返回true，否则返回false */
bool bitmap::bitmap_test(uint32_t bit_idx) {
   uint32_t byte_idx = bit_idx / 8;    // 向下取整用于索引数组下标
   uint32_t bit_odd  = bit_idx % 8;    // 取余用于索引数组内的位
   return (bits[byte_idx] & (BITMAP_MASK << bit_odd));
}

/* 在位图中申请连续cnt个位,成功则返回其起始位下标，失败返回-1 */
int bitmap::bitmap_alloc(uint32_t cnt) {
   uint32_t idx_byte = 0;	 // 用于记录空闲位所在的字节

/* 跳过所有位都被分配出去的B */
   while (( 0xff == bits[idx_byte]) && (idx_byte < btmp_bytes_len)) {
/* 1表示该位已分配,所以若为0xff,则表示该字节内已无空闲位,向下一字节继续找 */
      idx_byte++;
   }

   // ASSERT(idx_byte < btmp->btmp_bytes_len);
   // if (idx_byte == btmp->btmp_bytes_len) {  // 若该内存池找不到可用空间		
   //    return -1;
   // }

 /* 若在位图数组范围内的某字节内找到了空闲位，
  * 在该字节内逐位比对,返回空闲位的索引。*/
   int idx_bit = 0;
 /* 和btmp->bits[idx_byte]这个字节逐位对比 */
   while ((uint8_t)(BITMAP_MASK << idx_bit) & bits[idx_byte]) { 
	   idx_bit++;
   }
	 
   int bit_idx_start = idx_byte * 8 + idx_bit;    // 空闲位在位图内的下标
   if (cnt == 1) {
      return bit_idx_start;
   }

   uint32_t bit_left = (btmp_bytes_len * 8 - bit_idx_start);   // 记录还有多少位可以判断
   uint32_t next_bit = bit_idx_start + 1;
   uint32_t count = 1;	      // 用于记录找到的空闲位的个数

   bit_idx_start = -1;	      // 先将其置为-1,若找不到连续的位就直接返回
   while (bit_left-- > 0) {
      if (!(bitmap_test(next_bit))) {	 // 若next_bit为0
	      count++;
      } 
      else {
	      count = 0;
      }
      if (count == cnt) {	    // 若找到连续的cnt个空位
	      bit_idx_start = next_bit - cnt + 1;
	      break;
      }
      next_bit++;          
   }
   return bit_idx_start;
}

/* 将位图btmp的bit_idx位设置为value */
void bitmap::bitmap_set(uint32_t bit_idx, uint8_t value) {
   //ASSERT((value == 0) || (value == 1));
   uint32_t byte_idx = bit_idx / 8;    // 向下取整用于索引数组下标
   uint32_t bit_odd  = bit_idx % 8;    // 取余用于索引数组内的位

/* 一般都会用个0x1这样的数对字节中的位操作,
 * 将1任意移动后再取反,或者先取反再移位,可用来对位置0操作。*/
   if (value) {		      // 如果value为1
      bits[byte_idx] |= (BITMAP_MASK << bit_odd);
   } else {		      // 若为0
      bits[byte_idx] &= ~(BITMAP_MASK << bit_odd);
   }
}

