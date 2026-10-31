#pragma once
#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <cstddef>
#include <limits>
#pragma comment(lib,"bcrypt.lib")

// SHA256 for matching a prepared plugin to its compressed source archive.
// Shared loader integrity code; no character tables or save hooks are included.
namespace archive_integrity {
using Digest=std::array<unsigned char,32>;
inline bool digest(const void* data,size_t size,Digest& out) {
    if(size>(std::numeric_limits<ULONG>::max)())return false;
    BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE hash{};
    bool ok=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0&&
        BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0&&
        BCryptHashData(hash,(PUCHAR)data,static_cast<ULONG>(size),0)>=0&&
        BCryptFinishHash(hash,out.data(),static_cast<ULONG>(out.size()),0)>=0;
    if(hash)BCryptDestroyHash(hash);
    if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);
    return ok;
}
}
