/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/builtins/es_builtins.h"
#include "modules/ecmascript/carakan/src/builtins/es_regexp_builtins.h"
#include "modules/ecmascript/carakan/src/object/es_regexp_object.h"
#include "modules/ecmascript/carakan/src/object/es_array_object.h"
#include "modules/regexp/include/regexp_advanced_api.h"

#define ES_THIS_OBJECT(msg) if (!argv[-2].IsObject() || !argv[-2].GetObject(context)->IsRegExpObject()) { context->ThrowTypeError(msg); return FALSE; } ES_RegExp_Object *this_object = static_cast<ES_RegExp_Object *>(argv[-2].GetObject(context));

/* static */ BOOL
ES_RegExpBuiltins::constructor_call(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    if (argc >= 1 && argv[0].IsObject() && argv[0].GetObject(context)->IsRegExpObject() && (argc < 2 || argv[1].IsUndefined()))
    {
        return_value->SetObject(argv[0].GetObject());
        return TRUE;
    }
    else
        return constructor_construct(context, argc, argv, return_value);
}

/* static */ BOOL
ES_RegExpBuiltins::constructor_construct(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_RegExp_Object *existing;
    JString *source;

    if (argc >= 1 && argv[0].IsObject() && argv[0].GetObject(context)->IsRegExpObject())
    {
        existing = static_cast<ES_RegExp_Object *>(argv[0].GetObject(context));

        ES_Value_Internal value;
        existing->GetCachedAtIndex(ES_PropertyIndex(1), value);
        source = value.GetString();
    }
    else
    {
        existing = NULL;
        if (argc >= 1 && !argv[0].IsUndefined())
        {
            if (!argv[0].ToString(context))
                return FALSE;
            source = argv[0].GetString();
        }
        else
            source = context->rt_data->strings[STRING_empty];
    }

    ES_CollectorLock gclock(context);

    RegExpFlags flags;
    unsigned flagbits;

    if (argc >= 2 && !argv[1].IsUndefined())
    {
        if (existing)
        {
            context->ThrowTypeError("RegExp constructor called with invalid arguments");
            return FALSE;
        }

        if (!argv[1].ToString(context))
            return FALSE;

        if (!ES_RegExp_Object::ParseFlags(context, flags, flagbits, argv[1].GetString()))
            return FALSE;
    }
    else if (existing)
        existing->GetFlags(flags, flagbits);
    else
        ES_RegExp_Object::ParseFlags(context, flags, flagbits, context->rt_data->strings[STRING_empty]);

    ES_Object *object = ES_GET_GLOBAL_OBJECT()->GetDynamicRegExp(context, source, flags, flagbits);

    if (!object)
    {
        context->ThrowSyntaxError("RegExp constructor: invalid regular expression");
        return FALSE;
    }
    else
    {
        return_value->SetObject(object);
        return TRUE;
    }
}

/* static */ BOOL
ES_RegExpBuiltins::test(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_OBJECT("RegExp.prototype.test: this is not a RegExp object");

    ES_Value_Internal v;

    if (argc == 0)
        this_object->GetRegExpConstructor(context)->GetCachedAtIndex(ES_PropertyIndex(ES_RegExp_Constructor::INPUT), v);
    else
        v = argv[0];

    if (!v.ToString(context))
        return FALSE;

    ES_CollectorLock gclock(context, TRUE);

    JString *string = v.GetString();
    BOOL global = (this_object->GetFlagBits() & REGEXP_FLAG_GLOBAL) != 0;
    unsigned index;

    if (global)
    {
        ES_Value_Internal lastIndex;

        this_object->GetCachedAtIndex(ES_PropertyIndex(0), lastIndex);

        if (!lastIndex.ToNumber(context))
            return FALSE;

        double dindex = lastIndex.GetNumAsInteger();

        if (dindex < 0 || dindex > Length(string))
        {
            this_object->PutCachedAtIndex(ES_PropertyIndex(ES_RegExp_Object::LASTINDEX), static_cast<UINT32>(0));
            return_value->SetFalse();
            return TRUE;
        }

        index = lastIndex.GetNumAsUInt32();
    }
    else
        index = 0;

    RegExpMatch *matches = this_object->Exec(context, string, index);
    UINT32 last_index = 0;

    if (matches)
    {
        if (global)
            last_index = static_cast<UINT32>(matches[0].start + matches[0].length);

        return_value->SetTrue();
    }
    else
        return_value->SetFalse();

    if (global)
        this_object->PutCachedAtIndex(ES_PropertyIndex(ES_RegExp_Object::LASTINDEX), last_index);

    return TRUE;
}

