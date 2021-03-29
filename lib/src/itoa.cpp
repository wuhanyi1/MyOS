#include "stdint.h"
//return the length of result string. support only 10 radix for easy use and better performance
int itoa(int val, char* buf,int radix)
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

    *p-- = 0;

    //swap
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
int itoa(uint32_t val, char* buf,int radix)
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

    *p-- = 0;

    //swap
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
