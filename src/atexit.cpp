#include <cstdlib>
#include <pthread.h>

namespace {
    struct AtexitEntry {
        void (*fn)(void *);
        void *arg;
    };

    AtexitEntry *g_array = nullptr;
    size_t capacity = 8;
    size_t count = 0;
    pthread_mutex_t g_atexit_mutex = PTHREAD_MUTEX_INITIALIZER;
    inline void atexit_lock() {
        pthread_mutex_lock(&g_atexit_mutex);
    }

    inline void atexit_unlock() {
        pthread_mutex_unlock(&g_atexit_mutex);
    }

    extern "C" [[gnu::used]] int __cxa_atexit(void (*func)(void *), void *arg, void * /* dso */) {
        int result = -1;

        if (func != nullptr) {
            atexit_lock();
            count++;
            if (!g_array) {
                g_array = reinterpret_cast<AtexitEntry*>(malloc(capacity * sizeof(AtexitEntry)));
            }
            if (count > capacity) [[unlikely]] {
                capacity *= 2;
                g_array = reinterpret_cast<AtexitEntry*>(realloc(g_array, capacity * sizeof(AtexitEntry)));
            }
            g_array[count - 1].fn = func;
            g_array[count - 1].arg = arg;
            result = 0;
            atexit_unlock();
        }

        return result;
    }

    extern "C" [[gnu::used]] void __cxa_finalize(void * /* dso */) {
        atexit_lock();
        restart:
        if (count > 0) {
            size_t total = count;
            for (size_t i = count - 1;; --i) {
                if (g_array[i].fn == nullptr) continue;

                const AtexitEntry entry = g_array[i];
                g_array[i] = {};

                atexit_unlock();
                entry.fn(entry.arg);
                atexit_lock();

                if (count != total) {
                    goto restart;
                }
                if (i == 0) break;
            }

            free(g_array);
        }
        atexit_unlock();
    }
}
