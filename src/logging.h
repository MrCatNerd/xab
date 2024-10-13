#pragma once

#ifndef NDEBUG
#include <stdio.h>
#define LOG(...) printf(__VA_ARGS__)
#else
#define LOG(...)
#endif // !NDEBUG

#ifndef NVERBOSE
#include <stdio.h>
#define VLOG(...) printf(__VA_ARGS__)
#else
#define VLOG(...)
#endif // !NVERBOSE