/* static */ BOOL
ES_RegExpBuiltins::exec(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_RegExp_Object *this_object;

    if (!argv[-2].IsObject() || !argv[-2].GetObject(context)->IsRegExpObject())
    {
        if (argv[-2].IsObject() && argv[-1].GetObject(context)->IsRegExpObject())
            this_object = static_cast<ES_RegExp_Object *>(argv[-1].GetObject(context));
        else
        {
            context->ThrowTypeError("RegExp.prototype.exec: this is not a RegExp object");
            return FALSE;
        }
    }
    else
        this_object = static_cast<ES_RegExp_Object *>(argv[-2].GetObject(context));

    BOOL global = (this_object->GetFlagBits() & REGEXP_FLAG_GLOBAL) != 0;
    ES_Value_Internal v;

    if (argc == 0)
        this_object->GetRegExpConstructor(context)->GetCachedAtIndex(ES_PropertyIndex(ES_RegExp_Constructor::INPUT), v);
    else
        v = argv[0];

    if (!v.ToString(context))
        return FALSE;
    JString *string = v.GetString();

    ES_CollectorLock gclock(context, TRUE);

    ES_Value_Internal lastIndex;
    this_object->GetCachedAtIndex(ES_PropertyIndex(ES_RegExp_Object::LASTINDEX), lastIndex);
    if (!lastIndex.ToNumber(context))
        return FALSE;

    unsigned index;
    if (global)
    {
        double dindex = lastIndex.GetNumAsInteger();
        if (dindex < 0 || dindex > Length(string))
            goto failed;

        index = lastIndex.GetNumAsUInt32();
    }
    else
        index = 0;

    if (RegExpMatch *matches = this_object->Exec(context, string, index))
    {
        unsigned nmatches = this_object->GetValue()->GetNumberOfCaptures() + 1;

        ES_Object *array = ES_Object::MakeArray(context, ES_GET_GLOBAL_OBJECT()->GetRegExpExecResultClass(), ES_Compact_Indexed_Properties::AppropriateCapacity(nmatches));

        array->ChangeGCTag(GCTAG_ES_Object_Array);

        ES_Array::SetLength(context, array, nmatches);

        array->PutCachedAtIndex(ES_PropertyIndex(1), matches[0].start);
        array->PutCachedAtIndex(ES_PropertyIndex(2), string);

        ES_Compact_Indexed_Properties *indexed = static_cast<ES_Compact_Indexed_Properties *>(array->GetIndexedProperties());
        ES_Value_Internal *values = indexed->GetValues();

        for (unsigned index = 0; index < nmatches; ++index)
            SetMatchValue(context, values[index], string, matches[index]);

        if (global)
            this_object->PutCachedAtIndex(ES_PropertyIndex(ES_RegExp_Object::LASTINDEX), static_cast<UINT32>(matches[0].start + matches[0].length));

        return_value->SetObject(array);
        return TRUE;
    }

failed:
    this_object->PutCachedAtIndex(ES_PropertyIndex(ES_RegExp_Object::LASTINDEX), static_cast<UINT32>(0));

    return_value->SetNull();
    return TRUE;
}

/* static */ BOOL
ES_RegExpBuiltins::toString(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_OBJECT("RegExp.prototype.toString: this is not a RegExp object");

    ES_Value_Internal value;
    this_object->GetCachedAtIndex(ES_PropertyIndex(1), value);
    JString *source = value.GetString();

    if (Length(source) == 0)
        source = context->rt_data->strings[STRING_empty_regexp];

    unsigned flagbits = this_object->GetFlagBits();

    BOOL g = (flagbits & REGEXP_FLAG_GLOBAL) != 0;
    BOOL i = (flagbits & REGEXP_FLAG_IGNORECASE) != 0;
    BOOL m = (flagbits & REGEXP_FLAG_MULTILINE) != 0;

    unsigned length = 1 + Length(source) + 1 + static_cast<int>(g) + static_cast<int>(i) + static_cast<int>(m);

#ifdef ES_NON_STANDARD_REGEXP_FEATURES
    BOOL x = (flagbits & REGEXP_FLAG_EXTENDED) != 0;
    BOOL y = (flagbits & REGEXP_FLAG_NOSEARCH) != 0;

    length += x + y;
#endif // ES_NON_STANDARD_REGEXP_FEATURES

    JString *result = JString::Make(context, length);
    uni_char *storage = Storage(context, result);

    *storage++ = '/';
    op_memcpy(storage, Storage(context, source), Length(source) * sizeof(uni_char));
    storage += Length(source);
    *storage++ = '/';
    if (g) *storage++ = 'g';
    if (i) *storage++ = 'i';
    if (m) *storage++ = 'm';
#ifdef ES_NON_STANDARD_REGEXP_FEATURES
    if (x) *storage++ = 'x';
    if (y) *storage++ = 'y';
#endif // ES_NON_STANDARD_REGEXP_FEATURES

    return_value->SetString(result);
    return TRUE;
}

