/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */
/*
 * Copyright (C) Opera Software ASA  1995-2011
 */

#ifndef ES_CLONE_H
#define ES_CLONE_H

class EcmaScript_Object;
class ES_PersistentItem;
class ES_Object;

class ES_CloneBackend
{
public:
    virtual ~ES_CloneBackend() {}

    virtual void PushNullL() = 0;
    virtual void PushUndefinedL() = 0;
    virtual void PushBooleanL(BOOL value) = 0;
    virtual void PushNumberL(double value) = 0;
    virtual void PushStringL(const uni_char *value, unsigned length) = 0;
    virtual void PushStringL(unsigned index) = 0;
    virtual void PushObjectL(unsigned index) = 0;

    virtual void StartObjectL() = 0;
    virtual void StartObjectBooleanL() = 0;
    virtual void StartObjectNumberL() = 0;
    virtual void StartObjectStringL() = 0;
    virtual void StartObjectArrayL() = 0;
    virtual void StartObjectRegExpL() = 0;
    virtual void StartObjectDateL() = 0;
    virtual void StartObjectArrayBufferL(const unsigned char *value, unsigned length) = 0;
    virtual void StartObjectTypedArrayL(unsigned kind, unsigned offset, unsigned size) = 0;
    virtual void AddPropertyL(int attributes) = 0;

    virtual BOOL HostObjectL(EcmaScript_Object *source_object) = 0;
#ifdef ES_PERSISTENT_SUPPORT
    virtual BOOL HostObjectL(ES_PersistentItem *source_item) = 0;
#endif // ES_PERSISTENT_SUPPORT
};

class ES_CloneFrontend
{
public:
    virtual ~ES_CloneFrontend() {}

    virtual void CloneL(ES_CloneBackend *target) = 0;
};

#endif // !ES_CLONE_H
