#ifndef SQL_CONNECTION_POOL_H
#define SQL_CONNECTION_POOL_H

#include "../lock.h"
#include "../log/log.h"
#include<string>
#include<list>
#include<mysql/mysql.h>

//由于服务器需要频繁地访问数据库，即需要频繁创建和断开数据库连接，该过程是一个很耗时的操作，也会对数据库造成安全隐患。
//在程序初始化的时候，集中创建并管理多个数据库连接，可以保证较快的数据库读写速度，更加安全可靠。


class connection_pool
{
private:
    sem reserce;
    locker lock;
    //数据库主机地址
    string m_url;
    //登录数据库的用户名
    string m_username;
    //登录数据库的密码
    string m_passwd;
    //使用的数据库
    string m_dbname;
    //数据库端口号
    int m_port;
    //连接池 list底层是双向链表 方便添加和删除
    list<MYSQL *> connList;
    //空闲连接数
    int m_free_conn;
    //最大连接数
    int m_max_conn;
    //已连接的数量
    int m_cur_conn;
    //构造函数
    connection_pool();
    //析构函数
    ~connection_pool();
public:
    //单例模式
    static connection_pool* GetInstance();
    //初始化
    void init(string url,string username,string passwd,string DBName,int Max_conn,int port,int close_log);
    //获取数据库连接
    MYSQL* GetConnection();
    //释放连接
    bool ReleaseConnection(MYSQL*);
    //销毁连接池
    void DestroyPool();
    //获取空闲连接数
    int GetFreeConn();
    //是否关闭日志
    int m_close_log;
};

//RAII的核心思想是将资源与对象的生命周期绑定
class connectionRAII
{
private:
    connection_pool *poolRAII;
    MYSQL* connRAII;
public:
//通过二重指针对对MYSQL *con修改
    connectionRAII(MYSQL **con,connection_pool *connPool);
    ~connectionRAII();
};

#endif