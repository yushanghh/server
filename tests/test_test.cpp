//
// Created by chenmeng on 2020/4/21.
//

#include <iostream>
#include <memory>
#include <stdio.h>

using namespace std;

class A {
public:
    A(int val) : a(val) {}

    void print() {
        cout << a << endl;
    }

private:
    int a;
};


void test1(int a[10]) {
    cout << sizeof(a) << endl;
}

void test2(int a[]) {
    cout << sizeof(a) << endl;
}

void test3(int *a) {
    cout << sizeof(a) << endl;
}

class B {
public:
    void print(int a) {
        cout << a << endl;
    }
};

int main(int argc, char *argv[]) {
    int a[6] = {1, 2, 3, 4, 5, 6};
    char array[4] = {0x12, 0x34, 0x56, 0x78};
    printf("%x\n", *(unsigned short *) &array[2]);
    test1(a);
    test2(a);
    test3(a);
    ((B *) 0)->print(4);
//    char* str = "hello";
//    str[2] = 'a';
//    cout << str << endl;

//    shared_ptr<A> sp;
//    A *aa = new A(2);
//    sp.reset(aa);
//    sp->print();
//    return 0;
}