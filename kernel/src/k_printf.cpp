#include "string.h" 
#include "io.h"
#include "console.h"
static const uint32_t begin = 0xc00b8000;//显存区域首地址
static const uint32_t vga_end = begin + 160 * 25 - 1;
static const uint32_t last_line = begin + 24 * 160;//最后一行首地址

static uint16_t offset = 0;  //静态变量，记录屏幕上此时光标的位置

//解释模式串format，结果输出到buf中
void vsprintf(char* buf,const char* format,uint32_t* args){
    //注意参数是从右到左依次压栈，而栈从上向下增长
	int index = 0;
	//分析第一个参数,找出其中的%d,%s...
	while (*format != '\0') {
        //遇到了模式
		if (*format == '%')
		{
			//如果是字符
			if (*(format + 1) == 'c') {
                buf[index++] = *((char*)args);
				(char*)args++;//这个参数是char所以他左边的参数就在他上面1B
			}
			else if (*(format + 1) == 'd' || *(format + 1) == 'i')
			{
				char buffer[64] = { 0 };
				//取得这个整数
				int i = *args;
				//使用itoa将整数转换成字符
				itoa(i, buffer, 10);
				for(int i = 0;buffer[i];i++)
					buf[index++] = buffer[i];
				args += 1;
			}
			else if (*(format + 1) == 'p') //打印地址
			{
				char buffer[64] = { 0 };
				//取得这个整数
				uint32_t i = *args;
				//使用itoa将整数转换成字符
				itoa(i, buffer, 16);
				for(int i = 0;buffer[i];i++)
					buf[index++] = buffer[i];
				args += 1;
			}
			else if (*(format + 1) == 'o')
			{
				char buffer[64] = { 0 };
				//取得这个整数
				int i = *args;
				//使用itoa将整数转换成字符
				itoa(i, buffer, 8);
				for(int i = 0;buffer[i];i++)
					buf[index++] = buffer[i];
				args += 1;
			}
			else if (*(format + 1) == 'x')
			{
				char buffer[64] = { 0 };
				//取得这个整数
				int i = *args;
				//使用itoa将整数转换成字符
				itoa(i, buffer, 16);
				for(int i = 0;buffer[i];i++)
					buf[index++] = buffer[i];
				args += 1;
			}
			else if (*(format + 1) == 's')
			{
				//取得这个字符,args指针指向的区间的内容是地址，此时args为const char **类型
				char* str = (char*)(*args);
				for(int i = 0;str[i];i++)
					buf[index++] = str[i];
				args += 1;//指针也占4B，所以正好移动一个
			}
			
			format += 2;//越过%d这个模式的两个字符
		}
		else 
		{
			//直接输出
			buf[index++] = *format;
            format++;
		}
	}
	buf[index] = '\0';
}

void sprintf(char* buf,const char* format,...){
	uint32_t* args = (uint32_t*)((char*)&format + 4);//第一个参数的地址就是format的地址
	//注意参数是从右到左依次压栈，而栈从上向下增长
	int index = 0;
	//分析第一个参数,找出其中的%d,%s...
	while (*format != '\0') {
        //遇到了模式
		if (*format == '%')
		{
			//如果是字符
			if (*(format + 1) == 'c') {
                buf[index++] = *((char*)args);
				(char*)args++;//这个参数是char所以他左边的参数就在他上面1B
			}
			else if (*(format + 1) == 'd' || *(format + 1) == 'i')
			{
				char buffer[64] = { 0 };
				//取得这个整数
				int i = *args;
				//使用itoa将整数转换成字符
				itoa(i, buffer, 10);
				for(int i = 0;buffer[i];i++)
					buf[index++] = buffer[i];
				args += 1;
			}
			else if (*(format + 1) == 'p') //打印地址
			{
				char buffer[64] = { 0 };
				//取得这个整数
				uint32_t i = *args;
				//使用itoa将整数转换成字符
				itoa(i, buffer, 16);
				for(int i = 0;buffer[i];i++)
					buf[index++] = buffer[i];
				args += 1;
			}
			else if (*(format + 1) == 'o')
			{
				char buffer[64] = { 0 };
				//取得这个整数
				int i = *args;
				//使用itoa将整数转换成字符
				itoa(i, buffer, 8);
				for(int i = 0;buffer[i];i++)
					buf[index++] = buffer[i];
				args += 1;
			}
			else if (*(format + 1) == 'x')
			{
				char buffer[64] = { 0 };
				//取得这个整数
				int i = *args;
				//使用itoa将整数转换成字符
				itoa(i, buffer, 16);
				for(int i = 0;buffer[i];i++)
					buf[index++] = buffer[i];
				args += 1;
			}
			else if (*(format + 1) == 's')
			{
				//取得这个字符,args指针指向的区间的内容是地址，此时args为const char **类型
				char* str = (char*)(*args);
				for(int i = 0;str[i];i++)
					buf[index++] = str[i];
				args += 1;//指针也占4B，所以正好移动一个
			}
			
			format += 2;//越过%d这个模式的两个字符
		}
		else 
		{
			//直接输出
			buf[index++] = *format;
            format++;
		}
	}
	buf[index] = '\0';
}

