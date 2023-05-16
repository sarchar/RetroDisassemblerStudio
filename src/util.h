#pragma once

#include <algorithm>
#include <cassert>
#include <iostream>
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

template <class T>
inline void WriteVarInt(std::ostream& os, T const& v)
{
    if(v < 254) {
        u8 s = (u8)v;
        os.write((char*)&s, 1);
    } else if(v < 0x10000) {
        u8 c = 254;
        u16 s = (u16)v;
        os.write((char*)&c, 1);
        os.write((char*)&s, 2);
    } else {
        assert(v < 0x100000000LL);
        u8 c = 255;
        u32 s = (u32)v;
        os.write((char*)&c, 1);
        os.write((char*)&s, 4);
    }
}

template <class T>
inline T ReadVarInt(std::istream& is)
{
    u8 v;
    is.read((char*)&v, 1);
    if(v < 254) {
        return (T)v;
    } else if(v == 254) {
        u16 v;
        is.read((char*)&v, 2);
        return (T)v;
    } else {
        u32 v;
        is.read((char*)&v, 4);
        return (T)v;
    }

    return 0;
}

inline void WriteString(std::ostream& os, std::string const& s)
{
    WriteVarInt(os, s.size());
    os.write(s.c_str(), s.size());
}

inline void ReadString(std::istream& is, std::string& s)
{
    auto len = ReadVarInt<u32>(is);
    s.resize(len);
    is.read(&s[0], len);
}

template<class T>
inline void zero(T* mem) {
    memset(mem, 0, sizeof(T));
}


// https://stackoverflow.com/questions/3418231/replace-part-of-a-string-with-another-string
inline void strreplace(std::string& str, const std::string& from, const std::string& to) {
    if(from.empty()) return;

    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
}

