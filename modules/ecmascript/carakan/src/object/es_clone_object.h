/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */
/*
 * Copyright (C) Opera Software ASA  1995-2011
 */

#ifndef ES_CLONE_OBJECT_H
#define ES_CLONE_OBJECT_H

#include "modules/ecmascript/structured/es_clone.h"
#include "modules/util/OpHashTable.h"
#include "modules/util/adt/opvector.h"

class ES_CloneToObject
    : public ES_CloneBackend
{
public:
    ES_CloneToObject(ES_Runtime *runtime, ES_Context *context);

    virtual ~ES_CloneToObject();

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
#ifdef ES_PERSISTENT_SUPPORT
    virtual BOOL HostObjectL(ES_PersistentItem *source_item);
#endif // ES_PERSISTENT_SUPPORT

    ES_Value_Internal &GetResult() { OP_ASSERT(stack_used == 1); return stack[0]; }

    OP_STATUS AddTransferObject(ES_Object *object);

private:
    ES_Runtime *runtime;
    ES_Context *context;
    ES_Global_Object *global;

    OpVector<ES_Object> objects;
    void AddObjectL(ES_Object *object);
    BOOL HostObjectL(EcmaScript_Object *source_object, ES_PersistentItem *source_item);

    OpVector<JString> strings;

    ES_Value_Internal *stack;
    unsigned stack_used, stack_size;
    void GrowStackL();
};

class ES_CloneFromObject
    : public ES_CloneFrontend
{
public:
    ES_CloneFromObject(ES_Runtime *runtime, ES_Context *context, const ES_Value_Internal &value, ES_Runtime::CloneStatus &status, BOOL uncloned_to_null);

    virtual ~ES_CloneFromObject();

    virtual void CloneL(ES_CloneBackend *target);

    OP_STATUS AddTransferObject(ES_Object *object);

private:
    ES_Runtime *runtime;
    ES_Context *context;
    ES_Value_Internal value;
    ES_Runtime::CloneStatus &status;
    BOOL uncloned_to_null;

    ES_CloneBackend *target;

    OpPointerHashTable<ES_Object, int> objects;
    unsigned objects_count;

    ES_Identifier_List *strings;

    void OutputValueL(const ES_Value_Internal &value);
    void OutputStringL(JString *string);
    void OutputObjectL(ES_Object *object);
    void OutputPropertiesL(ES_Object *object);

    void FailL(ES_Runtime::CloneStatus::FaultReason reason, ES_Object *object);
};

#endif // ES_CLONE_OBJECT_H
