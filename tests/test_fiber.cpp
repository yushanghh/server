//
// Created by chenmeng on 2020/4/12.
//

#include "../include/server.h"

server::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void run_in_fiber(){
    SYLAR_LOG_INFO(g_logger) << "run in fiber begin";
    server::Fiber::YieldToHold();
    SYLAR_LOG_DEBUG(g_logger) << "run in fiber end";
    server::Fiber::YieldToHold();
}

void test_fiber() {
    SYLAR_LOG_INFO(g_logger) << "main begin -1";
    {
        server::Fiber::GetThis();
        SYLAR_LOG_INFO(g_logger) << "main begin";
        server::Fiber::ptr fiber(new server::Fiber(run_in_fiber));
        fiber->swapIn();
        SYLAR_LOG_INFO(g_logger) << "main after swapIn";
        fiber->swapIn();
        SYLAR_LOG_INFO(g_logger) << "main after end";
        fiber->swapIn();
    }
    SYLAR_LOG_INFO(g_logger) << "main after end2";
}


int main(int argc, char* argv[]){
//    server::Fiber::GetThis();
//    SYLAR_LOG_DEBUG(g_logger) << "main fiber begin";
//    server::Fiber::ptr fiber(new server::Fiber(run_in_fiber));
//    fiber->swapIn();
//    SYLAR_LOG_DEBUG(g_logger) << "main after swap in";
//    fiber->swapIn();
//    SYLAR_LOG_DEBUG(g_logger) << "main after end";

    SYLAR_LOG_INFO(g_logger) << "main begin -1";
    {
        server::Fiber::GetThis();
        SYLAR_LOG_INFO(g_logger) << "main begin";
        server::Fiber::ptr fiber(new server::Fiber(run_in_fiber));
        fiber->swapIn();
        SYLAR_LOG_INFO(g_logger) << "main after swapIn";
        fiber->swapIn();
        SYLAR_LOG_INFO(g_logger) << "main after end";
        fiber->swapIn();
    }
    SYLAR_LOG_INFO(g_logger) << "main after end2";
    return 0;
}