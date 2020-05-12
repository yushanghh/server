//
// Created by chenmeng on 2020/4/30.
//

#ifndef SERVER_TIMER_H
#define SERVER_TIMER_H

#include "thread.h"
#include <memory>

namespace server {

    class TimerManager;

    class Timer : public std::enable_shared_from_this<Timer> {
    private:
        friend class TimerManager;

    public:
        typedef std::shared_ptr<Timer> ptr;

    private:
        Timer(uint64_t ms, std::function<void()> cb, bool recurring, TimerManager *manager);

        Timer(uint64_t next);

    public:
        bool cancel();                               //取消该定时器
        bool reset(uint64_t ms, bool from_now);      //重置timer的时间间隔
        bool refresh();                              //刷新定时器的执行时间

    private:
        bool m_recurring = false;        //recurring 循环执行 执行完成之后继续执行
        uint64_t m_ms = 0;               //执行周期
        uint64_t m_next = 0;             //精确的执行时间
        std::function<void()> m_cb;
        TimerManager *m_manager = nullptr;

    private:
        struct Comparator {
            //左边小返回true，否则返回false
            bool operator()(const Timer::ptr &lhs, const Timer::ptr &rhs);
        };
    };

    class TimerManager {
    private:
        friend class Timer;

    public:
        typedef RWMutex RWMutexType;

        TimerManager();

        virtual ~TimerManager();

        Timer::ptr addTimer(uint64_t ms, std::function<void()> cb, bool recurring = false);

        Timer::ptr addConditionTimer(uint64_t ms, std::function<void()> cb,
                                     std::weak_ptr<void> weak_cond, bool recurring = false);

        uint64_t getNextTimer();

        void listExpiredCb(std::vector<std::function<void()>> &cbs);

    protected:
        virtual void onTimerInsertedAtFront() = 0;
        void addTimer(Timer::ptr val, RWMutexType::WriteLock &lock);
        bool hasTimer();             //是否还有未处理的timer

    private:
        bool detectClockRollover(uint64_t cur_ms);

    private:
        RWMutexType m_mutex;
        std::set<Timer::ptr, Timer::Comparator> m_timers;
        bool m_tickled = false;     //避免高频率触发onTimerInsertedAtFront，触发过一次之后，就不再触发
        uint64_t m_previousTime;
    };
}

#endif //SERVER_TIMER_H
