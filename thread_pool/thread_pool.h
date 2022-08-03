#ifndef THREADPOOL_H
#define THREADPOOL_H

#include "../lock.h"
#include<list>
#include "../CGImysql/sql_connection_pool.h"
using namespace std;

template<typename T>
class threadpool
{
private:
    static void* worker(void* arg);
    void run();
    int m_max_request;
    int m_thread_num;
    connection_pool* pool;
    int m_actor_model;
    locker m_queuelocker;
    sem m_queuesem;
    list<T*> m_workqueue;
    pthread_t *m_threads;
public:
    threadpool(int actor_model,connection_pool *connpool,int thread_num=0,int max_request=10000);
    ~threadpool();
    bool append(T* request,int state);
    bool append_p(T* request);
};

template<typename T>
threadpool<T>::threadpool(int actor_model,connection_pool *connpool,int thread_num,int max_request):m_actor_model(actor_model),pool(connpool),m_thread_num(thread_num),m_max_request(max_request){
    if(max_request<=0 && thread_num<=0){
        throw::exception();
    }
    m_threads=new pthread_t[m_thread_num];
    if(!m_thread){
        throw::exception();
    }
    for(int i=0;i<thread_num){
        if(pthread_create(m_threads+i,NULL,worker,this) !=0){
            delete[] m_threads;
            throw::exception();
        }
        if(pthread_detach(m_threads[i])){
            delete[] m_threads;
            throw::exception();
        }
    }
}

template<typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
}

template<typename T>
bool threadpool<T>::append(T* request,int state){
    m_queuelocker.lock();
    if(m_workqueue.size() ==m_max_request){
        m_queuelocker.unlock();
        return false;
    }
    m_actor_model=state;
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuesem.post();
    return true;
}

template<typename T>
bool threadpool<T>::append_p(T* request){
    m_queuelocker.lock();
    if(m_workqueue.size() ==m_max_request){
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuesem.post();
    return true;
}

template<typename T>
void * threadpool<T>::worker(void* arg){
    threadpool *pool=(threadpool*)arg;
    pool->run();
    return pool;
}



template<typename T>
void threadpool<T>::run(){
    while(true){
        m_queuesem.wait();
        m_queuelocker.lock();
        if(m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }
        T* request=m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if(!request){
            continue;
        }
        if(1==m_actor_model){
            if(0==request->m_state){
                if(request->read_once()){
                    request->improc=1;
                    connectionRAII mysqlcon(&mysql,connPool);     //不懂
                    request->process();
                }else{
                    request->improc=1;
                    request->timer_flag=1;
                }
            }else{
                if(request->write()){
                    request->improc=1;
                }else{
                    request->improc=1;
                    request->timer_flag=1;
                }
            }
        }else{
            connectionRAII mysqlcon(&mysql,connPool);
            request->process();
        }
    }
    

}

#endif
