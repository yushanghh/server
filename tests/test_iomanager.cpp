//
// Created by chenmeng on 2020/4/29.
//

#include "../include/server.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>

server::Logger::ptr g_logger = SYLAR_LOG_ROOT();

int sock;

void test_fiber() {
    SYLAR_LOG_INFO(g_logger) << "test_fiber";
}

void test1() {
    server::IOManager iom;
    iom.schedule(&test_fiber);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(sock, F_SETFL, O_NONBLOCK);
    sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (!connect(sock, (sockaddr *) &addr, sizeof(addr))) {

    } else if (errno = EINPROGRESS) {          //连接在进行中
        SYLAR_LOG_INFO(g_logger) << "add event errno=" << errno << " " << strerror(errno);
        server::IOManager::GetThis()->addEvent(sock, server::IOManager::READ, []() -> void {
            SYLAR_LOG_INFO(g_logger) << "read callback";
        });
        server::IOManager::GetThis()->addEvent(sock, server::IOManager::WRITE, []() -> void {
            SYLAR_LOG_INFO(g_logger) << "write callback";
            server::IOManager::GetThis()->cancelEvent(sock, server::IOManager::READ);
//            server::IOManager::GetThis()->cancelEvent(sock, server::IOManager::WRITE);
            close(sock);
        });
    } else {
        SYLAR_LOG_INFO(g_logger) << "else" << errno << " " << strerror(errno);
    }
}

//void test_timer() {
//    server::IOManager iom(2);
//    iom.addTimer(500, []() -> void {
//        SYLAR_LOG_INFO(g_logger) << "hello timer";
//    }, true);
//}

server::Timer::ptr s_timer;

void test_timer() {
    server::IOManager iom(2);
    s_timer = iom.addTimer(1000, [](){
        static int i = 0;
        SYLAR_LOG_INFO(g_logger) << "hello timer i=" << i;
        if(++i == 3) {
            s_timer->reset(2000, true);
            //s_timer->cancel();
        }
    }, true);
}

int main(int argc, char *argv[]) {
//    test1();
    test_timer();
    return 0;
}