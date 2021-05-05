#include "string.h" //需要gcc时指定目录

//两个构造函数
string::string(const char*& str){
    this->str_len = 0;
    int i = 0;
    while(str[i] != '\0')
        buf[str_len++] = str[i++];
    buf[str_len] = '\0';//添加\0作为结尾，但是不增加长度
}

string::string(const string& str){
    this->str_len = str.size();
    for(size_t i = 0;i < this->str_len;i++)
        this->buf[i] = str.buf[i];
}

size_t string::size() const{ return this->str_len; } //常函数定义时也要有const

//比较两个string对象是否相等
bool operator==(const string& str1,const string& str2){
    if(str1.size() != str2.size()) return false;
    int n = str1.size();
    for(int i = 0;i < n;i++)
        if(str1.buf[i] != str2.buf[i])
            return false;
    return true;
}

//比较string对象是否和C风格字符串相等
bool operator==(const string& str1,const char* str2){
    int n = str1.size();
    int i;
    for(i = 0;i < n && str2[i] != '\0';i++){
        if(str1.buf[i] != str2[i]) return false;
    }
    if(i == n) return true;
    return false;
}

bool operator>(const string& str1,const string& str2){
    if(str1.size() > str2.size()) return true;
    if(str1.size() < str2.size()) return false;
    int n = str1.size();
    for(int i = 0;i < n;i++)
        if(str1.buf[i] != str2.buf[i])
            return str1.buf[i] > str2.buf[i] ? true : false;
    
    return false;
}
bool operator<(const string& str1,const string& str2){
    if(str1.size() < str2.size()) return true;
    if(str1.size() > str2.size()) return false;
    int n = str1.size();
    for(int i = 0;i < n;i++)
        if(str1.buf[i] != str2.buf[i])
            return str1.buf[i] < str2.buf[i] ? true : false;
    
    return false;
}

const char* string::c_str(){
    return this->buf;
}

char& string::operator[](size_t i){
    return this->buf[i];
}

/* 将dst_起始的size个字节置为val */
void memset(void* src_,uint8_t val,uint32_t size){
    char* src = (char*)src_;
    for(uint32_t i = 0;i < size;i++)
        *(src + i) = val;
}

/* 将src_起始的size个字节复制到dst_ */
void memcpy(void* dst_, void* src_, uint32_t size){
    char* dst = (char*)dst_,*src = (char*)src_;
    for(uint32_t i = 0;i < size;i++)
        *(dst + i) = *(src + i);
}

/* 连续比较以地址a_和地址b_开头的size个字节,若相等则返回0,若a_大于b_返回+1,否则返回-1 */
int memcmp(const void* a_, const void* b_, uint32_t size) {
   const char* a = (const char*)a_;
   const char* b = (const char*)b_;
   while (size-- > 0) {
      if(*a != *b) {
	 return *a > *b ? 1 : -1; 
      }
      a++;
      b++;
   }
   return 0;
}

/* 将字符串从src_复制到dst_ */
char* strcpy(char* dst_, const char* src_) {
   char* r = dst_;		       // 用来返回目的字符串起始地址
   while((*dst_++ = *src_++));
   return r;
}

/* 返回字符串长度 */
uint32_t strlen(const char* str) {
   const char* p = str;
   while(*p++);
   return (p - str - 1);
}

/* 比较两个字符串,若a_中的字符大于b_中的字符返回1,相等时返回0,否则返回-1. */
int8_t strcmp (const char* a, const char* b) {
   while (*a != 0 && *a == *b) {
      a++;
      b++;
   }
/* 如果*a小于*b就返回-1,否则就属于*a大于等于*b的情况。在后面的布尔表达式"*a > *b"中,
 * 若*a大于*b,表达式就等于1,否则就表达式不成立,也就是布尔值为0,恰恰表示*a等于*b */
   return *a < *b ? -1 : *a > *b;
}

/* 从左到右查找字符串str中首次出现字符ch的地址(不是下标,是地址) */
char* strchr(const char* str, const uint8_t ch) {
   while (*str != 0) {
      if (*str == ch) {
	 return (char*)str;	    // 需要强制转化成和返回值类型一样,否则编译器会报const属性丢失,下同.
      }
      str++;
   }
   return nullptr;
}

/* 从后往前查找字符串str中首次出现字符ch的地址(不是下标,是地址) */
char* strrchr(const char* str, const uint8_t ch) {
   const char* last_char = nullptr;
   /* 从头到尾遍历一次,若存在ch字符,last_char总是该字符最后一次出现在串中的地址(不是下标,是地址)*/
   while (*str != 0) {
      if (*str == ch) {
	    last_char = str;
      }
      str++;
   }
   return (char*)last_char;
}

/* 将字符串src_拼接到dst_后,将回拼接的串地址 */
char* strcat(char* dst_, const char* src_) {
   char* str = dst_;
   while (*str++);
   --str;      // 别看错了，--str是独立的一句，并不是while的循环体
   while((*str++ = *src_++));	 // 当*str被赋值为0时,此时表达式不成立,正好添加了字符串结尾的0.
   return dst_;
}

/* 在字符串str中查找指定字符ch出现的次数 */
uint32_t strchrs(const char* str, uint8_t ch) {
   uint32_t ch_cnt = 0;
   const char* p = str;
   while(*p != 0) {
      if (*p == ch) {
	    ch_cnt++;
      }
      p++;
   }
   return ch_cnt;
}
