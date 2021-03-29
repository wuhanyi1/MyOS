#ifndef __INTERRUPT_H__
#define __INTERRUPT_H__
#include "stdint.h"

typedef void* intr_handler; //每个中断的处理函数,实际上就是一个void*类型的地址
//using intr_handler = void(void);//每个中断的处理函数
void idt_init(void);

/*中断门描述符结构体*/
struct gate_desc {
   uint16_t    func_offset_low_word;
   uint16_t    selector;
   uint8_t     dcount;   //此项为双字计数字段，是门描述符中的第4字节。此项固定值，不用考虑
   uint8_t     attribute;
   uint16_t    func_offset_high_word;
};

/* 定义中断的两种状态:
 * INTR_OFF值为0,表示关中断,
 * INTR_ON值为1,表示开中断 */
enum intr_status {		 // 中断状态
    INTR_OFF,			 // 中断关闭
    INTR_ON		         // 中断打开
};

enum intr_status intr_get_status(void);//获得当前中断状态
enum intr_status intr_set_status (enum intr_status);//设置中断状态
enum intr_status intr_enable (void);//开中断
enum intr_status intr_disable (void);//关中断
void register_handler(uint8_t vector_no, intr_handler function);//注册中断处理函数
#endif
