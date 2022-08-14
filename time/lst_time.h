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

class util_timer;

struct client_data
{
    struct sockaddr_in address;
    int sockfd;
    util_timer* timer;
};

class util_timer
{
public:
    util_timer():prev(NULL),next(NULL){};
    ~util_timer(){};
    time_t expire;
    void (*cb_func)(client_data*);   //
    client_data* usr_data;
    util_timer* prev;
    util_timer* next;
};

class sort_timer_lst
{
public:
    sort_timer_lst(/* args */);
    ~sort_timer_lst();
    void add_timer(util_timer* timer);
    void adjust_timer(util_timer* timer);
    void del_timer(util_timer* timer);
    void tick();
private:
    void add_timer(util_timer* timer,util_timer* lst_timer);
    util_timer* head;
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
    //将内核时间表注册读事件
    void addfd(int epollfd,int fd,bool one_shot,int TRIGmode);
    //信号处理函数
    static void sig_handler(int sig);
    //捕捉信号
    void addsig(int sig,void(handler)(int),bool restart=true);           //void(handler)(int)
    //定时处理任务，重新计时
    void timer_handler();
    void show_error(int connfd,const char * str);
};

void cb_func(client_data* user_data);




#endif 