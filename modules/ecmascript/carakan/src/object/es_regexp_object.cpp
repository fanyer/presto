/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  1999-2004
 */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"

#include "modules/ecmascript/carakan/src/builtins/es_regexp_builtins.h"
#include "modules/ecmascript/carakan/src/object/es_regexp_object.h"
#include "modules/ecmascript/carakan/src/object/es_global_object.h"
#include "modules/regexp/include/regexp_advanced_api.h"

static JString *
CreateEscapedSource(ES_Context *context, JString *source)
{
    const uni_char *storage = Storage(context, source);
    unsigned length = Length(source), additional = 0, index;
    BOOL in_class = FALSE;

    for (index = 0; index < length; ++index)
        switch (storage[index])
        {
        case '[':
            in_class = TRUE;
            break;

        case ']':
            in_class = FALSE;
            break;

        case '\\':
            ++index;
            break;

        case '/':
            if (!in_class)
                ++additional;
        }

    if (additional == 0)
        return source;
    else
    {
        JString *escaped = JString::Make(context, length + additional);
        uni_char *ptr = Storage(context, escaped);

        for (index = 0; index < length; ++index)
        {
            switch (storage[index])
            {
            case '[':
                in_class = TRUE;
                break;

            case ']':
                in_class = FALSE;
                break;

            case '\\':
                *ptr++ = storage[index++];
                *ptr++ = storage[index];
                continue;

            case '/':
                if (!in_class)
                    *ptr++ = '\\';
            }

            *ptr++ = storage[index];
        }

        return escaped;
    }
}

class ES_RegExpSuspension
    : public RegExpSuspension,
      public ES_StackGCRoot
{
public:
    ES_RegExpSuspension(ES_Execution_Context *context)
        : ES_StackGCRoot(context),
          context(context),
          memory(NULL)
    {
    }

    ~ES_RegExpSuspension();

    virtual void Yield();
    virtual void *AllocateL(unsigned nbytes);

    virtual void GCTrace();

private:
    ES_Execution_Context *context;
    ES_Box *memory;
    BOOL allocating;
};

ES_RegExpSuspension::~ES_RegExpSuspension()
{
    ES_Box *box = memory;

    while (box)
    {
        ES_Box *next = *reinterpret_cast<ES_Box **>(box->Unbox());

        context->heap->Free(box);

        box = next;
    }
}

/* virtual */ void
ES_RegExpSuspension::Yield()
{
    context->CheckOutOfTime();
}

/* virtual */ void *
ES_RegExpSuspension::AllocateL(unsigned nbytes)
{
    ES_Box *box = ES_Box::Make(context, sizeof(void *) + nbytes);

    *reinterpret_cast<ES_Box **>(box->Unbox()) = memory;
    memory = box;

    return box->Unbox() + sizeof(void *);
}

/* virtual */ void
ES_RegExpSuspension::GCTrace()
{
    ES_Box *box = memory;

    while (box)
    {
        GC_PUSH_BOXED(context->heap, box);
        box = *reinterpret_cast<ES_Box **>(box->Unbox());
    }
}

