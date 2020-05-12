//
// Created by chenmeng on 2020/4/30.
//

#include "../include/timer.h"

namespace server {

    bool Timer::Comparator::operator()(const Timer::ptr &lhs, const Timer::ptr &rhs) {
        if (!lhs && !rhs) {
            return false;
        }
        if (!lhs) {
            return true;
        }
        if (!rhs) {
            return false;
        }
        if (lhs->m_ms < rhs->m_ms) {
            return true;
        } else if (lhs->m_ms > rhs->m_ms) {
            return false;
        } else {
            return lhs.get() < rhs.get();
        }
    }

    Timer::Timer(uint64_t ms, std::function<void()> cb, bool recurring, TimerManager *manager)
            : m_ms(ms), m_cb(cb), m_recurring(recurring), m_manager(manager) {
        m_next = server::GetCurrentMS() + m_ms;
    }

    Timer::Timer(uint64_t next) : m_next(next) {

    }

    bool Timer::cancel() {
        TimerManager::RWMutexType::WriteLock lock();
        if (m_cb) {
            m_cb = nullptr;
            auto it = m_manager->m_timers.find(shared_from_this());
            m_manager->m_timers.erase(it);
            return true;
        }
        return false;
    }

    bool Timer::refresh() {
        TimerManager::RWMutexType::WriteLock lock(m_manager->m_mutex);
        if (!m_cb) {
            return false;
        }
        auto it = m_manager->m_timers.find(shared_from_this());
        if (it == m_manager->m_timers.end()) {
            return false;
        }
        m_manager->m_timers.erase(it);
        m_next = server::GetCurrentMS() + m_ms;
        m_manager->m_timers.insert(shared_from_this());
        return true;
    }

    bool Timer::reset(uint64_t ms, bool from_now) {
        if(ms == m_ms && !from_now){
            return true;
        }
        TimerManager::RWMutexType::WriteLock lock(m_manager->m_mutex);
        auto it = m_manager->m_timers.find(shared_from_this());
        if(it == m_manager->m_timers.end()){
            return false;
        }
        m_manager->m_timers.erase(it);
        uint64_t start = 0;
        if (from_now){
            start = server::GetCurrentMS();
        } else {
           start = m_next - m_ms;
        }
        m_ms = ms;
        m_next = start + m_ms;
        m_manager->addTimer(shared_from_this(), lock);
    }

    TimerManager::TimerManager() {
        m_previousTime = server::GetCurrentMS();
    }

    TimerManager::~TimerManager() {

    }

    Timer::ptr TimerManager::addTimer(uint64_t ms, std::function<void()> cb, bool recurring) {
        Timer::ptr timer(new Timer(ms, cb, recurring, this));
        RWMutexType::WriteLock lock(m_mutex);
        auto it = m_timers.insert(timer).first;
        bool at_front = (it == m_timers.begin()); //插入的刚好在最前面，需要尝试唤醒
        //wait的时间太长了，唤醒IOManager去修改wait的时间
        lock.unlock();
        if (at_front) {
            onTimerInsertedAtFront();
        }
        return timer;                            //可以取消该定时器
    }

    void TimerManager::addTimer(Timer::ptr val, RWMutexType::WriteLock &lock) {
        auto it = m_timers.insert(val).first;
        bool at_front = (it == m_timers.begin()) && !m_tickled;
        if(at_front){
            m_tickled = true;
        }
        lock.unlock();
        if(at_front){
            onTimerInsertedAtFront();
        }
    }

    bool TimerManager::hasTimer() {
        RWMutexType::ReadLock lock(m_mutex);
        return !m_timers.empty();
    }

    static void OnTimer(std::weak_ptr<void> weak_cond, std::function<void()> cb) {
        std::shared_ptr<void> tmp = weak_cond.lock();  //如果没有释放可以得到智能指针，否则为空
        if (tmp) {
            cb();
        }
    }

    Timer::ptr TimerManager::addConditionTimer(uint64_t ms, std::function<void()> cb, std::weak_ptr<void> weak_cond,
                                               bool recurring) {
        return addTimer(ms, std::bind(&OnTimer, weak_cond, cb), recurring);
    }

    uint64_t TimerManager::getNextTimer() {
        RWMutexType::ReadLock lock(m_mutex);
        m_tickled = false;
        if (m_timers.empty()) {
            return ~0ull;
        }
        const Timer::ptr &next = *m_timers.begin();
        uint64_t cur_ms = server::GetCurrentMS();
        if (cur_ms >= next->m_ms) {
            return 0;
        } else {
            return (next->m_ms - cur_ms);
        }
    }

    void TimerManager::listExpiredCb(std::vector<std::function<void()>> &cbs) {
        uint64_t cur_ms = server::GetCurrentMS();
        std::vector<Timer::ptr> expired;          //已经超时的定时器
        {
            RWMutexType::ReadLock lock(m_mutex);
            if (m_timers.empty()) {
                return;
            }
        }
        RWMutexType::WriteLock lock(m_mutex);

        bool rollover = detectClockRollover(cur_ms);
        if(!rollover && (*m_timers.begin())->m_next > cur_ms){
            return;
        }

        Timer::ptr cur_timer(new Timer(cur_ms));
        //将所有timer设为超时
        auto iter = rollover ? m_timers.end() : m_timers.lower_bound(cur_timer);
        while (iter != m_timers.end() && (*iter)->m_next == cur_ms) {
            ++iter;
        }
        expired.insert(expired.begin(), m_timers.begin(), iter);
        m_timers.erase(m_timers.begin(), iter);
        cbs.reserve(expired.size());

        for (auto &timer : expired) {
            cbs.push_back(timer->m_cb);
            if (timer->m_recurring) {
                timer->m_next = cur_ms + timer->m_ms;
                m_timers.insert(timer);
            } else {
                timer->m_cb = nullptr;
            }
        }
    }

    bool TimerManager::detectClockRollover(uint64_t cur_ms) {
        bool rollover = false;
        if(cur_ms < m_previousTime && cur_ms < (m_previousTime - 60 * 60 * 1000)){
            rollover = true;
        }
        m_previousTime = cur_ms;
        return rollover;
    }


}