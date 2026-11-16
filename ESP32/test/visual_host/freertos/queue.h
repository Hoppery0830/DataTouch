#pragma once
#include <deque>
#include <vector>
#include <cstring>
struct Queue {size_t count,size;std::deque<std::vector<unsigned char>> data;};
using QueueHandle_t=Queue*;
inline QueueHandle_t xQueueCreate(size_t n,size_t s){return new Queue{n,s,{}};}
inline int xQueueSend(QueueHandle_t q,const void*p,int){
 if(q->data.size()==q->count)return 0;
 auto b=static_cast<const unsigned char*>(p);q->data.emplace_back(b,b+q->size);return 1;
}
inline int xQueueReceive(QueueHandle_t q,void*p,int){
 if(q->data.empty())return 0;
 memcpy(p,q->data.front().data(),q->size);q->data.pop_front();return 1;
}
