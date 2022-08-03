#include "sql_connection_pool.h"

connection_pool* connection_pool::GetInstance(){
    static connection_pool connPool;
    return &connPool;
}

connection_pool::connection_pool(){
    m_free_conn=0;
    m_cur_conn=0;
}

void connection_pool::init(string url,string username,string passwd,string DBName,int Max_conn,int port,int close_log){
    m_url=url;
    m_username=username;
    m_passwd=passwd;
    m_port=port;
    m_dbname=DBName;
    m_close_log=close_log;
    for(int i=0;i<Max_conn;i++){
        MYSQL* connect=NULL;
        connect=mysql_init(connect);
        if(!connect){
            LOG_ERROR("mysql error");
            exit(-1);
        }
        connect=mysql_real_connect(connect,url.c_str(),username.c_str(),passwd.c_str(),DBName.c_str(),port,NULL,0); //fd==NULL
        if(!connect){
            LOG_ERROR("mysql error");
            exit(-1);
        }
        connList.push_back(connect);
        m_free_conn++;
    }
    reserce=sem(m_free_conn);
    m_max_conn=m_free_conn;
}

MYSQL* connection_pool::GetConnection(){
    MYSQL* connect=NULL;
    if(connList.size()==0){
        return NULL;
    }
    reserce.wait();
    lock.lock();
    connect=connList.front();
    connList.pop_front();
    m_free_conn--;
    m_cur_conn++;
    lock.unlock();
    return connect;
}

bool connection_pool::ReleaseConnection(MYSQL* connect){
    if(connect==NULL){
        return false;
    }
    lock.lock();
    connList.push_back(connect);
    m_free_conn++;
    m_cur_conn--;
    lock.unlock();
    reserce.post();
    return true;
}


void connection_pool::DestroyPool(){
    lock.lock();
    if(connList.size()>0){
        list<MYSQL*>::iterator it;
        for(it=connList.begin();it !=connList.end();it++){
            MYSQL* connect=*it;
            mysql_close(connect);
        }
        m_free_conn=0;
        m_cur_conn=0;
        connList.clear();
    }
    lock.unlock();
}

int connection_pool::GetFreeConn(){
    return this->m_free_conn;
}

connection_pool::~connection_pool(){
    DestroyPool();
}

connectRAII::connectRAII(MYSQL **con,connection_pool *connPool){       //不懂为啥这里要整一个**
    *con=connPool->GetConnection();
    connRAII=*con;
    poolRAII=connPool;
}

connectRAII::~connectRAII(){
    poolRAII->ReleaseConnection(connRAII);
}