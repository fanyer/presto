/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#ifdef ES_OPPSEUDOTHREAD
#include "modules/ecmascript/oppseudothread/oppseudothread.h"

#ifdef _WIN32
# define HAVE_WIN32_THREADS_SUPPORT
#endif // _WIN32

#if defined(OPPSEUDOTHREAD_THREADED) && defined(HAVE_WIN32_THREADS_SUPPORT)
# include "modules/ecmascript/carakan/src/es_pch.h"
# include "modules/ecmascript/oppseudothread/oppseudothread_threaded.h"
# include <windows.h>
# include <process.h>

class Win32Mutex
    : public OpSystemMutex
{
public:
    Win32Mutex();
    virtual ~Win32Mutex();

    virtual void Lock();
    virtual void Unlock();

private:
    friend class Win32Condition;
    CRITICAL_SECTION lock;
};

/* static */ OP_STATUS
OpSystemMutex::Create(OpSystemMutex *&mutex)
{
    mutex = OP_NEW(Win32Mutex, ());
    return (mutex != NULL ? OpStatus::OK : OpStatus::ERR_NO_MEMORY);
}

Win32Mutex::Win32Mutex()
{
    InitializeCriticalSection(&lock);
}

Win32Mutex::~Win32Mutex()
{
    DeleteCriticalSection(&lock);
}

void
Win32Mutex::Lock()
{
    EnterCriticalSection(&lock);
}

void
Win32Mutex::Unlock()
{
    LeaveCriticalSection(&lock);
}

/** Win32Condition - condition variables, all credit due to Doug Schmidt - http://www.cs.wustl.edu/~schmidt/win32-cv-1.html */

class Win32Condition
    : public OpSystemCondition
{
public:
    Win32Condition();
    virtual ~Win32Condition();

    void Wait(OpSystemMutex *mutex);
    void Signal();

private:

    CRITICAL_SECTION lock_wait;
    unsigned wait_count;

    CRITICAL_SECTION lock_release;
    unsigned release_count;

    unsigned wait_gen;

    HANDLE hEvent;
};

/* static */ OP_STATUS
OpSystemCondition::Create(OpSystemCondition *&condition)
{
    condition = OP_NEW(Win32Condition, ());
    return (condition != NULL ? OpStatus::OK : OpStatus::ERR_NO_MEMORY);
}

Win32Condition::Win32Condition()
{
    InitializeCriticalSection(&lock_wait);
    InitializeCriticalSection(&lock_release);
    wait_gen = wait_count = release_count = 0;

    hEvent = CreateEvent(NULL/*security attributes*/, TRUE/*manual*/, FALSE/*non-signalled*/, NULL/*no name*/);
    OP_ASSERT(hEvent != NULL);
}

Win32Condition::~Win32Condition()
{
    DeleteCriticalSection(&lock_wait);
    DeleteCriticalSection(&lock_release);
    CloseHandle(hEvent);
}

void
Win32Condition::Wait(OpSystemMutex *mutex)
{
    EnterCriticalSection(&lock_wait);

    wait_count++;
    unsigned current_gen = wait_gen;

    LeaveCriticalSection(&lock_wait);

    mutex->Unlock();
    while (1)
    {
        unsigned result = WaitForSingleObject(hEvent, INFINITE);
        if (result == WAIT_OBJECT_0)
        {
            EnterCriticalSection(&lock_wait);
            BOOL wait_done = release_count > 0 && wait_gen != current_gen;
            LeaveCriticalSection(&lock_wait);

            if (wait_done)
                break;
        }
        else
        {
            OP_ASSERT(0);
            break;
        }
    }
    mutex->Lock();

    EnterCriticalSection(&lock_wait);

    wait_count--;
    release_count--;
    BOOL last_one = release_count == 0;

    LeaveCriticalSection(&lock_wait);

    if (last_one)
        ResetEvent(hEvent);
}

void
Win32Condition::Signal()
{
    EnterCriticalSection(&lock_wait);

    if (wait_count > release_count)
    {
        SetEvent(hEvent);
        release_count++;
        wait_gen++;
    }

    LeaveCriticalSection(&lock_wait);
}

