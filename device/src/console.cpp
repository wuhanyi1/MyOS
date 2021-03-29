#include "console.h"
#include "k_printf.h"
#include "stdint.h"
#include "sync.h"
#include "thread.h"
static struct lock console_lock;    // 控制台锁

/* 初始化终端 */
void console_init() {
    console_lock.init();
}

/* 获取终端 */
void console_acquire() {
   console_lock.lock_acquire();
}

/* 释放终端 */
void console_release() {
   console_lock.lock_release();
}

/* 终端中输出字符串 */
void console_put_str(const char* str) {
   console_acquire(); 
   k_printf("%s ",str); 
   console_release();
}

/* 终端中输出字符 */
void console_put_char(uint8_t char_asci) {
   console_acquire(); 
   k_printf("%c ",char_asci); 
   console_release();
}

/* 终端中输出16进制整数 */
void console_put_int(uint32_t num) {
   console_acquire(); 
   k_printf("%d ",num); 
   console_release();
}
