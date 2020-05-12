//
// Created by chenmeng on 2020/4/4.
//

#ifndef SERVER_MUTEX_H
#define SERVER_MUTEX_H

#include "noncopyable.h"
#include <functional>
#include <pthread.h>
#include <semaphore.h>

namespace server {
    //局部锁的模板
    template<class T>
    class ScopedLockImpl {
    public:
        ScopedLockImpl(T &mutex) : m_mutex(mutex) {
            m_mutex.lock();
            m_locked = true;
        }

        ~ScopedLockImpl() {
            unlock();
        }

        void lock() {
            if (!m_locked) {
                m_mutex.lock();
                m_locked = true;
            }
        }

        void unlock() {
            if (m_locked) {
                m_mutex.unlock();
                m_locked = false;
            }
        }

    private:
        T &m_mutex;
        bool m_locked;
    };

    template<class T>
    class ReadScopedLockImpl {
    public:
        ReadScopedLockImpl(T &mutex) : m_mutex(mutex) {
            m_mutex.rdlock();
            m_locked = true;
        }

        ~ReadScopedLockImpl() {
            unlock();
        }

        void lock() {
            if (!m_locked) {
                m_mutex.rdlock();
                m_locked = true;
            }
        }

        void unlock() {
            if (m_locked) {
                m_mutex.unlock();
                m_locked = false;
            }
        }

    private:
        T &m_mutex;
        bool m_locked;
    };

    template<class T>
    class WriteScopedLockImpl {
    public:
        WriteScopedLockImpl(T &mutex) : m_mutex(mutex) {
            m_mutex.wrlock();
            m_locked = true;
        }

        ~WriteScopedLockImpl() {
            unlock();
        }

        void lock() {
            if (!m_locked) {
                m_mutex.wrlock();
                m_locked = true;
            }
        }

        void unlock() {
            if (m_locked) {
                m_mutex.unlock();
                m_locked = false;
            }
        }

    private:
        T &m_mutex;
        bool m_locked;
    };

    class Mutex : NonCopyable {
    public:
        typedef ScopedLockImpl<Mutex> Lock;

        Mutex() {
            pthread_mutex_init(&m_mutex, NULL);
        }

        ~Mutex() {
            pthread_mutex_destroy(&m_mutex);
        }

        void lock() {
            pthread_mutex_lock(&m_mutex);
        }

        void unlock() {
            pthread_mutex_unlock(&m_mutex);
        }

    private:
        pthread_mutex_t m_mutex;
    };

    class RWMutex : NonCopyable {
    public:
        typedef ReadScopedLockImpl<RWMutex> ReadLock;
        typedef WriteScopedLockImpl<RWMutex> WriteLock;

        RWMutex() {
            pthread_rwlock_init(&m_lock, nullptr);
        }

        ~RWMutex() {
            pthread_rwlock_destroy(&m_lock);
        }

        void rdlock() {
            pthread_rwlock_rdlock(&m_lock);
        }

        void wrlock() {
            pthread_rwlock_wrlock(&m_lock);
        }

        void unlock() {
            pthread_rwlock_unlock(&m_lock);
        }

    private:
        pthread_rwlock_t m_lock;
    };

    class SpinLock : NonCopyable {
    public:
        typedef ScopedLockImpl<SpinLock> Lock;

        SpinLock() {
            pthread_spin_init(&m_mutex, 0);
        }

        ~SpinLock() {
            pthread_spin_destroy(&m_mutex);
        }

        void lock() {
            pthread_spin_lock(&m_mutex);
        }

        void unlock() {
            pthread_spin_unlock(&m_mutex);
        }

    private:
        pthread_spinlock_t m_mutex;
    };

    class Semaphore : NonCopyable {
    public:
        Semaphore(u_int32_t count = 0) {
            if (sem_init(&m_sem, 0, count)) {
                throw std::logic_error("sem_init error");
            }
        }

        ~Semaphore() {
            sem_destroy(&m_sem);
        }

        void wait() {
            if (sem_wait(&m_sem)) {
                throw std::logic_error("sem_wait error");
            }
        }

        void notify() {
            if (sem_post(&m_sem)) {
                throw std::logic_error("sem_post error");
            }
        }

    private:
        sem_t m_sem;
    };
}

#endif //SERVER_MUTEX_H
