//
// Created by chenmeng on 2020/4/22.
//

#include "../include/iomanager.h"

namespace server {
    static server::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    IOManager::IOManager(size_t threads, bool use_caller, const std::string &name) :
            Scheduler(threads, use_caller, name) {
        m_epfd = epoll_create(5000);      //该参数可以忽略，大于0即可
        SYLAR_ASSERT(m_epfd > 0);

        int ret = pipe(m_tickleFds);
        SYLAR_ASSERT(ret == 0);

        epoll_event event;
        memset(&event, 0, sizeof(epoll_event));
        event.events = EPOLLIN | EPOLLET;
        event.data.fd = m_tickleFds[0];        //管道的读端

        ret = fcntl(m_tickleFds[0], F_SETFL, O_NONBLOCK);
        SYLAR_ASSERT(ret == 0);

        ret = epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_tickleFds[0], &event);
        SYLAR_ASSERT(ret == 0);

        contextResize(32);
        start();
    }

    IOManager::~IOManager() {
        stop();
        close(m_epfd);
        close(m_tickleFds[0]);
        close(m_tickleFds[1]);
        for (size_t i = 0; i < m_fdContexts.size(); ++i) {
            if (m_fdContexts[i])
                delete m_fdContexts[i];
        }
    }

    int IOManager::addEvent(int fd, Event event, std::function<void()> cb) {
        FdContext *fd_ctx = nullptr;
        RWMutexType::ReadLock r_lock(m_mutex);
        if (m_fdContexts.size() > fd) {
            fd_ctx = m_fdContexts[fd];
            r_lock.unlock();
        } else {
            r_lock.unlock();
            RWMutexType::WriteLock w_lock(m_mutex);
            contextResize(fd * 1.5);
            fd_ctx = m_fdContexts[fd];
            w_lock.unlock();
        }
        FdContext::MutexType::Lock lock2(fd_ctx->mutex);
        if (fd_ctx->events & event) {           //加同一个事件，事件是用位图来表示的，说明有多个线程在访问
            SYLAR_LOG_ERROR(g_logger) << "addEvent assert fd" << fd
                                      << " event=" << event
                                      << " fd_ctx.events=" << fd_ctx->events;
            SYLAR_ASSERT(!(fd_ctx->events & event));
        }
        //events为0，说明该fd上没有任何事件，可以删除
        int op = fd_ctx->events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
        epoll_event ep_event;
        ep_event.events = EPOLLET | fd_ctx->events | event;
        ep_event.data.ptr = fd_ctx;

        int ret = epoll_ctl(m_epfd, op, fd, &ep_event);
        if (ret) {
            SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
                                      << op << ", " << ep_event.events << "):"
                                      << ret << " (" << errno << ") (" << strerror(errno) << ")";
            return -1;
        }
        ++m_pendingEventCount;
        fd_ctx->events = (Event) (fd_ctx->events | event);
        FdContext::EventContext &event_ctx = fd_ctx->getContext(event);
        SYLAR_ASSERT(!event_ctx.scheduler && !event_ctx.fiber && !event_ctx.cb);
        event_ctx.scheduler = Scheduler::GetThis();
        if (cb) {
            event_ctx.cb.swap(cb);
        } else {
            event_ctx.fiber = Fiber::GetThis();
            SYLAR_ASSERT(event_ctx.fiber->getState() == Fiber::EXEC);
        }
        return 0;
    }


    bool IOManager::deleteEvent(int fd, Event event) {
        RWMutexType::ReadLock r_lock(m_mutex);
        if (m_fdContexts.size() <= fd) {
            return false;                      //该fd不存在，fd就是数组的索引
        }
        FdContext *fd_ctx = m_fdContexts[fd];
        r_lock.unlock();

        FdContext::MutexType::Lock lock(fd_ctx->mutex);
        if (!(fd_ctx->events & event)) {
            return false;                      //该fd上没有该事件
        }

        Event new_events = (Event) (fd_ctx->events & ~event);
        int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
        epoll_event ep_event;
        ep_event.events = new_events | EPOLLET;
        ep_event.data.ptr = fd_ctx;

        int ret = epoll_ctl(m_epfd, op, fd, &ep_event);
        if (ret) {
            SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
                                      << op << ", " << ep_event.events << "):"
                                      << ret << " (" << errno << ") (" << strerror(errno) << ")";
            return false;
        }
        fd_ctx->events = new_events;
        FdContext::EventContext &event_ctx = fd_ctx->getContext(event);
        fd_ctx->resetContext(event_ctx);
        --m_pendingEventCount;
        lock.unlock();
        return true;
    }

    bool IOManager::cancelEvent(int fd, Event event) {
        RWMutexType::ReadLock r_lock(m_mutex);
        if (m_fdContexts.size() <= fd) {
            return false;                      //该fd不存在，fd就是数组的索引
        }
        FdContext *fd_ctx = m_fdContexts[fd];
        r_lock.unlock();

        FdContext::MutexType::Lock lock(fd_ctx->mutex);
        if (!(fd_ctx->events & event)) {
            return false;                      //该fd上没有该事件
        }

        Event new_events = (Event) (fd_ctx->events & ~event);
        int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
        epoll_event ep_event;
        ep_event.events = new_events | EPOLLET;
        ep_event.data.ptr = fd_ctx;

        int ret = epoll_ctl(m_epfd, op, fd, &ep_event);
        if (ret) {
            SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
                                      << op << ", " << ep_event.events << "):"
                                      << ret << " (" << errno << ") (" << strerror(errno) << ")";
            return false;
        }
        //强制触发
        fd_ctx->triggerEvent(event);
        --m_pendingEventCount;
        lock.unlock();
        return true;
    }

    bool IOManager::cancelAll(int fd) {
        RWMutexType::ReadLock r_lock(m_mutex);
        if (m_fdContexts.size() <= fd) {
            return false;
        }
        r_lock.unlock();
        FdContext *fd_ctx = m_fdContexts[fd];
        FdContext::MutexType::Lock lock(fd_ctx->mutex);
        if (!fd_ctx->events) {
            return false;
        }

        int op = EPOLL_CTL_DEL;
        epoll_event ep_event;
        ep_event.events = 0;
        ep_event.data.ptr = fd_ctx;
        int ret = epoll_ctl(m_epfd, op, fd, &ep_event);
        if (ret) {
            SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
                                      << op << ", " << ep_event.events << "):"
                                      << ret << " (" << errno << ") (" << strerror(errno) << ")";
            return false;
        }
        if (fd_ctx->events & READ) {
            fd_ctx->triggerEvent(READ);
            --m_pendingEventCount;
        }
        if (fd_ctx->events & WRITE) {
            fd_ctx->triggerEvent(WRITE);
            --m_pendingEventCount;
        }
        SYLAR_ASSERT(fd_ctx->events == 0);
        lock.unlock();
        return true;
    }

    void IOManager::contextResize(size_t size) {
        m_fdContexts.resize(size);
        for (size_t i = 0; i < m_fdContexts.size(); ++i) {
            if (!m_fdContexts[i]) {
                m_fdContexts[i] = new FdContext;
                m_fdContexts[i]->fd = i;
            }
        }
    }

    IOManager *IOManager::GetThis() {
        return dynamic_cast<IOManager *>(Scheduler::GetThis());
    }

    bool IOManager::stopping() {
        return m_pendingEventCount == 0 && Scheduler::stopping();
    }

    void IOManager::tickle() {
        if (!hasIdleThreads()) {
            return;
        }
        int ret = write(m_tickleFds[1], "T", 1);
        SYLAR_LOG_INFO(g_logger) << "tickle";
        SYLAR_ASSERT(ret == 1);
    }

    void IOManager::idle() {
        epoll_event *events = new epoll_event[64]();    //加了()在分配内存的同时会进行初始化
        std::shared_ptr<epoll_event> shared_events(events,
                                                   [](epoll_event *ptr) -> void { delete[] ptr; });
        while (true) {
            uint64_t next_timeout = 0;
            if (stopping()) {
                next_timeout = getNextTimer();
                if (next_timeout == ~0ull) {
                    SYLAR_LOG_INFO(g_logger) << "name=" << getName()
                                             << " idle stopping exit";
                    break;
                }
            }

            int ret = 0;
            do {
                static const int MAX_TIMEOUT = 1000;
                if (next_timeout != ~0ull) {
                    next_timeout = (int) next_timeout > MAX_TIMEOUT ? MAX_TIMEOUT : next_timeout;
                } else {
                    next_timeout = MAX_TIMEOUT;
                }

                ret = epoll_wait(m_epfd, events, 64, (int) next_timeout);
//                SYLAR_LOG_DEBUG(g_logger) << "epoll wait ret=" << ret << " next timeout=" << next_timeout;
                if (ret < 0 && errno == EINTR) {
                } else {
                    break;
                }
            } while (true);

            //将所有超时的timer全部进行调度
            std::vector<std::function<void()>> cbs;
            listExpiredCb(cbs);
            if (!cbs.empty()) {
                SYLAR_LOG_DEBUG(g_logger) << "expired cb size=" << cbs.size();
                schedule(cbs.begin(), cbs.end());
                cbs.clear();
            }

            for (int i = 0; i < ret; ++i) {
                epoll_event &event = events[i];
                if (event.data.fd == m_tickleFds[0]) {
                    uint8_t dummy;
                    while (read(m_tickleFds[0], &dummy, 1) == 1);
                    continue;
                }
                FdContext *fd_ctx = (FdContext *) event.data.ptr;
                FdContext::MutexType::Lock lock(fd_ctx->mutex);
                if (event.events & (EPOLLERR | EPOLLHUP)) {       //epoll出错和epoll中断
                    event.events |= EPOLLIN | EPOLLOUT;
                }
                int real_events = NONE;
                if (event.events & EPOLLIN) {
                    real_events |= READ;
                }
                if (event.events & EPOLLOUT) {
                    real_events |= WRITE;
                }
                if ((fd_ctx->events & real_events) == NONE) {     //被其它人处理了
                    continue;
                }
                int left_events = (fd_ctx->events & ~real_events);      //剩余的事件
                int op = left_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
                event.events = EPOLLET & left_events;
                //如果还有剩余的未处理的事件，则将其重新加入到epoll监听中
                int ret2 = epoll_ctl(m_epfd, op, fd_ctx->fd, &event);
                if (ret2) {
                    SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", " << op << ", "
                                              << fd_ctx->fd << ", " << event.events << "):"
                                              << ret2 << " (" << errno << ") (" << strerror(errno) << ")";
                    continue;
                }

                if (real_events & READ) {
                    fd_ctx->triggerEvent(READ);
                    --m_pendingEventCount;
                }
                if (real_events & WRITE) {
                    fd_ctx->triggerEvent(WRITE);
                    --m_pendingEventCount;
                }
            }
            Fiber::ptr cur = Fiber::GetThis();
            auto raw_ptr = cur.get();
            cur.reset();
            raw_ptr->swapOut();
        }
    }

    void IOManager::onTimerInsertedAtFront() {
        tickle();
    }

    IOManager::FdContext::EventContext &IOManager::FdContext::getContext(Event event) {
        switch (event) {
            case IOManager::READ:
                return read;
            case IOManager::WRITE:
                return write;
            default:
                SYLAR_ASSERT2(false, "getContext");
                break;
        }
    }

    void IOManager::FdContext::resetContext(EventContext &ctx) {
        ctx.scheduler = nullptr;
        ctx.fiber.reset();
        ctx.cb = nullptr;
    }

    void IOManager::FdContext::triggerEvent(Event event) {
        SYLAR_ASSERT(events & event);
        events = (Event) (events & ~event);
        EventContext &ctx = getContext(event);
        if (ctx.cb) {
            ctx.scheduler->schedule(&ctx.cb);
        } else {
            ctx.scheduler->schedule(&ctx.fiber);
        }
        ctx.scheduler = nullptr;
    }
}