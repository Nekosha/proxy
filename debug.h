#pragma once
/* debug.h */
#ifdef MC_DEBUG_H
  #warning debug.h: file included more than once
#else
  #define MC_DEBUG_H
  #ifdef NDEBUG
    #define _log(...) do {} while(0)
    #define die(...) do {} while(0)
  #else
  extern int vflag;
  void _log(char const *fmt, ...);
  void die(const char *fmt, ...);
  #endif
#endif
