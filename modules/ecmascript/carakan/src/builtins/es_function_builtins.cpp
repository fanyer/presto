/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/builtins/es_builtins.h"
#include "modules/ecmascript/carakan/src/builtins/es_function_builtins.h"
#include "modules/ecmascript/carakan/src/builtins/es_array_builtins.h"
#include "modules/ecmascript/carakan/src/object/es_indexed.h"
#include "modules/ecmascript/carakan/src/compiler/es_parser.h"
#include "modules/ecmascript/carakan/src/compiler/es_disassembler.h"

#define ES_CHECK_CALLABLE_THIS(msg) do { if (!argv[-2].IsCallable(context)) { context->ThrowTypeError(msg); return FALSE; } } while (0)

/* static */ BOOL
ES_FunctionBuiltins::constructor(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    JString *args, *body;

    if (argc < 2)
        args = context->rt_data->strings[STRING_empty];
    else
    {
        if (!argv[0].ToString(context))
            return FALSE;

        argv[0].SetString(args = Share(context, argv[0].GetString()));

        for (unsigned argi = 1; argi < argc - 1; ++argi)
        {
            if (!argv[argi].ToString(context))
                return FALSE;

            Append(context, argv[0].GetString(), ",");
            Append(context, argv[0].GetString(), argv[argi].GetString());
        }
    }

    if (argc < 1)
        body = context->rt_data->strings[STRING_empty];
    else
    {
        if (!argv[argc - 1].ToString(context))
            return FALSE;

        body = argv[argc - 1].GetString();
    }

    ES_SuspendedParseFunction suspended_parse_function(ES_GET_GLOBAL_OBJECT(), Storage(context, args), Length(args), Storage(context, body), Length(body));

    context->SuspendedCall(&suspended_parse_function);

    if (!suspended_parse_function.success)
        if (OpStatus::IsMemoryError(suspended_parse_function.status))
            context->AbortOutOfMemory();
        else
        {
            context->ThrowSyntaxError("Function constructor: failed to compile function");
            return FALSE;
        }

//#define ES_DISASSEMBLE_FN
#ifdef ES_DISASSEMBLE_FN
    ES_Disassembler disassembler(context);

    disassembler.Disassemble(suspended_parse_function.code);

    unsigned length = disassembler.GetLength();
    const uni_char *unicode = disassembler.GetOutput();
    char *ascii = OP_NEWA(char, length + 1);
    if (!ascii)
        context->AbortOutOfMemory();

    for (unsigned index = 0; index < length + 1; ++index)
        ascii[index] = (char) unicode[index];

    fprintf(stdout, "--------------------------------------------------------------------------------\n%s--------------------------------------------------------------------------------\n", ascii);
#endif // ES_DISASSEMBLE_FN

    ES_CollectorLock gclock(context);

    return_value->SetObject(ES_Function::Make(context, ES_GET_GLOBAL_OBJECT(), suspended_parse_function.code, 0));
    return TRUE;
}

