// Copyright (c) Seong Woo Lee
// Licensed under the MIT license (https://opensource.org/license/mit/)

// =================================
// OS-Specific Timer
//
static inline u64 ReadOSTimer(void)
{
    LARGE_INTEGER Result;
    QueryPerformanceCounter(&Result);
    return Result.QuadPart;
}

static inline u64 GetOSTimerFrequency(void)
{
    LARGE_INTEGER Result;
    QueryPerformanceFrequency(&Result);
    return Result.QuadPart;
}

// =================================
// x86 rdtsc
//
static inline u64 ReadCPUTimer(void)
{
    return __rdtsc();
}

static inline u64 EstimateCPUTimerFrequency(void)
{
    u64 const OSTimerFrequency = GetOSTimerFrequency();
    u64 const OSTimeIn100MS = (u64)(0.1f*(f64)OSTimerFrequency + 0.5f);

    u64 OSAccum  = 0;
    u64 CPUAccum = 0;

    u64 CPUTimerOld = ReadCPUTimer();
    u64 OSTimerOld  = ReadOSTimer();

    while (OSAccum < OSTimeIn100MS)
    {
        u64 CPUTimerNew = ReadCPUTimer();
        u64 OSTimerNew  = ReadOSTimer();

        u64 CPUTimerElapsed = CPUTimerNew - CPUTimerOld;
        u64 OSTimerElapsed  = OSTimerNew - OSTimerOld;

        CPUAccum += CPUTimerElapsed;
        OSAccum  += OSTimerElapsed;
    }

    f64 const TimeElapsed = (f64)OSAccum / (f64)OSTimerFrequency;
    u64 const CPUTimerFrequency = (u64)((f64)CPUAccum / TimeElapsed + 0.5f);
    return CPUTimerFrequency;
}

// =================================
// Scope Timer
//
scope_clock::scope_clock(const char *_Text)
{
    ClockBegin = ReadCPUTimer();
    Text = _Text;
}

scope_clock::~scope_clock(void)
{
    ClockEnd = ReadCPUTimer();
    u64 ClockElapsed = ClockEnd - ClockBegin;
    u64 ElapsedMs = (u64)((f64)ClockElapsed*State.InverseCPUTimerFrequency*1000.f + 0.5f);
    printf(Text, ElapsedMs);
}
