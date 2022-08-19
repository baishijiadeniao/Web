#ifndef THREADPOOL_H
#define THREADPOOL_H

#include "../lock.h"
#include<list>
#include "../CGImysql/sql_connection_pool.h"
#include<stdio.h>
using namespace std;

template<typename T>
class threadpool
{
private:
//工作线程运行的函数，它不断从任务队列中取出任务并执行它
    static void* worker(void* arg);
    void run();
    //任务队列中允许的最大任务数
    int m_max_request;
    //线程数
    int m_thread_num;
    //指向数据库连接池的指针
    connection_pool* pool;
    //并发模型的选择，reactor还是Proactor
    int m_actor_model;
    //互斥锁
    locker m_queuelocker;
    //是否有任务需要处理的信号量
    sem m_queuesem;
    //任务队列
    list<T*> m_workqueue;
    //线程数组
    pthread_t *m_threads;
public:
    threadpool(int actor_model,connection_pool *connpool,int thread_num=0,int max_request=10000);
    ~threadpool();
    // 向任务队列中插入任务
    bool append(T* request,int state);
    bool append_p(T* request);
};

//构造函数
template<typename T>
threadpool<T>::threadpool(int actor_model,connection_pool *connpool,int thread_num,int max_request):m_actor_model(actor_model),pool(connpool),m_thread_num(thread_num),m_max_request(max_request){
    if(max_request<=0 && thread_num<=0){
        throw::exception();
    }
    m_threads=new pthread_t[m_thread_num];
    if(!m_threads){
        throw::exception();
    }
    //循环创建线程，并将线程和函数指针绑定
    for(int i=0;i<thread_num;i++){
        if(pthread_create(m_threads+i,NULL,worker,this) !=0){
            delete[] m_threads;
            throw::exception();
        }
        //线程分离，之后不用单独对工作线程进行回收
        if(pthread_detach(m_threads[i])){
            delete[] m_threads;
            throw::exception();
        }
    }
}

//析构函数销毁线程
template<typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
}

//向任务队列中添加任务
template<typename T>
bool threadpool<T>::append(T* request,int state){
    m_queuelocker.lock();
    //如果任务数量大于等于任务队列中允许的最大任务数
    if(m_workqueue.size() >=m_max_request){
        m_queuelocker.unlock();
        return false;
    }
    //state=0是读，=1时是写
    m_actor_model=state;
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    //信号量+1，提醒有任务要处理
    m_queuesem.post();
    return true;
}

template<typename T>
bool threadpool<T>::append_p(T* request){
    m_queuelocker.lock();
    // LOG_INFO("debug append_p");
    if(m_workqueue.size() ==m_max_request){
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuesem.post();
    return true;
}

//工作线程运行的函数
//线程创建时参数传递的是this，所以这里是arg应为指向空类型的指针
template<typename T>
void * threadpool<T>::worker(void* arg){
    //将参数类型转化为线程池类类型
    threadpool *pool=(threadpool*)arg;
    pool->run();
    return pool;
}



template<typename T>
void threadpool<T>::run(){
    while(true){
        //等待信号量
        m_queuesem.wait();
        m_queuelocker.lock();
        if(m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }
        //取出任务
        T* request=m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if(!request){
            continue;
        }
        //Reactor模型
        //主线程只负责监听文件描述符上是否有事件发生，有的话立即通知工作线程读写数据，处理业务逻辑
        if(1==m_actor_model){
            //读
            if(0==request->m_state){
                if(request->read_once()){
                    //improv表示已读
                    request->improv=1;
                    connectionRAII mysqlcon(&request->mysql,pool);     //获取一个mysql连接，保存在request->mysql
                    //整个业务逻辑
                    request->process();
                }else{
                    request->improv=1;
                    request->timer_flag=1;
                }
            }
            //写
            else{
                if(request->write()){
                    request->improv=1;
                }else{
                    request->improv=1;
                    request->timer_flag=1;
                }
            }
        }
        //proactor模型
        //主线程和内核负责读写数据，接受新的IO等操作，工作线程负责处理业务逻辑
        else{
            // LOG_INFO("debug %s","run");
            connectionRAII mysqlcon(&request->mysql,pool);
            //处理业务逻辑
            request->process();
        }
    }
    

}

#endif
