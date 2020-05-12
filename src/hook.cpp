//
// Created by chenmeng on 2020/5/9.
//

#include "../include/hook.h"
#include <dlfcn.h>
#include <include/fiber.h>
#include <include/server.h>

namespace server {
    static thread_local bool t_hook_enable = false;     //每个线程可以选择是否hook

#define HOOK_FUN(XX) \
    XX(sleep) \
    XX(usleep)

    void hook_init() {
        static bool is_inited = false;
        if (is_inited) {
            return;
        }

#define XX(name) name ## _f = (name ## _fun)dlsym(RTLD_NEXT, #name);
        HOOK_FUN(XX);
#undef XX
    }

    struct _HookIniter {
        _HookIniter() {
            hook_init();
        }
    };

    static _HookIniter s_hook_initer;      //静态变量会在main函数执行之前赋值，因此hook_init会在main函数执行之前调用

    bool is_hook_enable() {
        return t_hook_enable;
    }

    void set_hook_enable(bool flag) {
        t_hook_enable = flag;
    }

    unsigned int sleep(unsigned int seconds) {
        if (!t_hook_enable) {
            return sleep_f(seconds);
        }

        server::Fiber::ptr fiber = server::Fiber::GetThis();
        server::IOManager* iom = server::IOManager::GetThis();
    }
}