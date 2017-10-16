#ifndef __C99BOOL_H__
#define __C99BOOL_H__

/* Visual Studio 2013+ supports (some of) the c99 standard. */
#if defined(_MSC_VER) && _MSC_VER < 1800
#  define true (1)
#  define false (0)
#  define bool int
#else
#  include <stdbool.h>
#endif

#endif /* ifndef __C99BOOL_H__ */
