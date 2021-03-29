#ifndef __LIB_KERNEL_LIST_H
#define __LIB_KERNEL_LIST_H
#include "global.h"

#define offset(struct_type,member) (int)(&((struct_type*)0)->member)
#define elem2entry(struct_type, struct_member_name, elem_ptr) \
	 (struct_type*)((int)elem_ptr - offset(struct_type, struct_member_name))

//typedef bool (function)(struct list_elem*, int arg);

/**********   定义链表结点成员结构   ***********
*结点中不需要数据成元,只要求前驱和后继结点指针*/
struct list_elem {
    list_elem* prev; // 前躯结点
    list_elem* next; // 后继结点
};

/* 自定义函数类型function,用于在list_traversal中做回调函数,接收一个节点和一个int参数 */
using function = bool(list_elem*,int);
/* 链表结构,用来实现队列 */
struct list {
    //分别为链表的表头和表尾节点，相当于哨兵节点
    list_elem head;
    list_elem tail;
    uint32_t size;//链表长度
    void init();
    //下面的函数最好都用指针，否则调用前还要创建变量，会调用拷贝构造函数，开销大
    void insert_before(list_elem* before, list_elem* elem);//在before之前插入一个elem
    void push_front(list_elem* elem);//头插
    void push_back(list_elem* elem);//尾插
    void remove(list_elem* pelem);//链表中删除一个指定的元素
    list_elem* pop_back();//弹出队尾元素
    bool is_empty();//判断是否为空
    list_elem* list_traversal(function func, int arg);//遍历链表查看是否有满足func(arg)的节点
    bool find(list_elem* obj_elem);
};


#endif
