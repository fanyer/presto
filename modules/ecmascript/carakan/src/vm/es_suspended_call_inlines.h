/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_SUSPENDED_CALL_INLINES_H
#define ES_SUSPENDED_CALL_INLINES_H

template <typename T>
inline
T* Suspended_NEWA(ES_Execution_Context *context, unsigned count)
{
    ES_Suspended_NEWA<T> snewa(count);
    context->SuspendedCall(&snewa);
    return snewa.storage;
}

template <class T>
inline
void ES_Suspended_DELETEA<T>::DoCall(ES_Execution_Context *context)
{
    OP_DELETEA(storage);
}

template <typename T>
inline
void Suspended_DELETEA(ES_Execution_Context *context, T *storage)
{
    ES_Suspended_DELETEA<T> sdeletea(storage);
    context->SuspendedCall(&sdeletea);
}

template <typename T>
inline
void Suspended_DELETE(ES_Execution_Context *context, T *obj)
{
    ES_Suspended_DELETE<T> sdelete(obj);
    context->SuspendedCall(&sdelete);
}

template <typename T>
ES_SuspendedStackAutoPtr<T>::~ES_SuspendedStackAutoPtr()
{
    Suspended_DELETE(context, this->release());
    this->reset();
}

#endif // ES_SUSPENDED_CALL_INLINES_H
