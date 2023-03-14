/* varnum.c : Decode Minecraft's variable integer coding */
#include "varnum.h"
#define VARNUM_MORE_MASK 0x80
#define VARNUM_VALUE_MASK 0x7F
int
to_varint(unsigned char *varint, int i)
{
	int len = 0;
	while (1) {
		varint[len] = i & VARNUM_VALUE_MASK;
		if (((i >>= 7) == 0) || (len == VARINT_MAX - 1))
			break;
		varint[len] |= VARNUM_MORE_MASK;
		++len;
	}
	return len+1;
}
int
from_varint(unsigned char const *varint, int *i)
{
	int len = 0, group = 0;
	int num = 0;
	while (1) {
		num |= (int)(varint[len] & VARNUM_VALUE_MASK) << group;
		if (!(varint[len] & VARNUM_MORE_MASK) || len == VARINT_MAX - 1)
			break;
		++len;
		group += 7;
	}
	if (i)
		*i = num;
	return len+1;
}
int
to_varlong(unsigned char *varlong, long l)
{
	int len = 0;
	while (1) {
		varlong[len] = l & VARNUM_VALUE_MASK;
		if (((l >>= 7) == 0) || (len == VARLONG_MAX - 1))
			break;
		varlong[len] |= VARNUM_MORE_MASK;
		++len;
	}
	return len+1;
}
int
from_varlong(unsigned char const *varlong, long *l)
{
	int len = 0, group = 0;
	long num = 0;
	while (1) {
		num |= (long)(varlong[len] & VARNUM_VALUE_MASK) << group;
		if (!(varlong[len] & VARNUM_MORE_MASK) || len == VARLONG_MAX - 1)
			break;
		++len;
		group += 7;
	}
	if (l)
		*l = num;
	return len+1;
}
char
varint_size(int i) {
	for (int len = 0; len < VARINT_MAX - 1; ++len)
		if ((i >>= 7) == 0)
			return len+1;
	return VARINT_MAX;
}
char
varlong_size(long l) {
	for (int len = 0; len < VARLONG_MAX - 1; ++len)
		if ((l >>= 7) == 0)
			return len+1;
	return VARLONG_MAX;
}
#undef VARNUM_MORE_MASK
#undef VARNUM_VALUE_MASK