/* static */ BOOL
ES_RegExp_Object::ParseFlags(ES_Context *context, RegExpFlags &flags, unsigned &flagbits, JString *source)
{
    flags.ignore_case = NO;
    flags.multi_line = NO;
    flags.ignore_whitespace = FALSE;
    flags.searching = TRUE;
    flagbits = 0;

    if (source)
    {
        const uni_char *storage = Storage(context, source);
        unsigned length = Length(source);

        for (unsigned index = 0; index < length; ++index)
            switch (storage[index])
            {
            case 'g': if (flagbits & REGEXP_FLAG_GLOBAL) goto return_false; else flagbits |= REGEXP_FLAG_GLOBAL; break;
            case 'i': if (flagbits & REGEXP_FLAG_IGNORECASE) goto return_false; else { flagbits |= REGEXP_FLAG_IGNORECASE; flags.ignore_case = YES; } break;
            case 'm': if (flagbits & REGEXP_FLAG_MULTILINE) goto return_false; else { flagbits |= REGEXP_FLAG_MULTILINE; flags.multi_line = YES; } break;
#ifdef ES_NON_STANDARD_REGEXP_FEATURES
            case 'x': if (flagbits & REGEXP_FLAG_EXTENDED) goto return_false; else { flagbits |= REGEXP_FLAG_EXTENDED; flags.ignore_whitespace = TRUE; } break;
            case 'y': if (flagbits & REGEXP_FLAG_NOSEARCH) goto return_false; else { flagbits |= REGEXP_FLAG_NOSEARCH; flags.searching = FALSE; } break;
#endif // ES_NON_STANDARD_REGEXP_FEATURES
            default:
                goto return_false;
            }
    }

    return TRUE;

return_false:
    if (ES_Execution_Context *exec_context = context->GetExecutionContext())
        exec_context->ThrowSyntaxError("Invalid RegExp flags");

    return FALSE;
}

/* static */ ES_RegExp_Object *
ES_RegExp_Object::Make(ES_Context *context, ES_Global_Object *global_object, JString *source, RegExpFlags &flags, unsigned flagbits, BOOL escape_source)
{
    ES_SuspendedCreateRegExp suspended(Storage(context, source), Length(source), &flags);

    suspended.Execute(context);

    if (OpStatus::IsError(suspended.status))
        if (OpStatus::IsMemoryError(suspended.status))
            context->AbortOutOfMemory();
        else
            return NULL;

    ES_RegExp_Information info;

    info.regexp = suspended.regexp;
    info.flags = flagbits;

    ES_RegExp_Object *regexp;

    // FIXME: OOM handling.
    unsigned ncaptures = info.regexp->GetNumberOfCaptures();
    unsigned extra = ncaptures * sizeof(RegExpMatch);
    GC_ALLOCATE_WITH_EXTRA(context, regexp, extra, ES_RegExp_Object, (regexp, global_object->GetRegExpClass(), global_object, info));

    info.regexp->DecRef();

    ES_CollectorLock gclock(context);

    regexp->SetProperties(context, escape_source ? CreateEscapedSource(context, source) : source);

    return regexp;
}

/* static */ ES_RegExp_Object *
ES_RegExp_Object::Make(ES_Context *context, ES_Global_Object *global_object, const ES_RegExp_Information &info, JString *source)
{
    ES_RegExp_Object *regexp;

    unsigned ncaptures = info.regexp->GetNumberOfCaptures();
    unsigned extra = ncaptures * sizeof(RegExpMatch);
    GC_ALLOCATE_WITH_EXTRA(context, regexp, extra, ES_RegExp_Object, (regexp, global_object->GetRegExpClass(), global_object, info));
    ES_CollectorLock gclock(context);

    regexp->flags = info.flags;
    regexp->SetProperties(context, source);

#ifdef ES_NATIVE_SUPPORT
    /* Regular expression literals are JIT:ed immediately by the lexer, unless
       they're not supported, or JIT was disabled when the script was compiled.
       If the latter happens, and JIT is enabled, we can't dynamically JIT the
       regexp, because we'd then use the wrong allocator for executable memory
       and the cleanup would be wrong. */
    if (!regexp->native_matcher)
        regexp->calls = UINT_MAX;
#endif // ES_NATIVE_SUPPORT

    return regexp;
}

