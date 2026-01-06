// Copyright (c) Seong Woo Lee
// Licensed under the MIT license (https://opensource.org/license/mit/)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>


typedef void work_callback(void *Param);

struct work 
{
    work_callback *Callback;
    void *Param;
};

struct work_queue 
{
    work            Works[256];

    u32 volatile    IndexToWrite;
    u32 volatile    IndexToRead;

    u32 volatile    CompletionCount;
    u32 volatile    CompletionGoal;

    HANDLE          Semaphore;
};
