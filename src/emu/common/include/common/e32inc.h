#pragma once

#include <common/platform.h>

#ifdef _MSC_VER
#define __VC32__
#else
#define __GCC32__
#endif

#if EKA2L1_PLATFORM(WIN32)
#undef _FOFF
#endif

#define SYMBIAN_ENABLE_SPLIT_HEADERS
#define SYMBIAN_ENABLE_PUBLIC_PLATFORM_HEADER_SPLIT

#define E32_INCLUDE_BEGIN \
    namespace e32r {

#define E32_INCLUDE_END }