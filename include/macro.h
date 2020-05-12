//
// Created by chenmeng on 2020/4/4.
//

#ifndef SERVER_MACRO_H
#define SERVER_MACRO_H

#include "../include/util.h"
#include <assert.h>
#include <iostream>

#   define SYLAR_LIKELY(x)      (x)
#   define SYLAR_UNLIKELY(x)      (x)

//断言宏
#define SYLAR_ASSERT(x) \
    if(SYLAR_UNLIKELY(!(x))) { \
        std::cout << "ASSERTION: " #x \
            << "\nbacktrace:\n" \
            << server::BacktraceToString(100, 2, "    "); \
        assert(x); \
    }

#define SYLAR_ASSERT2(x, w) \
    if(SYLAR_UNLIKELY(!(x))) { \
        std::cout << "ASSERTION: " #x \
            << "\n" << w \
            << "\nbacktrace:\n" \
            << server::BacktraceToString(100, 2, "    "); \
        assert(x); \
    }

#endif //SERVER_MACRO_H
