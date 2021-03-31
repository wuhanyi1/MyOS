//将来给用户使用的标准输入输出函数，不参与编译到内核源码中,测试完毕，正确

//#include "stdio.h"
#include "syscall.h"
#include "k_printf.h"

#include "stdint.h"
//整数转换为字符串存到buf中
int ITOA(int val, char* buf,int radix)
{
    //const unsigned int radix = 10;

    char* p;
    unsigned int a;        //every digit
    int len;
    char* b;            //start of the digit char
    char temp;
    unsigned int u;

    p = buf;
    if(radix == 16) *p = '0',*(p + 1) = 'x',p += 2;
    if(radix == 8) *p = '0',p ++;

    if (val < 0)
    {
        *p++ = '-';
        val = 0 - val;
    }
    u = (unsigned int)val;

    b = p;

    do
    {
        a = u % radix;
        u /= radix;

        if(a > 9){
            switch(a){
                case 10:*p++ = 'A';break;
                case 11:*p++ = 'B';break;
                case 12:*p++ = 'C';break;
                case 13:*p++ = 'D';break;
                case 14:*p++ = 'E';break;
                case 15:*p++ = 'F';break;
            }
        }
        else *p++ = a + '0';

    } while (u > 0);

    len = (int)(p - buf);

    //*p-- = 0;
    p--;
    do
    {
        temp = *p;
        *p = *b;
        *b = temp;
        --p;
        ++b;

    } while (b < p);

    return len;
}

//无符号数的重载版本
int ITOA(uint32_t val, char* buf,int radix)
{
    //const unsigned int radix = 10;

    char* p;
    unsigned int a;        //every digit
    int len;
    char* b;            //start of the digit char
    char temp;

    p = buf;
    if(radix == 16) *p = '0',*(p + 1) = 'x',p += 2;
    if(radix == 8) *p = '0',p ++;

    
    uint32_t u = (unsigned int)val;

    b = p;

    do
    {
        a = u % radix;
        u /= radix;

        if(a > 9){
            switch(a){
                case 10:*p++ = 'A';break;
                case 11:*p++ = 'B';break;
                case 12:*p++ = 'C';break;
                case 13:*p++ = 'D';break;
                case 14:*p++ = 'E';break;
                case 15:*p++ = 'F';break;
            }
        }
        else *p++ = a + '0';

    } while (u > 0);

    len = (int)(p - buf);

    //*p-- = 0;//将字符串结尾加上'\0'
    p--;
    do
    {
        temp = *p;
        *p = *b;
        *b = temp;
        --p;
        ++b;

    } while (b < p);

    return len;
}

//实现printf()函数,可以格式化%c, %d||%i, %o, %x, %s
void printf(const char* format, ...) {
	char buf[1024] = {0};
    int index = 0;
    //注意参数是从右到左依次压栈，而栈从上向下增长
	//跳过第一个format参数，后面才是参数列表,所以偏移4个字节
	uint32_t* args = (uint32_t*)((char*)&format + 4);//第一个参数的地址就是format的地址
 
	//分析第一个参数,找出其中的%d,%s...
	while (*format != '\0') {
        //遇到了模式
		if (*format == '%')
		{
			//如果是字符
			if (*(format + 1) == 'c') {
                buf[index++] = *args;
				(char*)args++;//这个参数是char所以他左边的参数就在他上面1B
			}
			else if (*(format + 1) == 'd' || *(format + 1) == 'i')
			{
				char *buffer = &buf[index];
                //char buffer[64] = { 0 };
				//取得这个整数
				int i = *args;
				//使用ITOA将整数转换成字符
				int len = ITOA(i, buffer, 10);
                index += len;
                //k_printf("index is %d\n",index);
				args += 1;
			}
			else if (*(format + 1) == 'p') //打印地址
			{
				char *buffer = &buf[index];
				//取得这个整数
				uint32_t i = *args;
				//使用ITOA将整数转换成字符
				int len = ITOA(i, buffer, 16);
                index += len;
				args += 1;
			}
			else if (*(format + 1) == 'o')
			{
				char *buffer = &buf[index];
				//取得这个整数
				int i = *args;
				//使用ITOA将整数转换成字符
				int len = ITOA(i, buffer, 8);
                index += len;
				args += 1;
			}
			else if (*(format + 1) == 'x')
			{
				char *buffer = &buf[index];
				//取得这个整数
				int i = *args;
				//使用ITOA将整数转换成字符
				int len = ITOA(i, buffer, 16);
				index += len;
				args += 1;
			}
			else if (*(format + 1) == 's')
			{
				//取得这个字符,args指针指向的区间的内容是地址，此时args为const char **类型
				char* str = (char*)(*args);
                int i = 0;
				while(str[i] != '\0'){
                    buf[index++] = str[i++];//将字符串内容写到buf中
                }
				args += 1;//指针也占4B，所以正好移动一个
			}
			
			format += 2;//越过%d这个模式的两个字符
		}
		else 
		{
			//直接压入到缓冲区中
			buf[index++] = *format;
            format++;
		}
	}
	buf[index] = '\0';//加个结尾
    write(buf);//系统函数写入
}