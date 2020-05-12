//
// Created by chenmeng on 2020/4/4.
//

#ifndef SERVER_NONCOPYABLE_H
#define SERVER_NONCOPYABLE_H

namespace server {
    /* 构造函数公有
     * 拷贝构造函数私有
     * 拷贝赋值函数私有
     * */
    class NonCopyable {
    public:
        NonCopyable() = default;

        ~NonCopyable() = default;

    private:
        NonCopyable(const NonCopyable &) {};

        NonCopyable &operator=(const NonCopyable &) {};
    };
}

#endif //SERVER_NONCOPYABLE_H
