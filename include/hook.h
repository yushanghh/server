//
// Created by chenmeng on 2020/5/9.
//

#ifndef SERVER_HOOK_H
#define SERVER_HOOK_H

//unsigned int sleep(unsigned int seconds);

#include <unistd.h>

/*
 * hook用于将阻塞的SOCKET自动变成非阻塞的
 * 这样就可以不用等待事件的到来，当有事件到来的时候在切换过来执行
 * */

namespace server {
    bool is_hook_enable();

    void set_hook_enable(bool flag);


}

//C++为了实现函数的重载，编译阶段会修改函数名（函数名中包含参数之类的方式），使用extern "C"可以告诉编译器不要用C++风格修改函数名

extern "C" {
typedef unsigned int (*sleep_fun)(unsigned int seconds);
extern sleep_fun sleep_f;

typedef int (*usleep_fun)(useconds_t usec);
extern usleep_fun usleep_f;
}

#endif //SERVER_HOOK_H
