//
// Created by chenmeng on 2020/4/22.
//

#include "../include/mscheduler.h"


namespace server {

    static server::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    thread_local Fiber::ptr t_scheduler_fiber = nullptr;
    thread_local MScheduler *t_scheduler = nullptr;

    MScheduler::MScheduler(int threadCount, const std::string &name) : m_name(name) {
        SYLAR_ASSERT(threadCount > 0);
        m_threadCount = threadCount;
    }

    MScheduler::~MScheduler() {
        SYLAR_ASSERT(m_stopping);
    }

    void MScheduler::start() {
        MutexType::Lock lock(m_mutex);
        if (!m_stopping) {
            return;
        }
        m_stopping = false;
        m_threads.resize(m_threadCount);
        for (size_t i = 0; i < m_threadCount; ++i) {
            m_threads[i].reset(new Thread(std::bind(&MScheduler::run, this),
                                          "test_" + std::to_string(i)));
            m_threadIds.push_back(m_threads[i]->getId());
        }
        lock.unlock();
    }

    void MScheduler::stop() {
        if (m_stopping)
            return;

    }

    void MScheduler::setThis() {
        t_scheduler = this;
    }

    void MScheduler::run() {
        SYLAR_LOG_DEBUG(g_logger) << "run";

        Fiber::ptr idle_fiber(new Fiber(std::bind(&MScheduler::idle, this)));
        Fiber::ptr cb_fiber;

        FiberAndThread ft;
        while (true) {
            MutexType::Lock lock(m_mutex);
            bool tickle_me = false;
            auto it = m_fibers.begin();
            while (it != m_fibers.end()) {
                if (it->thread != -1 && it->thread != GetThreadId()) {
                    ++it;
                    tickle_me = true;
                    continue;
                }
                SYLAR_ASSERT(it->fiber || it->cb);
                ft = *it;
                m_fibers.erase(it++);
                break;
            }
            if (ft.fiber && ft.fiber->getState() == Fiber::EXEC) {
                ft.fiber->swapIn();
                //执行结束之后
                if (ft.fiber->getState() == Fiber::READY) {
                    t_scheduler->schedule(ft.fiber, ft.thread);
                }
                ft.reset();
            } else if (ft.cb) {
                if (cb_fiber) {
                    cb_fiber->reset(ft.cb);
                } else {
                    cb_fiber.reset(new Fiber(ft.cb));
                }
                cb_fiber->swapIn();
                if (cb_fiber->getState() == Fiber::READY) {
                    t_scheduler->schedule(cb_fiber, ft.thread);
                }
                cb_fiber.reset();
                ft.reset();
            } else {
                idle_fiber->swapIn();
                if (idle_fiber->getState() != Fiber::TERM && idle_fiber->getState() != Fiber::EXCEPT) {
                    idle_fiber->setState(Fiber::HOLD);
                }
            }
            lock.unlock();
        }
    }

    void MScheduler::idle() {

    }

    bool MScheduler::stopping() {
        MutexType::Lock lock(m_mutex);

    }


}