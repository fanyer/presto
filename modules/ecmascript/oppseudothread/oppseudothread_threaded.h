/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef OPPSEUDOTHREAD_THREADED_H
#define OPPSEUDOTHREAD_THREADED_H

class OpSystemMutex
{
public:
    static OP_STATUS Create(OpSystemMutex *&mutex);

    virtual ~OpSystemMutex() {}

    virtual void Lock() = 0;
    virtual void Unlock() = 0;
};

class OpSystemCondition
{
public:
    static OP_STATUS Create(OpSystemCondition *&condition);

    virtual ~OpSystemCondition() {}

    virtual void Wait(OpSystemMutex *mutex) = 0;
    virtual void Signal() = 0;
};

class OpSystemThread
{
public:
    static OP_STATUS Create(OpSystemThread *&thread);
    static void Stop();

    virtual ~OpSystemThread() {}

    virtual OP_STATUS Start(void (*run)(void *), void *data) = 0;
    virtual void Join() = 0;
    virtual void Aborted() = 0;

    virtual size_t GetStackSize() = 0;
    virtual size_t GetStackRemaining() = 0;
    virtual unsigned char *GetStackBase() = 0;
    virtual unsigned char *GetStackLimit() = 0;
};

#endif // OPPSEUDOTHREAD_THREADED_H
