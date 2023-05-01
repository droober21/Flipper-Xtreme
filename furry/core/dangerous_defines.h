#pragma once

/** Assign value to variable with const modifier
 * 
 * This macros is equivalent to `const_cast` from C++
 * Literally x = y, but with some magic.
 * It's as dangerous as only can be.
 * We don't advice you to use it unless you REALLY MUST.
 * Like REALLY REALLY.
 * 
 * @param x - const variable 
 * @param y - variable
 * 
 * @return assigned variable value
 */
#ifndef FURRY_CONST_ASSIGN
#define FURRY_CONST_ASSIGN_(T, x, y) \
    ({                              \
        T* tmp_x = (T*)&x;          \
        *tmp_x = y;                 \
        *tmp_x;                     \
    })
#define FURRY_CONST_ASSIGN_PTR(x, y) \
    ({                              \
        void** tmp_x = (void**)&x;  \
        *tmp_x = y;                 \
        *tmp_x;                     \
    })
#define FURRY_CONST_ASSIGN(x, y)                                        \
    _Generic((x), signed char                                          \
             : FURRY_CONST_ASSIGN_(signed char, x, y), unsigned char    \
             : FURRY_CONST_ASSIGN_(unsigned char, x, y), short          \
             : FURRY_CONST_ASSIGN_(short, x, y), unsigned short         \
             : FURRY_CONST_ASSIGN_(unsigned short, x, y), int           \
             : FURRY_CONST_ASSIGN_(int, x, y), unsigned                 \
             : FURRY_CONST_ASSIGN_(unsigned, x, y), long                \
             : FURRY_CONST_ASSIGN_(long, x, y), unsigned long           \
             : FURRY_CONST_ASSIGN_(unsigned long, x, y), long long      \
             : FURRY_CONST_ASSIGN_(long long, x, y), unsigned long long \
             : FURRY_CONST_ASSIGN_(unsigned long long, x, y), float     \
             : FURRY_CONST_ASSIGN_(float, x, y), double                 \
             : FURRY_CONST_ASSIGN_(double, x, y), long double           \
             : FURRY_CONST_ASSIGN_(long double, x, y))
#endif
