#ifndef LST_TIME
#define LST_TIME

#include<fcntl.h>
#include<stdio.h>
#include<sys/socket.h>
#include <arpa/inet.h>
#include<sys/time.h>
#include<time.h>
#include<sys/epoll.h>
#include<errno.h>
#include <signal.h>
#include<stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

//创建一个升序链表存储定时器，为每一个连接创建一个定时器，执行定时任务时，将定时器从链表中删除
//添加定时器的时间复杂度是O(n)(因为要调整定时器位置),删除定时器的时间复杂度是O(1)

//连接资源结构体需要用到定时器类，需要提前声明
class util_timer;

//连接资源结构体
struct client_data
{
    //客户端的socket地址
    struct sockaddr_in address;
    int sockfd;
    //定时器
    util_timer* timer;
};

//定时器类
class util_timer
{
public:
    util_timer():prev(NULL),next(NULL){};
    // ~util_timer(){};
    time_t expire;
    void (*cb_func)(client_data*);   //回调函数
    client_data* usr_data;
    //指向前面一个计时器的指针，即前驱计时器
    util_timer* prev;
    //后继计时器
    util_timer* next;
};

//定时器容器类
class sort_timer_lst
{
public:
    sort_timer_lst(/* args */);
    ~sort_timer_lst();
    //将目标添加到链表
    void add_timer(util_timer* timer);
    //任务发生变化时需要调整定时器位置
    void adjust_timer(util_timer* timer);
    //删除定时器
    void del_timer(util_timer* timer);

    //定时任务处理函数，处理链表容器中到期的定时器
    void tick();
private:
    //调整节点位置
    //设为私有函数的原因：因为该函数的参数是私有的，所以要设为私有方法
    void add_timer(util_timer* timer,util_timer* lst_timer);
    //头节点
    util_timer* head;
    //尾节点
    util_timer* tail;
};

class Utils
{
public:
    sort_timer_lst m_timer_lst;
    static int *u_piped;
    static int u_epollfd;
    // 超时时间
    int m_TIMESLOT;
public:
    Utils(){};
    ~Utils(){};
    void init(int time_slot);
    //将文件描述符设置为非阻塞
    int setnonblocking(int fd);
    //将内核事件表注册读事件
    void addfd(int epollfd,int fd,bool one_shot,int TRIGmode);
    //信号处理函数
    static void sig_handler(int sig);
    //捕捉信号
    void addsig(int sig,void(handler)(int),bool restart=true);           //void(handler)(int)
    //定时处理任务，重新计时
    void timer_handler();
    //向客户端发送错误信息
    void show_error(int connfd,const char * str);
};


void cb_func(client_data* user_data);




#endif 