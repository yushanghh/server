//
// Created by chenmeng on 2020/4/4.
//

#ifndef SERVER_FIBER_H
#define SERVER_FIBER_H

#include "log.h"
#include "macro.h"
#include <ucontext.h>
#include <atomic>
#include <memory>
#include <functional>

namespace server {
    //enable_shared_from_this功能类似于this指针，但是使用原始指针容易带来安全问题
    class Fiber : public std::enable_shared_from_this<Fiber> {
    public:
        typedef std::shared_ptr<Fiber> ptr;
        enum State {
            INIT,       //初始
            HOLD,       //暂停
            EXEC,       //执行
            TERM,       //结束
            READY,      //可执行
            EXCEPT      //异常
        };

        void setState(State state);

    private:
        Fiber();  //用于线程中第一个协程的构造

    public:
        Fiber(std::function<void()> cb, size_t stack_size = 0, bool use_caller = false);

        ~Fiber();

        void reset(std::function<void()> cb);       //重置协程执行函数
        void swapIn();                              //将当前协程切换为执行态
        void swapOut();                             //将当前协程切换到后台
        void call();                                //将当前协程切换为执行态
        void back();                                //将当前协程切换到后台
        u_int64_t getId() const { return m_id; }

        State getState() const { return m_state; }

    public:
        static void SetThis(Fiber *f);          //设置当前协程的运行协程
        static Fiber::ptr GetThis();            //返回当前所在的协程
        static void YieldToReady();             //将当前协程切换到后台并且设置为ready
        static void YieldToHold();              //将当前协程切换到后台并且设置为hold
        static uint64_t TotalFibers();          //返回协程的总数
        static void MainFunc();                 //协程执行的函数，执行完返回线程主协程
        static void CallerMainFunc();           //协程执行函数，执行完返回线程调度协程
        static uint64_t GetFiberId();           //返回当前协程的ID

    private:
        uint64_t m_id = 0;                            //协程ID
        State m_state = INIT;                         //协程状态
        uint32_t m_stack_size = 0;                    //栈大小
        ucontext_t m_ctx;                             //协程上下文
        void *m_stack = nullptr;                      //协程栈指针
        std::function<void()> m_cb;                   //协程执行函数
    };

}


#endif //SERVER_FIBER_H
