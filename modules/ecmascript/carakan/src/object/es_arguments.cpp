/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"

#include "modules/ecmascript/carakan/src/object/es_arguments.h"
#include "modules/ecmascript/carakan/src/object/es_indexed.h"

/* static */ ES_Arguments_Object *
ES_Arguments_Object::Make(ES_Context *context, ES_Function *function, unsigned length)
{
    BOOL is_strict_mode = function->GetFunctionCode()->data->is_strict_mode;
    ES_Arguments_Object *arguments;
    ES_Class *klass;
    ES_Global_Object *global_object = function->GetGlobalObject();

    if (is_strict_mode)
        klass = global_object->GetStrictArgumentsClass();
    else
        klass = global_object->GetArgumentsClass();

#ifndef ES_FAST_COLLECTOR_ALLOCATION
    if (GC_ALIGN(sizeof(ES_Arguments_Object)) + length * GC_ALIGN(sizeof(ES_Special_Aliased)) < LARGE_OBJECT_LIMIT)
    {
        void *first_aliased;

        GC_ALLOCATE_WITH_EXTRA_ALIGNED_PAYLOAD(context, arguments, first_aliased, length * GC_ALIGN(sizeof(ES_Special_Aliased)), ES_Arguments_Object, (arguments, klass, function, length));

        OP_ASSERT(static_cast<ES_Special_Aliased *>(first_aliased) == arguments->GetAliased(0));

        ES_Boxed *boxed = arguments;

        for (unsigned index = 0; index < length; ++index)
        {
            ES_Special_Aliased *aliased = arguments->GetAliased(index);
            boxed->SplitAllocation(aliased);
            ES_Special_Aliased::Initialize(aliased, NULL);
            boxed = aliased;
        }

        arguments->has_aliased = TRUE;
    }
    else
#endif // ES_FAST_COLLECTOR_ALLOCATION
        GC_ALLOCATE(context, arguments, ES_Arguments_Object, (arguments, klass, function, length));

    ES_CollectorLock gclock(context);

    arguments->AllocateProperties(context);

    ES_Compact_Indexed_Properties *indexed = ES_Compact_Indexed_Properties::Make(context, ES_Compact_Indexed_Properties::AppropriateCapacity(length));

    if (is_strict_mode)
    {
        arguments->SetIndexedProperties(indexed);

        ES_Special_Property *throw_type_error = global_object->GetThrowTypeErrorProperty();

        arguments->PutCachedAtIndex(ES_PropertyIndex(0), length);
        arguments->PutCachedAtIndex(ES_PropertyIndex(1), throw_type_error);
        arguments->PutCachedAtIndex(ES_PropertyIndex(2), throw_type_error);
    }
    else
    {
        indexed->SetHasSpecialProperties();

        arguments->SetIndexedProperties(indexed);

        arguments->PutCachedAtIndex(ES_PropertyIndex(0), length);
        arguments->PutCachedAtIndex(ES_PropertyIndex(1), function);
    }

    return arguments;
}

/* static */ ES_Arguments_Object *
ES_Arguments_Object::Make(ES_Context *context, ES_Function *function, ES_Value_Internal *registers, unsigned argc)
{
    ES_Arguments_Object *arguments = Make(context, function, argc);

    if (!function->GetFunctionCode()->data->is_strict_mode)
    {
        ES_Value_Internal *values = registers + 2;
        ES_CollectorLock gclock(context);

        for (unsigned index = 0; index < argc; ++index)
        {
            ES_Special_Aliased *alias;

            if (arguments->has_aliased)
            {
                alias = arguments->GetAliased(index);
                alias->property = values + index;
            }
            else
                alias = ES_Special_Aliased::Make(context, values + index);

            ES_Value_Internal value;
            value.SetBoxed(alias);
            ES_Indexed_Properties::PutL(context, arguments, index, value, arguments);
        }
    }

    return arguments;
}
