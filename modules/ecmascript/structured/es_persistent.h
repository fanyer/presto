/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  1995-2011
 */

#ifndef ES_PERSISTENT_H
#define ES_PERSISTENT_H

#ifdef ES_PERSISTENT_SUPPORT

#include "modules/ecmascript/structured/es_clone.h"

class ES_PersistentValue;

class ES_CloneToPersistent
    : public ES_CloneBackend
{
public:
    ES_CloneToPersistent(unsigned size_limit);

    virtual ~ES_CloneToPersistent();

    virtual void PushNullL();
    virtual void PushUndefinedL();
    virtual void PushBooleanL(BOOL value);
    virtual void PushNumberL(double value);
    virtual void PushStringL(const uni_char *value, unsigned length);
    virtual void PushStringL(unsigned index);
    virtual void PushObjectL(unsigned index);

    virtual void StartObjectL();
    virtual void StartObjectBooleanL();
    virtual void StartObjectNumberL();
    virtual void StartObjectStringL();
    virtual void StartObjectArrayL();
    virtual void StartObjectRegExpL();
    virtual void StartObjectDateL();
    virtual void StartObjectArrayBufferL(const unsigned char *value, unsigned length);
    virtual void StartObjectTypedArrayL(unsigned kind, unsigned offset, unsigned size);

    virtual void AddPropertyL(int attributes);

    virtual BOOL HostObjectL(EcmaScript_Object *source_object);
    virtual BOOL HostObjectL(ES_PersistentItem *source_item);

    ES_PersistentValue *GetResultL();

private:
    unsigned size_limit;

    unsigned *buffer;
    unsigned buffer_used, buffer_size;
    unsigned *result;

    OpVector<ES_PersistentItem> items;

    void GrowBufferL(unsigned n);
};

class ES_CloneFromPersistent
    : public ES_CloneFrontend
{
public:
    ES_CloneFromPersistent(ES_PersistentValue *value);

    virtual void CloneL(ES_CloneBackend *target);

private:
    ES_PersistentValue *value;
};

#endif // ES_PERSISTENT_SUPPORT
#endif // ES_PERSISTENT_H
