#ifndef __C99INT_H__
#define __C99INT_H__

/* Visual Studio 2013+ supports (some of) the c99 standard. */
#if defined(_MSC_VER) && _MSC_VER < 1800

typedef __int8 int8_t;
typedef unsigned __int8 uint8_t;

typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;

typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;

typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;

#  define INT16_MIN (-32767 - 1)
#  define INT16_MAX (32767)
#  define INT32_MIN (-2147483647L - 1)
#  define INT32_MAX (2147483647L)
#else
#  include <stdint.h>
#endif

#endif /* ifndef __C99INT_H__ */
