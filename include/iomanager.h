//
// Created by chenmeng on 2020/4/22.
//

#ifndef SERVER_IOMANAGER_H
#define SERVER_IOMANAGER_H

#include "scheduler.h"
#include "macro.h"
#include "timer.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/epoll.h>
#include <memory>

namespace server {
    class IOManager : public Scheduler, public TimerManager {
    public:
        typedef std::shared_ptr<IOManager> ptr;
        typedef RWMutex RWMutexType;

        enum Event {
            NONE = 0x0,
            READ = 0x1,               //EPOLLIN
            WRITE = 0x4               //EPOLLOUT
        };

    private:
        struct FdContext {
            typedef Mutex MutexType;
            struct EventContext {                     //事件上下文
                Scheduler *scheduler = nullptr;      //事件执行的调度器
                Fiber::ptr fiber;                    //事件协程
                std::function<void()> cb;            //事件的回调函数
            };

            EventContext &getContext(Event event);

            void resetContext(EventContext &ctx);

            void triggerEvent(Event event);

            EventContext read;                       //读事件
            EventContext write;                      //写事件
            int fd = 0;                              //事件关联的文件描述符
            Event events = NONE;                     //已经注册的事件
            MutexType mutex;
        };

    public:
        IOManager(size_t threads = 1, bool use_caller = true, const std::string &name = "");

        ~IOManager();

        int addEvent(int fd, Event event, std::function<void()> cb = nullptr);        //增加事件,0表示成功
        bool deleteEvent(int fd, Event event);                                        //删除事件
        bool cancelEvent(int fd, Event event);                                        //取消事件，强制触发
        bool cancelAll(int fd);                                                       //取消所有事件
        static IOManager *GetThis();

    protected:
        void tickle() override;

        bool stopping() override;

        void idle() override;

        void onTimerInsertedAtFront() override;      //如果插入的timer刚好在最前面，需要执行

        void contextResize(size_t size);

        bool stopping(uint64_t &timeout);

    private:
        int m_epfd;                             //epoll的文件描述符
        int m_tickleFds[2];                     //pipe文件描述符，用于通知
        std::atomic<size_t> m_pendingEventCount = {0};  //当前等待执行的事件数量
        RWMutexType m_mutex;
        std::vector<FdContext *> m_fdContexts;
    };
}

#endif //SERVER_IOMANAGER_H
