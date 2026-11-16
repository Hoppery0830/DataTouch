#pragma once
#include <string>
#include <stdint.h>
#include <stddef.h>
#include <cstring>
#include <cstdio>
class String {
    std::string s;
public:
    String(const char *v=""):s(v){}
    String(const std::string &v):s(v){}
    String &operator+=(const char *v){s+=v;return *this;}
    bool isEmpty() const{return s.empty();}
    size_t length() const{return s.size();}
    const char *c_str() const{return s.c_str();}
    bool operator==(const String &v)const{return s==v.s;}
};
inline size_t strlcpy(char *d,const char *s,size_t n){size_t l=strlen(s);if(n){strncpy(d,s,n-1);d[n-1]=0;}return l;}
extern uint32_t fake_now;
inline uint32_t millis(){return fake_now;}
struct Console {void println(const char*){} template<class... A> void printf(const char*,A...){}};
inline Console Serial;
