//
// Created by chenmeng on 2020/4/4.
//

#include "../include/fiber.h"
#include "../include/scheduler.h"

namespace server {

    static Logger::ptr g_logger = SYLAR_LOG_NAME("system");
    static std::atomic<uint64_t> s_fiber_id{0};
    static std::atomic<uint64_t> s_fiber_count{0};

    static const int g_fiber_stack_size = 128 * 1024;

    static thread_local Fiber *t_fiber = nullptr;              //线程中当前正在执行的协程
    static thread_local Fiber::ptr t_threadFiber = nullptr;    //线程的主协程

    class MallocStackAllocator {
    public:
        static void *allocate(size_t size) {
            return malloc(size);
        }

        static void deallocate(void *p) {
            return free(p);
        }
    };

    uint64_t Fiber::GetFiberId() {
        if (t_fiber) {
            return t_fiber->getId();
        }
        return 0;
    }

    //主协程，将当前线程当作第一个协程，也就是主协程
    Fiber::Fiber() {
        m_state = EXEC;
        SetThis(this);
        if (getcontext(&m_ctx)) {                          //获取当前线程的上下文状态
            SYLAR_ASSERT2(false, "getcontext");
        }
        ++s_fiber_count;
        SYLAR_LOG_DEBUG(g_logger) << "Fiber::Fiber main";
    }

    Fiber::Fiber(std::function<void()> cb, size_t stack_size, bool use_caller)
            : m_id(++s_fiber_id), m_cb(cb) {
        ++s_fiber_count;
        m_stack_size = stack_size > 0 ? stack_size : g_fiber_stack_size;
        m_stack = MallocStackAllocator::allocate(m_stack_size);
        if (getcontext(&m_ctx)) {
            SYLAR_ASSERT2(false, "getcontext");
        }
        m_ctx.uc_link = nullptr;                 //上下文关联的指针
        m_ctx.uc_stack.ss_sp = m_stack;
        m_ctx.uc_stack.ss_size = m_stack_size;
        //如果use_caller设置为true，那么当前线程的执行函数被视为一个协程，此外还有一个专门负责执行调度的协程
        //普通的协程切换是从执行协程切换到负责调度的协程，然后再从负责调度的协程切换出去
        //当负责调度的协程执行结束之后，如果use_caller设置为true，需要切换到线程的执行函数中
        if (!use_caller){
            makecontext(&m_ctx, &Fiber::MainFunc, 0);   //创建函数执行的上下文，便于切换
        } else {
            makecontext(&m_ctx, &Fiber::CallerMainFunc, 0);   //创建函数执行的上下文，便于切换
        }
        SYLAR_LOG_DEBUG(g_logger) << "Fiber::Fiber sub fiber, fiber id=" << m_id;
    }

    Fiber::~Fiber() {
        --s_fiber_count;
        if (m_stack) {
            SYLAR_ASSERT(m_state == TERM
                         || m_state == EXCEPT
                         || m_state == INIT);
            MallocStackAllocator::deallocate(m_stack);
        } else {
            //主协程
            SYLAR_ASSERT(!m_cb);
            SYLAR_ASSERT(m_state == EXEC);    //?

            Fiber *cur = t_fiber;
            if (cur == this) {
                SetThis(nullptr);
            }
        }

        SYLAR_LOG_DEBUG(g_logger) << "Fiber::~Fiber id = " << m_id << " total= "
                                  << s_fiber_count;
    }

    void Fiber::setState(State state) {
        m_state = state;
    }

    //重置协程函数，之前的函数执行完成之后，可以继 续执行下一个函数
    void Fiber::reset(std::function<void()> cb) {
        SYLAR_ASSERT(m_stack);
        SYLAR_ASSERT(m_state == TERM
                     || m_state == EXCEPT
                     || m_state == INIT);
        m_cb = cb;
        if (getcontext(&m_ctx)) {
            SYLAR_ASSERT2(false, "getcontext");
        }

        m_ctx.uc_link = nullptr;
        m_ctx.uc_stack.ss_sp = m_stack;
        m_ctx.uc_stack.ss_size = m_stack_size;
        makecontext(&m_ctx, &Fiber::MainFunc, 0);
        m_state = INIT;
    }

