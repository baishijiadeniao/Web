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
    //懒汉单例模式
    //公有静态方法指向唯一实例
    //C++11以后,使用局部变量懒汉不用加锁
    static loger* get_instance(){
        static loger instance;
        return &instance;
    }
    void write_log(int level,const char* format,...);
    //异步写入方式公有方法，使用阻塞队列
    static void* flush_log_thread(void *args){
        loger::get_instance()->async_write_log();           //这里要加loger::
    }
    //初始化日志
    bool init(const char *filename,int close_log,int log_buffer_size=8192,int split_lines=5000000,int max_queue_size=0);
    //刷新缓冲区
    void flush();

private:
    loger();
    virtual ~loger();           //这里为啥要变虚函数，不懂; 不加virtual的话派生类无法调用析构函数释放资源
    void *async_write_log(){
        string singal_log;
        //从阻塞队列中取出一个string，并写入日志
        while(m_log_queue->pop(singal_log)){
            m_mutex.lock();
            fputs(singal_log.c_str(),m_fp);
            m_mutex.unlock();
        }
    }
    char dirname[128];  //路径名
    char logname[128];  //日志名
    int m_close_log;    //是否关闭日志
    FILE *m_fp;         //文件指针
    int m_split_lines;  //日志最大行数
    int m_log_buffer_size;//日志缓冲区大小
    long long m_count;  //日志行数记录
    char *m_buf;        //日志缓冲区
    block_queue<string> *m_log_queue;   //阻塞队列
    bool m_is_async;        //是否同步标志位
    locker m_mutex;         //互斥锁
    int m_today;            //今天的日期
};

//VA_ARGS是一个可变参数的宏
//VA_ARGS前面加##的作用是：当可变参数的个数为0时，把前面的","删去，否则会编译出错

#define LOG_DEBUG(format,...) if(m_close_log==0) {loger::get_instance()->write_log(0,format,##__VA_ARGS__); loger::get_instance()->flush();}
#define LOG_INFO(format,...) if(m_close_log==0) {loger::get_instance()->write_log(1,format,##__VA_ARGS__); loger::get_instance()->flush();}
#define LOG_WARN(format,...) if(m_close_log==0) {loger::get_instance()->write_log(2,format,##__VA_ARGS__); loger::get_instance()->flush();}
#define LOG_ERROR(format,...) if(m_close_log==0) {loger::get_instance()->write_log(3,format,##__VA_ARGS__); loger::get_instance()->flush();}

#endif 