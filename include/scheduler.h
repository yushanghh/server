//
// Created by chenmeng on 2020/4/12.
//

#ifndef SERVER_SCHEDULER_H
#define SERVER_SCHEDULER_H

#include "log.h"
#include "fiber.h"
#include "thread.h"
#include <memory>
#include <vector>
#include <string>

namespace server {
    /*
     * 一个调度器包含多个线程，一个线程包含多个协程
     * 每个协程可以指定一个运行的线程ID
     * 如果use_caller设置为true，那么将本线程也用作协程调度（线程数量中也包含主线程），将当前线程执行的函数视为一个协程
     * 如果use_caller设置为false，那么本线程不参与协程调度，新建thread_count个线程
     * 此外，每个线程中还创建一个执行scheduler中run方法的协程，一个执行idle方法的协程，一个执行带调度的方法的协程
     *
     * 加入IOManager之后，idle方法就是在epoll wait等待事件的到来，事件到来之后将每个事件创建为一个fdcontext对象，然后插入到
     * 协程队列中；此外，epoll wait中还监听了一个特殊的文件描述符（用管道的方式），用于唤醒
     *
     * tickle方法就是往特殊的文件描述符中写入一个字符来唤醒处于idle的线程。每个线程当没有任务处理的时候就一直都在执行idle方法。
     * 如果要将调度器停止，那么就需要用这种方式来唤醒。
     * */
    class Scheduler {
    public:
        typedef std::shared_ptr<Scheduler> ptr;
        typedef Mutex MutexType;

        //是否会将协程加入到调度器中
        Scheduler(size_t threads = 1, bool use_caller = true, const std::string &name = "");

        virtual ~Scheduler();

        const std::string &getName() const { return m_name; }

        static Scheduler *GetThis();

        static Fiber *GetMainFiber();                              //当前协程调度器正在调度的协程

        void start();

        void stop();

        void switchTo(int thread = -1);

        std::ostream &dump(std::ostream &os);

        template<class FiberOrCb>
        void schedule(FiberOrCb fc, int thread = -1) {               //-1表示任意协程
            bool need_tickle = false;
            {                                          //块作用域而可以保证该锁会自动释放
                MutexType::Lock lock(m_mutex);
                need_tickle = scheduleNoLock(fc, thread);
            }
            if (need_tickle) {
                tickle();
            }
        }

        template<class InputIterator>
        void schedule(InputIterator begin, InputIterator end) {
            bool need_tickle = false;
            {
                MutexType::Lock lock(m_mutex);
                while (begin != end) {
                    need_tickle = scheduleNoLock(*begin, -1);
                    ++begin;
                }
            }
            if (need_tickle) {
                tickle();
            }
        }

    protected:
        virtual void tickle();                                      //通知有协程需要调度
        virtual bool stopping();

        virtual void idle();

        void run();                                                 //协程调度函数
        void setThis();

        bool hasIdleThreads() const { return m_idleThreadCount > 0; }

    private:
        template<class FiberOrCb>
        bool scheduleNoLock(FiberOrCb fc, int thread) {
            //是否需要通知
            FiberAndThread ft(fc, thread);
            if (ft.fiber || ft.cb) {
                m_fibers.push_back(ft);
            }
            return !m_fibers.empty();
        }

    private:
        struct FiberAndThread {
            Fiber::ptr fiber;                    //协程
            std::function<void()> cb;            //函数
            int thread;                          //线程ID

            FiberAndThread(Fiber::ptr f, int thr) : fiber(f), thread(thr) {}

            FiberAndThread(Fiber::ptr *f, int thr) : thread(thr) { fiber.swap(*f); }

            FiberAndThread(std::function<void()> f, int thr) : cb(f), thread(thr) {}

            FiberAndThread(std::function<void()> *f, int thr) : thread(thr) { cb.swap(*f); }

            FiberAndThread() : thread(-1) {}

            void reset() {
                fiber = nullptr;
                cb = nullptr;
                thread = -1;
            }
        };

    protected:
        std::vector<int> m_threadIds;                         //协程下的线程ID数组
        size_t m_threadCount = 0;                             //线程数量
        std::atomic<size_t> m_activeThreadCount = {0};     //工作线程数量
        std::atomic<size_t> m_idleThreadCount = {0};       //空闲线程数量
        bool m_stop = true;                                   //是否停止
        bool m_autoStop = false;                              //是否主动停止
        int m_rootThread = 0;                                 //主线程ID（use_caller）

    private:
        MutexType m_mutex;
        std::vector<Thread::ptr> m_threads;                   //线程池
        std::list<FiberAndThread> m_fibers;                   //待调用的协程队列
        //协程队列，放置待执行的协程(可以是function，也可以是协程)
        //协程还可能需要在固定的线程ID中执行
        Fiber::ptr m_rootFiber;                               //用于调度的协程（use_caller为true时生效）
        std::string m_name;                                   //协程调度器名称
    };

    class SchedulerSwitcher : public NonCopyable {
    public:
        SchedulerSwitcher(Scheduler *target = nullptr);

        ~SchedulerSwitcher();

    private:
        Scheduler *m_caller;
    };
}

#endif //SERVER_SCHEDULER_H
