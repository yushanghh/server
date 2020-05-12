//
// Created by chenmeng on 2020/4/4.
//

#ifndef SERVER_UTIL_H
#define SERVER_UTIL_H

#include "../include/fiber.h"
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <dirent.h>
#include <pthread.h>
#include <unistd.h>
#include <execinfo.h>
#include <string.h>
#include <signal.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <string>
#include <fstream>

namespace server {
    pid_t GetThreadId();       //获取当前线程ID
    uint64_t GetFiberId();     //获取当前协程ID
    void Backtrace(std::vector<std::string> &bt, int size = 64, int skip = 1);  //获取当前函数调用堆栈
    std::string BacktraceToString(int size = 64, int skip = 2, const std::string &prefix = "");
    uint64_t GetCurrentMS();
    uint64_t GetCurrentUS();

    class FSUtil {
    public:
        static void ListAllFile(std::vector<std::string> &files,
                                const std::string &path,
                                const std::string &sub_fix);

        static bool Mkdir(const std::string &dir_name);

        static bool IsRunningPidFile(const std::string &pid_file);

        static bool Rm(const std::string &path);

        static bool Mv(const std::string &from, const std::string &to);

        static bool RealPath(const std::string &path, std::string &rpath);

        static bool Symlink(const std::string &frm, const std::string &to);

        static bool Unlink(const std::string &file_name, bool exist = false);

        static std::string Dirname(const std::string &file_name);

        static std::string Basename(const std::string &file_name);

        static bool OpenForRead(std::ifstream &ifs, const std::string &file_name, std::ios_base::openmode mode);

        static bool OpenForWrite(std::ofstream &ofs, const std::string &file_name, std::ios_base::openmode mode);
    };


}

#endif //SERVER_UTIL_H
