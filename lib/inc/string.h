#ifndef __STRING_H__
#define __STRING_H__
#include "stdint.h"

int itoa(int val, char* buf,int radix); //iota的函数声明也放这里
int itoa(uint32_t val, char* buf,int radix); //iota的函数声明也放这里
void memset(void* src_,uint8_t val,uint32_t size);
void memcpy(void* dst_, void* src_, uint32_t size);
char* strcpy(char* dst_, const char* src_);

const int MAXBUF = 4096;
class string;//前向声明，不写会出错
bool operator==(const string& str1,const string& str2);
bool operator==(const string& str1,const char* str2);
bool operator>(const string& str1,const string& str2);
bool operator<(const string& str1,const string& str2);
class string{
public:
    friend bool operator==(const string& str1,const string& str2);
    friend bool operator==(const string& str1,const char* str2);
    friend bool operator>(const string& str1,const string& str2);
    friend bool operator<(const string& str1,const string& str2);

    string(const char*& str);
    string(const string& str);
    size_t size() const;
    const char* c_str();
    char& operator[](size_t i);

private:
    size_t str_len;
    char buf[MAXBUF];
};



#endif