/* static */ BOOL
ES_FunctionBuiltins::toString(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_CHECK_CALLABLE_THIS("Function.prototype.toString: this object not a function");

    ES_Object *fn = static_cast<ES_Object *>(argv[-2].GetObject(context));

    ES_FunctionCode *code = fn->IsHostObject() ? NULL : static_cast<ES_Function *>(fn)->GetFunctionCode();
    unsigned extra;

    if (code)
    {
        ES_FunctionCodeStatic *data = code->GetData();

        if (JString *source = data->source.GetSource(context))
        {
            return_value->SetString(source);
            return TRUE;
        }

        extra = uni_strlen(UNI_L("native"));

        if (JString *name = code->GetName())
            extra += Length(name);

        if (data->formals_count)
        {
            extra += data->formals_count - 1;
            for (unsigned i = 0; i < data->formals_count; ++i)
                extra += Length(code->GetString(data->formals_and_locals[i]));
        }
    }
    else if (fn->IsHostObject())
        extra = uni_strlen(UNI_L("builtin")) + Length(static_cast<ES_Host_Function *>(fn)->GetHostFunctionName());
    else
        extra = uni_strlen(UNI_L("builtin")) + Length(static_cast<ES_Function *>(fn)->GetBuiltInName(context));

    JString *string = JString::Make(context, uni_strlen(UNI_L("function () { /*  */ }")) + extra);
    uni_char *storage = Storage(context, string);

    uni_strcpy(storage, UNI_L("function "));
    storage += uni_strlen(UNI_L("function "));
    if (code)
    {
        ES_FunctionCodeStatic *data = code->GetData();

        if (JString *name = code->GetName())
        {
            uni_strcpy(storage, StorageZ(context, name));
            storage += Length(name);
        }
        if (data->formals_count)
        {
            *storage++ = '(';
            for (unsigned i = 0; i < data->formals_count; ++i)
            {
                if (i)
                    *storage++ = ',';
                JString *formal = code->GetString(data->formals_and_locals[i]);
                uni_strcpy(storage, StorageZ(context, formal));
                storage += Length(formal);
            }
            uni_strcpy(storage, UNI_L(") { /* native */ }"));

        }
        else
            uni_strcpy(storage, UNI_L("() { /* native */ }"));
    }
    else
    {
        JString *function_name;
        if (fn->IsHostObject())
            function_name = static_cast<ES_Host_Function *>(fn)->GetHostFunctionName();
        else
            function_name = static_cast<ES_Function *>(fn)->GetBuiltInName(context);

        uni_strncpy(storage, Storage(context, function_name), Length(function_name));
        storage += Length(function_name);
        uni_strcpy(storage, UNI_L("() { [native code] }"));
    }

    return_value->SetString(string);
    return TRUE;
}

static BOOL
CloneArray(ES_Execution_Context *context, unsigned length, ES_Object *array, ES_Value_Internal *return_value, ES_Global_Object *global_object, ES_Value_Internal &scratch_register)
{
    // Clone any 'array-like' object into array.

    unsigned capacity = 0;
    ES_Indexed_Properties *properties = array->GetIndexedProperties();
    if (properties && ES_Indexed_Properties::GetType(properties) == ES_Indexed_Properties::TYPE_COMPACT)
        capacity = static_cast<ES_Compact_Indexed_Properties *>(properties)->Capacity();

    ES_Array_Property_Iterator iter(context, array, length);
    unsigned index;

    if (length == 1)
    {
        if (iter.Next(index) && index == 0)
        {
            if (!iter.GetValue(scratch_register))
                return FALSE;

            if (scratch_register.IsNumber())
                /* The case where the single argument defines the length of the
                   array: */
                return ES_ArrayBuiltins::constructor(context, 1, &scratch_register, return_value);
        }

        iter.Reset();
    }

    ES_Object *new_array = ES_Array::Make(context, global_object, capacity, length);
    return_value->SetObject(new_array);

    unsigned next_available_index = 0;
    while (iter.Next(index))
    {
        for (unsigned empty = next_available_index; empty < index; ++empty)
            new_array->PutL(context, empty, ES_Value_Internal());

        if (!iter.GetValue(scratch_register))
            return FALSE;

        new_array->PutL(context, index, scratch_register);

        next_available_index = index + 1;
    }
    for (unsigned empty = next_available_index; empty < length; ++empty)
        new_array->PutL(context, empty, ES_Value_Internal());

    return TRUE;
}