/* static */ ES_RegExp_Object *
ES_RegExp_Object::MakePrototypeObject(ES_Context *context, ES_Global_Object *global_object, ES_Class *&instance)
{
    JString **idents = context->rt_data->idents;
    ES_Class_Singleton *prototype_class = ES_Class::MakeSingleton(context, global_object->GetObjectPrototype(), "RegExp", idents[ESID_RegExp], ES_RegExpBuiltins::ES_RegExpBuiltinsCount);
    prototype_class->Prototype()->AddInstance(context, prototype_class, TRUE);

    ES_RegExpBuiltins::PopulatePrototypeClass(context, prototype_class);

    RegExpFlags re_flags;
    re_flags.ignore_case = NO;
    re_flags.multi_line = NO;

    ES_SuspendedCreateRegExp suspended(UNI_L("(?:)"), 4, &re_flags);

    suspended.Execute(context);
    if (OpStatus::IsError(suspended.status))
        if (OpStatus::IsMemoryError(suspended.status))
            context->AbortOutOfMemory();
        else
            return NULL;

    ES_RegExp_Information re_info;

    re_info.regexp = suspended.regexp;
    re_info.flags = 0;
    re_info.source = UINT_MAX;

    ES_RegExp_Object *prototype_object;

    unsigned ncaptures = re_info.regexp->GetNumberOfCaptures();
    unsigned extra = ncaptures * sizeof(RegExpMatch);
    GC_ALLOCATE_WITH_EXTRA(context, prototype_object, extra, ES_RegExp_Object, (prototype_object, prototype_class, global_object, re_info));
    re_info.regexp->DecRef();
    ES_CollectorLock gclock(context);

    prototype_class->AddInstance(context, prototype_object);

    prototype_object->flags = re_info.flags;
    prototype_object->AllocateProperties(context);
    ES_RegExpBuiltins::PopulatePrototype(context, global_object, prototype_object);

    ES_Class *sub_object_class = ES_Class::MakeRoot(context, prototype_object, "RegExp", idents[ESID_RegExp], TRUE);
    prototype_object->SetSubObjectClass(context, sub_object_class);

    sub_object_class = ES_Class::ExtendWithL(context, sub_object_class, idents[ESID_lastIndex], ES_Property_Info(DE|DD), ES_STORAGE_WHATEVER);
    sub_object_class = ES_Class::ExtendWithL(context, sub_object_class, idents[ESID_source], ES_Property_Info(RO|DE|DD), ES_STORAGE_WHATEVER);
    sub_object_class = ES_Class::ExtendWithL(context, sub_object_class, idents[ESID_global], ES_Property_Info(RO|DE|DD), ES_STORAGE_WHATEVER);
    sub_object_class = ES_Class::ExtendWithL(context, sub_object_class, idents[ESID_ignoreCase], ES_Property_Info(RO|DE|DD), ES_STORAGE_WHATEVER);
    instance = ES_Class::ExtendWithL(context, sub_object_class, idents[ESID_multiline], ES_Property_Info(RO|DE|DD), ES_STORAGE_WHATEVER);

    return prototype_object;
}

JString *
ES_RegExp_Object::GetFlags(ES_Context *context)
{
    char buffer[5], *ptr = buffer; /* ARRAY OK jl 2011-02-26 */

    if (flags & REGEXP_FLAG_GLOBAL)
        *ptr++ = 'g';
    if (flags & REGEXP_FLAG_IGNORECASE)
        *ptr++ = 'i';
    if (flags & REGEXP_FLAG_MULTILINE)
        *ptr++ = 'm';
#ifdef ES_NON_STANDARD_REGEXP_FEATURES
    if (flags & REGEXP_FLAG_EXTENDED)
        *ptr++ = 'x';
    if (flags & REGEXP_FLAG_NOSEARCH)
        *ptr++ = 'y';
#endif // ES_NON_STANDARD_REGEXP_FEATURES

    return JString::Make(context, buffer, ptr - buffer);
}

void
ES_RegExp_Object::GetFlags(RegExpFlags &flags, unsigned &flagbits)
{
    flagbits = this->flags;

    flags.ignore_case = ((flagbits & REGEXP_FLAG_IGNORECASE) != 0) ? YES : NO;
    flags.multi_line = ((flagbits & REGEXP_FLAG_MULTILINE) != 0) ? YES : NO;
#ifdef ES_NON_STANDARD_REGEXP_FEATURES
    flags.ignore_whitespace = (flagbits & REGEXP_FLAG_EXTENDED) != 0;
    flags.searching = (flagbits & REGEXP_FLAG_NOSEARCH) == 0;
#else
    flags.ignore_whitespace = FALSE;
    flags.searching = TRUE;
#endif // ES_NON_STANDARD_REGEXP_FEATURES
}

