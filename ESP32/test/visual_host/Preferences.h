#pragma once
#include <vector>
#include <cstring>
inline std::vector<unsigned char> flash;
inline bool flash_fail=false;
class Preferences {
public:
 bool begin(const char*,bool){return true;}
 size_t getBytesLength(const char*){return flash.size();}
 size_t getBytes(const char*,void *p,size_t n){memcpy(p,flash.data(),n);return n;}
 size_t putBytes(const char*,const void *p,size_t n){
  if(flash_fail)return 0;
  auto b=static_cast<const unsigned char*>(p);flash.assign(b,b+n);return n;
 }
};