/* static */ BOOL
ES_FunctionBuiltins::apply(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_CHECK_CALLABLE_THIS("Function.prototype.apply: this object not callable");

    ES_Value_Internal thisArg;

    if (argc >= 1)
        thisArg = argv[0];

    ES_Object *call_function_object = argv[-2].GetObject(context);
    ES_Object *call_argv_object;
    unsigned call_argc;

    if (argc < 2 || argv[1].IsNullOrUndefined())
    {
        call_argc = 0;
        call_argv_object = NULL;
    }
    else
    {
        if (!argv[1].IsObject())
        {
            context->ThrowTypeError("Function.prototype.apply: argArray is not an array-like object");
            return FALSE;
        }

        call_argv_object = argv[1].GetObject(context);

        if (call_argv_object->GetLength(context, call_argc) == PROP_GET_FAILED)
            return FALSE;
    }

    if (call_function_object->IsFunctionObject() && !call_function_object->IsHostObject())
    {
        ES_Function::BuiltIn *call = static_cast<ES_Function *>(call_function_object)->GetCall();
        if (call_argc && call && call == &ES_ArrayBuiltins::constructor)
            return CloneArray(context, call_argc, call_argv_object, return_value, ES_GET_GLOBAL_OBJECT(), argv[-1]);
    }

    if (call_argc > 1048576)
    {
        context->ThrowInternalError("Function.prototype.apply: argArray is too large");
        return FALSE;
    }

    ES_Value_Internal *registers = context->SetupFunctionCall(call_function_object, call_argc, ES_VirtualStackFrame::VIA_FUNCTION_APPLY);

    registers[0] = thisArg;
    registers[1].SetObject(call_function_object);

    if (call_argv_object)
    {
        ES_Array_Property_Iterator iter(context, call_argv_object, call_argc);
        ES_Value_Internal *call_argv = registers + 2;
        unsigned next_available_index = 0, index;

        while (iter.Next(index))
        {
            for (unsigned empty = next_available_index; empty < index; ++empty)
                call_argv[empty].SetUndefined();

            if (!iter.GetValue(call_argv[index]))
            {
                context->AbortFunctionCall(call_function_object, call_argc);
                return FALSE;
            }

            next_available_index = index + 1;
        }

        for (unsigned empty = next_available_index; empty < call_argc; ++empty)
            call_argv[empty].SetUndefined();
    }

    return context->CallFunction(registers, call_argc, return_value);
}

/* static */ BOOL
ES_FunctionBuiltins::call(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_CHECK_CALLABLE_THIS("Function.prototype.call: this object not callable");

    ES_Value_Internal thisArg;

    if (argc >= 1)
        thisArg = argv[0];

    ES_Object *call_function_object = argv[-2].GetObject(context);
    unsigned call_argc = argc == 0 ? 0 : argc - 1;

    ES_Value_Internal *registers = context->SetupFunctionCall(call_function_object, call_argc, ES_VirtualStackFrame::VIA_FUNCTION_CALL);

    registers[0] = thisArg;
    registers[1].SetObject(call_function_object);

    for (unsigned index = 0; index < call_argc; ++index)
        registers[index + 2] = argv[index + 1];

    return context->CallFunction(registers, call_argc, return_value);
}

/* static */ BOOL
ES_FunctionBuiltins::bind(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_CHECK_CALLABLE_THIS("Function.prototype.bind: this object not callable");

    ES_Value_Internal thisArg;

    if (argc >= 1)
        thisArg = argv[0];

    ES_Function *target_function = static_cast<ES_Function *>(argv[-2].GetObject(context));

    ES_Value_Internal *bound_args;
    unsigned bound_args_count;
    if (argc > 1)
        bound_args = argv + 1, bound_args_count = argc - 1;
    else
        bound_args = NULL, bound_args_count = 0;

    unsigned length;
    if (!target_function->IsHostObject() && op_strcmp(target_function->ObjectName(), "Function") == 0)
    {
        length = target_function->GetLength();
        if (length > bound_args_count)
            length -= bound_args_count;
        else
            length = 0;
    }
    else
        length = 0;

    return_value->SetObject(ES_BoundFunction::Make(context, target_function, thisArg, bound_args, bound_args_count, length));
    return TRUE;
}

