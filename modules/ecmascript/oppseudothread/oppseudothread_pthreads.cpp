/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"

#ifdef OPPSEUDOTHREAD_THREADED_PTHREADS
# include "modules/ecmascript/oppseudothread/oppseudothread_threaded.h"

# include <pthread.h>
# include <signal.h>

class PosixSystemMutex
    : public OpSystemMutex
{
public:
    PosixSystemMutex();
    virtual ~PosixSystemMutex();

    virtual void Lock();
    virtual void Unlock();

private:
    friend class PosixSystemCondition;
    pthread_mutex_t mutex;
};

/* static */ OP_STATUS
OpSystemMutex::Create(OpSystemMutex *&mutex)
{
    mutex = OP_NEW(PosixSystemMutex, ());
    return mutex ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

PosixSystemMutex::PosixSystemMutex()
{
    pthread_mutex_init(&mutex, NULL);
}

PosixSystemMutex::~PosixSystemMutex()
{
    pthread_mutex_destroy(&mutex);
}

void
PosixSystemMutex::Lock()
{
    pthread_mutex_lock(&mutex);
}

void
PosixSystemMutex::Unlock()
{
    pthread_mutex_unlock(&mutex);
}

class PosixSystemCondition
    : public OpSystemCondition
{
public:
    PosixSystemCondition();
    virtual ~PosixSystemCondition();

    void Wait(OpSystemMutex *mutex);
    void Signal();

private:
    pthread_cond_t condition;
};

/* static */ OP_STATUS
OpSystemCondition::Create(OpSystemCondition *&condition)
{
    condition = OP_NEW(PosixSystemCondition, ());
    return condition ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

PosixSystemCondition::PosixSystemCondition()
{
    pthread_cond_init(&condition, NULL);
}

PosixSystemCondition::~PosixSystemCondition()
{
    pthread_cond_destroy(&condition);
}

void
PosixSystemCondition::Wait(OpSystemMutex *mutex)
{
    pthread_cond_wait(&condition, &static_cast<PosixSystemMutex *>(mutex)->mutex);
}

void
PosixSystemCondition::Signal()
{
    pthread_cond_signal(&condition);
}

class PosixSystemThread
    : public OpSystemThread
{
public:
    virtual ~PosixSystemThread();

    OP_STATUS Start(void (*run)(void *), void *data);

    void Stop();
    void Join();
    void Aborted();

    size_t GetStackSize();
    size_t GetStackRemaining();
    unsigned char *GetStackBase();
    unsigned char *GetStackLimit();

private:
    friend OP_STATUS OpSystemThread::Create(OpSystemThread *&);

    pthread_mutex_t idle_mutex;
    pthread_cond_t idle_condition;
    BOOL joined;

    pthread_t thread;
    pthread_attr_t thread_attributes;
    unsigned char *stackbase;

    static void *Run(void *);

    void (*fn)(void *);
    void *data;
};

/* static */ OP_STATUS
OpSystemThread::Create(OpSystemThread *&thread)
{
    PosixSystemThread *pthread = OP_NEW(PosixSystemThread, ());

    if (!pthread)
        return OpStatus::ERR_NO_MEMORY;

    pthread->joined = FALSE;
    pthread->fn = NULL;

    pthread_mutex_init(&pthread->idle_mutex, NULL);
    pthread_cond_init(&pthread->idle_condition, NULL);
    pthread_attr_init(&pthread->thread_attributes);

    if (pthread_create(&pthread->thread, &pthread->thread_attributes, PosixSystemThread::Run, pthread) != 0)
    {
        OP_DELETE(pthread);

        /* We've probably created too many threads; signal OOM. */
        return OpStatus::ERR_NO_MEMORY;
    }

    thread = pthread;
    return OpStatus::OK;
}

/* static */ void
OpSystemThread::Stop()
{
    pthread_exit(NULL);
}

PosixSystemThread::~PosixSystemThread()
{
    pthread_mutex_destroy(&idle_mutex);
    pthread_cond_destroy(&idle_condition);
}

OP_STATUS
PosixSystemThread::Start(void (*run)(void *), void *data0)
{
    pthread_mutex_lock(&idle_mutex);

    fn = run;
    data = data0;

    pthread_cond_signal(&idle_condition);
    pthread_mutex_unlock(&idle_mutex);

    return OpStatus::OK;
}

void
PosixSystemThread::Join()
{
    pthread_mutex_lock(&idle_mutex);

    joined = TRUE;

    pthread_cond_signal(&idle_condition);
    pthread_mutex_unlock(&idle_mutex);

    void *result;
    pthread_join(thread, &result);
}

void
PosixSystemThread::Aborted()
{
    pthread_mutex_unlock(&idle_mutex);
}

size_t
PosixSystemThread::GetStackSize()
{
    size_t stacksize;
    pthread_attr_getstacksize(&thread_attributes, &stacksize);
    return stacksize;
}

size_t
PosixSystemThread::GetStackRemaining()
{
    unsigned char x;

    /* Thread stack size, minus what we know is used, minus a 16k
       safety margin to account for what we don't know is used. */
    return GetStackSize() - (stackbase - &x) - 16384;
}

unsigned char *
PosixSystemThread::GetStackBase()
{
    return stackbase;
}

unsigned char *
PosixSystemThread::GetStackLimit()
{
    return stackbase - (GetStackSize() - 16384);
}

/* static */ void *
PosixSystemThread::Run(void *thread0)
{
    PosixSystemThread *thread = static_cast<PosixSystemThread *>(thread0);

    pthread_mutex_lock(&thread->idle_mutex);

    unsigned char x; thread->stackbase = &x;

    while (TRUE)
    {
        if (!thread->fn && !thread->joined)
            pthread_cond_wait(&thread->idle_condition, &thread->idle_mutex);

        if (thread->joined)
            break;
        else
        {
            thread->fn(thread->data);
            thread->fn = NULL;
        }
    }

    pthread_mutex_unlock(&thread->idle_mutex);
    return NULL;
}

#endif // OPPSEUDOTHREAD_THREADED_PTHREADS
