//
// Created by chenmeng on 2020/4/22.
//

#ifndef SERVER_MSCHEDULER_H
#define SERVER_MSCHEDULER_H

#include "fiber.h"
#include "mutex.h"
#include "thread.h"
#include "macro.h"

#include <memory>
#include <vector>
#include <list>
#include <string>

namespace server {
    class MScheduler {
    public:
        typedef std::shared_ptr<MScheduler> ptr;
        typedef Mutex MutexType;

        MScheduler(int threadCount, const std::string &name = "");

        virtual ~MScheduler();

        const std::string &getName() const { return m_name; }

        static MScheduler *GetThis();

        void start();                                    //启动调度器
        void stop();                                     //关闭调度器

        void run();                                      //线程执行的协程调度函数

        template<typename FiberOrCb>
        void schedule(FiberOrCb &foc, int thread = -1) {
            MutexType::Lock lock(m_mutex);
            scheduleNonLock(foc, thread);
            lock.unlock();
        }

    protected:
        virtual void idle();                             //没有协程可以调度时应该做什么
        virtual void tickle();                           //如何通知其它协程进行调度

    private:
        //支持fiber或者函数，可以指定调度到的线程ID
        struct FiberAndThread {
            Fiber::ptr fiber;
            std::function<void()> cb;
            int thread;

            FiberAndThread(Fiber::ptr f, int thr) : fiber(f), thread(thr) {}

            FiberAndThread(Fiber::ptr *f, int thr) : thread(thr) {
                fiber.swap(*f);
            }

            FiberAndThread(std::function<void()> f, int thr) : cb(f), thread(thr) {}

            FiberAndThread(std::function<void()> *f, int thr) : thread(thr) {
                cb.swap(*f);
            }

            FiberAndThread() : thread(-1) {}

            void reset() {
                fiber = nullptr;
                cb = nullptr;
                thread = -1;
            }
        };

    private:
        template<typename FiberOrCb>
        void scheduleNonLock(FiberOrCb &foc, int thread) {
            FiberAndThread ft(foc, thread);
            SYLAR_ASSERT(ft.fiber || ft.cb);
            m_fibers.push_back(ft);
        }

        void setThis();

        bool stopping();


    private:
        MutexType m_mutex;
        int m_threadCount;                               //线程数量
        std::vector<Thread::ptr> m_threads;              //每个线程
        std::vector<int> m_threadIds;                    //每个线程的ID
        std::list<FiberAndThread> m_fibers;              //待调度的协程队列
        std::atomic<size_t> m_activeThreadCount = {0};//活跃线程数
        std::atomic<size_t> m_idleThreadCount = {0};  //空闲线程数
        bool m_stopping = true;                          //是否处于停止的状态
        std::string m_name;                              //协程调度器的名称
        bool m_autoStop = false;                         //是否自动停止，用于停止的二次确认
    };

}

#endif //SERVER_MSCHEDULER_H
