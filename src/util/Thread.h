#ifndef UTIL_THREAD_H
#define UTIL_THREAD_H

#include <string>

namespace util {
class Thread {
    public:
        /// Sets the thread's name.
        static void setName(const std::string &str) {
            setName(str.c_str());
        }
        /// Sets the thread's name.
        static void setName(const char *str);
};
};

#endif
