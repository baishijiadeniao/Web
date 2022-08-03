#include"lst_time.h"

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
    if(!head){
        head=tail=timer;
        return;
    }
    if(timer->expire < head->expire){
        timer->next=head;
        head->prev=timer;
        head=timer;
        return;
    }
    add_timer(timer,head);
}

void sort_timer_lst::add_timer(util_timer* timer,util_timer* lst_timer){
    util_timer* prev=lst_timer;
    util_timer* tmp=prev->next;
    //插入节点
    while(tmp){
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
    if(!tmp){
        prev->next=timer;
        timer->prev=prev;
        timer->next=NULL;
        tail=timer;
    }
}

void sort_timer_lst::adjust_timer(util_timer* timer){
    if(!timer)
        return;
    util_timer* tmp=timer->next;
    if(!tmp || timer->expire<tmp->expire)
        return;
    if(timer==head){
        head=head->next;
        head->prev=NULL;
        timer->next=NULL;
        add_timer(timer,head);
    }else{
        timer->next->prev=timer->prev;
        timer->prev->next=timer->next;
        add_timer(timer,timer->next);
    }
}

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

void sort_timer_lst::tick(){
    if(!head)
        return;
    util_timer* tmp=head;
    time_t cur=time(NULL);
    while(tmp){
        if(!tmp || tmp->expire>cur)
            return;
        tmp->cb_func(tmp->usr_data);
        head=tmp->next;
        if(head)
            head->prev=NULL;
        delete tmp;
        tmp=head;
    }
}

void Utils::init(int time_slot){
    m_TIMESLOT=time_slot;
}

void Utils::setnonblocking(int fd){

}

void Utils::addfd(int epollfd,int fd,bool one_shot,int TRIGmode){
    struct epoll_event event;
    event.data.fd=fd;
    if(TRIGmode==1){
        event.events= EPOLLIN | EPOLLET | EPOLLHUP;
    }else{
        event.events= EPOLLIN | EPOLLHUP;
    }
    if(one_shot){
        event.events |=EPOLLONESHOT;
    }
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
    setnonblocking(fd);
}

void Utils::sig_handler(int sig){
    int save_error=errno;
    int msg=sig;
    send(u_piped[1],(char*)&sig,1,0);
    errno=save_error;
}

void Utils::addsig(int sig,void(handler)(int),bool restart=true){
    struct sigaction sa;
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler=handler;
    if(restart==true){
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    sigaction(sig,&sa,NULL);
}

void Utils::timer_handler(){
    m_timer_lst->tick();
    alarm(m_TIMESLOT);           //不懂，为什么这里要重新定时
}

void Utils::show_error(int connfd,const char * str){
    send(connfd,str,strlen(str),0);
    close(connfd);
}

int *Utils::u_piped=0;
int Utils::u_epollfd=0;


// 类的成员函数需要隐含的this指针 而回调函数没有办法提供
void cb_func(client_data* user_data){
    epoll_ctl(Utils::u_epollfd,EPOLL_CTL_DEL,user_data->sockfd,0);
    assert(user_data);        //不懂
    close(user_data->sockfd);
    // http_conn::m_user_count--;
}

// init()->addfd()->addsig(sig_handler())->cb_func()->timer_handler()