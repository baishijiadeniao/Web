#include "./webserver.h"

WebServer::WebServer(){
    //创建MAX_FD个http_conn对象
    this->users=new http_conn[MAX_FD];
    //设置当前目录的绝对路径
    char server_path[200];
    getcwd(server_path,200);
    char root[6]="/root";           //char*和char[6]的区别：char*在内存常量区不可以被修改
    //网站资源文件夹的路径
    m_root=(char*)malloc(strlen(server_path)+strlen(root)+1);
    strcpy(m_root,server_path);
    strcat(m_root,root);

    //创建MAX_FD个连接对象
    this->user_timer=new client_data[MAX_FD];
}

WebServer::~WebServer(){
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipe[0]);
    close(m_pipe[1]);
    delete[] user_timer;
    delete[] users;
    delete m_pool;           //不懂为啥这里不用delete[] m_pool
}

//初始化
void WebServer::init(int port,string user,string password,string database_name,int log_write,int opt_linger,int trigmode,int actor_model,int close_log,int sql_num, int thread_num){
    //端口
    this->m_port=port;
    this->m_user=user;
    this->m_password=password;
    this->m_database_name=database_name;
    //日志写入方式：同步or异步
    this->m_log_write=log_write;
    //触发组合方式
    this->m_TRIGMode=trigmode;
    //并发模型选择
    this->m_actor_model=actor_model;
    //是否关闭日志
    this->m_close_log=close_log;
    //线程数量
    this->m_thread_num=thread_num;
    //数据库连接池数量
    this->m_sql_num=sql_num;
    m_OPT_Linger = opt_linger;
}

void WebServer::trigmode(){
    // LT + LT
    if(0==m_TRIGMode){
        m_LISTENTrigmode=0;
        m_CONNTrigmode=0;
    }
    // LT + ET
    if(1==m_TRIGMode){
        m_LISTENTrigmode=0;
        m_CONNTrigmode=1;
    }
    // ET + LT
    if(2==m_TRIGMode){
        m_LISTENTrigmode=1;
        m_CONNTrigmode=0;
    }
    // ET + ET
    if(3==m_TRIGMode){
        m_LISTENTrigmode=1;
        m_CONNTrigmode=1;
    }
}

// 初始化日志
void WebServer::log_write(){
    if(0==m_close_log){
        //异步写入方式
        if(1==m_log_write){
            loger::get_instance()->init("./ServerLog",m_close_log,2000,800000,800);
        }
        //同步写入方式
        else{
            loger::get_instance()->init("./ServerLog",m_close_log,2000,800000,0);
        }
    }
}

// 初始化数据库连接池
void WebServer::sql_pool(){
    m_connection_pool=connection_pool::GetInstance();
    m_connection_pool->init("localhost",m_user,m_password,m_database_name,100,m_port,m_close_log);  //改小一点便于测试
    users->initmysql_result(m_connection_pool);
}

void WebServer::thread_pool(){
    m_pool=new threadpool<http_conn>(m_actor_model,m_connection_pool,m_thread_num);
}

