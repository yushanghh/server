//
// Created by chenmeng on 2020/4/21.
//

#include "../include/server.h"

static server::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test_fiber() {
    static int s_count = 5;
    SYLAR_LOG_INFO(g_logger) << "test in fiber s_count=" << s_count;
    sleep(1);
    if (--s_count >= 0) {
        server::Scheduler::GetThis()->schedule(&test_fiber, -1);
    }
}

int main(int argc, char *argv[]) {
    SYLAR_LOG_INFO(g_logger) << "main";
    server::Scheduler sc(3, false, "test");
    sc.start();
    sleep(2);
    SYLAR_LOG_INFO(g_logger) << "scheduler";
    sc.schedule(&test_fiber);
    sc.stop();
    SYLAR_LOG_INFO(g_logger) << "over";
    return 0;
}
