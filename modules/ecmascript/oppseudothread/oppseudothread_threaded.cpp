/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/oppseudothread/oppseudothread.h"

#ifdef OPPSEUDOTHREAD_THREADED
# include "modules/ecmascript/oppseudothread/oppseudothread_threaded.h"

OpSystemThread *g_cached_thread = NULL;

OpPseudoThread::OpPseudoThread()
    : is_using_standard_stack(TRUE),
      mutex(NULL),
      condition(NULL),
      aborted(FALSE),
      finished(FALSE),
      current_thread(NULL),
      suspend(NULL)
{
}

/* virtual */
OpPseudoThread::~OpPseudoThread()
{
    if (!finished)
    {
        aborted = TRUE;

        mutex->Lock();

        while (current_thread)
        {
            current_thread->condition->Signal();
            condition->Wait(mutex);

            Thread *to_delete = current_thread;
            current_thread = current_thread->previous;
            OP_DELETE(to_delete);
        }

        mutex->Unlock();
    }

    OP_DELETE(mutex);
    OP_DELETE(condition);
}

OP_STATUS
OpPseudoThread::Initialize(unsigned stacksize)
{
    RETURN_IF_ERROR(OpSystemMutex::Create(mutex));
    RETURN_IF_ERROR(OpSystemCondition::Create(condition));

    return OpStatus::OK;
}

BOOL
OpPseudoThread::Start(Callback callback)
{
    current_thread = OP_NEW(Thread, ());

    if (g_cached_thread)
    {
        current_thread->thread = g_cached_thread;
        g_cached_thread = NULL;
    }
    else
        OpSystemThread::Create(current_thread->thread);

    OpSystemCondition::Create(current_thread->condition);

    current_thread->callback = callback;
    mutex->Lock();
    is_using_standard_stack = FALSE;
    current_thread->thread->Start(ThreadRun, this);

    while (TRUE)
    {
        is_using_standard_stack = FALSE;

        condition->Wait(mutex);

        is_using_standard_stack = TRUE;

        if (suspend)
        {
            suspend(this);
            suspend = NULL;

            current_thread->condition->Signal();
        }
        else
            break;
    }

    mutex->Unlock();

    if (finished)
    {
        if (!g_cached_thread)
        {
            g_cached_thread = current_thread->thread;
            current_thread->thread = NULL;
        }

        OP_DELETE(current_thread);
        current_thread = NULL;

        return TRUE;
    }
    else
        return FALSE;
}

BOOL
OpPseudoThread::Resume()
{
    mutex->Lock();

    while (TRUE)
    {
        is_using_standard_stack = FALSE;

        current_thread->condition->Signal();
        condition->Wait(mutex);

        is_using_standard_stack = TRUE;

        if (suspend)
        {
            suspend(this);
            suspend = NULL;
        }
        else
            break;
    }

    mutex->Unlock();

    is_using_standard_stack = TRUE;

    if (finished)
    {
        if (!g_cached_thread)
        {
            g_cached_thread = current_thread->thread;
            current_thread->thread = NULL;
        }

        OP_DELETE(current_thread);
        current_thread = NULL;

        return TRUE;
    }
    else
        return FALSE;
}

void
OpPseudoThread::Yield()
{
    mutex->Lock();
    condition->Signal();
    current_thread->condition->Wait(mutex);

    if (aborted)
    {
        current_thread->thread->Aborted();

        condition->Signal();
        mutex->Unlock();

        OpSystemThread::Stop();
    }
    else
        mutex->Unlock();
}

void
OpPseudoThread::Suspend(Callback callback)
{
    if (is_using_standard_stack)
        callback(this);
    else
    {
        mutex->Lock();

        suspend = callback;

        condition->Signal();
        current_thread->condition->Wait(mutex);

        mutex->Unlock();
    }
}

OP_STATUS
OpPseudoThread::Reserve(Callback callback, unsigned stacksize)
{
    OP_STATUS status = OpStatus::OK;

    if (StackSpaceRemaining() < stacksize)
    {
        current_thread = OP_NEW(Thread, (current_thread));
        current_thread->callback = callback;

        OpSystemThread::Create(current_thread->thread);
        OpSystemCondition::Create(current_thread->condition);

        mutex->Lock();
        current_thread->thread->Start(ThreadRun, this);
        current_thread->previous->condition->Wait(mutex);

        status = current_thread->status;

        Thread *joined = current_thread;
        current_thread = current_thread->previous;

        joined->thread->Join();
        OP_DELETE(joined);

        if (aborted)
        {
            condition->Signal();
            mutex->Unlock();

            OpSystemThread::Stop();
        }
        else
            mutex->Unlock();
    }
    else
        TRAP(status, callback(this));

    return status;
}

unsigned
OpPseudoThread::StackSpaceRemaining()
{
    if (current_thread)
        return current_thread->thread->GetStackRemaining();
    else
        return UINT_MAX;
}

unsigned char *
OpPseudoThread::StackLimit()
{
    if (current_thread)
        return current_thread->thread->GetStackLimit();
    else
        return NULL;
}

unsigned char *
OpPseudoThread::StackTop()
{
    return current_thread->thread->GetStackBase();
}

#ifndef CONSTANT_DATA_IS_EXECUTABLE
/* static */ void
OpPseudoThread::InitCodeVectors()
{
}
#endif // CONSTANT_DATA_IS_EXECUTABLE

/* static */ void
OpPseudoThread::ThreadRun(void *thread0)
{
    OpPseudoThread *thread = static_cast<OpPseudoThread *>(thread0);

    TRAP(thread->current_thread->status, thread->current_thread->callback(thread));

    thread->mutex->Lock();

    if (thread->current_thread->previous)
        thread->current_thread->previous->condition->Signal();
    else
    {
        thread->finished = TRUE;
        thread->condition->Signal();
    }

    thread->mutex->Unlock();
}

OpPseudoThread::Thread::~Thread()
{
    if (thread)
    {
        thread->Join();
        OP_DELETE(thread);
    }

    OP_DELETE(condition);
}

#endif // OPPSEUDOTHREAD_THREADED
