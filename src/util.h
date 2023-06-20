// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 
#pragma once

#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

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

// The signed and unsigned versions of WriteVarInt have different meaning for the encoded values!
template <class T>
inline typename std::enable_if<std::is_unsigned<T>::value, void>::type 
    WriteVarInt(std::ostream& os, T const& v)
{
    // 255: reserved for future use
    // 254: 64-bit numbers
    // 253: 32-bit numbers
    // 252: 16-bit numbers and 8 bit numbers < 250
    // 251: reserved for negative numbers
    // 250: negative numbers
    // <250: an 8 bit number directly
    if(v < 250) {
        u8 s = (u8)v;
        os.write((char*)&s, 1);
    } else if(v < 0x10000) {
        u8 c = 252;
        u16 s = (u16)v;
        os.write((char*)&c, 1);
        os.write((char*)&s, 2);
    } else if((u64)v < 0x100000000LL) {
        u8 c = 253;
        u32 s = (u32)v;
        os.write((char*)&c, 1);
        os.write((char*)&s, 4);
    } else {
        assert(sizeof(T) == 8);
        u8 c = 254;
        u64 s = (u64)v;
        os.write((char*)&c, 1);
        os.write((char*)&s, 8);
    }
}

// treat all signed inputs as unsigned values. -1 as s8 (value 0xFF) is encoded as 0xFA 0x01,
// but since negative values in this system are generally rare, that overhead is OK. On the other
// hand, -1 as s64 is also encoded as 0xFA 0x01, which is much better than the 8 byte 0xFFFFFFFFFFFFFFFF.
template <class T>
inline typename std::enable_if<!std::is_unsigned<T>::value, void>::type 
    WriteVarInt(std::ostream& os, T const& v)
{
    using unsigned_type = std::make_unsigned<T>::type;

    // signed data types with value >=0 are casted to the unsigned version and encoded normally
    if(v >= 0) {
        WriteVarInt<unsigned_type>(os, (unsigned_type)v);
    }

    // unsigned have 250 written and then the negative stored
    // 251 is reserved for future use
    else {
        u8 c = 250;
        os.write((char*)&c, 1);
        WriteVarInt<unsigned_type>(os, (unsigned_type)(-v));
    }
}


// version 0 did not support negative or 64-bit numbers
// util_readvarint_version needs to be set before calling ReadVarInt()
enum UTIL_READVARINT_VERSION {
    UTIL_READVARINT_VERSION_INVALID = -1,
    UTIL_READVARINT_VERSION_OLD     = 0,  // didn't support negative or 64-bit
    UTIL_READVARINT_VERSION2        = 1   // does
};

extern UTIL_READVARINT_VERSION util_readvarint_version;

template <class T>
inline typename std::enable_if<std::is_unsigned<T>::value, T>::type
    ReadVarInt(std::istream& is)
{
    u8 v;
    is.read((char*)&v, 1);
    if(util_readvarint_version == UTIL_READVARINT_VERSION_OLD) {
        // old version did not support 64-bits
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
    } else if(util_readvarint_version == UTIL_READVARINT_VERSION2) {
        // new version can read 64-bit
        if(v < 250) {
            return (T)v;
        } else if(v == 252) {
            u16 v;
            is.read((char*)&v, 2);
            return (T)v;
        } else if(v == 253) {
            u32 v;
            is.read((char*)&v, 4);
            return (T)v;
        } else if(v == 254) {
            u64 v;
            is.read((char*)&v, 8);
            return (T)v;
        } else {
            // values 250 (not valid), 251 and 255 are not allowed here
            assert(false);
        }
    } else {
        // invalid util_readvarint_version
        assert(false);
    }
    return 0;
}

// If you read a signed type but wrote an unsigned type you will read corrupted data!
template <class T>
inline typename std::enable_if<!std::is_unsigned<T>::value, T>::type
    ReadVarInt(std::istream& is)
{
    using unsigned_type = std::make_unsigned<T>::type;

    if(util_readvarint_version == UTIL_READVARINT_VERSION_OLD) {
        // old version treated signed and unsigned values the same
        return (T)ReadVarInt<unsigned_type>(is);
    } else if(util_readvarint_version == UTIL_READVARINT_VERSION2) {
        // peek ahead to see if we get the negative flag
        u8 v = (u8)is.peek();
        if(v == 250) { // next value is negative
            is.read((char*)&v, 1);
            auto uv = ReadVarInt<unsigned_type>(is);
            return -(T)uv;
        } else if(v == 251) {
            assert(false);
            return 0;
        } else {
            // value was not negative
            return (T)ReadVarInt<unsigned_type>(is);
        }
    } else {
        // invalid util_readvarint_version
        assert(false);
        return 0;
    }
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

// implement WriteVectorElement and ReadVectorElement for vectors you want to write
template<class T> inline bool WriteVectorElement(std::ostream& os, std::string& errmsg, T const& val);
template<class T> inline bool ReadVectorElement(std::istream& is, std::string& errmsg, T& out, void* userdata);

template<class T>
inline bool WriteVector(std::ostream& os, std::string& errmsg, std::vector<T> const& vec)
{
    WriteVarInt(os, (int)vec.size());
    for(auto& elem : vec) if(!WriteVectorElement(os, errmsg, elem)) return false;
    return true;
}

template<class T>
inline bool ReadVector(std::istream& is, std::string& errmsg, std::vector<T>& vec, void* userdata = nullptr)
{
    vec.clear();
    int size = ReadVarInt<int>(is);
    for(int i = 0; i < size; i++) {
        T elem;
        if(!ReadVectorElement(is, errmsg, elem, userdata)) return false;
        vec.push_back(elem);
    }
    return true;
}

template<class T>
inline bool WriteVectorElement(std::ostream& os, std::string& errmsg, std::vector<T> const& vec)
{
    return WriteVector(os, errmsg, vec);
}

template<class T>
inline bool ReadVectorElement(std::istream& is, std::string& errmsg, std::vector<T>& vec, void* userdata)
{
    return ReadVector(is, errmsg, vec, userdata);
}

template<class T>
inline void WriteEnum(std::ostream& os, T const& enum_value)
{
    WriteVarInt(os, static_cast<int>(enum_value));
}

template<class T>
inline T ReadEnum(std::istream& is)
{
    int tmp = ReadVarInt<int>(is);
    return static_cast<T>(tmp);
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

inline std::string strlower(std::string const& s)
{
    std::string ret = s;
    std::transform(ret.begin(), ret.end(), ret.begin(), [](unsigned char c){ return std::tolower(c); });
    return ret;
}

inline bool file_exists(std::string const& name)
{
    std::ifstream f(name);
    return f.good();
}

// from https://stackoverflow.com/questions/66588729/is-there-an-alternative-to-stdbind-that-doesnt-require-placeholders-if-functi
// TODO convert the whole of the code base away from std::bind
template<typename Func, typename Obj>
inline auto quick_bind(Func f, Obj* obj) 
{
    return [=](auto&&... args) {
        return (obj->*f)(std::forward<decltype(args)>(args)...);
    };
}

// from https://stackoverflow.com/questions/47203255/convert-stdvariant-to-another-stdvariant-with-super-set-of-types
template <class... Args>
struct variant_cast_proxy {
    std::variant<Args...> v;

    template <class... ToArgs>
    operator std::variant<ToArgs...>() const {
        return std::visit([](auto&& arg) -> std::variant<ToArgs...> { return arg ; }, v);
    }
};

template <class... Args>
auto variant_cast(const std::variant<Args...>& v) -> variant_cast_proxy<Args...>
{
    return {v};
}
