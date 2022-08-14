#include "./log.h"
#include<string.h>
#include<time.h>
#include<stdarg.h>
#include<sys/time.h>
#include "block_queue.h"
using namespace std;

loger::loger(){
    m_count=0;
    m_is_async=false;
}

loger::~loger(){
    if(m_fp != NULL){
        fclose(m_fp);
    }
}

bool loger::init(const char *filename,int close_log,int log_buffer_size,int split_lines,int max_queue_size){       //这里不用设置默认值
    if(max_queue_size>=1){
        m_is_async=true;
        m_log_queue=new block_queue<string>(max_queue_size);
        pthread_t p;
        pthread_create(&p,NULL,flush_log_thread,NULL);
        // pthread_detach(p);
    }
    m_close_log=close_log;
    m_log_buffer_size=log_buffer_size;
    m_split_lines=split_lines;
    m_buf=new char[m_log_buffer_size];
    memset(m_buf,'\0',m_log_buffer_size);
    const char* p=strrchr(filename,'/');           //不能用"/",因为这里的的参数要为整数，所以要用''转为ascall码
    time_t t=time(NULL);
    struct tm *sys_tm=localtime(&t);
    struct tm my_tm=*sys_tm;

    char log_full_name[259]={0};
    if(p==NULL){
        snprintf(log_full_name,255,"%d_%02d_%2d_%s",my_tm.tm_year+1900,my_tm.tm_mon+1,my_tm.tm_mday,filename);        
    }else{
        strcpy(logname,p+1);
        strncpy(dirname,filename,p-filename+1);
        snprintf(log_full_name,255,"%s%d_%02d_%2d_%s",dirname,my_tm.tm_year+1900,my_tm.tm_mon+1,my_tm.tm_mday,logname);
    }
    m_today=my_tm.tm_mday;
    m_fp=fopen(log_full_name,"a");          //不能用'a'，因为'a'是char，这里的参数要为char*
    if(m_fp==NULL)                          //防止fclose报错
        return false;
    return true;
}

void loger::write_log(int level,const char* format,...){
    struct timeval now={0,0};
    gettimeofday(&now,NULL);
    time_t t=now.tv_sec;
    struct tm *sys_tm=localtime(&t);
    struct tm my_tm=*sys_tm;
    char s[16]={0};
    switch (level)
    {
    case 0:
        strcpy(s,"[debug]:");
        break;
    case 1:
        strcpy(s,"[info]:");
        break;
    case 2:
        strcpy(s,"[warn]:");
        break;
    case 3:
        strcpy(s,"[error]:");
        break;
    default:
        strcpy(s,"[info]:");
        break;
    }
    m_mutex.lock();
    m_count++;
    if(m_today !=my_tm.tm_mday || m_count%m_split_lines==0){                 //同一天中m_count只有加没有减所以用%
        char newlog[256]={0};
        char tail[16];
        fflush(m_fp);           //把旧文件的东西刷新
        fclose(m_fp);           //关闭旧文件
        snprintf(tail,15,"%d_%02d_%02d",my_tm.tm_year+1900,my_tm.tm_mon+1,my_tm.tm_mday);
        if(m_today !=my_tm.tm_mday){
            snprintf(newlog,255,"%s%s%s",dirname,tail,logname);
            m_today=my_tm.tm_mday;
            m_count=0;
        }else{
            snprintf(newlog,255,"%s%s_%s.%lld",dirname,tail,logname,m_count/m_split_lines);  //%lld用来输出长整型
        }
        m_fp=fopen(newlog,"a");
    }
    m_mutex.unlock();

    va_list valist;
    va_start(valist,format);
    string log_str;
    m_mutex.lock();
    int n=snprintf(m_buf,48,"%d-%02d%-02d %02d:%02d:%02d.%06ld %s",my_tm.tm_year+1900,my_tm.tm_mon+1,       //不能写%d_%d
                my_tm.tm_mday,my_tm.tm_hour,my_tm.tm_min,my_tm.tm_sec,now.tv_usec,s);
    int m=vsnprintf(m_buf+n,m_log_buffer_size-1,format,valist);
    m_buf[n+m]='\n';
    m_buf[n+m+1]='\0';
    log_str=m_buf;
    m_mutex.unlock();
    if(m_is_async && !m_log_queue->full()){
        m_log_queue->push(log_str);
    }
    else{
        m_mutex.lock();
        fputs(log_str.c_str(),m_fp);
        m_mutex.unlock();
    }
    va_end(valist);
}

void loger::flush(){
    m_mutex.lock();
    fflush(m_fp);
    m_mutex.unlock();
}