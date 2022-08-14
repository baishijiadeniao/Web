#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "./log/log.h"
#include "lock.h"
#include "./thread_pool/thread_pool.h"
#include "./http/http.h"
#include "./CGImysql/sql_connection_pool.h"
#include<assert.h>
#include <signal.h>
#include<time.h>
#include<sys/epoll.h>

const int MAX_FD=65536;
const int MAX_EVENT_NUMBER=10000;
const int TIME_SLOT=5;

class WebServer
{
private:
    char* m_root;
    http_conn* users;
    client_data* user_timer;
    threadpool<http_conn> *m_pool;
    connection_pool* m_connection_pool;
    int m_epollfd;
    int m_listenfd;
    int m_pipe[2];
    int m_port;
    string m_user;
    string m_password;
    string m_database_name;
    int m_log_write;
    int m_TRIGMode;
    int m_actor_model;
    int m_close_log;
    int m_LISTENTrigmode;
    int m_CONNTrigmode;
    int m_sql_num;
    int m_thread_num;
    int m_OPT_Linger;
    Utils utils;
    epoll_event events[MAX_EVENT_NUMBER];
public:
    WebServer(/* args */);
    ~WebServer();
    void init(int port,string user,string password,string database_name,int log_write,int opt_linger,int trigmode,int actor_model,int close_log,int sql_num, int thread_num);
    void trigmode();
    void log_write();
    void sql_pool();
    void thread_pool();
    void eventListen();
    void timer(int,struct sockaddr_in);
    void adjust_timer(util_timer* timer);
    void deal_timer(util_timer* timer,int sockfd);
    bool dealclientdata();
    bool dealwithsignal(bool& timeout,bool& stop_server);
    void dealwithread(int sockfd);
    void dealwithwrite(int sockfd);
    void eventloop();
};

#endif 