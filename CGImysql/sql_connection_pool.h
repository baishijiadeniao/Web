#ifndef SQL_CONNECTION_POOL_H
#define SQL_CONNECTION_POOL_H

#include "../lock.h"
#include "../log/log.h"
#include<string>
#include<list>
#include<mysql/mysql.h>


class connection_pool
{
private:
    sem reserce;
    locker lock;
    string m_url;
    string m_username;
    string m_passwd;
    string m_dbname;
    int m_port;
    list<MYSQL *> connList;
    int m_free_conn;
    int m_max_conn;
    int m_cur_conn;
    connection_pool();
    ~connection_pool();
    int m_close_log;
public:
    static connection_pool* GetInstance();
    void init(string url,string username,string passwd,string DBName,int Max_conn,int port,int close_log);
    MYSQL* GetConnection();
    bool ReleaseConnection(MYSQL*);
    void DestroyPool();
    int GetFreeConn();
};

class connectionRAII
{
private:
    connection_pool *poolRAII;
    MYSQL* connRAII;
public:
    connectionRAII(MYSQL **con,connection_pool *connPool);
    ~connectionRAII();
};

#endif