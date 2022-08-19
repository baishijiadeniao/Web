#include "CGImysql/sql_connection_pool.h"
#include "thread_pool/thread_pool.h"
#include "log/log.h"
#include "time/lst_time.h"
#include "webserver.h"
#include "http/http.h"
#include "./config.h"
using namespace std;

int main(int argc,char* argv[]){
    string username="root";
    string password="lrdLRD141122!";
    string database_name="yourdb";

    WebServer server;
    config c;
    c.parse_arg(argc,argv);
    //初始化
    server.init(c.PORT,username,password,database_name,
                c.log_write,c.OPT_LINGER,c.TRIGmode,c.actor_model,c.close_log,c.sql_num,c.thread_num);

    //日志 要放在最前面初始化
    server.log_write();
    //初始化数据库连接
    server.sql_pool();
    //初始化线程池
    server.thread_pool();
    server.trigmode();
    //监听
    server.eventListen();
    //运行
    server.eventloop();
}
