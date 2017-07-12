
#ifndef COMMON_H
#define COMMON_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#if defined(WIN32) && !defined(__MINGW32__) && !defined(__CYGWIN__)
#define CONFIG_WIN32
#endif

// 内联函数的关键字在linux gcc 和windows vc 中的定义是不同的，gcc 是inline，vc 是__inline。
// 因为代码是从linux 下移植过来的，在这里做一个宏定义修改相对简单。
#ifdef CONFIG_WIN32
#define inline __inline
#endif

typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

// 简单的数据类型定义， linux gcc 和windows vc 编译器有稍许不同，
// 用宏开关CONFIG_WIN32来屏蔽64位整数类型的差别。
#ifdef CONFIG_WIN32
typedef signed __int64 int64_t;
typedef unsigned __int64 uint64_t;
#else
typedef signed long long int64_t;
typedef unsigned long long uint64_t;
#endif

// 64 位整数的定义语法，linux gcc 和windows vc 编译器有稍许不同，用宏开关CONFIG_WIN32 来屏蔽64位整数定义的差别。
// Linux 用LL / ULL 来表示64 位整数，VC 用i64 来表示64 位整数。##是连接符，把##前后的两个字符串连接成一个字符串。
#ifdef CONFIG_WIN32
#define int64_t_C(c)     (c ## i64)
#define uint64_t_C(c)    (c ## i64)
#else
#define int64_t_C(c)     (c ## LL)
#define uint64_t_C(c)    (c ## ULL)
#endif

// 定义最大的64 位整数。
#ifndef INT64_MAX
#define INT64_MAX int64_t_C(9223372036854775807)
#endif

// 大小写敏感的字符串比较函数。在ffplay中只关心是否相等，不关心谁大谁小。
static int strcasecmp(char *s1, const char *s2)
{
    while (toupper((unsigned char)*s1) == toupper((unsigned char)*s2++))
	if (*s1++ == '\0')
	    return 0;

    return (toupper((unsigned char)*s1) - toupper((unsigned char) *--s2));
}

// 限幅函数，这个函数使用简单的比较逻辑来实现，比较语句多，容易中断CPU 的指令流水线，导致性能低下。
// 如果变量a 的取值范围比较小，可以用常规的空间换时间的查表方法来优化。
static inline int clip(int a, int amin, int amax)
{
    if (a < amin)
	return amin;
    else if (a > amax)
	return amax;
    else
	return a;
}

#endif
