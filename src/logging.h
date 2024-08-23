#pragma once

#ifndef NDEBUG
#include <stdio.h>
#define LOG(...) printf(__VA_ARGS__);

#ifndef NVERBOSE
#define VLOG(...) printf(__VA_ARGS__);
#else
#define VLOG(...)
#endif // !NVERBOSE

#else
#define LOG(...)
#define VLOG(...)
#endif // !NDEBUG
