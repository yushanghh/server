//
// Created by chenmeng on 2020/4/26.
//

#include <iostream>
#include <memory>
using namespace std;

template <typename  T>
class SharedPtr{
public:
    SharedPtr(T* ptr = nullptr) : m_t(ptr), m_count(1) {}
    SharedPtr(SharedPtr& sp) {
        ++sp.m_count;
        m_t = sp.m_t;
        m_count = sp.m_count;
    }

    SharedPtr& operator=(SharedPtr& sp){
        ++sp.m_count;
        m_count = sp.m_count;
        m_t = sp.m_t;
        return *this;
    }

    T* operator->(){
        return m_t;
    }

    T& operator*(){
        return *m_t;
    }

    int useCount() const { return m_count; }

    ~SharedPtr(){
        --m_count;
        if(m_count == 0){
            cout << "delete" << endl;
            delete(m_t);
        }
    }

    void reset(){
        --m_count;
        if(m_count == 0){
            cout << "delete" << endl;
            delete(m_t);
        }
    }

private:
    T* m_t;
    int m_count;
};

int main(int argc, char* argv[]){
//    shared_ptr<int> p1 = make_shared<int>();
//    *p1 = 78;
//    cout << *p1 << " " << p1.use_count() << endl;
//
////    shared_ptr<int> p2(p1);
//    shared_ptr<int> p2 = p1;
//    cout << *p2 << " " << p2.use_count() << endl;
//    cout << *p1 << " " << p1.use_count() << endl;
//
//    if(p1 == p2){
//        cout << "p1 is equal to p2" << endl;
//    }
//
////    p1.reset();
//    cout << *p2 << " " << p2.use_count() << endl;
//    cout << p1.use_count() << endl;
//
//    p1.reset(new int(11));
//    cout << *p2 << " " << p2.use_count() << endl;
//    cout << *p1 << " " << p1.use_count() << endl;
//
//    p1 = nullptr;
//    cout << p1.use_count() << endl;

    SharedPtr<int> p1(new int(11));
    cout << *p1 << " " << p1.useCount() << endl;
    SharedPtr<int> p2(p1);
    cout << *p1 << " " << p1.useCount() << endl;
    cout << *p2 << " " << p2.useCount() << endl;

    p1.reset();
    cout << *p1 << " " << p1.useCount() << endl;
    cout << *p2 << " " << p2.useCount() << endl;


    return 0;
}
