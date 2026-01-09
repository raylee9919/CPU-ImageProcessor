// Copyright (c) Seong Woo Lee
// Licensed under the MIT license (https://opensource.org/license/mit/)

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <immintrin.h>


typedef uint8_t     u8;
typedef uint16_t    u16;
typedef uint32_t    u32;
typedef uint64_t    u64;
typedef float       f32;
typedef double      f64;
typedef int32_t     s32;
typedef int32_t     b32;

#define Max(a, b) (((a)>(b))?(a):(b))
#define Min(a, b) (((a)<(b))?(a):(b))
#define CountOf(arr) (sizeof(arr)/sizeof(arr[0]))
#define CONCAT_(A, B) A ## B
#define CONCAT(A, B) CONCAT_(A, B)

#ifdef _MSC_VER
#  define Assert(exp) if (!(exp)) do { __debugbreak(); } while(0)
#else
#  define Assert(exp) if (!(exp)) do { *(volatile int *)0 = 0; } while(0)
#endif

