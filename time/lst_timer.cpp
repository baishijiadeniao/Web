#include"lst_time.h"
#include "../http/http.h"

sort_timer_lst::sort_timer_lst(){
    head=NULL;
    tail=NULL;
}

sort_timer_lst::~sort_timer_lst(){
    util_timer* temp=head;
    while(head){
        head=temp->next;
        delete temp;
        temp=head;
    }
}

void sort_timer_lst::add_timer(util_timer* timer){
    if(!timer)
        return;
    //如果当前链表只有头尾节点
    if(!head){
        //直接插入
        head=tail=timer;
        return;
    }
    //如果新的计时器超时时间小于头节点计时器
    if(timer->expire < head->expire){
        //直接插到头节点前面
        timer->next=head;
        head->prev=timer;
        head=timer;
        return;
    }
    //其他情况则调用私有函数
    add_timer(timer,head);
}

void sort_timer_lst::add_timer(util_timer* timer,util_timer* lst_timer){
    util_timer* prev=lst_timer;
    util_timer* tmp=prev->next;
    //插入节点
    while(tmp){
        //遍历链表直到找到超时时间大于待插入定时器的超时时间的定时器
        if(timer->expire < head->expire){
            prev->next=timer;
            timer->next=tmp;
            timer->prev=prev;
            tmp->prev=timer;
            return;
        }
        prev=tmp;
        tmp=tmp->next;
    }
    //未找到，直接将该定时器插到尾部
    if(!tmp){
        prev->next=timer;
        timer->prev=prev;
        timer->next=NULL;
        tail=timer;
    }
}

//调整定时器位置
void sort_timer_lst::adjust_timer(util_timer* timer){
    if(!timer)
        return;
    util_timer* tmp=timer->next;
    // 被调整的定时器是链表尾结点 或 定时器超时值仍然小于下一个定时器超时值，调整后定时器的时间肯定比原来的大
    if(!tmp || timer->expire<tmp->expire)
        //不调整
        return;
    //被调整节点是头节点
    if(timer==head){
        //将头节点取出
        head=head->next;
        head->prev=NULL;
        timer->next=NULL;
        //重新插入
        add_timer(timer,head);
    }else{
        //取出该节点
        timer->next->prev=timer->prev;
        timer->prev->next=timer->next;
        //重新插入
        add_timer(timer,timer->next);
    }
}

//删除节点
void sort_timer_lst::del_timer(util_timer* timer){
    if(!timer)
        return;
    if(head==timer && timer==tail){
        delete timer;
        head=NULL;
        tail=NULL;
        return;
    }
    if(head==timer){
        head=head->next;
        head->prev=NULL;
        delete timer;
        return;
    }
    if(timer==tail){
        tail=tail->prev;
        tail->next=NULL;
        delete timer;
        return;        
    }
    timer->next->prev=timer->prev;
    timer->prev->next=timer->next;
    delete timer;
}

//到期定时器处理函数
void sort_timer_lst::tick(){
    if(!head)
        return;
    util_timer* tmp=head;
    //获取当前时间戳
    time_t cur=time(NULL);
    //遍历定时器链表，对超时的定时器逐个处理
    while(tmp){
        // 链表容器为升序排列，当前时间小于该定时器的超时时间，则后面的定时器也没有到期
        if(!tmp || tmp->expire>cur)
            return;
        tmp->cb_func(tmp->usr_data);
        head=tmp->next;
        if(head)
            head->prev=NULL;
        //这里删除节点不需要调用del_timer函数，因为我们已经找到该tmp的位置了而且它就在链表头，所以不需要重新调整链表
        delete tmp;
        tmp=head;
    }
}

//设置超时时间
void Utils::init(int time_slot){
    m_TIMESLOT=time_slot;
}

//将文件描述符设置为非阻塞的
int Utils::setnonblocking(int fd){
    //获取文件的flags，即open函数的第二个参数
    int old_option=fcntl(fd,F_GETFL);
    //增加文件的某个flags，比如文件是阻塞的，想设置成非阻塞
    int new_option=old_option | O_NONBLOCK;
    //设置文件的flags
    fcntl(fd,F_SETFL,new_option);
    return old_option;
}

//将内核事件表注册读事件
void Utils::addfd(int epollfd,int fd,bool one_shot,int TRIGmode){
    struct epoll_event event;
    event.data.fd=fd;
    if(TRIGmode==1){
        event.events= EPOLLIN | EPOLLET | EPOLLHUP;
    }else{
        event.events= EPOLLIN | EPOLLHUP;
    }
    //EPOLLONESHOT：当处理一个connfd，一个线程处理socket时，其他线程无法处理，该线程处理完后，需要重新添加EPOLLONESHOT事件到内核事件表
    if(one_shot){
        event.events |=EPOLLONESHOT;
    }
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
    setnonblocking(fd);
}

//使用统一事件源，信号处理函数不直接处理，而是将信号发送给主线程让主线程处理
//可重用性：中断后再次进入该函数环境变量和之前相同，不会丢失数据，一般要保存好全局变量
void Utils::sig_handler(int sig){
    // Linux中系统调用的错误都存储于errno中，errno由操作系统维护，存储就近发生的错误。
    // 下一次的错误码会覆盖掉上一次的错误，为保证函数的可重入性，保留原来的errno
    int save_error=errno;
    int msg=sig;
    //将信号值从管道写端u_piped[1]写入，传输要为字符类型而非整型
    send(u_piped[1],(char*)&sig,1,0);
    errno=save_error;
}


//设置需要捕捉的信号
void Utils::addsig(int sig,void(handler)(int),bool restart){
    struct sigaction sa;
    memset(&sa,'\0',sizeof(sa));
    //设置该信号的回调函数
    sa.sa_handler=handler;
    if(restart==true){
        //使被打断的系统调用自动重新发起
        sa.sa_flags |= SA_RESTART;
    }
    //信号处理函数执行期间屏蔽所有的信号
    sigfillset(&sa.sa_mask);
    sigaction(sig,&sa,NULL);
}

//这个信号处理函数主要用来删除链表中的到期定时器
void Utils::timer_handler(){
    m_timer_lst.tick();
    // 重新定时以不断触发SIGALRM信号
    alarm(m_TIMESLOT);           //不懂，为什么这里要重新定时
}

//将错误信息发送给客户端
void Utils::show_error(int connfd,const char * str){
    send(connfd,str,strlen(str),0);
    close(connfd);
}

//静态成员初始化
int *Utils::u_piped=0;
int Utils::u_epollfd=0;


// 类的成员函数需要隐含的this指针 而回调函数没有办法提供
void cb_func(client_data* user_data){
    epoll_ctl(Utils::u_epollfd,EPOLL_CTL_DEL,user_data->sockfd,0);
    assert(user_data);        //不懂
    close(user_data->sockfd);
    http_conn::m_user_count--;
}

// init()->addfd()->addsig(sig_handler())->cb_func()->timer_handler()