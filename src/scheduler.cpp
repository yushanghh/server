//
// Created by chenmeng on 2020/4/19.
//

#include "../include/scheduler.h"

namespace server {
    static server::Logger::ptr g_logger = SYLAR_LOG_NAME("system");
    static thread_local Scheduler *t_scheduler = nullptr;
    static thread_local Fiber *t_scheduler_fiber = nullptr;            //主协程函数

    Scheduler::Scheduler(size_t threads, bool use_caller, const std::string &name) : m_name(name) {
        SYLAR_ASSERT(threads > 0);
        if (use_caller) {
            server::Fiber::GetThis();                     //创建main fiber
            --threads;
            SYLAR_ASSERT(GetThis() == nullptr);
            t_scheduler = this;
            m_rootFiber.reset(new Fiber(std::bind(&Scheduler::run, this), 0, true));   //创建fiber1
            server::Thread::SetName(m_name);
            t_scheduler_fiber = m_rootFiber.get();
            m_rootThread = server::GetThreadId();
            m_threadIds.push_back(m_rootThread);
        } else {
            m_rootThread = -1;
        }
        m_threadCount = threads;
    }

    Scheduler::~Scheduler() {
        SYLAR_ASSERT(m_stop);
        if (GetThis() == this) {
            t_scheduler = nullptr;
        }
    }

    Scheduler *Scheduler::GetThis() {
        return t_scheduler;
    }

    Fiber* Scheduler::GetMainFiber() {
        return t_scheduler_fiber;
    }

    void Scheduler::start() {
        MutexType::Lock lock(m_mutex);
        if (!m_stop) {
            return;
        }
        m_stop = false;
        SYLAR_ASSERT(m_threads.empty());

        m_threads.resize(m_threadCount);
        for (size_t i = 0; i < m_threadCount; ++i) {
            m_threads[i].reset(new Thread(std::bind(&Scheduler::run, this),
                                          m_name + "_" + std::to_string(i)));
            m_threadIds.push_back(m_threads[i]->getId());
        }
    }

    void Scheduler::stop() {
        m_autoStop = true;
        if (m_rootFiber && m_threadCount == 0 &&
            (m_rootFiber->getState() == Fiber::TERM || m_rootFiber->getState() == Fiber::INIT)) {  //没有跑起来
            SYLAR_LOG_INFO(g_logger) << this << " stopped";
            m_stop = true;
            if (stopping())
                return;
        }

        if (m_rootThread != -1) {
            //use_caller
            SYLAR_ASSERT(GetThis() == this);       //stop要在创建的那个线程中执行
        } else {
            SYLAR_ASSERT(GetThis() != this);
        }
        m_stop = true;
        for (size_t i = 0; i < m_threadCount; ++i) {
            tickle();
        }
        if (m_rootFiber) {
            tickle();                                 //用于使线程唤醒，之后每个线程都执行结束方法
        }
        if (m_rootFiber) {
            if (!stopping()){
                m_rootFiber->call();
            }
        }
        std::vector<Thread::ptr> threads;
        {
            MutexType::Lock lock(m_mutex);
            threads.swap(m_threads);
        }
        for (auto &i : threads) {
            i->join();
        }
    }

    bool Scheduler::stopping() {
        MutexType::Lock lock(m_mutex);
        return m_autoStop && m_stop && m_fibers.empty() && m_activeThreadCount == 0;
    }

    void Scheduler::tickle() {
        SYLAR_LOG_INFO(g_logger) << "tickle";
    }

    void Scheduler::setThis() {
        t_scheduler = this;
    }

