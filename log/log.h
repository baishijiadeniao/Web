#ifndef LOGER_H
#define LOGER_H

#include "../lock.h"
#include "./block_queue.h"
#include<string>
#include<stdio.h>
using namespace std;

class loger
{
public:
    static loger* get_instance(){
        static loger instance;
        return &instance;
    }
    void write_log(int level,const char* format,...);
    static void* flush_log_thread(void *args){
        loger::get_instance()->async_write_log();           //这里要加loger::
    }
    bool init(const char *filename,int close_log,int log_buffer_size=8192,int split_lines=5000000,int max_queue_size=0);
    void flush();
    int get_m_close_log(){
        return m_close_log;
    }
private:
    loger();
    virtual ~loger();           //这里为啥要变虚函数，不懂; 不加virtual的话派生类无法调用析构函数释放资源
    void *async_write_log(){
        string singal_log;
        while(m_log_queue->pop(singal_log)){
            m_mutex.lock();
            fputs(singal_log.c_str(),m_fp);
            m_mutex.unlock();
        }
    }
    char dirname[128];
    char logname[128];
    int m_close_log;
    FILE *m_fp;
    int m_split_lines;
    int m_log_buffer_size;
    long long m_count;
    char *m_buf; 
    block_queue<string> *m_log_queue;
    bool m_is_async;
    locker m_mutex;
    int m_today;
};

#define LOG_DEBUG(format,...) if(m_close_log==0) {loger::get_instance()->write_log(0,format,##__VA_ARGS__); loger::get_instance()->flush();}
#define LOG_INFO(format,...) {loger::get_instance()->write_log(1,format,##__VA_ARGS__); loger::get_instance()->flush();}
#define LOG_WARN(format,...) if(m_close_log==0) {loger::get_instance()->write_log(2,format,##__VA_ARGS__); loger::get_instance()->flush();}
#define LOG_ERROR(format,...) if(m_close_log==0) {loger::get_instance()->write_log(3,format,##__VA_ARGS__); loger::get_instance()->flush();}

#endif 