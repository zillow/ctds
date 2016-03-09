#ifndef __MACROS_H__
#define __MACROS_H__


#define CONCAT_(a, b) a##b
#define CONCAT(a, b) CONCAT_(a, b)

#define STRINGIFY_(x) #x
#define STRINGIFY(x) STRINGIFY_(x)

#define ARRAYSIZE(_x) (sizeof((_x)) / sizeof(*(_x)))
#define STRLEN(_x) (ARRAYSIZE((_x)) - 1)

#define MIN(_x, _y) (((_x) < (_y)) ? (_x) : (_y))
#define MAX(_x, _y) (((_x) > (_y)) ? (_x) : (_y))


/* Indicate an argument is unused and work-around -Wunused-parameter */
#define UNUSED(_x) (void)(_x)

#endif /* ifndef __MACROS_H__ */
