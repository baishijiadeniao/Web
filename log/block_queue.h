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
    int m_max_size;
    int m_front;
    int m_back;
    int m_size;
    T* m_array;
    locker m_mutex;
    cond m_cond;
public:
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
    bool empty(){
        m_mutex.lock();
        if(m_size==0){
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }
    bool full(){
        m_mutex.lock();
        if(m_size>=m_max_size){
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }
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
    bool pop(T &item,int timeout){
        m_mutex.lock();
        struct timeval now={0,0};
        struct timespec t={0,0};
        if(m_size<=0){
            t.tv_sec=now.tv_sec+timeout/1000;
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