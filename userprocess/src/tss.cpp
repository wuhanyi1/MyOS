#include "tss.h"
#include "stdint.h"
#include "global.h"
#include "string.h"
#include "k_printf.h"

static struct tss tss;

/* 更新tss中esp0字段的值为pthread的0级线,即设置当前进程的内核栈顶指针，也就是在PCB中 */
void update_tss_esp(task_struct* pthread) {
   tss.esp0 = (uint32_t*)((uint32_t)pthread + PG_SIZE);
}

/* 创建gdt描述符 */
static gdt_desc make_gdt_desc(uint32_t* desc_addr, uint32_t limit, uint8_t attr_low, uint8_t attr_high) {
   uint32_t desc_base = (uint32_t)desc_addr;
   gdt_desc desc;
   desc.limit_low_word = limit & 0x0000ffff;
   desc.base_low_word = desc_base & 0x0000ffff;
   desc.base_mid_byte = ((desc_base & 0x00ff0000) >> 16);
   desc.attr_low_byte = (uint8_t)(attr_low);
   desc.limit_high_attr_high = (((limit & 0x000f0000) >> 16) + (uint8_t)(attr_high));
   desc.base_high_byte = desc_base >> 24;
   return desc;
}

/* 在gdt中创建tss并重新加载gdt */
void tss_init() {
   k_printf("tss_init start\n");
   uint32_t tss_size = sizeof(tss);
   memset(&tss, 0, tss_size);
   tss.ss0 = SELECTOR_K_STACK;//内核栈段选择子
   tss.io_base = tss_size;//表示没有位图

/*gdt段基址为0x900,把tss放到第4个位置,前面有空、内核代码，内核数据段，内核显存段4个描述符了，也就是0x900+0x20的位置*/

  /* 在gdt中添加dpl为0的TSS描述符 */
  *((gdt_desc*)0xc0000920) = make_gdt_desc((uint32_t*)&tss, tss_size - 1, TSS_ATTR_LOW, TSS_ATTR_HIGH);

  /* 在gdt中添加dpl为3即用户的数据段和代码段描述符 */
  *((gdt_desc*)0xc0000928) = make_gdt_desc((uint32_t*)0, 0xfffff, GDT_CODE_ATTR_LOW_DPL3, GDT_ATTR_HIGH);
  *((gdt_desc*)0xc0000930) = make_gdt_desc((uint32_t*)0, 0xfffff, GDT_DATA_ATTR_LOW_DPL3, GDT_ATTR_HIGH);
   
  /* gdt 16位的limit 32位的段基址 */
   uint64_t gdt_operand = ((8 * 7 - 1) | ((uint64_t)(uint32_t)0xc0000900 << 16));   // 7个描述符大小
   asm volatile ("lgdt %0" : : "m" (gdt_operand));
   asm volatile ("ltr %w0" : : "r" (SELECTOR_TSS));//将TSS所在选择子加载到TR寄存器中
   k_printf("tss_init and ltr done\n");
}

