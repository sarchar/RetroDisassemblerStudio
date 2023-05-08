#pragma once

#include <algorithm>
#include <string>

typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short int u16;
typedef short int s16;
typedef unsigned int u32;
typedef int s32;
typedef unsigned long long u64;
typedef long long s64;

inline bool StringEndsWith(std::string const& value, std::string const& ending)
{
    if (ending.size() > value.size()) return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

inline std::string StringLower(std::string const& s)
{
    std::string ret = s;
    std::transform(ret.begin(), ret.end(), ret.begin(), [](unsigned char c){ return std::tolower(c); });
    return ret;
}

template<class T>
inline void zero(T* mem) {
    memset(mem, 0, sizeof(T));
}