class Win32Thread
    : public OpSystemThread
{
public:
    virtual ~Win32Thread();

    OP_STATUS Start(void (*run)(void *), void *data);

    void Join();
    void Aborted();

    size_t GetStackSize();
    size_t GetStackRemaining();
    unsigned char *GetStackBase();
    unsigned char *GetStackLimit();

public:
    static void *Run(void *);

private:
    friend OP_STATUS OpSystemThread::Create(OpSystemThread *&);

    Win32Mutex *idle_mutex;
    Win32Condition *idle_condition;
    BOOL joined;

    unsigned *thread_ptr;
    unsigned char *stackbase;

    void (*fn)(void *);
    void *data;
};

static unsigned __stdcall ThreadFunc( void* pArguments )
{
    Win32Thread *th = static_cast<Win32Thread*>(pArguments);

    return reinterpret_cast<unsigned>(Win32Thread::Run(th));
}

/* static */ OP_STATUS
OpSystemThread::Create(OpSystemThread *&thread)
{
    Win32Thread *wthread = OP_NEW(Win32Thread, ());

    wthread->joined = FALSE;
    wthread->fn = NULL;

    wthread->idle_mutex = OP_NEW(Win32Mutex, ());
    wthread->idle_condition = OP_NEW(Win32Condition, ());

    /* ToDo: good story on thread stack sizes and/or determining the system setting for default size to expect. */
    wthread->thread_ptr = reinterpret_cast<unsigned*>(_beginthreadex(NULL/*security*/, 65536/*stack size..*/, ThreadFunc, reinterpret_cast<void*>(wthread)/*start param*/, CREATE_SUSPENDED, NULL/*thread ID*/));
    OP_ASSERT(wthread->thread_ptr);

    thread = wthread;
    ResumeThread(reinterpret_cast<HANDLE>(wthread->thread_ptr));
    return OpStatus::OK;
}

Win32Thread::~Win32Thread()
{
    OP_DELETE(idle_mutex);
    OP_DELETE(idle_condition);
}

OP_STATUS
Win32Thread::Start(void (*run)(void *), void *data0)
{
    idle_mutex->Lock();

    fn = run;
    data = data0;

    idle_condition->Signal();
    idle_mutex->Unlock();
    return OpStatus::OK;
}

/* static */ void
OpSystemThread::Stop()
{
    unsigned return_value = 0;
    _endthreadex(return_value);
}

void
Win32Thread::Join()
{
    idle_mutex->Lock();
    joined = TRUE;

    idle_condition->Signal();
    idle_mutex->Unlock();

    WaitForSingleObject(static_cast<HANDLE>(thread_ptr), INFINITE);
}

void
Win32Thread::Aborted()
{
    idle_mutex->Unlock();
}

size_t
Win32Thread::GetStackSize()
{
    /* ToDo: grovel around for the default thread stack size on creation (or explicitly limit it to some N.) */
    return 65536;
}

size_t
Win32Thread::GetStackRemaining()
{
    unsigned char x;

    /* Thread stack size, minus what we know is used, minus a 16k
       safety margin to account for what we don't know is used. */
    return GetStackSize() - (stackbase - &x) - 16384;
}

unsigned char *
Win32Thread::GetStackBase()
{
    return stackbase;
}

unsigned char *
Win32Thread::GetStackLimit()
{
    return stackbase - (GetStackSize() - 16384);
}

/* static */ void *
Win32Thread::Run(void *thread0)
{
    Win32Thread *thread = static_cast<Win32Thread *>(thread0);

    thread->idle_mutex->Lock();

    unsigned char x; thread->stackbase = &x;

    while (TRUE)
    {
        if (!thread->fn && !thread->joined)
            thread->idle_condition->Wait(thread->idle_mutex);

        if (thread->joined)
            break;
        else
        {
            thread->fn(thread->data);
            thread->fn = NULL;
        }
    }
    thread->idle_mutex->Unlock();
    return NULL;
}

#endif // OPPSEUDOTHREAD_THREADED && HAVE_WIN32_THREADS_SUPPORT
#endif // ES_OPPSEUDOTHREAD
