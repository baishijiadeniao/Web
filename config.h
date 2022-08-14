#ifndef CONFIG_H
#define CONFIG_H
#include "webserver.h"
class config
{
private:
public:
    config();
    ~config(){};      //因为没有定义，要加上{}

    void parse_arg(int argc,char* argv[]);   //解析参数
    int TRIGmode;
    int PORT;
    int close_log;
    int thread_num;
    int sql_num;
    int log_write;
    int LISTENmode;
    int CONNECTmode;
    int OPT_LINGER;
    int actor_model;
};


#endif