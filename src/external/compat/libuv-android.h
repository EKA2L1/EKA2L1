#pragma once

#include <pthread.h>

// New NDK headers expose the macro even when the barrier functions are
// unavailable. libuv uses it to choose between bionic and its own barrier.
#if __ANDROID_API__ < 24
#undef PTHREAD_BARRIER_SERIAL_THREAD
#endif
