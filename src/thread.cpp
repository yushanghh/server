//
// Created by chenmeng on 2020/4/14.
//

#include "../include/thread.h"

namespace server {
    static thread_local Thread *t_thread = nullptr;               //当前正在运行的线程
    static thread_local std::string t_thread_name = "UNKOWN";     //当前正在运行的线程的名称
    static server::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    Thread *Thread::GetThis() {
        return t_thread;
    }

    const std::string &Thread::GetName() {
        return t_thread_name;
    }

    void Thread::SetName(const std::string &name) {
        if (name.empty()) {
            return;
        }
        if (t_thread) {
            t_thread->m_name = name;
        }
        t_thread_name = name;
    }

    Thread::Thread(std::function<void()> cb, const std::string &name) : m_cb(cb), m_name(name) {
        if (name.empty()) {
            m_name = "UNKNOWN";
        }
        int ret = pthread_create(&m_thread, NULL, &Thread::Run, this);
        if (ret) {
            SYLAR_LOG_ERROR(g_logger) << "pthread create thread fail, rt=" << ret << " name=" << name;
            throw std::logic_error("pthread create error");
        }
        m_sem.wait();       //循环创建多个线程的时候，可以保证可以获得线程的ID，也就是保证线程可以串行创建
    }

    Thread::~Thread() {
        if (m_thread) {
            pthread_detach(m_thread);
        }
    }

    void Thread::join() {
        if (m_thread) {
            int ret = pthread_join(m_thread, NULL);
            if (ret) {
                SYLAR_LOG_ERROR(g_logger) << "pthread join thread fail, ret=" << ret << " name=" << "m_name";
                throw std::logic_error("pthread join error");
            }
            m_thread = 0;
        }
    }

    void *Thread::Run(void *arg) {
        Thread *thread = static_cast<Thread *>(arg);
        t_thread = thread;
        t_thread_name = thread->m_name;
        thread->m_id = server::GetThreadId();
        pthread_setname_np(pthread_self(), thread->m_name.substr(0, 15).c_str());
        std::function<void()> cb;
        cb.swap(thread->m_cb);
        thread->m_sem.notify();
        cb();
        return 0;
    }

}