/* static */ BOOL
ES_RegExpBuiltins::compile(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_OBJECT("RegExp.prototype.compile: this is not a RegExp object");

    JString *source;
    ES_RegExp_Object *existing = NULL;
    if (argc >= 1 && !argv[0].IsUndefined())
    {
        ES_Object *object;
        if (argv[0].IsObject() && (object = argv[0].GetObject(context))->IsRegExpObject())
        {
            existing = static_cast<ES_RegExp_Object *>(object);
            source = existing->GetSource();
        }
        else if (!argv[0].ToString(context))
            return FALSE;
        else
            source = argv[0].GetString();
    }
    else
        source = context->rt_data->strings[STRING_empty];

    ES_CollectorLock gclock(context);

    RegExpFlags flags;
    unsigned flagbits;

    if (argc >= 2 && !argv[1].IsUndefined())
    {
        if (!argv[1].ToString(context))
            return FALSE;

        if (!ES_RegExp_Object::ParseFlags(context, flags, flagbits, argv[1].GetString()))
            return FALSE;
    }
    else if (existing)
        existing->GetFlags(flags, flagbits);
    else if (!ES_RegExp_Object::ParseFlags(context, flags, flagbits, context->rt_data->strings[STRING_empty]))
        return FALSE;

    ES_SuspendedUpdateRegExp suspended(context, this_object, source, &flags, flagbits);
    if (OpStatus::IsError(suspended.status))
        if (OpStatus::IsMemoryError(suspended.status))
            context->AbortOutOfMemory();
        else
        {
            context->ThrowSyntaxError("RegExp.compile: invalid regular expression");
            return FALSE;
        }

    return_value->SetUndefined();
    return TRUE;
}

/* static */ void
ES_RegExpBuiltins::PopulatePrototype(ES_Context *context, ES_Global_Object *global_object, ES_Object *prototype)
{
    ES_Value_Internal value;
    unsigned flags = static_cast<ES_RegExp_Object *>(prototype)->GetFlagBits();

    ASSERT_CLASS_SIZE(ES_RegExpBuiltins);

    APPEND_PROPERTY(ES_RegExpBuiltins, lastIndex,   0);
    APPEND_PROPERTY(ES_RegExpBuiltins, source,      JString::Make(context, "(?:)"));
    value.SetBoolean((flags & REGEXP_FLAG_GLOBAL) != 0);
    APPEND_PROPERTY(ES_RegExpBuiltins, global,      value);
    value.SetBoolean((flags & REGEXP_FLAG_IGNORECASE) != 0);
    APPEND_PROPERTY(ES_RegExpBuiltins, ignoreCase,  value);
    value.SetBoolean((flags & REGEXP_FLAG_MULTILINE) != 0);
    APPEND_PROPERTY(ES_RegExpBuiltins, multiline,   value);

    ES_Object *toString_fn, *test_fn, *exec_fn, *compile_fn;

    value.SetUndefined();
    APPEND_PROPERTY(ES_RegExpBuiltins, constructor, value);
    APPEND_PROPERTY(ES_RegExpBuiltins, toString, toString_fn = MAKE_BUILTIN(0, toString));
    APPEND_PROPERTY(ES_RegExpBuiltins, test, test_fn = MAKE_BUILTIN(1, test));
    APPEND_PROPERTY(ES_RegExpBuiltins, exec, exec_fn = MAKE_BUILTIN(1, exec));
    APPEND_PROPERTY(ES_RegExpBuiltins, compile, compile_fn = MAKE_BUILTIN(2, compile));

    /* Our optimization to return the same RegExp object again and again from a
       RegExp literal expression depends on the RegExp prototype changing class
       ID if it's modified, including if any of the built-in functions are
       overridden.  Setting the 'has been inlined' flag on these functions
       accomplishes this. */
    toString_fn->SetHasBeenInlined();
    test_fn->SetHasBeenInlined();
    exec_fn->SetHasBeenInlined();
    compile_fn->SetHasBeenInlined();

    ASSERT_OBJECT_COUNT(ES_RegExpBuiltins);
}

/* static */ void
ES_RegExpBuiltins::PopulatePrototypeClass(ES_Context *context, ES_Class_Singleton *prototype_class)
{
    OP_ASSERT(prototype_class->GetPropertyTable()->Capacity() >= ES_RegExpBuiltinsCount);

    JString **idents = context->rt_data->idents;
    ES_Layout_Info layout;

    DECLARE_PROPERTY(ES_RegExpBuiltins, lastIndex,      DE | DD,        ES_STORAGE_INT32);
    DECLARE_PROPERTY(ES_RegExpBuiltins, source,         DE | DD | RO,   ES_STORAGE_STRING);
    DECLARE_PROPERTY(ES_RegExpBuiltins, global,         DE | DD | RO,   ES_STORAGE_BOOLEAN);
    DECLARE_PROPERTY(ES_RegExpBuiltins, ignoreCase,     DE | DD | RO,   ES_STORAGE_BOOLEAN);
    DECLARE_PROPERTY(ES_RegExpBuiltins, multiline,      DE | DD | RO,   ES_STORAGE_BOOLEAN);

    DECLARE_PROPERTY(ES_RegExpBuiltins, constructor,    DE,             ES_STORAGE_WHATEVER);
    DECLARE_PROPERTY(ES_RegExpBuiltins, toString,       DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_RegExpBuiltins, test,           DE | FN,        ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_RegExpBuiltins, exec,           DE | FN,        ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_RegExpBuiltins, compile,        DE,             ES_STORAGE_OBJECT);
}

#undef ES_THIS_OBJECT
