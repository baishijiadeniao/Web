#ifndef LOCK_H
#define LOCK_H

#include<pthread.h>
#include<semaphore.h>
#include<exception>
using namespace::std;

//信号量
class sem
{
private:
    /* data */
    sem_t mutex_sem;
public:
    sem(){
        if(sem_init(&mutex_sem,0,0) !=0){
            throw exception();
        }
    }
    sem(int n){
        if(sem_init(&mutex_sem,0,n) !=0){
            throw exception();
        }
    }
    ~sem(){
        sem_destroy(&mutex_sem);
    }
    bool wait(){
        return sem_wait(&mutex_sem)==0;
    }
    bool post(){
        return sem_post(&mutex_sem)==0;
    }
};

//互斥锁
class locker
{
private:
    pthread_mutex_t mutex;
public:
    locker(){
        pthread_mutex_init(&mutex,NULL);
    }
    ~locker(){
        if(pthread_mutex_destroy(&mutex) !=0){
            throw exception();
        }
    }
    bool lock(){
        return pthread_mutex_lock(&mutex)==0;
    }
    bool unlock(){
        return pthread_mutex_unlock(&mutex)==0;
    }
    pthread_mutex_t *get(){
        return &mutex;
    }
};

//条件变量
class cond
{
private:
    pthread_cond_t con;
public:
    cond(/* args */){
        if(pthread_cond_init(&con,NULL) !=0){
            throw exception();
        }
    }
    ~cond(){
        pthread_cond_destroy(&con);
    }
    bool wait(pthread_mutex_t *m){
        return pthread_cond_wait(&con,m)==0;
    }
    bool timewait(pthread_mutex_t *m,struct timespec t){
        return pthread_cond_timedwait(&con,m,&t)==0;
    }
    bool signal(){
        return pthread_cond_signal(&con)==0;
    }
    bool broadcast(){
        return pthread_cond_broadcast(&con)==0;
    }
};



#endif 