RegExpMatch *
ES_RegExp_Object::Exec(ES_Execution_Context *context, JString *string, unsigned index)
{
    BOOL matched;

    if (string == previous_string_positive && index == previous_index_positive)
        matched = TRUE;
    else if (string == previous_string_negative[0] && index == previous_index_negative[0] ||
             string == previous_string_negative[1] && index == previous_index_negative[1])
        matched = FALSE;
    else
    {
        if (last_ctor)
            last_ctor->BeforeMatch(this);

        const uni_char *storage = Storage(context, string);
        unsigned length = Length(string);

#ifdef ES_NATIVE_SUPPORT
        if (native_matcher)
            use_native_matcher: matched = native_matcher(GetMatchArray(), storage, index, length - index);
        else if (calls != UINT_MAX && context->UseNativeDispatcher() &&
                 (++calls > ES_REGEXP_JIT_CALLS_THRESHOLD || length > ES_REGEXP_JIT_LENGTH_THRESHOLD) &&
                 CreateNativeMatcher(context))
            goto use_native_matcher;
        else
#endif // ES_NATIVE_SUPPORT
        {
            ES_RegExpSuspension suspend(context);

#ifdef ES_NON_STANDARD_REGEXP_FEATURES
            matched = value->ExecL(storage, length, index, GetMatchArray(), &suspend, (flags & REGEXP_FLAG_NOSEARCH) == 0);
#else // ES_NON_STANDARD_REGEXP_FEATURES
            matched = value->ExecL(storage, length, index, GetMatchArray(), &suspend);
#endif // ES_NON_STANDARD_REGEXP_FEATURES
        }

        if (matched)
        {
            previous_string_positive = string;
            previous_index_positive = index;
        }
        else
        {
            previous_string_negative[1] = previous_string_negative[0];
            previous_index_negative[1] = previous_index_negative[0];
            previous_string_negative[0] = string;
            previous_index_negative[0] = index;

            /* A failed match still tramples 'match_array', so a cache of a
               positive previous match must be invalidated. */
            previous_string_positive = NULL;
        }
    }

    if (matched)
    {
        last_ctor = GetRegExpConstructor(context);
        last_ctor->SetCaptures(this, string);

        return GetMatchArray();
    }
    else
        return NULL;
}

unsigned
ES_RegExp_Object::GetNumberOfCaptures()
{
    return value->GetNumberOfCaptures();
}

#ifdef ES_NATIVE_SUPPORT

class ES_SuspendedCreateNativeDispatcher
    : public ES_SuspendedCall
{
public:
    ES_SuspendedCreateNativeDispatcher(ES_Execution_Context *context, RegExp *regexp)
        : context(context),
          regexp(regexp)
    {
        context->SuspendedCall(this);
    }

    virtual void DoCall(ES_Execution_Context *context)
    {
        status = regexp->CreateNativeMatcher(context->heap->GetExecutableMemory());
    }

    OP_STATUS status;

private:
    ES_Execution_Context *context;
    RegExp *regexp;
};

RegExpNativeMatcher *
ES_RegExp_Object::CreateNativeMatcher(ES_Execution_Context *context)
{
    if (calls != UINT_MAX)
    {
        ES_SuspendedCreateNativeDispatcher call(context, value);

        if (call.status == OpStatus::OK)
            return native_matcher = value->GetNativeMatcher();
        else if (call.status == OpStatus::ERR)
            calls = UINT_MAX;
        else
            context->AbortOutOfMemory();
    }

    return NULL;
}

#endif // ES_NATIVE_SUPPORT

ES_RegExp_Constructor *
ES_RegExp_Object::GetRegExpConstructor(ES_Execution_Context *context)
{
    return context->GetGlobalObject()->GetRegExpConstructor();
}

