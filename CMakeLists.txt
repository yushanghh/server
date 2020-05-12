cmake_minimum_required(VERSION 3.10)
project(server)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS -g)

#-fno-stack-protector

include_directories(.)
include_directories(include/)

set(LIB_SRC
        src/util.cpp
        src/log.cpp
        src/fiber.cpp
        src/thread.cpp
        src/scheduler.cpp
        src/iomanager.cpp
        src/timer.cpp
        src/hook.cpp)

add_library(server SHARED ${LIB_SRC})

find_library(PTHREAD pthread)

set(LIBS
        server
        ${PTHREAD})

add_executable(test_thread tests/test_thread.cpp)
add_dependencies(test_thread server)
target_link_libraries(test_thread ${LIBS})

add_executable(test_fiber tests/test_fiber.cpp)
add_dependencies(test_thread server)
target_link_libraries(test_fiber ${LIBS})

add_executable(test_scheduler tests/test_scheduler.cpp)
add_dependencies(test_scheduler server)
target_link_libraries(test_scheduler ${LIBS})

add_executable(test_iomanager tests/test_iomanager.cpp)
add_dependencies(test_iomanager server)
target_link_libraries(test_iomanager ${LIBS})

add_executable(test_test tests/test_test.cpp)

add_executable(test_ptr tests/test_ptr.cpp)


#add_executable(test_thread tests/test_thread.cpp src/fiber.cpp src/thread.cpp src/util.cpp src/log.cpp include/server.h include/mutex.h include/noncopyable.h)
#add_executable(test_fiber tests/test_fiber.cpp src/fiber.cpp src/util.cpp include/scheduler.h include/thread.h include/mutex.h src/thread.cpp include/server.h)
