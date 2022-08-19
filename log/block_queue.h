#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include "../lock.h"
#include<stdlib.h>
#include<sys/time.h>
#include<stdio.h>
using namespace std;

template<class T>
class block_queue
{
private:
    //队列最大长度
    int m_max_size;
    // 队首下标
    int m_front;
    //队尾下标
    int m_back;
    //队列长度
    int m_size;
    //循环数组实现队列
    T* m_array;
    //互斥锁
    locker m_mutex;
    //条件变量
    cond m_cond;
public:
    //构造函数，初始化队列
    block_queue(int max_size){
        if(max_size<=0){
            exit(-1);
        }
        m_array=new T[max_size];      //分配内存
        m_max_size=max_size;
        m_front=-1;
        m_back=-1;
        m_size=0;
    }
    ~block_queue(){
        delete[] m_array;
        m_front=-1;
        m_back=-1;
        m_size=0;
    }
    //返回队列的头任务
    bool front(T &value){
        m_mutex.lock();
        if(m_size==0){
            m_mutex.unlock();
            return false;
        }
        value=m_array[m_front];
        m_mutex.unlock();
        return true;
    }
    //返回队列的尾任务
    bool back(T &value){
        m_mutex.lock();
        if(m_size==0){
            m_mutex.unlock();
            return false;
        }
        value=m_array[m_back];
        m_mutex.unlock();
        return true;
    }
    //判断队列是否为空
    bool empty(){
        m_mutex.lock();
        if(m_size==0){
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }
    //判断队列是否满了
    bool full(){
        m_mutex.lock();
        if(m_size>=m_max_size){
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }
    //往队列中添加元素，要把所有使用队列的线程唤醒
    //若没有线程等待条件变量，则唤醒毫无意义
    bool push(const T &item){
        m_mutex.lock();
        if(m_size>=m_max_size){
            m_cond.broadcast();
            m_mutex.unlock();
            return false;
        }
        m_back=(m_back+1)%m_max_size;
        m_array[m_back]=item;
        m_size++;
        m_cond.broadcast();
        m_mutex.unlock();
        return true;
    }
    //pop时，若当前队列没有元素则线程将会等待
    bool pop(T &item){
        m_mutex.lock();
        while(m_size==0){
            if(!m_cond.wait(m_mutex.get())){
                m_mutex.unlock();
                return false;
            }
        }
        m_front=(m_front+1)%m_max_size;
        item=m_array[m_front];
        m_size--;
        m_mutex.unlock();
        return true;
    }
    //和上面的函数相比添加了等待时间,如果超时则返回false
    bool pop(T &item,int timeout){
        struct timeval now={0,0};                        //struct timespec有两个成员，一个是秒，一个是纳秒
        struct timespec t={0,0};                         //struct timeval有两个成员，一个是秒，一个是微秒  
        gettimeofday(&now, NULL);
        m_mutex.lock();
        if(m_size<=0){
            t.tv_sec=now.tv_sec+timeout/1000;           //为什么要两个都用，只用timespec不就好了
            t.tv_nsec=(timeout%1000)*1000;    
            if(!m_cond.timewait(m_mutex.get(),t)){
                m_mutex.unlock();
                return false;
            }
        }
        if(m_size<=0){
            m_mutex.unlock();
            return false;
        }
        m_front=(m_front+1)%m_max_size;
        item=m_array[m_front];
        m_size--;
        m_mutex.unlock();
        return true;
    }
};



#endif