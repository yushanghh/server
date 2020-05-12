//
// Created by chenmeng on 2020/4/4.
//
#include "../include/util.h"

namespace server {
    pid_t GetThreadId() {
        return syscall(SYS_gettid);
    }

    uint64_t GetFiberId() {
        return server::Fiber::GetFiberId();
    }

    uint64_t GetCurrentMS() {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return tv.tv_sec * 1000ul + tv.tv_usec / 1000;
    }

    uint64_t GetCurrentUS() {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return tv.tv_sec * 1000 * 1000ul + tv.tv_usec;
    };

    void Backtrace(std::vector<std::string> &bt, int size, int skip) {
        void **array = (void **) malloc((sizeof(void *) * size));
        size_t s = ::backtrace(array, size);

        char **strings = backtrace_symbols(array, s);
        if (strings == NULL) {
//            SYLAR_LOG_ERROR(g_logger) << "backtrace_synbols error";
            return;
        }

        for (size_t i = skip; i < s; ++i) {
            bt.push_back(strings[i]);
        }
        free(strings);
        free(array);
    }

    std::string BacktraceToString(int size, int skip, const std::string &prefix) {
        std::vector<std::string> bt;
        Backtrace(bt, size, skip);
        std::stringstream ss;
        for (size_t i = 0; i < bt.size(); ++i) {
            ss << prefix << bt[i] << std::endl;
        }
        return ss.str();
    }

    void FSUtil::ListAllFile(std::vector<std::string> &files, const std::string &path, const std::string &sub_fix) {
        if (access(path.c_str(), 0) != 0) {    //路径不存在
            return;
        }
        DIR *dir = opendir(path.c_str());
        if (dir == nullptr) {
            return;
        }
        struct dirent *dp = nullptr;
        while ((dp = readdir(dir)) != nullptr) {
            if (dp->d_type == DT_DIR) {
                if (!strcmp(dp->d_name, ".")
                    || !strcmp(dp->d_name, "..")) {
                    continue;
                }
                ListAllFile(files, path + "/" + dp->d_name, sub_fix);
            } else if (dp->d_type == DT_REG) {
                std::string file_name(dp->d_name);
                if (sub_fix.empty()) {
                    files.push_back(path + "/" + file_name);
                } else {
                    if (file_name.size() < sub_fix.size())
                        continue;
                    if (file_name.substr(file_name.size() - sub_fix.size()) == sub_fix)
                        files.push_back(path + "/" + file_name);
                }
            }
        }
        closedir(dir);
    }

    static int __lstat(const char *file, struct stat *st = nullptr) {
        struct stat lst;
        int ret = lstat(file, &lst);
        if (st)
            *st = lst;
        return ret;
    }

    static int __mkdir(const char *dir_name) {
        if (access(dir_name, F_OK) == 0)
            return 0;
        return mkdir(dir_name, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    }

    bool FSUtil::Mkdir(const std::string &dirname) {
        if (__lstat(dirname.c_str()) == 0) {
            return true;
        }
        char *path = strdup(dirname.c_str());
        char *ptr = strchr(path + 1, '/');
        do {
            for (; ptr; *ptr = '/', ptr = strchr(ptr + 1, '/')) {
                *ptr = '\0';
                if (__mkdir(path) != 0) {
                    break;
                }
            }
            if (ptr != nullptr) {
                break;
            } else if (__mkdir(path) != 0) {
                break;
            }
            free(path);
            return true;
        } while (0);
        free(path);
        return false;
    }

    bool FSUtil::IsRunningPidFile(const std::string &pidfile) {
        if (__lstat(pidfile.c_str()) != 0) {
            return false;
        }
        std::ifstream ifs(pidfile);
        std::string line;
        if (!ifs || !std::getline(ifs, line)) {
            return false;
        }
        if (line.empty()) {
            return false;
        }
        pid_t pid = atoi(line.c_str());
        if (pid <= 1) {
            return false;
        }
        if (kill(pid, 0) != 0) {
            return false;
        }
        return true;
    }

    bool FSUtil::Unlink(const std::string &filename, bool exist) {
        if (!exist && __lstat(filename.c_str())) {
            return true;
        }
        return ::unlink(filename.c_str()) == 0;
    }

    bool FSUtil::Rm(const std::string &path) {
        struct stat st;
        if (lstat(path.c_str(), &st)) {
            return true;
        }
        if (!(st.st_mode & S_IFDIR)) {
            return Unlink(path);
        }

        DIR *dir = opendir(path.c_str());
        if (!dir) {
            return false;
        }

        bool ret = true;
        struct dirent *dp = nullptr;
        while ((dp = readdir(dir))) {
            if (!strcmp(dp->d_name, ".")
                || !strcmp(dp->d_name, "..")) {
                continue;
            }
            std::string dirname = path + "/" + dp->d_name;
            ret = Rm(dirname);
        }
        closedir(dir);
        if (::rmdir(path.c_str())) {
            ret = false;
        }
        return ret;
    }

    bool FSUtil::Mv(const std::string &from, const std::string &to) {
        if (!Rm(to)) {
            return false;
        }
        return rename(from.c_str(), to.c_str()) == 0;
    }

    bool FSUtil::RealPath(const std::string &path, std::string &rpath) {
        if (__lstat(path.c_str())) {
            return false;
        }
        char *ptr = ::realpath(path.c_str(), nullptr);
        if (nullptr == ptr) {
            return false;
        }
        std::string(ptr).swap(rpath);
        free(ptr);
        return true;
    }

    bool FSUtil::Symlink(const std::string &from, const std::string &to) {
        if (!Rm(to)) {
            return false;
        }
        return ::symlink(from.c_str(), to.c_str()) == 0;
    }

    std::string FSUtil::Dirname(const std::string &filename) {
        if (filename.empty()) {
            return ".";
        }
        auto pos = filename.rfind('/');
        if (pos == 0) {
            return "/";
        } else if (pos == std::string::npos) {
            return ".";
        } else {
            return filename.substr(0, pos);
        }
    }

    std::string FSUtil::Basename(const std::string &filename) {
        if (filename.empty()) {
            return filename;
        }
        auto pos = filename.rfind('/');
        if (pos == std::string::npos) {
            return filename;
        } else {
            return filename.substr(pos + 1);
        }
    }

    bool FSUtil::OpenForRead(std::ifstream &ifs, const std::string &filename, std::ios_base::openmode mode) {
        ifs.open(filename.c_str(), mode);
        return ifs.is_open();
    }

    bool FSUtil::OpenForWrite(std::ofstream &ofs, const std::string &filename, std::ios_base::openmode mode) {
        ofs.open(filename.c_str(), mode);
        if (!ofs.is_open()) {
            std::string dir = Dirname(filename);
            Mkdir(dir);
            ofs.open(filename.c_str(), mode);
        }
        return ofs.is_open();
    }

}