void
ES_RegExp_Object::Update(ES_Context *context, RegExp *new_value, JString *new_source, unsigned new_flags)
{
    if (last_ctor)
        last_ctor->BeforeMatch(this);

    /* Update the matches array -- if new_value has fewer captures, continue using. If not, flag it as invalid
       and use new_value's match array. Which should be fine, as it is not shared. */
    bool valid_matches = false;

    if (value && new_value && value->GetNumberOfCaptures() > new_value->GetNumberOfCaptures())
        valid_matches = true;

    RegExpMatch *matches = GetMatchArray();

    for (unsigned index = 0; index < static_cast<unsigned>(value->GetNumberOfCaptures()) + 1; ++index)
    {
        matches[index].start  = 0;
        matches[index].length = UINT_MAX;
    }

    if (!valid_matches)
    {
        unsigned ncaptures = 1 + new_value->GetNumberOfCaptures();
        dynamic_match_array = ES_Box::Make(context, ncaptures * sizeof(RegExpMatch));
        RegExpMatch *matches = reinterpret_cast<RegExpMatch *>(dynamic_match_array->Unbox());
        for (unsigned index = 0; index < ncaptures; index++)
            matches[index].length = UINT_MAX;
    }
    value->DecRef();

    value = new_value;
    flags = new_flags;

#ifdef ES_NATIVE_SUPPORT
    native_matcher = value->GetNativeMatcher();
    calls = 0;
#endif // ES_NATIVE_SUPPORT

    SetProperties(context, new_source);

    previous_string_positive = previous_string_negative[0] = previous_string_negative[1] = NULL;
}

/* static */ void
ES_RegExp_Object::Initialize(ES_RegExp_Object *regexp, ES_Class *klass, ES_Global_Object *global_object, const ES_RegExp_Information &info)
{
#ifdef ES_REGEXP_IS_CALLABLE
    ES_Function::Initialize(regexp, klass, global_object, NULL, ES_RegExpBuiltins::exec, NULL);
#else
    ES_Function::Initialize(regexp, klass, global_object, NULL, NULL, NULL);
#endif // ES_REGEXP_IS_CALLABLE
    regexp->ChangeGCTag(GCTAG_ES_Object_RegExp);
    regexp->last_ctor = NULL;
    regexp->value = info.regexp;
    regexp->flags = info.flags;
#ifdef ES_NATIVE_SUPPORT
    regexp->native_matcher = regexp->value->GetNativeMatcher();
    regexp->calls = 0;
#endif // ES_NATIVE_SUPPORT

    unsigned ncaptures = 1 + regexp->value->GetNumberOfCaptures();
    for (unsigned index = 0; index < ncaptures; ++index)
        regexp->match_array[index].length = UINT_MAX;
    regexp->dynamic_match_array = 0;

    regexp->previous_string_positive = NULL;
    regexp->previous_string_negative[0] = NULL;
    regexp->previous_string_negative[1] = NULL;
    regexp->SetNeedDestroy();
    info.regexp->IncRef();
}

/* static */ void
ES_RegExp_Information::Destroy(ES_RegExp_Information *re_info)
{
    if (re_info && re_info->regexp)
        re_info->regexp->DecRef();
}

/* static */ void
ES_RegExp_Object::Destroy(ES_RegExp_Object *regexp)
{
    regexp->value->DecRef();
}

void
ES_RegExp_Object::SetProperties(ES_Context *context, JString *source)
{
    if (!properties)
        AllocateProperties(context);

    PutCachedAtIndex(ES_PropertyIndex(LASTINDEX), 0); // lastIndex

    PutCachedAtIndex(ES_PropertyIndex(SOURCE), source); // source

    ES_Value_Internal value;

    value.SetBoolean((flags & REGEXP_FLAG_GLOBAL) != 0);
    PutCachedAtIndex(ES_PropertyIndex(GLOBAL), value); // global

    value.SetBoolean((flags & REGEXP_FLAG_IGNORECASE) != 0);
    PutCachedAtIndex(ES_PropertyIndex(IGNORECASE), value); // ignoreCase

    value.SetBoolean((flags & REGEXP_FLAG_MULTILINE) != 0);
    PutCachedAtIndex(ES_PropertyIndex(MULTILINE), value); // multiline
}

