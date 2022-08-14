#ifndef HTTP_H
#define HTTP_H 
#include "../lock.h"
#include "../time/lst_time.h"
#include "../log/log.h"
#include "../CGImysql/sql_connection_pool.h"
#include<map>
#include<string.h>
#include<stdarg.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<unistd.h>
#include<sys/epoll.h>
#include<stdio.h>
#include<fcntl.h>
#include<sys/mman.h>
#include <mysql/mysql.h>

class http_conn
{
private:
    /* data */
public:
    http_conn(){};
    ~http_conn(){};
    static const int FILENAME_LEN=200;
    static const int READ_BUFFER_SIZE=2048;   //带有类内初始值设定项的成员必须为常量
    static const int WRITE_BUFFER_SIZE=2048;
    enum CHECK_STATE{
        CHECK_STATE_REQUESTLINE=0,      //要用',' 而不是';'
        CHECK_STATE_HEADER,
        CHECK_STAET_CONTENT
    };                                   //加分号
    enum METHOD{
        GET=0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONs,
        CONNECT,
        PATH
    };
    enum HTTP_CODE{
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        INTERNAL_REQUEST,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        CLOSED_CONNECTION
    };
    enum LINE_STATE{
        LINE_OK,
        LINE_BAD,
        LINE_OPEN,
    };
public:
    void init(int sockfd,const sockaddr_in &addr,char *,int ,int,string user,string passwd,string sqlname);
    LINE_STATE parse_line();
    HTTP_CODE parse_request_line(char*);
    HTTP_CODE parse_header(char*);
    HTTP_CODE parse_content(char*);
    HTTP_CODE process_read();
    bool add_response(const char* format,...);
    bool add_status_line(int state,const char *title);
    bool add_headers(int content_len);
    bool add_content_len(int content_len);
    bool add_blank_line();
    bool add_linger();
    bool add_content_type();
    bool add_content(const char*);
    bool process_write(HTTP_CODE ret);
    bool write();
    void unmap();
    HTTP_CODE do_request();
    void process();
    void initmysql_result(connection_pool *connPool);
    bool read_once();
    void close_conn(bool real_close=true);
    sockaddr_in* get_address(){
        return &m_address;
    }
    char * get_line(){return m_read_buf+m_start_line;}
public:
    static int m_epollfd;            //为啥要设为静态的：因为每一个都是一样的，不用重复初始化
    static int m_user_count;
    MYSQL *mysql;
    int m_state;
    int timer_flag;
    int improv;
private:
void init();
    int m_sockfd;
    sockaddr_in m_address;
    char m_read_buf[READ_BUFFER_SIZE];
    int m_read_idx;
    int m_checked_idx;
    int m_start_line;
    char m_write_buf[WRITE_BUFFER_SIZE];
    int m_write_idx;
    CHECK_STATE m_check_state;
    METHOD m_method;
    char m_real_file[FILENAME_LEN];
    char *m_url;
    char *m_version;
    char *m_host;
    int m_content_length;  //主体长度
    bool m_linger;
    char* m_file_address;
    struct stat m_file_stat;
    struct iovec m_iv[2];
    int m_iv_count;
    int cgi;
    char *m_string;
    int bytes_to_send;
    int bytes_have_send;
    char *doc_root;
    map<string,string> m_users;
    int m_TRIGMode;
    int m_close_log;
    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];
};

#endif