//监听事件
void WebServer::eventListen(){
    m_listenfd=socket(PF_INET,SOCK_STREAM,0);
    assert(m_listenfd>=0);
    // 非优雅关闭连接：向对端发送一个RST报文直接进入CLOSED状态。
    if(m_OPT_Linger==1){
        struct  linger tmp={1,1};
        setsockopt(m_listenfd,SOL_SOCKET,SO_LINGER,&tmp,sizeof(tmp));
    }
    // 优雅关闭连接：正常的"TCP四次挥手"。
    if(m_OPT_Linger==0){
        struct  linger tmp={0,1};
        setsockopt(m_listenfd,SOL_SOCKET,SO_LINGER,&tmp,sizeof(tmp));
    }
    //设置端口复用
    int flag=1;
    setsockopt(m_listenfd,SOL_SOCKET,SO_REUSEPORT,&flag,sizeof(flag));

    // 设置超时时间
    utils.init(TIME_SLOT);

    struct sockaddr_in addr;
    bzero(&addr,sizeof(addr));
    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=htonl(INADDR_ANY);
    addr.sin_port=htons(m_port); 

    int res=bind(m_listenfd,(struct sockaddr*)&addr,sizeof(addr));
    assert(res>=0);
    res=listen(m_listenfd,5);
    assert(res>=0);

    this->m_epollfd=epoll_create(5);
    http_conn::m_epollfd=this->m_epollfd;

    utils.addfd(m_epollfd,m_listenfd,false,m_LISTENTrigmode);
    struct epoll_event events[MAX_EVENT_NUMBER];

    res=socketpair(PF_UNIX,SOCK_STREAM,0,m_pipe);
    assert(res !=-1);
    utils.setnonblocking(m_pipe[1]);
    // 管道读端在内核事件表注册读事件
    utils.addfd(m_epollfd,m_pipe[0],false,0);
    // 解决"对端关闭"问题
    utils.addsig(SIGPIPE,SIG_IGN);
    // 设置捕捉定时器超时信号
    utils.addsig(SIGALRM,utils.sig_handler,false);
    // 设置捕捉程序结束信号（kill命令 或 Ctrl+C）
    utils.addsig(SIGTERM,utils.sig_handler,false);
    alarm(TIME_SLOT);       //不懂为啥现在开始计时而不是等连接后
    Utils::u_piped=m_pipe;
    Utils::u_epollfd=m_epollfd;
}

void WebServer::timer(int connfd,struct sockaddr_in client_address){
    users[connfd].init(connfd,client_address,m_root,m_TRIGMode,m_close_log,m_user,m_password,m_database_name);
    //客户端的地址信息
    user_timer[connfd].address=client_address;
    //客户端的文件描述符
    user_timer[connfd].sockfd=connfd;
    //创建定时器
    util_timer* timer=new util_timer;
    //绑定客户端信息
    timer->usr_data=&user_timer[connfd];
    //获得此时的时间戳
    time_t cur=time(NULL);
    //设置限制时间  那上面那个alarm又是什么
    timer->expire=cur+3*TIME_SLOT;
    //设置回调函数
    timer->cb_func=cb_func;
    user_timer[connfd].timer=timer;
    utils.m_timer_lst.add_timer(timer);
}

void WebServer::adjust_timer(util_timer* timer){
    time_t cur=time(NULL);
    timer->expire=cur+3*TIME_SLOT;
    utils.m_timer_lst.adjust_timer(timer);
    LOG_INFO("%s","Delay the timer's timeout");
}

void WebServer::deal_timer(util_timer* timer,int sockfd){
    timer->cb_func(&user_timer[sockfd]);
    if(timer){
        utils.m_timer_lst.del_timer(timer);
    }
    LOG_INFO("%s","Delete the timer");
}

//不懂返回true和返回false有什么区别，
bool WebServer::dealclientdata(){
    struct sockaddr_in client_address;
    if(0 == m_LISTENTrigmode){
        //LT模式，可以不用一次性处理完连接请求
        socklen_t tmp = sizeof(client_address);
        int connfd=accept(m_listenfd,(struct sockaddr*)&client_address,&tmp);
        if(connfd<0){
            LOG_ERROR("%s errno is:%s","accept error",errno);
            return false;
        }
        if(http_conn::m_user_count >= MAX_FD){
            utils.show_error(connfd,"Internet busy");
            LOG_ERROR("%s","Internet busy");
            return false;
        }
        timer(connfd,client_address);
    }else{
        //ET模式，有多个连接请求要一次性处理完，所以要用while处理到不能处理为止
        //不懂这里循环一直不退出怎么办
        while(true){
            //这里不知道对不对
            int connfd=accept(m_listenfd,(struct sockaddr*)&client_address,(socklen_t*)(sizeof(client_address)));
            if(connfd==-1){
                LOG_ERROR("%s errno is:%s","accept error",errno);
                break;
            }
            if(http_conn::m_user_count >= MAX_FD){
                utils.show_error(connfd,"Internet busy");
                LOG_ERROR("%s","Internet busy");
                break;
            }
            timer(connfd,client_address);
        }
        return false;
    }
    return true;
}