static ES_Special_RegExp_Capture *
ES_Special_RegExp_Capture_Make(ES_Context *context, ES_RegExp_Constructor::IndexType index)
{
    return ES_Special_RegExp_Capture::Make(context, static_cast<unsigned>(index));
}

/* static */ ES_RegExp_Constructor *
ES_RegExp_Constructor::Make(ES_Context *context, ES_Global_Object *global_object)
{
    ES_RegExp_Constructor *ctor;

    GC_ALLOCATE(context, ctor, ES_RegExp_Constructor, (ctor, global_object));

    ctor->AllocateProperties(context);
    ctor->data.builtin.information = ESID_RegExp | (2 << 16);

    ctor->PutCachedAtIndex(ES_PropertyIndex(ES_Function::LENGTH), 2);
    ctor->PutCachedAtIndex(ES_PropertyIndex(ES_Function::NAME), context->rt_data->idents[ESID_RegExp]);
    ctor->PutCachedAtIndex(ES_PropertyIndex(ES_Function::PROTOTYPE), global_object->GetRegExpPrototype());
    ctor->PutCachedAtIndex(ES_PropertyIndex(ES_RegExp_Constructor::INPUT), ES_Value_Internal());

    unsigned index = 1;

    for (index = 1; index <= 9; ++index)
    {
        ES_Value_Internal capture;
        capture.SetBoxed(ES_Special_RegExp_Capture::Make(context, index));
        ctor->PutCachedAtIndex(ES_PropertyIndex(ES_RegExp_Constructor::INPUT + index), capture);
    }

    ES_Value_Internal value;

    value.SetBoxed(ES_Special_RegExp_Capture_Make(context, INDEX_INPUT));

    index += ES_RegExp_Constructor::INPUT;
    ctor->PutCachedAtIndex(ES_PropertyIndex(index), value);

    value.SetBoxed(ES_Special_RegExp_Capture_Make(context, INDEX_LASTMATCH));
    ctor->PutCachedAtIndex(ES_PropertyIndex(++index), value);
    ctor->PutCachedAtIndex(ES_PropertyIndex(++index), value);

    value.SetBoxed(ES_Special_RegExp_Capture_Make(context, INDEX_LASTPAREN));
    ctor->PutCachedAtIndex(ES_PropertyIndex(++index), value);
    ctor->PutCachedAtIndex(ES_PropertyIndex(++index), value);

    value.SetBoxed(ES_Special_RegExp_Capture_Make(context, INDEX_LEFTCONTEXT));
    ctor->PutCachedAtIndex(ES_PropertyIndex(++index), value);
    ctor->PutCachedAtIndex(ES_PropertyIndex(++index), value);

    value.SetBoxed(ES_Special_RegExp_Capture_Make(context, INDEX_RIGHTCONTEXT));
    ctor->PutCachedAtIndex(ES_PropertyIndex(++index), value);
    ctor->PutCachedAtIndex(ES_PropertyIndex(++index), value);

    value.SetBoxed(ES_Special_RegExp_Capture_Make(context, INDEX_MULTILINE));
    ctor->PutCachedAtIndex(ES_PropertyIndex(++index), value);
    ctor->PutCachedAtIndex(ES_PropertyIndex(++index), value);

    return ctor;
}

/* static */ void
ES_RegExp_Constructor::Initialize(ES_RegExp_Constructor *self, ES_Global_Object *global_object)
{
    ES_Function::Initialize(self, global_object->GetRegExpConstructorClass(), global_object, NULL, ES_RegExpBuiltins::constructor_call, ES_RegExpBuiltins::constructor_construct);
    self->ChangeGCTag(GCTAG_ES_Object_RegExp_CTOR);
    self->object = NULL;
    self->string = NULL;
    self->multiline = FALSE;
    self->use_backup = FALSE;
}

