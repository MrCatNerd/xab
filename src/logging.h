#pragma once

#ifndef NDEBUG
#include <stdio.h>
#define LOG(...) printf(__VA_ARGS__);
#else
#define LOG(...)
#endif