    //将当前正在运行的协程切换到后台，然后运行自己
    void Fiber::swapIn() {
        SetThis(this);
        SYLAR_ASSERT(m_state != EXEC);
        m_state = EXEC;
        if (swapcontext(&Scheduler::GetMainFiber()->m_ctx, &m_ctx)) {
            SYLAR_ASSERT2(false, "swap context");
        }
    }

    void Fiber::swapOut() {
        SetThis(Scheduler::GetMainFiber());
        if (swapcontext(&m_ctx, &Scheduler::GetMainFiber()->m_ctx)) {
            SYLAR_ASSERT2(false, "swap context");
        }
    }

    void Fiber::call() {
        SetThis(this);
        m_state = EXEC;
        if (swapcontext(&t_threadFiber->m_ctx, &m_ctx)) {
            SYLAR_ASSERT2(false, "swap context");
        }
    }

    void Fiber::back() {
        SetThis(t_threadFiber.get());
        if (swapcontext(&m_ctx, &t_threadFiber->m_ctx)) {
            SYLAR_ASSERT2(false, "swap context");
        }
    }

    void Fiber::SetThis(Fiber *f) {
        t_fiber = f;
    }

    //如果线程当前没有创建协程就
    Fiber::ptr Fiber::GetThis() {
        if (t_fiber) {
            return t_fiber->shared_from_this();
        }
        Fiber::ptr main_fiber(new Fiber);
        SYLAR_ASSERT(t_fiber == main_fiber.get());
        t_threadFiber = main_fiber;
        return t_fiber->shared_from_this();
    }

    void Fiber::YieldToReady() {
        Fiber::ptr cur = GetThis();
        cur->m_state = READY;
        cur->swapOut();
    }

    void Fiber::YieldToHold() {
        Fiber::ptr cur = GetThis();
        cur->m_state = HOLD;
        cur->swapOut();
    }

    uint64_t Fiber::TotalFibers() {
        return s_fiber_count;
    }

    void Fiber::MainFunc() {
        Fiber::ptr cur = GetThis();
        SYLAR_ASSERT(cur);
        try {
            cur->m_cb();
            cur->m_cb = nullptr;
            cur->m_state = TERM;
        } catch (std::exception &ex) {
            cur->m_state = EXCEPT;
            //打印出错信息
            SYLAR_LOG_ERROR(g_logger) << "Fiber Except: " << ex.what()
                                      << " fiber_id=" << cur->getId()
                                      << std::endl
                                      << server::BacktraceToString();
        } catch (...) {
            cur->m_state = EXCEPT;
            SYLAR_LOG_ERROR(g_logger) << "Fiber Except"
                                      << " fiber_id=" << cur->getId()
                                      << std::endl
                                      << server::BacktraceToString();
        }
        auto raw_ptr = cur.get();
        cur.reset();
        raw_ptr->swapOut();

        SYLAR_ASSERT2(false, "never reach fiber_id=" + std::to_string(raw_ptr->getId()));
    }

    void Fiber::CallerMainFunc() {
        Fiber::ptr cur = GetThis();
        SYLAR_ASSERT(cur);
        try {
            cur->m_cb();
            cur->m_cb = nullptr;
            cur->m_state = TERM;
        } catch (std::exception &ex) {
            cur->m_state = EXCEPT;
            //打印出错信息
            SYLAR_LOG_ERROR(g_logger) << "Fiber Except: " << ex.what()
                                      << " fiber_id=" << cur->getId()
                                      << std::endl
                                      << server::BacktraceToString();
        } catch (...) {
            cur->m_state = EXCEPT;
            SYLAR_LOG_ERROR(g_logger) << "Fiber Except"
                                      << " fiber_id=" << cur->getId()
                                      << std::endl
                                      << server::BacktraceToString();
        }
        auto raw_ptr = cur.get();
        cur.reset();
        raw_ptr->back();

        SYLAR_ASSERT2(false, "never reach fiber_id=" + std::to_string(raw_ptr->getId()));
    }
}