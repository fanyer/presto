/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"

#include "modules/ecmascript/carakan/src/object/es_function.h"
#include "modules/ecmascript/carakan/src/object/es_arguments.h"
#include "modules/ecmascript/carakan/src/builtins/es_function_builtins.h"
#include "modules/ecmascript/carakan/src/compiler/es_native.h"

BOOL ES_CALLING_CONVENTION
empty(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    return_value->SetUndefined();
    return TRUE;
}

/* static */ ES_Object *
ES_Function::MakePrototypeObject(ES_Context *context, ES_Global_Object *global_object, ES_Class *&instance)
{
    JString **idents = context->rt_data->idents;
    ES_Class_Singleton *prototype_class = ES_Class::MakeSingleton(context, global_object->GetObjectPrototype(), "Function", idents[ESID_Function], ES_FunctionBuiltins::ES_FunctionBuiltinsCount);
    prototype_class->Prototype()->AddInstance(context, prototype_class, TRUE);

    ES_CollectorLock gclock(context);

    ES_FunctionBuiltins::PopulatePrototypeClass(context, prototype_class);

    ES_Function *prototype_object;
    GC_ALLOCATE(context, prototype_object, ES_Function, (prototype_object, prototype_class, global_object, NULL, empty, NULL));

    prototype_class->AddInstance(context, prototype_object);
    prototype_object->AllocateProperties(context);
    prototype_object->data.builtin.information = 0;

    // In theory we want to populate the prototype here, but we can't, so do it later (in ES_Global_Object)
    // ES_FunctionBuiltins::PopulatePrototype(context, global_object, prototype_object);

    instance = ES_Class::MakeRoot(context, prototype_object, "Function", idents[ESID_Function], TRUE);
    prototype_object->SetSubObjectClass(context, instance);

    return prototype_object;
}

/* static */ void
ES_Function::CreateFunctionClasses(ES_Context *context, ES_Class *function_class, ES_Class *&native_function_class, ES_Class *&native_function_with_proto_class, ES_Class *&builtin_function_class)
{
    JString **idents = context->rt_data->idents;
    ES_Class *sub_object_class = function_class;

    sub_object_class = ES_Class::ExtendWithL(context, sub_object_class, idents[ESID_length], ES_Property_Info(RO|DE|DD|SP), ES_STORAGE_WHATEVER);
    sub_object_class = ES_Class::ExtendWithL(context, sub_object_class, idents[ESID_name], ES_Property_Info(RO|DE|DD|SP), ES_STORAGE_WHATEVER);
    sub_object_class = ES_Class::ExtendWithL(context, sub_object_class, idents[ESID_prototype], ES_Property_Info(DE|DD|SP), ES_STORAGE_WHATEVER);
    sub_object_class = ES_Class::ExtendWithL(context, sub_object_class, idents[ESID_arguments], ES_Property_Info(RO|DE|DD|SP), ES_STORAGE_WHATEVER);
    native_function_class = ES_Class::ExtendWithL(context, sub_object_class, idents[ESID_caller], ES_Property_Info(RO|DE|DD|SP), ES_STORAGE_WHATEVER);

    // CI_NATIVE_FUNCTION and CI_NATIVE_FUNCTION_WITH_PROTOTYPE differ by the SP bit on the prototype property;
    // SP indicating that it is a getter property for CI_NATIVE_FUNCTION. Having a getter allows the on-demand/lazy
    // creation of the prototype property.
    ES_Property_Info prototype_info(DE|DD);

    BOOL dummy_needs_conversion = TRUE;

    native_function_with_proto_class = ES_Class::ChangeAttribute(context, native_function_class, idents[ESID_prototype], prototype_info, UINT_MAX, dummy_needs_conversion);

    // builtins differ only in not providing a prototype.
    sub_object_class = function_class;

    sub_object_class = ES_Class::ExtendWithL(context, sub_object_class, idents[ESID_length], ES_Property_Info(RO|DE|DD|SP), ES_STORAGE_BOXED);
    sub_object_class = ES_Class::ExtendWithL(context, sub_object_class, idents[ESID_name], ES_Property_Info(RO|DE|DD|SP), ES_STORAGE_BOXED);
    sub_object_class = ES_Class::ExtendWithL(context, sub_object_class, idents[ESID_arguments], ES_Property_Info(RO|DE|DD), ES_STORAGE_NULL);
    builtin_function_class = ES_Class::ExtendWithL(context, sub_object_class, idents[ESID_caller], ES_Property_Info(RO|DE|DD), ES_STORAGE_NULL);
}

