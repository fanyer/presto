/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_ARGUMENTS_H
#define ES_ARGUMENTS_H

#include "modules/ecmascript/carakan/src/object/es_object.h"

class ES_Arguments_Object : public ES_Object
{
public:
    static ES_Arguments_Object *Make(ES_Context *context, ES_Function *function, unsigned length);
    static ES_Arguments_Object *Make(ES_Context *context, ES_Function *function, ES_Value_Internal *registers, unsigned argc);

    ES_Function *&GetFunction() { return function; }
    unsigned GetLength() const { return length; }

    BOOL WasUsed() { return used; }
    void SetUsed() { used = TRUE; }
    void ResetUsed() { used = FALSE; }
    BOOL HasAliased() { return has_aliased; }

    ES_Special_Aliased *GetAliased(unsigned index) { return reinterpret_cast<ES_Special_Aliased *>(reinterpret_cast<UINTPTR>(this) + GC_ALIGN(sizeof(ES_Arguments_Object)) + index * GC_ALIGN(sizeof(ES_Special_Aliased))); }

protected:
    static void Initialize(ES_Arguments_Object *arguments, ES_Class *klass, ES_Function *function, unsigned length)
    {
        ES_Object::Initialize(arguments, klass);
        arguments->ChangeGCTag(GCTAG_ES_Object_Arguments);
        arguments->function = function;
        arguments->length = length;
        arguments->used = FALSE;
        arguments->has_aliased = FALSE;
    }

    ES_Function *function;

    unsigned length;
    BOOL used, has_aliased;
};

#endif // ES_ARGUMENTS_H
