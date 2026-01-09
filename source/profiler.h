// Copyright (c) Seong Woo Lee
// Licensed under the MIT license (https://opensource.org/license/mit/)


static inline u64 ReadOSTimer(void);
static inline u64 GetOSTimerFrequency(void);
static inline u64 ReadCPUTimer(void);
static inline u64 EstimateCPUTimerFrequency(void);

struct scope_clock 
{
    u64 ClockBegin;
    u64 ClockEnd;
    const char *Text;

    scope_clock(const char *_Text);
    ~scope_clock(void);
};
#define ScopeClock(Text) scope_clock CONCAT(ScopeClock_, __LINE__)(Text)
