#include "config.h"

config::config(){
    PORT=9006;
    TRIGmode=0;
    close_log=0;
    thread_num=2;
    sql_num=2;
    log_write=0;
    LISTENmode=0;
    CONNECTmode=0;
    OPT_LINGER=0;
    actor_model=0;
}

void config::parse_arg(int argc,char* argv[]){
    int ch;
    const char *str = "p:l:m:o:s:t:c:a:";
    while(ch=getopt(argc,argv,str) !=-1){       //返回获取a选项
        switch (ch)
        {
        case 'p':
            PORT=atoi(optarg);          //optarg是选项参数
            break;
        case 'l':
            log_write=atoi(optarg);
            break;
        case 'm':
            TRIGmode=atoi(optarg);
            break;
        case 'o':
            OPT_LINGER=atoi(optarg);
            break;
        case 's':
            sql_num=atoi(optarg);
            break;
        case 't':
            thread_num=atoi(optarg);
            break;
        case 'c':
            close_log=atoi(optarg);
            break;
        case 'a':
            actor_model=atoi(optarg);
            break;
        }
    }
}