#pragma once
#include <stdint.h>
#include <string>
#include <cstring>
#include <cstdio>
using String=std::string;
uint32_t millis();
inline size_t strlcpy(char *d,const char *s,size_t n){size_t len=strlen(s);if(n){size_t k=len<n-1?len:n-1;memcpy(d,s,k);d[k]=0;}return len;}
struct SerialStub{template<class... T>void printf(const char *,T...) {}};
extern SerialStub Serial;
