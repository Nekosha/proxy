#pragma once
/* varnum.h */
#ifdef MC_VARNUM_H
    #warning varnum.h: file included more than once
#else
    #define MC_VARNUM_H
	#define VARINT_MAX 5
	#define VARLONG_MAX 10
    int to_varint(unsigned char *varint, int const i);
    int from_varint(unsigned char const *varint, int *i);
    int to_varlong(unsigned char *varlong, long const l);
    int from_varlong(unsigned char const *varlong, long *l);
    char varint_size(int const i);
    char varlong_size(long const l);
#endif
