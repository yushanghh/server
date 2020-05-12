//
// Created by chenmeng on 2020/4/13.
//

#ifndef SERVER_THREAD_H
#define SERVER_THREAD_H

#include "mutex.h"
#include "log.h"
#include <boost/function.hpp>
#include <pthread.h>
#include <memory>

namespace server {
    class Thread {
    public:
        typedef std::shared_ptr<Thread> ptr;

        Thread(std::function<void()> cb, const std::string &name);

        ~Thread();

        pid_t getId() const { return m_id; }

        const std::string &getName() const { return m_name; }

        void join();

        static Thread *GetThis();

        static const std::string &GetName();

        static void SetName(const std::string &name);

    private:
        static void *Run(void *arg);

    private:
        pid_t m_id;
        pthread_t m_thread;
        std::function<void()> m_cb;
        std::string m_name;
        Semaphore m_sem;
    };


}

#endif //SERVER_THREAD_H
