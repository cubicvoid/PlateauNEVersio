#pragma once

#include <atomic>
#include <string>

namespace bogaudio {

// https://stackoverflow.com/questions/26583433/c11-implementation-of-spinlock-using-atomic
struct SpinLock {
    std::atomic_flag locked = ATOMIC_FLAG_INIT ;

    inline void lock() {
        while (locked.test_and_set(std::memory_order_acquire)) {}
    }

    inline void unlock() {
        locked.clear(std::memory_order_release);
    }
};

std::string format(const char* fmt, ...);

}
