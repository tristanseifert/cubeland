#include "Thread.h"

#include <thread>

using namespace util;

#ifdef _WIN32
#include <windows.h>

/// Windows set thread name implementation
void Thread::setName(const char* threadName) {
    SetThreadName(GetCurrentThreadId(),threadName);
}

#elif defined(__linux__)
#include <sys/prctl.h>
/// Linux method to set thread name
void Thread::setName(const char* threadName) {
    prctl(PR_SET_NAME,threadName,0,0,0);
}

#else
#include <pthread.h>

/// POSIX version of setting the thread for all other platforms
void Thread::setName(const char* threadName) {
   pthread_setname_np(threadName);
}
#endif