/* static */ ES_Function *
ES_Function::Make(ES_Context *context, ES_Global_Object *global_object, ES_FunctionCode *function_code, unsigned scope_chain_length)
{
    ES_Function *function;
    ES_Value_Internal function_value;

    GC_STACK_ANCHOR(context, global_object);

    GC_ALLOCATE_WITH_EXTRA(context, function, scope_chain_length * sizeof(ES_Object *), ES_Function, (function, global_object->GetNativeFunctionClass(), global_object, function_code, NULL, NULL));
    ES_CollectorLock gclock(context);

    function_value.SetObject(function);

    if (function_code->data->is_strict_mode)
        function->SetProperties(global_object->GetDefaultStrictFunctionProperties());
    else
        function->SetProperties(global_object->GetDefaultFunctionProperties());

    OP_ASSERT(function->RemainingCapacity() == 0);

    return function;
}

/* static */ ES_Function *
ES_Function::Make(ES_Context *context, ES_Global_Object *global_object, unsigned nformals, BuiltIn *call, unsigned builtin_name, BOOL is_builtin_function, ES_Class *klass)
{
    if (!klass)
    {
        OP_ASSERT(is_builtin_function);
        klass = global_object->GetBuiltInFunctionClass();
    }

    ES_Function *function;

    GC_ALLOCATE(context, function, ES_Function, (function, klass, global_object, NULL, call, NULL));
    ES_CollectorLock gclock(context);

    function->SetProperties(is_builtin_function ? global_object->GetDefaultBuiltinFunctionProperties() : global_object->GetDefaultFunctionProperties());

    function->data.builtin.information = builtin_name | (nformals << 16);

#ifdef DEBUG_ENABLE_OPASSERT
    if (klass == global_object->GetBuiltInFunctionClass() || klass == global_object->GetNativeFunctionClass())
        OP_ASSERT(function->RemainingCapacity() == 0);
#endif

    return function;
}

/* static */ ES_Function *
ES_Function::Make(ES_Context *context, ES_Class *function_class, ES_Global_Object *global_object, unsigned nformals, BuiltIn *call, BuiltIn *construct, unsigned builtin_name, JString *name, ES_Object *prototype_property)
{
    ES_Function *function;

    GC_ALLOCATE(context, function, ES_Function, (function, function_class, global_object, NULL, call, construct));
    ES_CollectorLock gclock(context);

    function->AllocateProperties(context);
    function->data.builtin.information = builtin_name | (nformals << 16);

    function->PutCachedAtIndex(ES_PropertyIndex(LENGTH), nformals);
    function->PutCachedAtIndex(ES_PropertyIndex(NAME), builtin_name ? context->rt_data->idents[builtin_name] : name ? name : context->rt_data->strings[STRING_empty]);

    ES_Value_Internal null_val;
    null_val.SetNull();

    function->PutCachedAtIndex(ES_PropertyIndex(PROTOTYPE), prototype_property ? prototype_property : null_val);

    if (function_class->Count() > NATIVE_CALLER)
    {
        function->PutCachedAtIndex(ES_PropertyIndex(NATIVE_ARGUMENTS), null_val);
        function->PutCachedAtIndex(ES_PropertyIndex(NATIVE_CALLER), null_val);
    }

    return function;
}

JString *
ES_Function::GetBuiltInName(ES_Context *context)
{
    unsigned id = GetBuiltInInformation() & 0xffffu;
    JString *name;

    if (id)
        name = context->rt_data->idents[id];
    else
    {
        ES_Layout_Info layout = klass->GetLayoutInfoAtInfoIndex(ES_PropertyIndex(NAME));
        ES_Object::ES_Value_Internal_Ref value_ref(properties, layout, ES_PropertyIndex(NAME));
        ES_Value_Internal value;

        value_ref.Read(value);

        if (value.IsString())
            name = value.GetString();
        else
            name = context->rt_data->strings[STRING_empty];
    }

    return name;
}

