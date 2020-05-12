//
// Created by chenmeng on 2020/4/17.
//

#include "../include/server.h"
#include <unistd.h>

server::Logger::ptr g_logger = SYLAR_LOG_ROOT();

int count = 0;

server::RWMutex s_mutex;

void fun1(){
    SYLAR_LOG_INFO(g_logger) << "name: " << server::Thread::GetName()
                             << " this.name: " << server::Thread::GetThis()->getName()
                             << " id: " << server::GetThreadId()
                             << " this.id: " << server::Thread::GetThis()->getId();
    for(int i = 0; i < 100000; ++i){
        server::RWMutex::WriteLock lock(s_mutex);
        ++count;
    }
}

void fun2(){
    while(true) {
        SYLAR_LOG_INFO(g_logger) << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
    }
}

void fun3(){
    while(true) {
        SYLAR_LOG_INFO(g_logger) << "========================================";
    }
}

int main(int argc, char* argv[]){
    SYLAR_LOG_INFO(g_logger) << "thread test begin";
    std::vector<server::Thread::ptr> thrs;
    for(int i = 0; i < 9; ++i){
        server::Thread::ptr thr(new server::Thread(&fun1, "name_" + std::to_string(i * 2)));
        thrs.push_back(thr);
    }
    for(size_t i = 0; i < thrs.size(); ++i){
        thrs[i]->join();
    }

    SYLAR_LOG_INFO(g_logger) << "thread test end";
    SYLAR_LOG_INFO(g_logger) << "count=" << count;

    return 0;
}