void
ES_RegExp_Constructor::GetCapture(ES_Context *context, ES_Value_Internal &value, unsigned index)
{
    JString *result;

    if (object && index - 1 < object->GetNumberOfCaptures() && (use_backup ? backup_matches : object->GetMatchArray())[index].length != UINT_MAX)
    {
        if (use_backup)
            result = JString::Make(context, string, backup_matches[index].start, backup_matches[index].length);
        else
            result = JString::Make(context, string, object->GetMatchArray()[index].start, object->GetMatchArray()[index].length);
    }
    else
        result = context->rt_data->strings[STRING_empty];

    value.SetString(result);
}

void
ES_RegExp_Constructor::GetLastMatch(ES_Context *context, ES_Value_Internal &value)
{
    JString *result;

    if (object)
    {
        if (use_backup)
            result = JString::Make(context, string, backup_matches[0].start, backup_matches[0].length);
        else
            result = JString::Make(context, string, matches[0].start, matches[0].length);
    }
    else
        result = context->rt_data->strings[STRING_empty];

    value.SetString(result);
}

void
ES_RegExp_Constructor::GetLastParen(ES_Context *context, ES_Value_Internal &value)
{
    if (object)
        GetCapture(context, value, es_minu(object->GetNumberOfCaptures(), 10));
    else
        value.SetString(context->rt_data->strings[STRING_empty]);
}

void
ES_RegExp_Constructor::GetLeftContext(ES_Context *context, ES_Value_Internal &value)
{
    JString *result;

    if (object)
    {
        if (use_backup)
            result = JString::Make(context, string, 0, backup_matches[0].start);
        else
            result = JString::Make(context, string, 0, matches[0].start);
	}
    else
        result = context->rt_data->strings[STRING_empty];

    value.SetString(result);
}

void
ES_RegExp_Constructor::GetRightContext(ES_Context *context, ES_Value_Internal &value)
{
    JString *result;

    if (object)
    {
        unsigned offset = use_backup ? (backup_matches[0].start + backup_matches[0].length) : (matches[0].start + matches[0].length);
        result = JString::Make(context, string, offset, Length(string) - offset);
    }
    else
        result = context->rt_data->strings[STRING_empty];

    value.SetString(result);
}

void
ES_RegExp_Constructor::BackupMatches()
{
    if (matches != backup_matches && !use_backup)
    {
        backup_matches[0] = matches[0];

        unsigned ncaptures = object->GetNumberOfCaptures();

        for (unsigned index = 1, count = es_minu(ncaptures, 9); index <= count; ++index)
            backup_matches[index] = matches[index];

        if (ncaptures > 9)
            backup_matches[10] = matches[ncaptures];

        use_backup = TRUE;
    }
}

/* virtual */ void
ES_SuspendedCreateRegExp::DoCall(ES_Execution_Context *context)
{
    regexp = OP_NEW(RegExp, ());

    if (!regexp)
        status = OpStatus::ERR_NO_MEMORY;
    else
    {
        status = regexp->Init(pattern, length, NULL, flags);

        if (OpStatus::IsError(status))
            regexp->DecRef();
    }
}

void
ES_SuspendedCreateRegExp::Execute(ES_Context *context)
{
    if (!context->IsUsingStandardStack())
        static_cast<ES_Execution_Context *>(context)->SuspendedCall(this);
    else
        DoCall(static_cast<ES_Execution_Context *>(context));
}

/* virtual */ void
ES_SuspendedUpdateRegExp::DoCall(ES_Execution_Context *context)
{
    RegExp *regexp = OP_NEW(RegExp, ());

    if (!regexp)
        status = OpStatus::ERR_NO_MEMORY;
    else
    {
        status = regexp->Init(Storage(context, source), Length(source), NULL, flags);

        if (OpStatus::IsError(status))
            regexp->DecRef();
        else
            update_object->Update(context, regexp, source, flagbits);
    }
}
