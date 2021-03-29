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

/* 将字符串从src_复制到dst_ */
char* strcpy(char* dst_, const char* src_) {
   //ASSERT(dst_ != NULL && src_ != NULL);
   char* r = dst_;		       // 用来返回目的字符串起始地址
   while((*dst_++ = *src_++));
   return r;
}
