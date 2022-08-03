#include "log.h"

int main(){
    loger::get_instance()->init("./ServerLog",0,2000,800000,800);
    LOG_INFO("log test %s","success");
    // void* args;
    // loger::get_instance()->flush_log_thread(args);
}