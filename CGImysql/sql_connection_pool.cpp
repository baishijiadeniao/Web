#include "sql_connection_pool.h"

//获取连接池实例，单例模式
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
    // 创建MaxConn条数据库连接
    for(int i=0;i<Max_conn;i++){
        //初始化连接环境
        MYSQL* connect=NULL;
        connect=mysql_init(connect);
        if(!connect){
            LOG_ERROR("mysql error");
            exit(-1);
        }
        //连接Mysql服务器
        connect=mysql_real_connect(connect,url.c_str(),username.c_str(),passwd.c_str(),DBName.c_str(),port,NULL,0); //fd==NULL
        if(!connect){
            LOG_ERROR("mysql error");
            exit(-1);
        }
        //将连接放入数据库连接池
        connList.push_back(connect);
        //当前空闲的连接+1
        m_free_conn++;
    }
    //将信号量初始化为最大连接数
    reserce=sem(m_free_conn);
    //设置最大连接数
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

connectionRAII::connectionRAII(MYSQL **con,connection_pool *connPool){       //不懂为啥这里要整一个**
    *con=connPool->GetConnection();
    connRAII=*con;
    poolRAII=connPool;
}

connectionRAII::~connectionRAII(){
    poolRAII->ReleaseConnection(connRAII);
}