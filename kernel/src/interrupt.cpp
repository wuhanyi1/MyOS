#include "interrupt.h"
#include "stdint.h"
#include "global.h"
#include "io.h"
#include "string.h"
#include "k_printf.h"

#define PIC_M_CTRL 0x20	       // 这里用的可编程中断控制器是8259A,主片的控制端口是0x20
#define PIC_M_DATA 0x21	       // 主片的数据端口是0x21
#define PIC_S_CTRL 0xa0	       // 从片的控制端口是0xa0
#define PIC_S_DATA 0xa1	       // 从片的数据端口是0xa1

#define IDT_DESC_CNT 0x81      // 目前总共支持的中断数，即256个

#define EFLAGS_IF   0x00000200       // eflags寄存器中的if位为1
#define GET_EFLAGS(EFLAG_VAR) asm volatile("pushfl; popl %0" : "=g" (EFLAG_VAR))  //获得eflags寄存器的值

static struct gate_desc idt[IDT_DESC_CNT];   // idt是中断描述符表,本质上就是个中断门描述符数组

const char* intr_name[IDT_DESC_CNT];		     // 用于保存异常的名字，异常就相当于CPU保留的中断号

extern "C"{
   uint32_t syscall_handler(void);//0x80的中断处理函数
}

/********    定义中断处理程序数组    ********
 * 在kernel.S中定义的intrXXentry只是中断处理程序的入口,
 * 最终调用的是ide_table中的处理程序,也就是所有的中断来了之后都执行一段相同的代码，然后根据
 * 中断向量号到事先定义的表中查找对应的函数并跳转去执行
 * idt_table就是一个函数指针数组，记录每个中断处理函数的首地址
 * 
 */
intr_handler idt_table[IDT_DESC_CNT]; 
/********************************************/
extern intr_handler intr_entry_table[IDT_DESC_CNT];	    // 声明引用定义在kernel.S中的中断处理函数入口数组

/* 初始化可编程中断控制器8259A */
static void pic_init(void) {

   /* 初始化主片 */
   outb (PIC_M_CTRL, 0x11);   // ICW1: 边沿触发,级联8259, 需要ICW4.
   outb (PIC_M_DATA, 0x20);   // ICW2: 起始中断向量号为0x20,也就是IR[0-7] 为 0x20 ~ 0x27.
   outb (PIC_M_DATA, 0x04);   // ICW3: IR2接从片. 
   outb (PIC_M_DATA, 0x01);   // ICW4: 8086模式, 正常EOI

   /* 初始化从片 */
   outb (PIC_S_CTRL, 0x11);    // ICW1: 边沿触发,级联8259, 需要ICW4.
   outb (PIC_S_DATA, 0x28);    // ICW2: 起始中断向量号为0x28,也就是IR[8-15] 为 0x28 ~ 0x2F.
   outb (PIC_S_DATA, 0x02);    // ICW3: 设置从片连接到主片的IR2引脚
   outb (PIC_S_DATA, 0x01);    // ICW4: 8086模式, 正常EOI
   
// /* 测试键盘,只打开键盘中断，其它全部关闭 */
//    outb (PIC_M_DATA, 0xfd);
//    outb (PIC_S_DATA, 0xff);

// /* 打开主片上IR0,也就是目前只接受时钟产生的中断 */
//    outb (PIC_M_DATA, 0xfe);
//    outb (PIC_S_DATA, 0xff);

    /* IRQ2用于级联从片,必须打开,否则无法响应从片上的中断
  主片上打开的中断有IRQ0的时钟,IRQ1的键盘和级联从片的IRQ2,其它全部关闭 */
   outb (PIC_M_DATA, 0xf8);

   /* 打开从片上的IRQ14,此引脚接收硬盘控制器的中断 */
   outb (PIC_S_DATA, 0xbf);

   k_printf("   pic_init done\n");
}

/* 创建中断门描述符，function即中断处理函数的地址 */
static void make_idt_desc(gate_desc& p_gdesc, uint8_t attr, intr_handler function) { 
   p_gdesc.func_offset_low_word = (uint32_t)function & 0x0000FFFF;//中断处理程序的偏移量
   p_gdesc.selector = SELECTOR_K_CODE;
   p_gdesc.dcount = 0;
   p_gdesc.attribute = attr;
   p_gdesc.func_offset_high_word = ((uint32_t)function & 0xFFFF0000) >> 16;
}

/*初始化中断描述符表*/
static void idt_desc_init(void) {
   int lastindex = IDT_DESC_CNT - 1;
   //为操作系统除0x80以外的终端们描述符的初始化
   for (int i = 0; i < IDT_DESC_CNT; i++) {
      make_idt_desc(idt[i], IDT_DESC_ATTR_DPL0, intr_entry_table[i]); //系统异常DPL为0，只能内核访问
   }
/* 为0x80即系统调用的中断号创建描述符，系统调用对应的中断门dpl为3,
 * 中断处理程序为单独的syscall_handler */
   make_idt_desc(idt[lastindex], IDT_DESC_ATTR_DPL3, (void*)syscall_handler);
   k_printf("   idt_desc_init done\n");
}