/* static */ void
ES_Function::Initialize(ES_Function *function, ES_Class *function_class, ES_Global_Object *global_object, ES_FunctionCode *function_code, BuiltIn *call, BuiltIn *construct)
{
    ES_Object::Initialize(function, function_class);
    function->ChangeGCTag(GCTAG_ES_Object_Function);

    function->global_object = global_object;
    function->function_code = function_code;

    if (function_code)
    {
        function->data.native.unused_arguments = NULL;
        function->object_bits |= ES_Object::MASK_IS_NATIVE_FN;

#ifdef ES_NATIVE_SUPPORT
        function->data.native.native_ctor_dispatcher = NULL;
        function->data.native.native_ctor_code_block = NULL;

        function->SetNeedDestroy();
#endif // ES_NATIVE_SUPPORT
    }
    else
    {
        function->object_bits |= ES_Object::MASK_IS_BUILTIN_FN | (ES_BUILTIN_FN_OTHER << ES_Object::FUNCTION_ID_SHIFT);

        function->data.builtin.call = call;
        function->data.builtin.construct = construct;
        function->data.builtin.information = 0;
    }

    function->scope_chain_length = 0;
}

#ifdef ES_NATIVE_SUPPORT

void
ES_Function::SetHasCreatedArgumentsArray(ES_Context *context)
{
    if (!function_code->GetData()->has_created_arguments_array)
    {
        function_code->GetData()->has_created_arguments_array = TRUE;

        if (ES_NativeCodeBlock *block = function_code->native_code_block)
        {
            block->Reset();
            context->heap->DeprecateNativeDispatcher(block);
        }
    }

    if ((object_bits & MASK_FUNCTION_ARGUMENTS_READ) == 0)
    {
        object_bits |= MASK_FUNCTION_ARGUMENTS_READ;

        if (ES_NativeCodeBlock *block = data.native.native_ctor_code_block)
        {
            block->Reset();
            context->heap->DeprecateNativeDispatcher(block);
        }
    }
}

void
ES_Function::ResetNativeConstructorDispatcher()
{
    data.native.native_ctor_dispatcher = NULL;
    data.native.native_ctor_code_block = NULL;

    object_bits &= ~MASK_IS_DISPATCHED_CTOR;
}

/* static */ unsigned
ES_Function::GetPropertyOffset(ClassPropertyIndex p)
{
    return 4 + p * sizeof(ES_Value_Internal);
}

#endif // ES_NATIVE_SUPPORT

/* static */ void
ES_Function::Destroy(ES_Function *function)
{
#ifdef ES_NATIVE_SUPPORT
    if (function->function_code && function->data.native.native_ctor_code_block)
        OP_DELETE(function->data.native.native_ctor_code_block);
#endif // ES_NATIVE_SUPPORT
}

JString *
ES_Function::GetName(ES_Context *context)
{
    ES_FunctionCode *fncode = GetFunctionCode();
    JString *name;

    if (fncode)
        name = fncode->GetName();
    else
    {
        unsigned id = GetBuiltInInformation() & 0xffffu;
        name = id ? context->rt_data->idents[id] : NULL;
    }

    if (!name)
        name = context->rt_data->strings[STRING_empty];

    return name;
}

/* static */ ES_BoundFunction *
ES_BoundFunction::Make(ES_Execution_Context *context, ES_Object *target_function, const ES_Value_Internal &bound_this, const ES_Value_Internal *bound_args, unsigned bound_args_count, unsigned length)
{
    ES_Global_Object *global_object = context->GetGlobalObject();

    unsigned extra_size;
    if (bound_args_count > 1)
        extra_size = (bound_args_count - 1) * sizeof(ES_Value_Internal);
    else
        extra_size = 0;

    ES_BoundFunction *function;

    GC_ALLOCATE_WITH_EXTRA(context, function, extra_size, ES_BoundFunction, (function, global_object->GetNativeFunctionClass(), global_object, target_function, bound_this, bound_args, bound_args_count, length));

    function->SetProperties(global_object->GetDefaultStrictFunctionProperties());

    OP_ASSERT(function->RemainingCapacity() == 0);

    return function;
}

/* static */ void
ES_BoundFunction::Initialize(ES_BoundFunction *function, ES_Class *function_class, ES_Global_Object *global_object, ES_Object *target_function, const ES_Value_Internal &bound_this, const ES_Value_Internal *bound_args, unsigned bound_args_count, unsigned length)
{
    ES_Function::Initialize(function, function_class, global_object, NULL, ES_FunctionBuiltins::bound_call, ES_FunctionBuiltins::bound_construct);

    function->data.builtin.information = length << 16;
    function->target_function = target_function;
    function->bound_this = bound_this;
    function->bound_args_count = bound_args_count;

    if (bound_args_count)
        op_memcpy(function->bound_args, bound_args, bound_args_count * sizeof(ES_Value_Internal));
}