    void Scheduler::run() {
        setThis();
        if (server::GetThreadId() != m_rootThread) {
            t_scheduler_fiber = Fiber::GetThis().get();        //获取执行调度器的协程
        }

        SYLAR_LOG_INFO(g_logger) << "run in " << t_scheduler_fiber->getId();

        Fiber::ptr idle_fiber(new Fiber(std::bind(&Scheduler::idle, this)));   //创建空闲协程
        Fiber::ptr cb_fiber;

        FiberAndThread ft;
        while (true) {
            ft.reset();
            bool tickle_me = false;
            bool is_active = false;
            {
                MutexType::Lock lock(m_mutex);
                auto it = m_fibers.begin();
                while (it != m_fibers.end()) {
                    //自己无法处理，去找一个自己可以处理的
                    if (it->thread != -1 && it->thread != GetThreadId()) {
                        ++it;
                        tickle_me = true;      //有事件需要处理，而且该线程无法处理
                        continue;
                    }
                    tickle_me = false;    //该线程可以处理，不需要其它线程

//                    SYLAR_ASSERT(it->fiber || it->cb);
//                    if (it->fiber && it->fiber->getState() == Fiber::EXEC) {
//                        ++it;
//                        continue;
//                    }
                    ft = *it;
                    m_fibers.erase(it++);
                    ++m_activeThreadCount;
                    is_active = true;
                    break;
                }
//                tickle_me = tickle_me | (it == m_fibers.end());
            }
            if (tickle_me) {
                tickle();
            }
            if (ft.fiber && (ft.fiber->getState() != Fiber::TERM && ft.fiber->getState() != Fiber::EXCEPT)) {
                ft.fiber->swapIn();
                --m_activeThreadCount;
                if (ft.fiber->getState() == Fiber::READY) {        //需要再次执行
                    schedule(ft.fiber);
                } else if (ft.fiber->getState() != Fiber::TERM && ft.fiber->getState() != Fiber::EXCEPT) {
//                    ft.fiber->YieldToHold();
                    ft.fiber->setState(Fiber::HOLD);
                }
                ft.reset();
            } else if (ft.cb) {
                if (cb_fiber) {
                    cb_fiber->reset(ft.cb);
                } else {
                    cb_fiber.reset(new Fiber(ft.cb));
                }
                ft.reset();
                cb_fiber->swapIn();
                --m_activeThreadCount;
                if (cb_fiber->getState() == Fiber::READY) {
                    schedule(cb_fiber);
                    cb_fiber.reset();
                } else if (cb_fiber->getState() == Fiber::EXCEPT || cb_fiber->getState() == Fiber::TERM) {
                    cb_fiber->reset(nullptr);
                } else {
//                    cb_fiber->YieldToHold();
                    cb_fiber->setState(Fiber::HOLD);
                    cb_fiber.reset();
                }
            } else {
                if (is_active) {
                    --m_activeThreadCount;
                    continue;
                }
                if (idle_fiber && idle_fiber->getState() == Fiber::TERM) {           //执行结束了
                    SYLAR_LOG_INFO(g_logger) << "idle fiber term";
                    break;
                }
                ++m_idleThreadCount;
                idle_fiber->swapIn();
                --m_idleThreadCount;
                if (idle_fiber && idle_fiber->getState() != Fiber::TERM && idle_fiber->getState() != Fiber::EXCEPT) {
                    idle_fiber->setState(Fiber::HOLD);
                    //不能YieldToHold，idle协程需要持续epoll wait，如果YieldToHold，后面还会被swapin，将状态重新设置为EXEC
//                    idle_fiber->YieldToHold();
                }
            }
        }
    }

    void Scheduler::idle() {                      //没有协程调度，也不能停止线程
        SYLAR_LOG_INFO(g_logger) << "idle";
        while (!stopping()) {
            server::Fiber::YieldToHold();
        }
    }

    void Scheduler::switchTo(int thread) {
        SYLAR_ASSERT(Scheduler::GetThis() != nullptr);
        if (Scheduler::GetThis() == this) {
            if (thread == -1 || thread == server::GetThreadId()) {
                return;
            }
        }
        schedule(Fiber::GetThis(), thread);
        Fiber::YieldToHold();
    }

    std::ostream &Scheduler::dump(std::ostream &os) {
        os << "[Scheduler name=" << m_name
           << " size=" << m_threadCount
           << " active_count=" << m_activeThreadCount
           << " idle_count=" << m_idleThreadCount
           << " stopping=" << m_stop
           << " ]" << std::endl << "    ";
        for (size_t i = 0; i < m_threadIds.size(); ++i) {
            if (i) {
                os << ", ";
            }
            os << m_threadIds[i];
        }
        return os;
    }

    SchedulerSwitcher::SchedulerSwitcher(Scheduler *target) {
        m_caller = target;
        if (target) {
            target->switchTo();
        }
    }

    SchedulerSwitcher::~SchedulerSwitcher() {
        if (m_caller) {
            m_caller->switchTo();
        }
    }

}