/* 通用的中断处理函数,一般用在异常出现时的处理 */
static void general_intr_handler(uint8_t vec_nr) {
   if (vec_nr == 0x27 || vec_nr == 0x2f) {	// 0x2f是从片8259A上的最后一个irq引脚，保留
      return;		//IRQ7和IRQ15会产生伪中断(spurious interrupt),无须处理。
   }
  /* 将光标置为0,从屏幕左上角清出一片打印异常信息的区域,方便阅读 */
   set_cursor(0);
   clean_line(0,4);

   k_printf("!!!!!!!      excetion message begin  !!!!!!!!\n");
   set_cursor(88);	// 从第2行第8个字符开始打印
   k_printf("%s\n",intr_name[vec_nr]);
   
   // 若为Pagefault,将缺失的地址打印出来并悬停,以后要特殊处理(目前只对缺页异常做了特殊处理)
   if (vec_nr == 14) {	  
      int page_fault_vaddr = 0; 
      asm ("movl %%cr2, %0" : "=r" (page_fault_vaddr));	  // cr2是存放造成page_fault的地址
      k_printf("page fault addr is %p\n",page_fault_vaddr);
   }
   k_printf("!!!!!!!      excetion message end    !!!!!!!!\n");
  // 能进入中断处理程序就表示已经处在关中断情况下,
  // 不会出现调度进程的情况。故下面的死循环不会再被中断。
   
   /*while(1);是一条指令，它让CPU停在这个位置，一般用来检测中断,
   *只有cpu收到中断指令，才会跳出while(1)，进入中断服务子程序；而while(1){}是语句，中断也不会跳出循环
   *是死等，一般是为了等定时计数器到时间或放在程序最后防止跑飞
   */
   while(1);
}

/* 完成一般中断处理函数注册及异常名称注册 */
static void exception_init(void) {			    // 完成一般中断处理函数注册及异常名称注册
   for (int i = 0; i < IDT_DESC_CNT; i++) {

/* idt_table数组中的函数是在进入中断后根据中断向量号调用的,
 * 见kernel/kernel.S的call [idt_table + %1*4] */

      // 默认为general_intr_handler。这里要把void(*)(uin8_t)函数指针强转为void*
      idt_table[i] = (void*)general_intr_handler;	
							    // 以后会由register_handler来注册具体处理函数。
      intr_name[i] = "unknown";				    // 先统一赋值为unknown 
   }
   intr_name[0] = "#DE Divide Error";
   intr_name[1] = "#DB Debug Exception";
   intr_name[2] = "NMI Interrupt";
   intr_name[3] = "#BP Breakpoint Exception";
   intr_name[4] = "#OF Overflow Exception";
   intr_name[5] = "#BR BOUND Range Exceeded Exception";
   intr_name[6] = "#UD Invalid Opcode Exception";
   intr_name[7] = "#NM Device Not Available Exception";
   intr_name[8] = "#DF Double Fault Exception";
   intr_name[9] = "Coprocessor Segment Overrun";
   intr_name[10] = "#TS Invalid TSS Exception";
   intr_name[11] = "#NP Segment Not Present";
   intr_name[12] = "#SS Stack Fault Exception";
   intr_name[13] = "#GP General Protection Exception";
   intr_name[14] = "#PF Page-Fault Exception";
   // intr_name[15] 第15项是intel保留项，未使用
   intr_name[16] = "#MF x87 FPU Floating-Point Error";
   intr_name[17] = "#AC Alignment Check Exception";
   intr_name[18] = "#MC Machine-Check Exception";
   intr_name[19] = "#XF SIMD Floating-Point Exception";
   intr_name[32] = "clock interruption!";

}

/* 开中断并返回开中断前的状态*/
enum intr_status intr_enable() {
   enum intr_status old_status;
   if (INTR_ON == intr_get_status()) {
      old_status = INTR_ON;
      return old_status;
   } else {
      old_status = INTR_OFF;
      asm volatile("sti");	 // 开中断,sti指令将IF位置1
      return old_status;
   }
}

/* 关中断,并且返回关中断前的状态 */
enum intr_status intr_disable() {     
   enum intr_status old_status;
   if (INTR_ON == intr_get_status()) {
      old_status = INTR_ON;
      asm volatile("cli" : : : "memory"); // 关中断,cli指令将IF位置0
      return old_status;
   } else {
      old_status = INTR_OFF;
      return old_status;
   }
}

/* 将中断状态设置为status */
enum intr_status intr_set_status(enum intr_status status) {
   return status & INTR_ON ? intr_enable() : intr_disable();
}

/* 获取当前中断状态 */
enum intr_status intr_get_status() {
   uint32_t eflags = 0; 
   GET_EFLAGS(eflags);
   return (EFLAGS_IF & eflags) ? INTR_ON : INTR_OFF;
}

/* 在中断处理程序数组第vector_no个元素中注册安装中断处理程序function */
void register_handler(uint8_t vector_no, intr_handler function) {
/* idt_table数组中的函数是在进入中断后根据中断向量号调用的,
 * 见kernel/kernel.S的call [idt_table + %1*4] */
   idt_table[vector_no] = function; 
}

/*完成有关中断的所有初始化工作*/
void idt_init() {
   k_printf("idt_init start\n");
   idt_desc_init();	   // 初始化中断描述符表
   exception_init();	   // 异常名初始化并注册通常的中断处理函数
   pic_init();		   // 初始化8259A

   /* 加载idt,把48位数据加载到IDTR中。分别是IDT表的起始地址和界限 */
   uint64_t idt_operand = ((sizeof(idt) - 1) | ((uint64_t)(uint32_t)idt << 16));
   asm volatile("lidt %0" : : "m" (idt_operand));
   k_printf("idt_init done\n");
}
