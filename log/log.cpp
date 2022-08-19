#include "./log.h"
#include<string.h>
#include<time.h>
#include<stdarg.h>
#include<sys/time.h>
#include "block_queue.h"
using namespace std;

//构造函数
loger::loger(){
    //日志行数
    m_count=0;
    //同步标志位
    m_is_async=false;
}

//析构函数
loger::~loger(){
    if(m_fp != NULL){
        fclose(m_fp);
    }
}

//异步需要设置阻塞队列的长度，同步不需要设置
bool loger::init(const char *filename,int close_log,int log_buffer_size,int split_lines,int max_queue_size){       //这里不用设置默认值
    //如果设置了max_queue_size,则设置为异步
    if(max_queue_size>=1){
        //设置为异步写入模式
        m_is_async=true;
        //创建阻塞队列
        m_log_queue=new block_queue<string>(max_queue_size);
        //创建写线程来异步写入日志
        pthread_t p;
        //flush_log_thread为回调函数,这里表示创建线程异步写日志
        pthread_create(&p,NULL,flush_log_thread,NULL);
        // pthread_detach(p);
    }
    //是否关闭日志
    m_close_log=close_log;
    //日志缓冲区大小
    m_log_buffer_size=log_buffer_size;
    //日志最大行数
    m_split_lines=split_lines;
    //日志缓冲区
    m_buf=new char[m_log_buffer_size];
     //初始化日志缓冲区
    memset(m_buf,'\0',m_log_buffer_size);

    //从后往前找到第一个“/”的位置
    const char* p=strrchr(filename,'/');           //不能用"/",因为这里的的参数要为整数，所以要用''转为ascall码
    //获取此时的时间戳
    time_t t=time(NULL);
    //获取系统时间
    struct tm *sys_tm=localtime(&t);
    struct tm my_tm=*sys_tm;

    char log_full_name[259]={0};
    //若输入的文件名没有“/”
    if(p==NULL){
        // 自定义日志名为：年_月_日_文件名
        snprintf(log_full_name,255,"%d_%02d_%2d_%s",my_tm.tm_year+1900,my_tm.tm_mon+1,my_tm.tm_mday,filename);        
    }else{
        //把文件名字放入log_name中
        strcpy(logname,p+1);
        //把路径放入dir_name中,p-file_name是路径名的长度,+1是加上"/"
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
    //日志等级
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
    //行数+1
    m_count++;
    //如果日期不对或日志行数已经等于最大行数的倍数
    if(m_today !=my_tm.tm_mday || m_count%m_split_lines==0){                 //同一天中m_count只有加没有减所以用%
        // 新日志名
        char newlog[256]={0};
        //日志名中的时间部分
        char tail[16];
        fflush(m_fp);           //把旧文件的东西刷新
        fclose(m_fp);           //关闭旧文件
        //年_月_日
        snprintf(tail,15,"%d_%02d_%02d",my_tm.tm_year+1900,my_tm.tm_mon+1,my_tm.tm_mday);
        // 如果是时间不是今天
        if(m_today !=my_tm.tm_mday){
            // 路径年_月_日_文件名
            snprintf(newlog,255,"%s%s%s",dirname,tail,logname);
            //时间改成今天
            m_today=my_tm.tm_mday;
            //重置日志行数
            m_count=0;
        }else{
            // count/max_lines为"页号"
            // 路径年_月_日_文件名.页号
            snprintf(newlog,255,"%s%s_%s.%lld",dirname,tail,logname,m_count/m_split_lines);  //%lld用来输出长整型
        }
        m_fp=fopen(newlog,"a");
    }
    m_mutex.unlock();
    //将传入的format参数赋值给可变参数列表类型valist，以便格式化输出
    va_list valist;
    va_start(valist,format);
    string log_str;
    m_mutex.lock();
    // 写一条日志
    // 年-月-日 时:分:秒.微秒 [日志等级]:
    int n=snprintf(m_buf,48,"%d-%02d%-02d %02d:%02d:%02d.%06ld %s",my_tm.tm_year+1900,my_tm.tm_mon+1,       //不能写%d_%d
                my_tm.tm_mday,my_tm.tm_hour,my_tm.tm_min,my_tm.tm_sec,now.tv_usec,s);
    // 例：LOG_INFO("%s", "adjust timer once");
    int m=vsnprintf(m_buf+n,m_log_buffer_size-1,format,valist);
    m_buf[n+m]='\n';
    m_buf[n+m+1]='\0';
    log_str=m_buf;
    m_mutex.unlock();
    //异步写入日志
    if(m_is_async && !m_log_queue->full()){
        //将内容放入阻塞队列
        m_log_queue->push(log_str);
    }
    //同步写入日志
    else{
        m_mutex.lock();
        fputs(log_str.c_str(),m_fp);
        m_mutex.unlock();
    }
    va_end(valist);
}

//刷新缓冲区
//例子：使用多个输出函数连续进行多次输出到控制台时，上一个数据还在输出缓冲区中时，
//输出函数就把下一个数据加入输出缓冲区，结果冲掉了原来的数据，出现输出错误。
//不懂为啥这里也是临界资源
void loger::flush(){
    m_mutex.lock();
    fflush(m_fp);
    m_mutex.unlock();
}