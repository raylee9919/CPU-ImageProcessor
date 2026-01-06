// Copyright (c) Seong Woo Lee
// Licensed under the MIT license (https://opensource.org/license/mit/)

static u32
GetLogicalCoreCount(void) 
{
    SYSTEM_INFO sys_info = {};
    GetSystemInfo(&sys_info);
    DWORD core_count = sys_info.dwNumberOfProcessors;
    return core_count;
}

static void
AddWork(work_queue *Queue, work_callback *Callback, void *Param) 
{
    // @Note: We have a single producer atm. We gonna have to switch to cmpxchg 
    //        if we want mpmc, ultimately.

    u32 Index = Queue->IndexToWrite;
    u32 NewWriteIndex = (Queue->IndexToWrite + 1) % CountOf(Queue->Works);

    Assert(NewWriteIndex != Queue->IndexToRead);

    Queue->Works[Index].Callback = Callback;
    Queue->Works[Index].Param    = Param;
    _WriteBarrier();
    Queue->IndexToWrite = NewWriteIndex;
    Queue->CompletionGoal++;
    ReleaseSemaphore(Queue->Semaphore, 1, 0);
}

static b32
DoWorkOrShouldSleep(work_queue *Queue) 
{
    b32 ShouldSleep = false;

    u32 OldIndex = Queue->IndexToRead;
    u32 NewIndex = (OldIndex + 1) % CountOf(Queue->Works);

    if (OldIndex != Queue->IndexToWrite) 
    {
        u32 Index = InterlockedCompareExchange((LONG volatile *)&Queue->IndexToRead, NewIndex, OldIndex);
        if (Index == OldIndex) 
        {
            work Work = Queue->Works[Index];
            Work.Callback(Work.Param);
            InterlockedIncrement(&Queue->CompletionCount);
        }
    }
    else 
    {
        ShouldSleep = true;
    }

    return ShouldSleep;
}

static void
CompleteAllWork(work_queue *Queue) 
{
    while (Queue->CompletionCount != Queue->CompletionGoal) 
    {
        DoWorkOrShouldSleep(Queue);
    }

    Queue->CompletionCount = 0;
    Queue->CompletionGoal  = 0;
}

static DWORD
WINAPI WorkerThreadProc(LPVOID Param) 
{
    work_queue *Queue = (work_queue *)Param;

    for (;;) 
    {
        if (DoWorkOrShouldSleep(Queue)) 
        {
            WaitForSingleObject(Queue->Semaphore, INFINITE);
        }
    }
}

static void
InitWorkQueue(work_queue *Queue, u32 CoreCount) 
{
    u32 ThreadCount = CoreCount - 1;
    memset(Queue, 0, sizeof(*Queue));
    Queue->Semaphore = CreateSemaphore(NULL, 0, ThreadCount, 0);

    for (u32 i = 0; i < ThreadCount; ++i) 
    {
        DWORD ThreadID;
        HANDLE ThreadHandle = CreateThread(NULL, 0, WorkerThreadProc, Queue, 0, &ThreadID);
        CloseHandle(ThreadHandle);
    }
}
