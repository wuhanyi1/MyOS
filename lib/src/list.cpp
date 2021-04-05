#include "list.h"
#include "interrupt.h"
#include "k_printf.h"

void list::init(){
   head.prev = (list_elem*)NULL;
   head.next = &tail;
   tail.next = (list_elem*)NULL;
   tail.prev = &head;
   size = 0;
}

/* 把链表元素elem插入在元素before之前 */
void list::insert_before(list_elem* before, list_elem* elem) { 
   //关中断，保证是原子操作
   enum intr_status old_status = intr_disable();

/* 将before前驱元素的后继元素更新为elem, 暂时使before脱离链表*/ 
   before->prev->next = elem; 
   elem->prev = before->prev;
   elem->next = before;
   before->prev = elem;

   size++;//长度+1
   intr_set_status(old_status);
}

/* 添加元素到列表队首,类似栈push操作 */
void list::push_front(list_elem* elem) {
   insert_before(head.next, elem); // 在队头插入elem
}

/* 追加元素到链表队尾,类似队列的先进先出操作 */
void list::push_back(list_elem* elem) {
   insert_before(&tail, elem);     // 在队尾的前面插入
}

/* 使元素pelem脱离链表 */
void list::remove(list_elem* pelem) {
   enum intr_status old_status = intr_disable();
   pelem->prev->next = pelem->next;
   pelem->next->prev = pelem->prev;
   size--;
   intr_set_status(old_status);
}

/* 将链表队尾元素弹出并返回,类似栈的pop操作 */
list_elem* list::pop_back() {
   list_elem* elem = tail.prev;
   remove(elem);
   return elem;
} 

/* 从链表中查找元素obj_elem,成功时返回true,失败时返回false */
bool list::find(list_elem* obj_elem) {
   list_elem* elem = head.next;
   while (elem != &tail) {
      if (elem == obj_elem) return true;
      elem = elem->next;
   }
   return false;
}

/* 判断链表中是否有符合func这个条件的元素，有就返回。func是判断函数，判断元素是否符合条件arg */
list_elem* list::list_traversal(function func, int arg) {
   list_elem* elem = tail.prev;
/* 如果队列为空,就必然没有符合条件的结点,故直接返回NULL */
   if (is_empty()) { 
      return (list_elem*)NULL;
   }

   while (elem != &head) {
      if (func(elem, arg)) {		  // func返回ture则认为该元素在回调函数中符合条件,命中,故停止继续遍历
	      return elem;
      }					 
      elem = elem->prev;	       
   }
   return nullptr;
}

/* 判断链表是否为空,空时返回true,否则返回false */
bool list::is_empty() {		// 判断队列是否为空
   return size == 0;
}