void clean_line(uint8_t begin_,uint8_t line){//将begin_行开始的line行的字符清0
	char* pos = (char*)(begin + begin_ * 160);
	uint32_t num = line * 80; //一共要清除的字符数
	for(uint32_t i = 0;i < num;i++){
		*(pos + 2 * i) = 0;
	}
}

//设置光标
void set_cursor(uint16_t pos){
    offset = pos;
	char* ptr = (char*)&pos;
    uint8_t low = *(ptr),high = *(ptr + 1);
    asm volatile(
        "pusha\n\t"  //保存上下文
        "movw $0x03d4,%dx\n\t" //组索引寄存器
        "movb $0x0e,%al\n\t"    //组中提供高8位光标的寄存器
        "outb %al,%dx\n\t"
        "movw $0x03d5,%dx\n\t"
    );
    asm volatile("movb %0,%%al;outb %%al,%%dx"::"r"(high));

    asm volatile(
        "movw $0x03d4,%dx\n\t"
        "movb $0x0f,%al\n\t"
        "outb %al,%dx\n\t"
        "movw $0x03d5,%dx\n\t" 
        );

    asm volatile("movb %0,%%al;outb %%al,%%dx;popa"::"r"(low));
}



//滚屏
void Scrolling(){
	void* dst = (void*)begin,*src = (void*)(begin + 160);
	//char* tmp = (char*)(begin + 3840);
	memcpy(dst,src,160 * 24);//1 ~24拷贝到0~23
	//千万不能直接memset最后一行全为0，否则会把字符的属性也给清除掉，那样光标就显示不出来了
	clean_line(24,1);//将最后一行的字符清0
	// for(uint8_t i = 0;i < 80;i++)
	// 	*(tmp + 2 * i) = 0;
}

void k_printf(char v,char*& des){//引用指针，需要修改指针值
	//如果是回车键
    if(v == '\n'){  
		uint32_t pos = ((((uint32_t)des - begin)) / 160 + 1) * 160 + begin;
		if(pos > vga_end){//如果到了显存下一页的第一行,就滚屏
			Scrolling();
			pos -= 160;
		}
        des = (char*)pos;
        return;
    }
	else if(v == '\b'){//如果是退格键
		if((uint32_t)des == begin) return;//删除到第一页的最上端，结束
		*(des - 2) = 0;
		des -= 2;
		return;
	}
	else if(v == '\t'){//如果是tab建
		*des = 0;
		*(des + 2) = 0;
		des += 4;
		return;
	}
    *des = v;
    des += 2;
	if((uint32_t)des > vga_end){//如果光标出了显存第一页，回滚
		Scrolling();
		des = (char*)last_line;
	}    
}

//实现printf()函数,可以格式化%c, %d||%i, %o, %x, %s
void k_printf(const char* format, ...) {
	char* des = (char*)((uint32_t)(offset) * 2 + begin);
    //注意参数是从右到左依次压栈，而栈从上向下增长
	//跳过第一个format参数，后面才是参数列表,所以偏移4个字节
	uint32_t* args = (uint32_t*)((char*)&format + 4);//第一个参数的地址就是format的地址
	char buf[1024] = {0};
	vsprintf(buf,format,args);//解析模式串
	
	for(int i = 0;buf[i];i++)
		k_printf(buf[i],des);

	uint16_t pos = (uint16_t)(((uint32_t)des - begin) / 2); //光标的偏移,是以字符为偏移，一个字符2B，所以/2
	set_cursor(pos);//将光标的偏移重新赋值给offset,打印要保证原子性
}
 
void printfk(const char* format, ...){
	char buf[1024] = {0};
	uint32_t* args = (uint32_t*)((char*)&format + 4);//第一个参数的地址就是format的地址
	vsprintf(buf,format,args);//解析模式串
	console_put_str(buf);
}