// Copyright (c) Seong Woo Lee
// Licensed under the MIT license (https://opensource.org/license/mit/)

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <immintrin.h>


typedef uint8_t     u8;
typedef uint16_t    u16;
typedef uint32_t    u32;
typedef float       f32;
typedef int32_t     s32;
typedef int32_t     b32;

#define Max(a, b) (((a)>(b))?(a):(b))
#define Min(a, b) (((a)<(b))?(a):(b))
#define CountOf(arr) (sizeof(arr)/sizeof(arr[0]))
#define CONCAT_(A, B) A ## B
#define CONCAT(A, B) CONCAT_(A, B)
#define Clamp01(A) (Min(Max((A), 0), 1))

#ifdef _MSC_VER
#  define Assert(exp) if (!(exp)) do { __debugbreak(); } while(0)
#else
#  define Assert(exp) if (!(exp)) do { *(volatile int *)0 = 0; } while(0)
#endif

#if _MSC_VER
#  pragma section(".rdata$", read)
#  define read_only __declspec(allocate(".rdata$"))
#else
#  error Only msvc and Windows atm. Sorry.
#endif

struct scope_clock 
{
    clock_t ClockBegin;
    clock_t ClockEnd;
    const char *Text;

    scope_clock(const char *_Text) 
    {
        ClockBegin = clock();
        Text = _Text;
    }

    ~scope_clock() 
    {
        ClockEnd = clock();
        clock_t Elapsed = ClockEnd - ClockBegin;
        printf(Text, Elapsed);
    }
};

#define ScopeClock(Text) scope_clock CONCAT(ScopeClock_, __LINE__)(Text)