static BOOL
ES_BoundCallOrConstruct(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value, BOOL is_construct)
{
    ES_BoundFunction *bound = static_cast<ES_BoundFunction *>(argv[-1].GetObject(context));

    unsigned total_argc = bound->GetBoundArgsCount() + argc;

    ES_Value_Internal *registers = context->SetupFunctionCall(bound->GetTargetFunction(), total_argc, ES_VirtualStackFrame::VIA_FUNCTION_BIND, is_construct);

    if (!is_construct)
        registers[0] = bound->GetBoundThis();

    registers[1].SetObject(bound->GetTargetFunction());

    unsigned register_index = 2;

    for (unsigned index = 0; index < bound->GetBoundArgsCount(); ++index)
        registers[register_index++] = bound->GetBoundArgs()[index];

    for (unsigned index = 0; index < argc; ++index)
        registers[register_index++] = argv[index];

    return context->CallFunction(registers, total_argc, return_value, is_construct);
}

/* static */ BOOL
ES_FunctionBuiltins::bound_call(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    return ES_BoundCallOrConstruct(context, argc, argv, return_value, FALSE);
}

/* static */ BOOL
ES_FunctionBuiltins::bound_construct(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_BoundFunction *bound = static_cast<ES_BoundFunction *>(argv[-1].GetObject(context));
    ES_Function *function = static_cast<ES_Function *>(bound->GetTargetFunction());
    if (!function->IsHostObject() && !function->GetFunctionCode() && !function->GetConstruct())
    {
        context->ThrowTypeError("Function value is not a constructor");
        return FALSE;
    }
    return ES_BoundCallOrConstruct(context, argc, argv, return_value, TRUE);
}

/* static */ void
ES_FunctionBuiltins::PopulatePrototype(ES_Context *context, ES_Global_Object *global_object, ES_Object *prototype)
{
    ES_Value_Internal undefined;

    ASSERT_CLASS_SIZE(ES_FunctionBuiltins);

    APPEND_PROPERTY(ES_FunctionBuiltins, length,        0);
    APPEND_PROPERTY(ES_FunctionBuiltins, name,          JString::Make(context, "Function.prototype"));
    APPEND_PROPERTY(ES_FunctionBuiltins, constructor,   undefined);
    APPEND_PROPERTY(ES_FunctionBuiltins, toString,      MAKE_BUILTIN(0, toString));
    ES_Function *fn_apply = MAKE_BUILTIN_NO_PROTOTYPE(2, apply);
#ifdef ES_NATIVE_SUPPORT
    fn_apply->SetFunctionID(ES_BUILTIN_FN_apply);
#endif // ES_NATIVE_SUPPORT
    APPEND_PROPERTY(ES_FunctionBuiltins, apply,         fn_apply);
    APPEND_PROPERTY(ES_FunctionBuiltins, call,          MAKE_BUILTIN(1, call));
    APPEND_PROPERTY(ES_FunctionBuiltins, bind,          MAKE_BUILTIN(1, bind));

    ASSERT_OBJECT_COUNT(ES_FunctionBuiltins);
}

/* static */ void
ES_FunctionBuiltins::PopulatePrototypeClass(ES_Context *context, ES_Class_Singleton *prototype_class)
{
    OP_ASSERT(prototype_class->GetPropertyTable()->Capacity() >= ES_FunctionBuiltinsCount);

    JString **idents = context->rt_data->idents;
    ES_Layout_Info layout;

    DECLARE_PROPERTY(ES_FunctionBuiltins, length,       RO | DE | DD,   ES_STORAGE_INT32);
    DECLARE_PROPERTY(ES_FunctionBuiltins, name,         RO | DE | DD,   ES_STORAGE_STRING);
    DECLARE_PROPERTY(ES_FunctionBuiltins, constructor,  DE,             ES_STORAGE_WHATEVER);
    DECLARE_PROPERTY(ES_FunctionBuiltins, toString,     DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_FunctionBuiltins, apply,        DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_FunctionBuiltins, call,         DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_FunctionBuiltins, bind,         DE,             ES_STORAGE_OBJECT);
}

#undef ES_CHECK_CALLABLE_THIS