//引用参数
bool WebServer::dealwithsignal(bool &timeout,bool &stop_server){
    int signal[1024];
    int res=recv(m_pipe[0],signal,sizeof(signal),0);
    if(res ==0 || res==-1){
        return false;
    }else{
        for(int i=0;i<res;i++){
            switch (signal[i])
            {
            case SIGALRM:
                timeout=true;
                break;
            case SIGTERM:
                stop_server=true;
                break;             //可以没有default
            }
        }
    }
    return true;
}

void WebServer::dealwithread(int sockfd){
    LOG_INFO("%s","deal_with_read");
    util_timer* timer=new util_timer;
    timer=user_timer[sockfd].timer;
    if(m_actor_model==1){
        if(timer){
            adjust_timer(timer);
        }
        m_pool->append(users+sockfd,0);         //为什么第二个参数是0而不是1，不应该是m_actor_model吗
        while(true){
            // improv：尝试过读了
            // timer_flag：读失败了
            if (1==users[sockfd].improv)
            {
                if(1==users[sockfd].timer_flag){
                    deal_timer(timer,sockfd);
                    users[sockfd].timer_flag=0;
                }
                users[sockfd].improv=0;
                break;
            }
        }
    }
    else{
        if(users[sockfd].read_once()){
            LOG_INFO("deal with data from %s",inet_ntoa(users[sockfd].get_address()->sin_addr))
            m_pool->append_p(users+sockfd);
            if(timer){
                adjust_timer(timer);
            }
        }else{
            deal_timer(timer,sockfd);
        }
    }
}

void WebServer::dealwithwrite(int sockfd){
    util_timer* timer=new util_timer;
    timer=user_timer[sockfd].timer;
    if(m_actor_model==1){
        if(timer){
            adjust_timer(timer);
        }
        m_pool->append(users+sockfd,1);         
        while(true){
            // improv：尝试过读了
            // timer_flag：读失败了
            if (1==users[sockfd].improv)
            {
                if(1==users[sockfd].timer_flag){
                    deal_timer(timer,sockfd);
                    users[sockfd].timer_flag=0;
                }
                users[sockfd].improv=0;
                break;
            }
        }
    }
    else{
        if(users[sockfd].write()){
            LOG_INFO("send data to client %s",inet_ntoa(users[sockfd].get_address()->sin_addr))
            if(timer){
                adjust_timer(timer);
            }
        }else{
            deal_timer(timer,sockfd);
        }
    }
}

void WebServer::eventloop(){
    bool timeout=false;
    bool stop_server=false;
    while (!stop_server)
    {
        int res=epoll_wait(m_epollfd,events,MAX_EVENT_NUMBER,0);
        if(res<0 && errno != EINTR){
            LOG_ERROR("%s","epoll_wait failure");
            break;
        }
        for(int i=0;i<res;i++){
            int sockfd=events[i].data.fd;
            if(sockfd==m_listenfd){
                bool flag=dealclientdata();
                if(!flag){
                    continue;
                }
            }
            // 处理异常事件
            else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)){
                // 服务器端关闭连接
                util_timer *timer=user_timer[sockfd].timer;
                deal_timer(timer,sockfd);
            }
            // 处理管道中信号
            else if(sockfd==m_pipe[0] && (events[i].events & EPOLLIN)){
                bool flag=dealwithsignal(timeout,stop_server);
                if(!flag){
                    LOG_ERROR("%s","dealclientdata error");
                }
            }else if(events[i].events & EPOLLIN){
                dealwithread(sockfd);
            }else if(events[i].events & EPOLLOUT){
                dealwithwrite(sockfd);
            }
        }
        if(timeout){
            // 超时信号处理，并重新定时以不断触发SIGALRM信号
            utils.timer_handler();
            LOG_INFO("%s","connection timeout");
            timeout=false;
        }
    }
    
}