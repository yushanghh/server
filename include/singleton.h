//
// Created by chenmeng on 2020/4/4.
//

#ifndef SERVER_SINGLETON_H
#define SERVER_SINGLETON_H

namespace server {
    template<class T, class X = void, int N = 0>
    class Singleton {
    public:
        /**
         * @brief 返回单例裸指针
         */
        static T *GetInstance() {
            static T v;
            return &v;
            //return &GetInstanceX<T, X, N>();
        }
    };

}

#endif //SERVER_SINGLETON_H
