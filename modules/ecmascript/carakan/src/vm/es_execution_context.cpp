/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2008-2010
 *
 * @author Daniel Spang
 */
#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/ecmascript_object.h"
#include "modules/ecmascript/carakan/src/object/es_error_object.h"
#include "modules/ecmascript/carakan/src/vm/es_execution_context.h"
#ifdef ES_NATIVE_SUPPORT
#include "modules/ecmascript/carakan/src/compiler/es_native.h"
#include "modules/ecmascript/carakan/src/compiler/es_analyzer.h"
#endif // ES_NATIVE_SUPPORT
#include "modules/ecmascript/carakan/src/object/es_arguments.h"
#include "modules/ecmascript/carakan/src/object/es_special_property.h"
#include "modules/ecmascript/carakan/src/builtins/es_global_builtins.h"
#include "modules/ecmascript/carakan/src/builtins/es_math_builtins.h"
#if defined ES_HARDCORE_GC_MODE && !defined _STANDALONE
#include "modules/prefs/prefsmanager/collections/pc_js.h"
#endif // ES_HARDCORE_GC_MODE && !_STANDALONE

#include "modules/util/opstring.h"

#ifdef ADS12 // Work round Brew linker problems
#include "modules/ecmascript/carakan/src/vm/es_suspended_call_inlines.h"
template class ES_Suspended_DELETEA<ES_Value_Internal>;
template class ES_Suspended_DELETEA<ES_VirtualStackFrame>;
#endif // ADS12

void
AppendFunctionNameL(TempBuffer &buffer, ES_FunctionCode *fncode)
{
    if (JString *name = fncode->GetName())
        buffer.AppendL(Storage(NULL, name), Length(name));
    else if (JString *debug_name = fncode->GetDebugName())
    {
        buffer.AppendL("<anonymous function: ");
        buffer.AppendL(Storage(NULL, debug_name), Length(debug_name));
        buffer.AppendL(">");
    }
    else
        buffer.AppendL("<anonymous function>");
}

#define SHOULD_UPDATE_NATIVE_DISPATCHER() code->ShouldRegenerateNativeDispatcher()

#ifdef _DEBUG

static unsigned
ES_EstimateStackPerCall2(unsigned *head, void *dummy1, unsigned dummy2, unsigned dummy3)
{
    unsigned tail;

    int diff = op_abs(reinterpret_cast<UINTPTR>(head) - reinterpret_cast<UINTPTR>(&tail));
    int minimum = sizeof(void*) * 6;
    return diff < minimum ? minimum : diff;
}

static unsigned
ES_EstimateStackPerCall1(unsigned *dummy1, void *dummy2, unsigned dummy3, unsigned dummy4)
{
    unsigned head;

    return ES_EstimateStackPerCall2(&head, dummy2, dummy3, dummy4);
}

static unsigned
ES_EstimateStackPerCall()
{
    return ES_EstimateStackPerCall1(NULL, NULL, 1, 1);
}

#endif // _DEBUG

/* virtual */
ES_StackGCRoot::~ES_StackGCRoot()
{
    if (context)
        context->stack_root_objs = previous;
}

ES_Execution_Context::ES_Execution_Context(ESRT_Data *rt_data, ES_Global_Object *global_object, ES_Object *this_object, ES_Runtime *runtime, ES_Heap *heap)
    : ES_Context(rt_data, heap, runtime),
      ES_GCRoot(heap),
      frame_stack(64),
      register_blocks(this),
      context_global_object(global_object),
      this_object(this_object),
      external_scope_chain(NULL),
      numbertostring_cache_number(0.0),
      numbertostring_cache_string(NULL),
      property_caches_cache(NULL),
#ifdef ES_NATIVE_SUPPORT
      native_dispatcher(NULL),
      native_code_block(NULL),
      native_dispatcher_code(NULL),
      native_dispatcher_reg(NULL),
#endif // ES_NATIVE_SUPPORT
      suspended_call(NULL),
      is_aborting(FALSE),
      ExternalOutOfTime(NULL),
      external_out_out_time_data(NULL),
      stack_root_objs(NULL),
      host_arguments_length(0),
      host_arguments_used(0),
      is_debugged(FALSE),
      is_top_level_call(FALSE)
{
    exit_programs[EXIT_TO_BUILTIN_PROGRAM].instruction = ESI_EXIT_TO_BUILTIN;
    exit_programs[EXIT_TO_EVAL_PROGRAM].instruction = ESI_EXIT_TO_EVAL;
    exit_programs[EXIT_PROGRAM].instruction = ESI_EXIT;

#ifdef ES_BYTECODE_DEBUGGER
    bytecode_debugger_enabled = FALSE;
    bytecode_debugger_data = NULL;
#endif // ES_BYTECODE_DEBUGGER

#ifdef ES_NATIVE_SUPPORT
    BytecodeToNativeTrampoline = ES_Native::GetBytecodeToNativeTrampoline();
    ThrowFromMachineCodeTrampoline = ES_Native::GetThrowFromMachineCode();

#ifdef USE_CUSTOM_DOUBLE_OPS
    AddDoubles = ES_Native::CreateAddDoubles();
    SubtractDoubles = ES_Native::CreateSubtractDoubles();
    MultiplyDoubles = ES_Native::CreateMultiplyDoubles();
    DivideDoubles = ES_Native::CreateDivideDoubles();
    Sin = ES_Native::CreateSin();
    Cos = ES_Native::CreateCos();
    Tan = ES_Native::CreateTan();
#endif // USE_CUSTOM_DOUBLE_OPS
#endif // ES_NATIVE_SUPPORT

    cached_toString.Initialize(rt_data->idents[ESID_toString]);
    cached_valueOf.Initialize(rt_data->idents[ESID_valueOf]);

#ifdef _DEBUG
    stack_per_recursion = 32 * es_maxu(128, ES_EstimateStackPerCall());
#else // _DEBUG
    stack_per_recursion = ES_MINIMUM_STACK_REMAINING;
#endif // _DEBUG

    Reset();
}

ES_Execution_Context::~ES_Execution_Context()
{
#if 0
#ifdef ES_NATIVE_SUPPORT
    bytecode_to_native_block->owner->Free(bytecode_to_native_block);
    throw_from_machine_code_block->owner->Free(throw_from_machine_code_block);
#endif // ES_NATIVE_SUPPORT
#endif // 0

    FreeHostArguments();
    OP_DELETEA(external_scope_chain);

    ES_FrameStackIterator frames(this);
    ES_CollectorLock gclock(this);
    HEAP_DEBUG_LIVENESS_FOR(heap);

    while (frames.Next())
        if (ES_Code *code = frames.GetCode())
            if (code->type == ES_Code::TYPE_FUNCTION)
            {
                if (ES_Arguments_Object *arguments = frames.GetArgumentsObject())
                    DetachArgumentsObject(this, arguments);

                if (ES_Object *variables = frames.GetVariableObject())
                    if (variables->IsVariablesObject())
                        static_cast<ES_Variable_Object *>(variables)->CopyPropertiesFrom(this, frames.GetRegisterFrame() + 2);
                    else if (variables->HasStorage())
                    {
                        unsigned aliased_count = es_minu(static_cast<ES_FunctionCode *>(code)->klass->CountProperties(), variables->Class()->Count());

                        for (unsigned index = 0; index < aliased_count; ++index)
                        {
                            ES_Value_Internal *value = variables->GetValueAtIndex_loc(ES_PropertyIndex(index));

                            OP_ASSERT(value->IsSpecial());
                            OP_ASSERT(value->GetDecodedBoxed()->GCTag() == GCTAG_ES_Special_Aliased);

                            *value = *static_cast<ES_Special_Aliased *>(value->GetDecodedBoxed())->property;
                        }
                    }
                    else
                        frames.SetVariableObject(NULL);
            }

    while (property_caches_cache)
    {
        ES_Code::PropertyCache *to_delete = property_caches_cache;
        property_caches_cache = property_caches_cache->next;
        OP_DELETE(to_delete);
    }
}


OP_STATUS
ES_Execution_Context::Initialize()
{
    unsigned stacksize = 16384;

    while (stacksize < stack_per_recursion * 4)
        stacksize += stacksize;

    RETURN_IF_ERROR(frame_stack.Initialize(this));
    RETURN_IF_ERROR(register_blocks.Initialize(this));
    RETURN_IF_ERROR(OpPseudoThread::Initialize(stacksize));

    Reset();

    return OpStatus::OK;
}

void
ES_Execution_Context::SetExternalOutOfTime(BOOL (*new_ExternalOutOfTime)(void *), void *new_external_out_out_time_data)
{
    ExternalOutOfTime = new_ExternalOutOfTime;
    external_out_out_time_data = new_external_out_out_time_data;
}

void
ES_Execution_Context::Reset()
{
    current_exception.SetNoExceptionCookie();

    eval_status = ES_NORMAL;
    uncaught_exception = FALSE;
    started = FALSE;
    time_quota = 1024;
    time_until_check = time_quota;
    in_identifier_expression = FALSE;
    enumerated_class_id = 0;
    enumerated_string = NULL;

    stacktrace_length = 0;

#ifdef ES_BASE_REFERENCE_INFORMATION
    base_reference_object = NULL;
    base_reference_name = NULL;
#endif // ES_BASE_REFERENCE_INFORMATION

#ifdef ECMASCRIPT_DEBUGGER
    dbg_signal = ES_DebugListener::ESEV_NONE;
#endif // ECMASCRIPT_DEBUGGER

    scratch_values_used = FALSE;

    frame_stack.Reset();
    register_blocks.Reset();

    is_top_level_call = FALSE;

    OP_DELETEA(external_scope_chain);
    external_scope_chain = NULL;
    external_scope_chain_length = 0;
}

OP_STATUS
ES_Execution_Context::SetupExecution(ES_Code *program, ES_Object **scope_chain, unsigned scope_chain_length)
{
    Reset();

    // Setup the last instruction that is executed.

    ip = &exit_programs[EXIT_PROGRAM];
    code = NULL;
    variable_object = NULL;
    overlap = 0;
    in_constructor = FALSE;

    generate_result = static_cast<ES_ProgramCode *>(program)->GetData()->generate_result;

    ABORT_IF_MEMORY_ERROR(register_blocks.Allocate(this, first_in_block, reg, 1, overlap, 0));
    ABORT_IF_MEMORY_ERROR(frame_stack.Push(this)); // Note: reg and code not used.
    overlap = 1;
    ABORT_IF_MEMORY_ERROR(register_blocks.Allocate(this, first_in_block, reg, program->data->register_frame_size, overlap, 1));

    code = program;
    ip = code->data->codewords;

    OP_ASSERT(this_object || !code->global_object);
    reg[0].SetObject(this_object);

    if (scope_chain_length != 0)
    {
        external_scope_chain = OP_NEWA(ES_Object *, scope_chain_length);
        if (!external_scope_chain)
            return OpStatus::ERR_NO_MEMORY;
        op_memcpy(external_scope_chain, scope_chain, scope_chain_length * sizeof(ES_Object *));
        external_scope_chain_length = scope_chain_length;
    }
    else
    {
        external_scope_chain = NULL;
        external_scope_chain_length = 0;
    }

    return OpStatus::OK;
}

ES_Eval_Status
ES_Execution_Context::ExecuteProgram(BOOL want_string_result)
{
    OP_ASSERT(heap->HasReasonableLockValue());

#if defined ES_HARDCORE_GC_MODE && !defined _STANDALONE
    g_hardcode_gc_mode = !!g_pcjs->GetIntegerPref(PrefsCollectionJS::HardcoreGCMode);
#endif // ES_HARDCORE_GC_MODE && !_STANDALONE

    convert_result_to_string = want_string_result;

    if (!started)
    {
#ifdef _DEBUG
        yielded = FALSE;
#endif // _DEBUG

        if (code->type == ES_Code::TYPE_PROGRAM)
        {
            ES_CollectorLock gclock(this);
            TRAPD(status, static_cast<ES_ProgramCode *>(code)->StartExecution(runtime, this));

            if (OpStatus::IsError(status))
            {
                if (OpStatus::IsMemoryError(status))
                    return ES_ERROR_NO_MEMORY;
                else
                    return ES_ERROR;
            }
        }
        started = TRUE;
        Start(DoExecute);
    }
    else
    {
#ifdef _DEBUG
        OP_ASSERT(yielded);
        yielded = FALSE;
#endif // _DEBUG

        Resume();
    }

    if (eval_status == ES_NORMAL)
    {
        register_blocks.Free(1, overlap, 0, first_in_block);

        if (uncaught_exception)
        {
            SetThrewException();
            return_value = current_exception;
        }
        else if (generate_result)
            eval_status = ES_NORMAL_AFTER_VALUE;

        heap->MaybeCollectOffline(this);
    }

    OP_ASSERT(heap->HasReasonableLockValue());

    return eval_status;
}

ES_Value_Internal *
ES_Execution_Context::SetupFunctionCall(ES_Object *function, unsigned argc, ES_VirtualStackFrame::FrameType frame_type, BOOL is_construct)
{
    ES_FunctionCode *called_code;
    ES_Code *stored_code = code = Code();
    ES_Value_Internal *stored_reg = reg = Registers();

    // Push the current frame.
    ABORT_IF_MEMORY_ERROR(frame_stack.Push(this));

    overlap = 0;

    if (!function->IsHostObject() && (called_code = static_cast<ES_Function *>(function)->GetFunctionCode()))
    {
#ifdef ES_NATIVE_SUPPORT
        if (!called_code->native_dispatcher || is_construct)
#endif // ES_NATIVE_SUPPORT
        {
            // Setup a new exit instruction that will take us back to the
            // builtin when we finish the function call.
            ABORT_IF_MEMORY_ERROR(register_blocks.Allocate(this, first_in_block, reg, 1, overlap, 0));

            ip = &exit_programs[EXIT_TO_BUILTIN_PROGRAM];
            code = NULL;

            ABORT_IF_MEMORY_ERROR(frame_stack.Push(this));

            frame_stack.TopFrame()->frame_type = frame_type;

            overlap = 1;
        }

        ABORT_IF_MEMORY_ERROR(register_blocks.Allocate(this, first_in_block, reg, MAX(argc + 2, called_code->data->register_frame_size), overlap, 0));

        in_constructor = is_construct;

        if (in_constructor)
        {
            ES_Global_Object *global_object = GetGlobalObject();
            ES_Value_Internal prototype_value;
            ES_Object *prototype;
            GetResult result;

            if (function->Class() == global_object->GetNativeFunctionClass() || function->Class() == global_object->GetNativeFunctionWithPrototypeClass())
            {
                function->GetCachedAtIndex(ES_PropertyIndex(ES_Function::PROTOTYPE), prototype_value);

                if (prototype_value.IsSpecial())
                    result = static_cast<ES_Special_Property *>(prototype_value.GetDecodedBoxed())->SpecialGetL(this, function, prototype_value, function);
                else
                    result = PROP_GET_OK;
            }
            else
                result = function->GetL(this, RT_DATA.idents[ESID_prototype], prototype_value);

            if (GET_OK(result) && prototype_value.IsObject())
                prototype = prototype_value.GetObject(this);
            else
                prototype = global_object->GetObjectPrototype();

            ES_CollectorLock gclock(this, TRUE);
            ES_Class *klass = prototype->SetSubObjectClass(this);
            ES_Object *this_object = ES_Object::Make(this, klass);

            reg[0].SetObject(this_object);
        }
    }
    else
        // Call to another builtin function
        ABORT_IF_MEMORY_ERROR(register_blocks.Allocate(this, first_in_block, reg, argc + 2, overlap, 0));

    ES_Value_Internal *allocated_reg = reg;

    reg = stored_reg;
    code = stored_code;

    return allocated_reg;
}

class ES_ReservedCallBuiltIn
    : public ES_SuspendedCall
{
public:
    ES_ReservedCallBuiltIn(ES_Function::BuiltIn *fn, ES_Value_Internal *argv, unsigned argc, ES_Value_Internal *return_value)
        : fn(fn),
          argv(argv),
          return_value(return_value),
          argc(argc)
    {
    }

    virtual void DoCall(ES_Execution_Context *context)
    {
        success = fn(context, argc, argv, return_value);
    }

    BOOL success;

private:
    ES_Function::BuiltIn *fn;
    ES_Value_Internal *argv;
    ES_Value_Internal *return_value;
    unsigned argc;
};

void
ES_Execution_Context::AbortFunctionCall(ES_Object *function, unsigned argc)
{
    ES_FunctionCode *called_code;

    if (!function->IsHostObject() && (called_code = static_cast<ES_Function *>(function)->GetFunctionCode()))
    {
        register_blocks.Free(MAX(argc + 2, called_code->data->register_frame_size), overlap, 0, first_in_block);

#ifdef ES_NATIVE_SUPPORT
        if (!called_code->native_dispatcher)
#endif // ES_NATIVE_SUPPORT
        {
            frame_stack.Pop(this);

            register_blocks.Free(1, overlap, 0, first_in_block);
        }
    }
    else
        register_blocks.Free(argc + 2, 0, 0, first_in_block);

    frame_stack.Pop(this);
}

BOOL
ES_Execution_Context::CallFunction(ES_Value_Internal *registers, unsigned argc, ES_Value_Internal *return_value_out, BOOL is_construct)
{
    //OP_ASSERT(!heap->IsLocked());

    ES_Function *function = static_cast<ES_Function *> (registers[1].GetObject());
    ES_FunctionCode *called_code;

    if (--time_until_check == 0)
        CheckOutOfTime();

    if (frame_stack.TotalUsed() > ES_MAXIMUM_FUNCTION_RECURSION)
    {
        AbortFunctionCall(function, argc);

        ThrowRangeError("Maximum recursion depth exceeded");
        return FALSE;
    }

    if (!function->IsHostObject() && (called_code = function->GetFunctionCode()))
    {
        if (!in_constructor && !called_code->data->is_strict_mode)
            if (registers[0].IsNullOrUndefined())
                registers[0].SetObject(GetGlobalObject());
            else
                registers[0].ToObject(this);

#ifdef ECMASCRIPT_DEBUGGER
        if (IsDebugged())
        {
            SetCallDetails(registers[0], function, argc, registers + 2);
            SignalToDebugger(ES_DebugListener::ESEV_CALLSTARTING);
        }
#endif // ECMASCRIPT_DEBUGGER

#ifdef ES_NATIVE_SUPPORT
        if (!in_constructor && called_code->native_dispatcher)
        {
            reg = registers;
            variable_object = NULL;

            BOOL native_success = CallBytecodeToNativeTrampoline(NULL, registers, called_code->native_code_block, called_code->native_dispatcher, argc);

            stack_limit = StackLimit() + stack_per_recursion;

            if (return_value_out)
                *return_value_out = registers[0];

            register_blocks.Free(MAX(argc + 2, called_code->data->register_frame_size), overlap, 0, first_in_block);

            frame_stack.Pop(this);

            if (!native_success)
                return FALSE;
        }
        else
        {
            if (!in_constructor && UseNativeDispatcher())
            {
                called_code->slow_case_calls += called_code->data->codewords_count;

                if (called_code->data->CanHaveNativeDispatcher() && called_code->ShouldGenerateNativeDispatcher())
                {
                    ES_AnalyzeAndGenerateNativeDispatcher analyze_and_generate_native_dispatcher(this, function);

                    if (OpStatus::IsMemoryError(analyze_and_generate_native_dispatcher.status))
                        AbortOutOfMemory();
                }
            }
#endif // ES_NATIVE_SUPPORT
            reg = registers;

            SetupCallState(argc, function);

#ifdef ECMASCRIPT_DEBUGGER
            SignalToDebugger(ES_DebugListener::ESEV_ENTERFN, NULL);
#endif // ECMASCRIPT_DEBUGGER

            Reserve(DoExecute, stack_per_recursion);

#ifdef ES_NATIVE_SUPPORT
            stack_limit = StackLimit() + stack_per_recursion;
#endif // ES_NATIVE_SUPPORT

            if (return_value_out && return_value_out != &return_value)
            {
                *return_value_out = return_value;
#ifdef _DEBUG
                return_value.SetUndefined();
#endif // _DEBUG
            }
#ifdef ES_NATIVE_SUPPORT
        }
#endif // ES_NATIVE_SUPPORT

        // If we have an uncaught exception let the caller know about it.
        return current_exception.IsNoExceptionCookie();
    }
    else
    {
#ifdef ECMASCRIPT_DEBUGGER
        if (IsDebugged())
        {
            SetCallDetails(registers[0], function, argc, registers + 2);
            SignalToDebugger(ES_DebugListener::ESEV_CALLSTARTING);
        }
#endif // ECMASCRIPT_DEBUGGER

        reg = registers;
        code = NULL;
        variable_object = NULL;

        ES_Function::BuiltIn *fn;

        if (function->IsHostObject())
            fn = is_construct ? ES_Host_Function::Construct : ES_Host_Function::Call;
        else
            fn = is_construct ? function->GetConstruct() : function->GetCall();

        ES_ReservedCallBuiltIn call(fn, registers + 2, argc, &return_value);
        ReservedCall(&call);

        register_blocks.Free(argc + 2, 0, 0, first_in_block);

        frame_stack.Pop(this);

        // A builtin should not have succeeded if it called something that thrown an exception.
        OP_ASSERT(current_exception.IsNoExceptionCookie() == call.success);

#ifdef ECMASCRIPT_DEBUGGER
        if (IsDebugged())
            SignalToDebugger(ES_DebugListener::ESEV_CALLCOMPLETED);
#endif // ECMASCRIPT_DEBUGGER

        if (call.success)
        {
            if (return_value_out && return_value_out != &return_value)
            {
                *return_value_out = return_value;
#ifdef _DEBUG
                return_value.SetUndefined();
#endif // _DEBUG
            }

            return TRUE;
        }
        else
            return FALSE;
    }
}

void
ES_Execution_Context::PushCall(ES_Object *function_object, unsigned argc, const ES_Value* argv)
{
    ES_Function *function = static_cast<ES_Function *>(function_object);

    // create a program that performs IH_CALL on the function_object

    ES_ProgramCodeStatic *data = OP_NEW_L(ES_ProgramCodeStatic, ());
    ES_ProgramCode *call_code = ES_ProgramCode::Make(this, context_global_object, data);

    data->codewords = OP_NEWA_L(ES_CodeWord, 5);
#ifndef _STANDALONE
    MemoryManager::IncDocMemoryCount(sizeof(ES_CodeWord) * 5, FALSE);
#endif // !_STANDALONE

    data->codewords[0].instruction = ESI_CALL;
    data->codewords[1].index       = 1; // rel_frame_start
    data->codewords[2].index       = argc; // argc
    data->codewords[3].instruction = ESI_RETURN_VALUE;
    data->codewords[4].index       = 1;

    data->codewords_count = 5;
    data->register_frame_size = argc + 3; // arguments, global object, this object and function
    call_code->global_object = context_global_object;
    data->variable_declarations_count = 0;
    data->functions_count = 0;
    data->function_declarations_count = 0;
    data->first_temporary_register = 0;
    data->strings_count = 0;
    data->doubles_count = 0;
    data->constant_array_literals_count = 0;
    data->property_get_caches_count = 0;
    data->property_put_caches_count = 0;
    data->global_accesses_count = 0;
    data->exception_handlers_count = 0;
    data->inner_scopes_count = 0;
    data->generate_result = TRUE;
    data->privilege_level = ES_Runtime::PRIV_LVL_BUILTIN;

    // prepare for execution, setting up exit program etc. setting up the exit program should
    // ensure call_code is traced and considered live until pop'ed
    SetupExecution(call_code, NULL, 0);

    // reg[0] is the global object, set up by SetupExecution
    reg[1].SetObject(this_object); // this object is set by CreateContext
    reg[2].SetObject(function); // the function object to call

    ES_Value_Internal *registers = reg + 3;
    for (unsigned i = 0; i < argc; ++i)
        registers[i].ImportL(this, argv[i]);

    is_top_level_call = TRUE;

    return;
}

BOOL
ES_Execution_Context::Evaluate(ES_Code *eval_code, ES_Value_Internal *return_value_out, ES_Object *this_object)
{
    BOOL plain_eval = !this_object;
    ES_Code *calling_code = Code();
    ES_CodeStatic *eval_data = eval_code->data;

    // Push the current frame

    ABORT_IF_MEMORY_ERROR(frame_stack.Push(this));

    if (plain_eval)
        overlap = calling_code->data->register_frame_size;
    else
        overlap = 0;

    code = calling_code;

    ABORT_IF_MEMORY_ERROR(register_blocks.Allocate(this, first_in_block, reg, /* size = */ overlap, overlap, 0));

    ip = &exit_programs[EXIT_TO_EVAL_PROGRAM];
    code = NULL;

    ABORT_IF_MEMORY_ERROR(frame_stack.Push(this));

    // Allocate overlapping register frame
    ABORT_IF_MEMORY_ERROR(register_blocks.Allocate(this, first_in_block, reg, /* size = */ eval_code->data->register_frame_size, overlap, overlap));

    // in case of 'odd eval' (i.e. eval invoked indirectly (through alias))
    // overlap is 0 meaning THIS is not in eval's register frame
    // so we need to explicitly transfer it to reg[0] (see CARAKAN-313)
    if (overlap == 0)
        reg[0].SetObject(this_object);

    code = eval_code;
    ip = eval_data->codewords;
    in_constructor = FALSE;

    ES_FrameStackIterator frames(this);
    ES_Code *containing_code = NULL;

    frames.Next();

    while (frames.Next())
    {
        containing_code = frames.GetCode();
        if (containing_code && containing_code->type != ES_Code::TYPE_EVAL_PLAIN)
        {
#ifdef ES_NATIVE_SUPPORT
            ES_FrameStackIterator frames2(frames);
            if (frames2.Next() && frames2.GetNativeFrame() && frames2.GetCode() == frames.GetCode() && frames2.GetRegisterFrame() == frames.GetRegisterFrame())
                continue;
#endif // ES_NATIVE_SUPPORT

            break;
        }
    }

    if (containing_code->type == ES_Code::TYPE_PROGRAM)
        eval_code->PrepareForExecution(this);
    else if (containing_code->type == ES_Code::TYPE_FUNCTION)
    {
        ES_ProgramCodeStatic *program_data = static_cast<ES_ProgramCodeStatic *>(eval_data);

        if (plain_eval)
        {
            ES_Object *variables = frames.GetVariableObject();

            if (!variables)
                variables = CreateAndPropagateVariableObject(frames, static_cast<ES_FunctionCode *> (containing_code), reg + 2);
            else
            {
                variable_object = variables;

                if (first_in_block)
                    if (variables->IsVariablesObject())
                        variables->SetProperties(reg + 2);
                    else
                    {
                        unsigned locals_count = static_cast<ES_FunctionCode *>(containing_code)->GetData()->LocalsCount();

                        ES_Class *variable_class = variables->Class();
                        for (unsigned index = 0; index < locals_count; ++index)
                        {
                            ES_Value_Internal *value = variables->GetValue_loc(variable_class->GetLayoutInfoAtInfoIndex(ES_PropertyIndex(index)));

                            OP_ASSERT(value->IsSpecial());
                            OP_ASSERT(value->GetDecodedBoxed()->GCTag() == GCTAG_ES_Special_Aliased);

                            static_cast<ES_Special_Aliased *>(value->GetDecodedBoxed())->property = reg + 2 + index;
                        }
                    }
            }

            if (ES_Arguments_Object *arguments = frames.GetArgumentsObject())
            {
                ES_Indexed_Properties *values = arguments->GetIndexedProperties();
                unsigned arguments_count = MIN(static_cast<ES_FunctionCode *>(containing_code)->GetData()->formals_count, arguments->GetLength());

                for (unsigned index = 0; index < arguments_count; ++index)
                {
                    ES_Value_Internal *value = ES_Indexed_Properties::GetStorage(values, index);

                    OP_ASSERT(value->IsSpecial());
                    OP_ASSERT(value->GetDecodedBoxed()->GCTag() == GCTAG_ES_Special_Aliased);

                    static_cast<ES_Special_Aliased *>(value->GetDecodedBoxed())->property = reg + 2 + index;
                }
            }

            this_object = variables;
        }

        if (program_data->variable_declarations_count > 0 || program_data->function_declarations_count > 0)
            for (unsigned index = 0; index < program_data->variable_declarations_count; ++index)
            {
                unsigned variable_index = program_data->variable_declarations[index] & 0x7fffffffu;

                if (!this_object->HasProperty(this, eval_code->GetString(variable_index)))
                {
                    if (plain_eval && this_object->IsVariablesObject())
                        static_cast<ES_Variable_Object *>(this_object)->CreateAliasedFrom(this, reg + 2);

                    ES_Value_Internal undef_value;
                    undef_value.SetUndefined();
                    this_object->InitPropertyL(this, eval_code->GetString(variable_index), undef_value, 0, ES_STORAGE_WHATEVER, TRUE);
                }
            }
    }

    Reserve(DoExecute, stack_per_recursion);

#ifdef ES_NATIVE_SUPPORT
    stack_limit = StackLimit() + stack_per_recursion;
#endif // ES_NATIVE_SUPPORT

    if (return_value_out)
        *return_value_out = return_value;

    if (current_exception.IsNoExceptionCookie())
    {
        register_blocks.Free(overlap, overlap, 0, first_in_block);
        frame_stack.Pop(this);
    }

    heap->MaybeCollect(this);

    return current_exception.IsNoExceptionCookie();
}

#ifdef ES_NATIVE_SUPPORT

class ES_Suspended_CreateNativeDispatcher
    : public ES_SuspendedCall
{
public:
    ES_Suspended_CreateNativeDispatcher(ES_Code *code, ES_CodeWord *entry_point_cw, void **entry_point_address, BOOL light_weight_allowed)
        : block(NULL),
          code(code),
          entry_point_cw(entry_point_cw),
          entry_point_address(entry_point_address),
          light_weight_allowed(light_weight_allowed)
    {
    }

    virtual void DoCall(ES_Execution_Context *context);

    OP_STATUS status;
    ES_NativeCodeBlock *block;

private:
    ES_Code *code;
    ES_CodeWord *entry_point_cw;
    void **entry_point_address;
    BOOL light_weight_allowed;
};

/* virtual */ void
ES_Suspended_CreateNativeDispatcher::DoCall(ES_Execution_Context *context)
{
    if (!code->data->instruction_offsets)
    {
        TRAP(status, code->data->FindInstructionOffsets());
        if (OpStatus::IsError(status))
            return;
    }

    if (!code->data->optimization_data)
    {
        ES_Analyzer analyzer(context, code);
        TRAP(status, code->data->optimization_data = analyzer.AnalyzeL());
        if (OpStatus::IsError(status) || !code->data->optimization_data)
            return;
    }

    ES_Native nc(context, code);
    TRAP(status, block = nc.CreateNativeDispatcher(entry_point_cw, entry_point_address, FALSE, light_weight_allowed));
}

#endif // ES_NATIVE_SUPPORT

ES_Object *
ES_Execution_Context::CreateVariableObject(ES_FunctionCode *code, ES_Value_Internal *properties)
{
    code->ConstructClass(this);

    ES_Variable_Object *variables = ES_Variable_Object::Make(this, code->klass, properties);
    return variables;
}

ES_Object *
ES_Execution_Context::CreateAndPropagateVariableObject(ES_FrameStackIterator &current_frames, ES_FunctionCode *code, ES_Value_Internal *properties)
{
    ES_Object *variables = CreateVariableObject(code, reg + 2);

    ES_FrameStackIterator frames(this);
    while (frames.Next())
    {
        frames.SetVariableObject(variables);
        if (frames == current_frames)
            break;
    }

    return variables;
}


ES_Function *
ES_Execution_Context::NewFunction(ES_FunctionCode *&function_code, ES_CodeStatic::InnerScope *scope)
{
#ifdef USE_LOTS_OF_MEMORY
#ifdef ES_NATIVE_SUPPORT
    if (use_native_dispatcher && !function_code->native_dispatcher && !function_code->data->instruction_offsets)
    {
        ES_Suspended_CreateNativeDispatcher suspended(this, function_code, NULL, NULL, TRUE);

        SuspendedCall(&suspended);

        if (OpStatus::IsError(suspended.status))
            AbortOutOfMemory();

        if (suspended.block)
        {
            function_code->native_code_block = suspended.block;
            function_code->native_code_block->SetFunctionCode(function_code);
            function_code->native_dispatcher = suspended.block->GetAddress();
        }
    }
#endif // ES_NATIVE_SUPPORT
#endif // USE_LOTS_OF_MEMORY

    ES_FrameStackIterator frames(this);

    frames.Next();

    while (!frames.GetCode() || frames.GetCode()->type == ES_Code::TYPE_EVAL_PLAIN)
        frames.Next();

    BOOL is_in_function = frames.GetCode()->type == ES_Code::TYPE_FUNCTION;
    BOOL is_named_function_expr = function_code->GetData()->is_function_expr && function_code->GetName() != NULL;
    ES_Function *new_function;

    if (!function_code->GetData()->is_static_function && (is_in_function || scope) || is_named_function_expr || IsDebugged())
    {
        ES_Value_Internal *registers = frames.GetRegisterFrame();
        unsigned inner_scopes_count = scope ? scope->registers_count : 0;

        ES_Function *current_function = is_in_function ? static_cast<ES_Function *>(registers[1].GetObject()) : NULL;
        unsigned current_scope_chain_length = is_in_function ? current_function->GetScopeChainLength() : 0;
        unsigned new_scope_chain_length = current_scope_chain_length + (is_in_function ? 1 : 0) + inner_scopes_count;

        if (is_named_function_expr)
            ++new_scope_chain_length;

        new_function = ES_Function::Make(this, GetGlobalObject(), function_code, new_scope_chain_length);

        ES_CollectorLock gclock(this);

        ES_Object **new_scope_chain = new_function->GetScopeChain();
        unsigned new_scope_chain_index = 0;

        while (inner_scopes_count)
            new_scope_chain[new_scope_chain_index++] = registers[scope->registers[--inner_scopes_count]].GetObject(this);

        if (is_named_function_expr)
        {
            ES_Class *klass = function_code->global_object->GetEmptyClass();
            klass = ES_Class::ExtendWithL(this, klass, function_code->GetName(), DD | RO, ES_STORAGE_WHATEVER);

            ES_Object *function_name = ES_Object::Make(this, klass);
            function_name->PutCachedAtIndex(ES_PropertyIndex(0), new_function);

            new_scope_chain[new_scope_chain_index++] = function_name;
        }

        if (is_in_function)
        {
#ifdef ES_MOVABLE_OBJECTS
            /* Update current function since it could have been moved during gc. */
            current_function = static_cast<ES_Function *>(registers[1].GetObject());
#endif // ES_MOVABLE_OBJECTS

            ES_Object **current_scope_chain = current_function->GetScopeChain();
            ES_Object *variables = frames.GetVariableObject();

            if (!variables)
                variables = CreateAndPropagateVariableObject(frames, current_function->GetFunctionCode(), reg + 2);

            new_scope_chain[new_scope_chain_index++] = variables;

            op_memcpy(new_scope_chain + new_scope_chain_index, current_scope_chain, current_scope_chain_length * sizeof(ES_Object *));
            new_scope_chain_index += current_scope_chain_length;
        }

        OP_ASSERT(new_scope_chain_index == new_scope_chain_length);

        new_function->SetScopeChainLength(new_scope_chain_length);
    }
    else
        new_function = ES_Function::Make(this, GetGlobalObject(), function_code, 0);

    return new_function;
}

ES_Value_Internal *
ES_Execution_Context::AllocateRegisters(unsigned count)
{
    ES_Value_Internal *registers;
    ABORT_IF_MEMORY_ERROR(register_blocks.Allocate(this, registers, count));
    return registers;
}

void
ES_Execution_Context::FreeRegisters(unsigned count)
{
    register_blocks.Free(count);
}

ES_Value_Internal*
ES_Execution_Context::SaveScratchRegisters()
{
#ifdef ES_NATIVE_SUPPORT
    code = Code();
#endif // ES_NATIVE_SUPPORT

    ES_Value_Internal *register_storage = AllocateRegisters(3);
    register_storage[0] = src_val1;
    register_storage[1] = src_val2;
    register_storage[2] = src_val3;
    return register_storage;
}

void
ES_Execution_Context::RestoreScratchRegisters(ES_Value_Internal *register_storage)
{
    src_val1 = register_storage[0];
    src_val2 = register_storage[1];
    src_val3 = register_storage[2];
    FreeRegisters(3);
}

/* virtual */ void
ES_Execution_Context::GCTrace()
{
    ES_FrameStackIterator iter(this);

    while (iter.Next())
    {
        GC_PUSH_BOXED(heap, iter.GetCode());

        if (ES_Object *variable_object = iter.GetVariableObject())
        {
            GC_PUSH_BOXED(heap, variable_object);
            iter.SetVariableObject(variable_object);
        }

        if (ES_Arguments_Object *arguments_object = iter.GetArgumentsObject())
        {
            GC_PUSH_BOXED(heap, arguments_object);
            iter.SetArgumentsObject(arguments_object);
        }
    }

#ifdef ES_NATIVE_SUPPORT
    BOOL readjust_register_usage = FALSE;

    ES_Value_Internal *stored_reg = reg;
    ES_Code *stored_code = code;

    if (native_stack_frame)
    {
        reg = ES_NativeStackFrame::GetRegisterFrame(native_stack_frame);
        code = ES_NativeStackFrame::GetCode(native_stack_frame);

        readjust_register_usage = register_blocks.AdjustUsed() == OpBoolean::IS_TRUE;
    }
#endif // ES_NATIVE_SUPPORT

    register_blocks.Trim();

    unsigned index;

    for (ES_Block<ES_Value_Internal> *block = static_cast<ES_Block<ES_Value_Internal> *>(register_blocks.GetLastUsedBlock());
         block;
         block = static_cast<ES_Block<ES_Value_Internal> *>(block->Pred()))
    {
        ES_Value_Internal *values = block->Storage();
        unsigned used = block->Used();

#if 0
#ifdef ES_NATIVE_SUPPORT
        if (current)
        {
            if (reg != values && reg[1].IsObject() && reg[1].GetObject()->IsFunctionObject())
                if (ES_FunctionCode *code = static_cast<ES_Function *>(reg[1].GetObject())->GetFunctionCode())
                {
                    ES_Value_Internal *reg_top = reg + code->data->register_frame_size;

                    if (reg_top > values + used)
                        used = reg_top - values;
                }

            current = FALSE;
        }
#endif // ES_NATIVE_SUPPORT
#endif // 0

        for (index = 0; index < used; ++index)
            GC_PUSH_VALUE(heap, values[index]);

        for (unsigned capacity = block->Capacity(); index < capacity; ++index)
            values[index].SetUndefined();
    }

#ifdef ES_NATIVE_SUPPORT
    if (readjust_register_usage)
        register_blocks.ReadjustUsed();

    reg = stored_reg;
    code = stored_code;
#endif // ES_NATIVE_SUPPORT

    GC_PUSH_VALUE(heap, current_exception);
    GC_PUSH_VALUE(heap, return_value);
    GC_PUSH_BOXED(heap, numbertostring_cache_string);

    if (scratch_values_used)
    {
        GC_PUSH_VALUE(heap, src_val1);
        GC_PUSH_VALUE(heap, src_val2);
        GC_PUSH_VALUE(heap, src_val3);
    }
    else
    {
        src_val1.SetUndefined();
        src_val2.SetUndefined();
        src_val3.SetUndefined();
    }

    for (index = 0; index < stacktrace_length; ++index)
        GC_PUSH_BOXED(heap, stacktrace[index].code);

#ifdef ES_BASE_REFERENCE_INFORMATION
    GC_PUSH_BOXED(heap, base_reference_object);
    GC_PUSH_BOXED(heap, base_reference_name);
#endif // ES_BASE_REFERENCE_INFORMATION

    for (index = 0; index < external_scope_chain_length; ++index)
        GC_PUSH_BOXED(heap, external_scope_chain[index]);

    for (index = 0; index < host_arguments_used; ++index)
        if (host_arguments[index].type == VALUE_OBJECT)
            GC_PUSH_BOXED(heap, host_arguments[index].value.object);

    ES_StackGCRoot *root = stack_root_objs;
    while (root)
    {
        root->GCTrace();
        root = root->previous;
    }
}

/* virtual */ void
ES_Execution_Context::NewHeap(ES_Heap *new_heap)
{
    heap = new_heap;
}

/* static */ void
ES_Execution_Context::DoExecute(OpPseudoThread *thread)
{
    ES_Execution_Context *context = static_cast<ES_Execution_Context *>(thread);
    context->Execute();
}

/* static */ void
ES_Execution_Context::DoSuspendedCall(OpPseudoThread *thread)
{
    ES_Execution_Context *context = static_cast<ES_Execution_Context *>(thread);
    context->suspended_call->DoCall(context);
}

void
ES_Execution_Context::SuspendedCall(ES_SuspendedCall *call)
{
    suspended_call = call;
    Suspend(DoSuspendedCall);
    suspended_call = NULL;
}

void
ES_Execution_Context::ReservedCall(ES_SuspendedCall *call, unsigned amount)
{
    suspended_call = call;
    Reserve(DoSuspendedCall, amount ? amount : stack_per_recursion);
#ifdef ES_NATIVE_SUPPORT
    stack_limit = StackLimit() + stack_per_recursion;
#endif // ES_NATIVE_SUPPORT
    suspended_call = NULL;
}

ES_Global_Object *&
ES_Execution_Context::GetGlobalObjectSlow()
{
    ES_FrameStackIterator frames(this);

    if (frames.Next())
        do
            if (ES_Code *code = frames.GetCode())
                return code->global_object;
        while (frames.Next());

    /* This should probably never happen, and the caller probably doesn't handle
       NULL. */
    OP_ASSERT(frame_stack.TotalUsed() == 0);
    return context_global_object;
}

/* static */ void ES_CALLING_CONVENTION
ES_Execution_Context::CreateArgumentsObject(ES_Execution_Context *context, ES_Object *function, ES_Value_Internal *registers, unsigned argc)
{
    /* We need to lock the garbage collector here.  If 2+argc is greater than
       the called function's register frame size, ES_Execution_Context::GCTrace
       will trace too few registers, and will also reset all untraced registers
       to undefined.  So in order to protect the extra arguments' values until
       we've copied them into the arguments object, we lock. */
    ES_CollectorLock gclock(context, TRUE);

    ES_Function *fn = static_cast<ES_Function *>(function);
    ES_FunctionCode *fncode = fn->GetFunctionCode();
    ES_FunctionCodeStatic *data = fncode->GetData();
    ES_Arguments_Object *arguments;
    ES_Compact_Indexed_Properties *indexed = NULL;

    data->has_created_arguments_array = TRUE;

    if (fn->data.native.unused_arguments && fn->data.native.unused_arguments->GetLength() == argc)
    {
        arguments = fn->data.native.unused_arguments;
        fn->data.native.unused_arguments = NULL;

        if (argc)
            if (ES_Indexed_Properties::GetType(arguments->GetIndexedProperties()) == ES_Indexed_Properties::TYPE_COMPACT)
            {
                indexed = static_cast<ES_Compact_Indexed_Properties *>(arguments->GetIndexedProperties());
                if (argc > indexed->Capacity())
                    indexed = NULL;
            }
    }
    else
    {
        arguments = ES_Arguments_Object::Make(context, static_cast<ES_Function *>(function), argc);
        indexed = static_cast<ES_Compact_Indexed_Properties *>(arguments->GetIndexedProperties());
    }

    unsigned formals_count = es_minu(argc, data->formals_count);
    if (!indexed)
    {
        /* This means we reused an unused arguments object. */
        indexed = ES_Compact_Indexed_Properties::Make(context, ES_Compact_Indexed_Properties::AppropriateCapacity(argc));

        if (argc != 0)
            indexed->SetHasSpecialProperties();

        arguments->SetIndexedProperties(indexed);

        arguments->PutCachedAtIndex(ES_PropertyIndex(0), argc);
        if (!data->is_strict_mode)
            arguments->PutCachedAtIndex(ES_PropertyIndex(1), function);
    }

    unsigned index = 0;

    if (!data->is_strict_mode)
        for (; index < formals_count; ++index)
        {
            ES_Special_Aliased *alias;

            if (arguments->HasAliased())
            {
                alias = arguments->GetAliased(index);
                alias->property = &registers[index + 2];
            }
            else
                alias = ES_Special_Aliased::Make(context, &registers[index + 2]);

            ES_Value_Internal value;
            value.SetBoxed(alias);

            indexed->PutSimpleNew(index, value);
        }

    for (; index < argc; ++index)
        indexed->PutSimpleNew(index, registers[index + 2]);

    if (data->arguments_index != ES_FunctionCodeStatic::ARGUMENTS_NOT_USED)
    {
        registers[data->arguments_index].SetObject(arguments);
        arguments->SetUsed();
    }

#ifdef ES_NATIVE_SUPPORT
    if (context->native_stack_frame)
        ES_NativeStackFrame::SetArgumentsObject(context->native_stack_frame, arguments);
    else
#endif // ES_NATIVE_SUPPORT
        context->arguments_object = arguments;
}

/* static */ void ES_CALLING_CONVENTION
ES_Execution_Context::DetachVariableObject(ES_Execution_Context *context, ES_Object *variables)
{
    /* Only functions have variable objects! */
    OP_ASSERT(context->Code()->type == ES_Code::TYPE_FUNCTION);

    if (variables->IsVariablesObject())
    {
        ES_CollectorLock gclock(context);
        HEAP_DEBUG_LIVENESS_FOR(context->heap);
        static_cast<ES_Variable_Object *>(variables)->CopyPropertiesFrom(context, context->Registers() + 2);
    }
    else
    {
        unsigned aliased_count = static_cast<ES_FunctionCode *>(context->Code())->klass->CountProperties();
        ES_Class *variables_class = variables->Class();

        BOOL needs_conversion = FALSE;
        ES_Class *old_klass = ES_Class::ActualClass(variables_class);

        if (aliased_count > 0)
            variables_class = ES_Class::ChangeAttributeFromTo(context, variables_class, 0, SP, ES_PropertyIndex(0), ES_PropertyIndex(aliased_count - 1), variables->Count(), needs_conversion);

        for (unsigned index = 0; index < aliased_count; ++index)
        {
            ES_Value_Internal *value = variables->GetValue_loc(variables_class->GetLayoutInfoAtInfoIndex(ES_PropertyIndex(index)));

            OP_ASSERT(value->IsSpecial());
            OP_ASSERT(value->GetDecodedBoxed()->GCTag() == GCTAG_ES_Special_Aliased);

            *value = *static_cast<ES_Special_Aliased *>(value->GetDecodedBoxed())->property;
        }

        if (needs_conversion)
            variables->ConvertObject(context, old_klass, variables_class);
        else
            variables->SetClass(variables_class);
    }
}

/* static */ void ES_CALLING_CONVENTION
ES_Execution_Context::PopVariableObject(ES_Execution_Context *context, ES_Object *variables)
{
    ES_FrameStackIterator frames(context);

    frames.Next();
    frames.Next();

    while ((!frames.GetCode() || frames.GetCode()->type != ES_Code::TYPE_FUNCTION) && frames.Next())
        ;

    if (frames.GetVariableObject() != variables)
        DetachVariableObject(context, variables);
    else
    {
        ES_Value_Internal *old_registers = context->reg, *new_registers = frames.GetRegisterFrame();

        if (new_registers != old_registers)
        {
            while (!frames.GetCode() || frames.GetCode()->type != ES_Code::TYPE_FUNCTION)
                frames.Next();

            if (variables->IsVariablesObject())
                variables->SetProperties(new_registers + 2);
            else
            {
                unsigned locals_count = static_cast<ES_FunctionCode *>(frames.GetCode())->GetData()->LocalsCount();
                ES_Class *variables_class = variables->Class();

                for (unsigned index = 0; index < locals_count; ++index)
                {
                    ES_Value_Internal *value = variables->GetValue_loc(variables_class->GetLayoutInfoAtInfoIndex(ES_PropertyIndex(index)));

                    OP_ASSERT(value->IsSpecial());
                    OP_ASSERT(value->GetDecodedBoxed()->GCTag() == GCTAG_ES_Special_Aliased);

                    static_cast<ES_Special_Aliased *>(value->GetDecodedBoxed())->property = new_registers + 2 + index;
                }
            }

            if (ES_Arguments_Object *arguments = frames.GetArgumentsObject())
            {
                ES_Indexed_Properties *values = arguments->GetIndexedProperties();
                unsigned arguments_count = MIN(arguments->GetLength(), static_cast<ES_FunctionCode *>(frames.GetCode())->GetData()->formals_count);

                for (unsigned index = 0; index < arguments_count; ++index)
                {
                    ES_Value_Internal *value = ES_Indexed_Properties::GetStorage(values, index);

                    OP_ASSERT(value->IsSpecial());
                    OP_ASSERT(value->GetDecodedBoxed()->GCTag() == GCTAG_ES_Special_Aliased);

                    static_cast<ES_Special_Aliased *>(value->GetDecodedBoxed())->property = new_registers + 2 + index;
                }
            }
        }
    }
}

/* static */ void ES_CALLING_CONVENTION
ES_Execution_Context::DetachArgumentsObject(ES_Execution_Context *context, ES_Arguments_Object *arguments)
{
    context->reg = context->Registers();

    ES_Function *function = arguments->GetFunction();

    if (arguments->WasUsed() || !arguments->HasAliased())
    {
        arguments->ResetUsed();

        ES_FunctionCode *fncode = function->GetFunctionCode();
        ES_FunctionCodeStatic *data = fncode->GetData();
        ES_CollectorLock gclock(context);

        unsigned argc = arguments->GetLength();
        unsigned formals = data->formals_count;

        ES_Value_Internal value;

        /* Convert magic couplings between indeces in the arguments object and
           local variables into regular properties on the arguments object. */

        for (unsigned index = 0, count = es_minu(argc, formals); index < count; ++index)
            if (arguments->GetNativeL(context, index, value))
            {
                arguments->DeleteOwnPropertyL(context, index);
                arguments->PutNativeL(context, index, 0, value);
            }
    }
    else
        function->data.native.unused_arguments = arguments;
}

void
ES_Execution_Context::HandleThrow()
{
#ifdef ES_NATIVE_SUPPORT
    if (native_stack_frame)
        ThrowFromMachineCode(this);
#endif // ES_NATIVE_SUPPORT

    for (;;)
    {
        unsigned handler_ip;
        ES_CodeStatic::ExceptionHandler::Type handler_type;

        if (code->data->FindExceptionHandler(ip - code->data->codewords, handler_ip, handler_type))
        {
            ip = code->data->codewords + handler_ip;
            return;
        }

#ifdef ECMASCRIPT_DEBUGGER
        SignalToDebugger(ES_DebugListener::ESEV_LEAVEFN, ip);
#endif // ECMASCRIPT_DEBUGGER

        register_blocks.Free(code->data->register_frame_size, overlap, 0, first_in_block);

        frame_stack.Pop(this);

#ifdef ECMASCRIPT_DEBUGGER
        SignalToDebugger(ES_DebugListener::ESEV_CALLCOMPLETED, ip - 1);
#endif // ECMASCRIPT_DEBUGGER

        if (frame_stack.IsEmpty())
        {
            uncaught_exception = TRUE;
            ip = &exit_programs[EXIT_PROGRAM];
            return;
        }

        // We stop popping frames if we find a builtin.
        if (ip->instruction == ESI_EXIT_TO_BUILTIN || ip->instruction == ESI_EXIT_TO_EVAL || ip->instruction == ESI_EXIT)
        {
#ifdef ECMASCRIPT_DEBUGGER
            SignalToDebugger(ES_DebugListener::ESEV_CALLCOMPLETED, NULL);
#endif // ECMASCRIPT_DEBUGGER
            return;
        }
    }
}

void
ES_Execution_Context::HandleFinally(unsigned target)
{
#ifdef ES_NATIVE_SUPPORT
    OP_ASSERT(!native_stack_frame);
#endif // ES_NATIVE_SUPPORT

    unsigned handler_ip;

    implicit_bool = code->data->FindFinallyHandler(ip - code->data->codewords, target, handler_ip);

    if (implicit_bool)
        ip = code->data->codewords + handler_ip;

    return;
}

void ES_Execution_Context::ThrowValue(const ES_Value_Internal &exception_value, ES_CodeWord *ip)
{
    current_exception = exception_value;

    CaptureStackTrace(ip);
}

void ES_Execution_Context::ThrowValue(const ES_Value &exception_value)
{
    current_exception.ImportL(this, exception_value);

    ES_Error *error;

    if (current_exception.IsObject() && current_exception.GetObject()->IsErrorObject())
    {
        error = static_cast<ES_Error *>(current_exception.GetObject());
        if (error->GetStackTraceLength() != 0)
            error = NULL;
    }
    else
        error = NULL;

    CaptureStackTrace(NULL, error);
}

void
ES_Execution_Context::ThrowErrorBase(ES_CodeWord *ip, ErrorType error_type, ExpectedType expected_type)
{
    ES_Class *error_class = error_type == TYPE_ERROR ? GetGlobalObject()->GetTypeErrorClass() : GetGlobalObject()->GetReferenceErrorClass();
    ES_Error *error = ES_Error::Make(this, GetGlobalObject(), error_class);

    JString *message = NULL;

    if (ip)
    {
        ES_CodeStatic::DebugRecord *debug_record = Code()->data->FindDebugRecord(this, ES_CodeStatic::DebugRecord::TYPE_BASE_EXPRESSION, ip);

        if (debug_record)
        {
            ES_CollectorLock gclock(this);

            if (JString *extent = Code()->data->source.GetExtent(this, debug_record->location))
            {
                const uni_char *prefix, *suffix;

                if (expected_type == EXPECTED_OBJECT)
                    prefix = UNI_L("Cannot convert '"), suffix = UNI_L("' to object");
                else if (expected_type == EXPECTED_LVALUE)
                    prefix = UNI_L("Cannot assign to '"), suffix = UNI_L("'");
                else
                {
                    prefix = UNI_L("'");

                    if (expected_type == EXPECTED_FUNCTION)
                        suffix = UNI_L("' is not a function");
                    else
                        suffix = UNI_L("' is not a constructor");
                }

                message = JString::Make(this, uni_strlen(prefix) + uni_strlen(suffix) + debug_record->location.Length());

                uni_char *storage = Storage(this, message);
                uni_strcpy(storage, prefix);
                uni_strncat(storage, Storage(this, extent), Length(extent));
                uni_strcat(storage, suffix);

#ifdef ES_BASE_REFERENCE_INFORMATION
                if (base_reference_object)
                {
                    Append(this, message, " (object was [object ");
                    Append(this, message, base_reference_object->Class()->ObjectName());
                    Append(this, message, "] and property was '");

                    if (base_reference_name)
                        Append(this, message, base_reference_name);
                    else
                    {
                        ES_Value_Internal name = Registers()[base_reference_register];

                        if (!name.IsString())
                            name.ToString(this);

                        Append(this, message, name.GetString());
                        Append(this, message, "')");
                    }

                    base_reference_object = NULL;
                    base_reference_name = NULL;
                }
#endif // ES_BASE_REFERENCE_INFORMATION
            }
        }
    }

    CaptureStackTrace(ip, error);

    current_exception.SetObject(error);

    if (!message)
    {
        ES_CollectorLock gclock(this);
        switch (expected_type)
        {
        case EXPECTED_OBJECT:
            message = JString::Make(this, "Cannot convert null or undefined to object");
            break;
        case EXPECTED_FUNCTION:
            message = JString::Make(this, "Not a function");
            break;
        case EXPECTED_CONSTRUCTOR:
            message = JString::Make(this, "Not a constructor");
            break;
        default:
            OP_ASSERT(FALSE);
        case EXPECTED_LVALUE:
            message = JString::Make(this, "Cannot assign to non-LValue");
            break;
        }
    }

    error->PutCachedAtIndex(ES_PropertyIndex(ES_Error::PROP_message), message);
}

void
ES_Execution_Context::ThrowTypeError(const char *msg, ES_CodeWord *ip)
{
    ThrowTypeError(msg, NULL, ~0u, ip);
}

void
ES_Execution_Context::ThrowTypeError(const char *msg, const uni_char *name, unsigned length, ES_CodeWord *ip)
{
    ES_Error *error = ES_Error::Make(this, GetGlobalObject(), GetGlobalObject()->GetTypeErrorClass());
    current_exception.SetObject(error);

    JString *str = JString::Make(this, msg);
    ES_CollectorLock gclock(this);
    if (name)
        Append(this, str, name, length);

    error->PutCachedAtIndex(ES_PropertyIndex(ES_Error::PROP_message), str);

    CaptureStackTrace(ip, error);
}

void
ES_Execution_Context::ThrowRangeError(const char *msg, ES_CodeWord *ip)
{
    ES_Error *error = ES_Error::Make(this, GetGlobalObject(), GetGlobalObject()->GetRangeErrorClass());
    current_exception.SetObject(error);

    error->PutCachedAtIndex(ES_PropertyIndex(ES_Error::PROP_message), JString::Make(this, msg));

    CaptureStackTrace(ip, error);
}

void
ES_Execution_Context::ThrowReferenceError(const char *msg, const uni_char *name, unsigned length, ES_CodeWord *ip)
{
    ES_Error *error = ES_Error::Make(this, GetGlobalObject(), GetGlobalObject()->GetReferenceErrorClass());
    current_exception.SetObject(error);

    JString *str = JString::Make(this, msg);
    ES_CollectorLock gclock(this);
    if (name)
        Append(this, str, name, length);

    error->PutCachedAtIndex(ES_PropertyIndex(ES_Error::PROP_message), str);

    CaptureStackTrace(ip, error);
}

void
ES_Execution_Context::ThrowSyntaxError(const char *msg, ES_CodeWord *ip)
{
    ES_Error *error = ES_Error::Make(this, GetGlobalObject(), GetGlobalObject()->GetSyntaxErrorClass());
    current_exception.SetObject(error);

    error->PutCachedAtIndex(ES_PropertyIndex(ES_Error::PROP_message), JString::Make(this, msg));

    CaptureStackTrace(ip, error);
}

void
ES_Execution_Context::ThrowSyntaxError(JString *msg, ES_CodeWord *ip)
{
    ES_Error *error = ES_Error::Make(this, GetGlobalObject(), GetGlobalObject()->GetSyntaxErrorClass());
    current_exception.SetObject(error);

    error->PutCachedAtIndex(ES_PropertyIndex(ES_Error::PROP_message), msg);

    CaptureStackTrace(ip, error);
}

void
ES_Execution_Context::ThrowURIError(const char *msg, ES_CodeWord *ip)
{
    ES_Error *error = ES_Error::Make(this, GetGlobalObject(), GetGlobalObject()->GetURIErrorClass());
    current_exception.SetObject(error);

    error->PutCachedAtIndex(ES_PropertyIndex(ES_Error::PROP_message), JString::Make(this, msg));

    CaptureStackTrace(ip, error);
}

void
ES_Execution_Context::ThrowEvalError(const char *msg, ES_CodeWord *ip)
{
    ES_Error *error = ES_Error::Make(this, GetGlobalObject(), GetGlobalObject()->GetEvalErrorClass());
    current_exception.SetObject(error);

    error->PutCachedAtIndex(ES_PropertyIndex(ES_Error::PROP_message), JString::Make(this, msg));

    CaptureStackTrace(ip, error);
}

void
ES_Execution_Context::ThrowInternalError(const char *msg, ES_CodeWord *ip)
{
    ES_Error *error = ES_Error::Make(this, GetGlobalObject(), GetGlobalObject()->GetErrorClass());
    current_exception.SetObject(error);

    error->PutCachedAtIndex(ES_PropertyIndex(ES_Error::PROP_message), JString::Make(this, msg));

    CaptureStackTrace(ip, error);
}

class ES_SuspendedCheckOutOfTime
    : public ES_SuspendedCall
{
public:
    ES_SuspendedCheckOutOfTime(ES_Execution_Context *context, BOOL (*callback)(void *), void *data)
        : callback(callback),
          data(data)
    {
        context->SuspendedCall(this);
    }

    BOOL result;

private:
    virtual void DoCall(ES_Execution_Context *context)
    {
        result = callback(data);
    }

    BOOL (*callback)(void *);
    void *data;
};

void
ES_Execution_Context::CheckOutOfTime()
{
    if (ES_SuspendedCheckOutOfTime(this, ExternalOutOfTime, external_out_out_time_data).result)
    {
        YieldExecution();
        time_quota /= 2;
        time_quota++; // Prevents it from beeing 0.
    }
    else
        time_quota *=2;

    time_until_check = time_quota;
}

#ifdef ES_NATIVE_SUPPORT

/* static */ void ES_CALLING_CONVENTION
ES_Execution_Context::CheckOutOfTimeFromMachineCode(ES_Execution_Context *context)
{
    context->CheckOutOfTime();
}

#endif // ES_NATIVE_SUPPORT

ES_Code *
ES_Execution_Context::GetCallingCode(BOOL skip_plain_eval)
{
    if (ES_Code *code = Code())
        if (!skip_plain_eval || code->type != ES_Code::TYPE_EVAL_PLAIN)
            return code;
        else
        {
            ES_FrameStackIterator frames(this);

            frames.Next();
            frames.Next();

            while (!frames.GetCode() || frames.GetCode()->type == ES_Code::TYPE_EVAL_PLAIN)
                frames.Next();

            return frames.GetCode();
        }
    else
        return NULL;
}
void
ES_Execution_Context::SuspendExecution()
{
    time_until_check = time_quota;
    SetSuspended();
}

void
ES_Execution_Context::MaybeYield()
{
    if (ShouldYield() && !IsUsingStandardStack())
        YieldImpl();
}

void
ES_Execution_Context::YieldExecution()
{
    SetSuspended();
    YieldImpl();
}

void
ES_Execution_Context::Block()
{
    SetBlocked();
    YieldImpl();
}

void
ES_Execution_Context::AllocateHostArguments(unsigned argc)
{
    FreeHostArguments();

    ES_Suspended_NEWA<ES_Value> new_values(argc);
    SuspendedCall(&new_values);

    if (!new_values.storage)
        AbortOutOfMemory();

    host_arguments = new_values.storage;

    ES_Suspended_NEWA<ES_ValueString> new_value_strings(argc);
    SuspendedCall(&new_value_strings);

    if (!new_value_strings.storage)
        AbortOutOfMemory();

    host_argument_strings = new_value_strings.storage;
    host_arguments_length = argc;
}

void
ES_Execution_Context::FreeHostArguments()
{
    if (host_arguments_length != 0)
    {
        ES_Suspended_DELETEA<ES_Value> delete_values(host_arguments);
        SuspendedCall(&delete_values);

        ES_Suspended_DELETEA<ES_ValueString> delete_value_strings(host_argument_strings);
        SuspendedCall(&delete_value_strings);
    }
}

ES_Value_Internal&
ES_Execution_Context::ReturnValue()
{
    return return_value;
}

void
ES_Execution_Context::MigrateTo(ES_Global_Object* new_global_object, ES_Runtime *new_runtime)
{
    context_global_object = new_global_object;
    runtime = new_runtime;
}

OP_STATUS
ES_Execution_Context::GetCallerInformation(ES_Runtime::CallerInformation &call_info)
{
    ES_FrameStackIterator frames(this);

    while (frames.Next())
        if (ES_Code *code = frames.GetCode())
        {
            if (code->url)
            {
                TRAPD(status, call_info.url = StorageZ(this, code->url));
                RETURN_IF_ERROR(status);
            }

            call_info.privilege_level = code->data->privilege_level;

            call_info.script_guid = code->GetScriptGuid();
            // The ip of the frame will sometimes be on the next line or statement
            // which causes the wrong line number to be reported. To avoid this we
            // subtract by 1 to get an ip that belongs to the previous opcode.
            call_info.line_no = GetCallLine(code, frames.GetCodeWord() - 1);

            return OpStatus::OK;
        }

    return OpStatus::OK;
}

OP_STATUS
ES_Execution_Context::GetErrorStackCallerInformation(ES_Object* error_object, unsigned stack_position, BOOL resolve, ES_Runtime::CallerInformation& call_info)
{
    if (!error_object->IsErrorObject())
        return OpStatus::ERR;

    ES_Error *es_error = static_cast<ES_Error *>(error_object);
    if (es_error->GetStackTraceLength() <= stack_position)
        return OpStatus::ERR;

    ES_StackTraceElement &element = es_error->GetStackTrace()[stack_position];
    if (element.code->url)
        TRAP_AND_RETURN(status, call_info.url = StorageZ(this, element.code->url));

    ES_CodeStatic::DebugRecord *debug_record = element.code->data->FindDebugRecord(this, ES_CodeStatic::DebugRecord::TYPE_EXTENT_INFORMATION, element.codeword);
    if (debug_record == NULL)
        return OpStatus::ERR;

    ES_SourcePosition position;
    element.code->data->source.Resolve(debug_record->location, element.code->data->source_storage_owner, position);

    call_info.line_no = resolve ? position.GetAbsoluteLine() : position.GetRelativeLine();
    call_info.privilege_level = element.code->data->privilege_level;
    call_info.script_guid = element.code->GetScriptGuid();

    return OpStatus::OK;
}

#ifdef ES_NATIVE_SUPPORT

class ES_ReservedCallBytecodeToNativeTrampoline
    : public ES_SuspendedCall
{
public:
    ES_ReservedCallBytecodeToNativeTrampoline(ES_Code *calling_code, ES_Value_Internal *register_frame, ES_NativeCodeBlock *ncblock, void *native_dispatcher, unsigned arguments_count, BOOL reconstruct_native_stack, BOOL prologue_entry_point)
        : calling_code(calling_code),
          register_frame(register_frame),
          ncblock(ncblock),
          native_dispatcher(native_dispatcher),
          arguments_count(arguments_count),
          reconstruct_native_stack(reconstruct_native_stack),
          prologue_entry_point(prologue_entry_point)
    {
    }

    virtual void DoCall(ES_Execution_Context *context)
    {
        result = context->DoCallBytecodeToNativeTrampoline(calling_code, register_frame, ncblock, native_dispatcher, arguments_count, reconstruct_native_stack, prologue_entry_point);
    }

    BOOL result;

private:
    ES_Code *calling_code;
    ES_Value_Internal *register_frame;
    ES_NativeCodeBlock *ncblock;
    void *native_dispatcher;
    unsigned arguments_count;
    BOOL reconstruct_native_stack, prologue_entry_point;
};

BOOL
ES_Execution_Context::CallBytecodeToNativeTrampoline(ES_Code *calling_code, ES_Value_Internal *register_frame, ES_NativeCodeBlock *ncblock, void *native_dispatcher, unsigned arguments_count, BOOL reconstruct_native_stack, BOOL prologue_entry_point)
{
    ES_ReservedCallBytecodeToNativeTrampoline call(calling_code, register_frame, ncblock, native_dispatcher, arguments_count, reconstruct_native_stack, prologue_entry_point);
    ReservedCall(&call, 2 * stack_per_recursion);
    return call.result;
}

BOOL
ES_Execution_Context::DoCallBytecodeToNativeTrampoline(ES_Code *calling_code, ES_Value_Internal *register_frame, ES_NativeCodeBlock *ncblock, void *native_dispatcher, unsigned arguments_count, BOOL reconstruct_native_stack, BOOL prologue_entry_point)
{
    BOOL stored_need_adjust_register_usage = need_adjust_register_usage;

    stack_limit = StackLimit() + stack_per_recursion;
    reg_limit = register_blocks.Limit();
    need_adjust_register_usage = TRUE;

    void *pointers[ES_Native::TRAMPOLINE_POINTERS_COUNT];

    union Cast
    {
        void *(ES_CDECL_CALLING_CONVENTION * fn)(ES_Execution_Context *, void **);
        void *ptr;
    } cast;

    cast.fn = &ES_Execution_Context::ReconstructNativeStack;

    OP_ASSERT(reg <= register_frame);

    OP_ASSERT(!calling_code || register_frame - reg <= static_cast<int>(calling_code->data->register_frame_size));
    OP_ASSERT(calling_code || register_frame == reg);

    pointers[ES_Native::TRAMPOLINE_POINTER_CONTEXT] = this;
    pointers[ES_Native::TRAMPOLINE_POINTER_CONTEXT_REG] = reg;
    pointers[ES_Native::TRAMPOLINE_POINTER_CONTEXT_NATIVE_STACK_FRAME] = &native_stack_frame;
    pointers[ES_Native::TRAMPOLINE_POINTER_CONTEXT_STACK_PTR] = &stack_ptr;
#ifdef ARCHITECTURE_AMD64
    pointers[ES_Native::TRAMPOLINE_POINTER_INSTRUCTION_HANDLERS] = g_ecma_instruction_handlers;
#endif // ARCHITECTURE_AMD64
    pointers[ES_Native::TRAMPOLINE_POINTER_REGISTER_FRAME] = register_frame;
    pointers[ES_Native::TRAMPOLINE_POINTER_TRAMPOLINE_DISPATCHER] = reconstruct_native_stack ? ES_Native::GetReconstructNativeStackTrampoline(prologue_entry_point) : native_dispatcher;
    pointers[ES_Native::TRAMPOLINE_POINTER_NATIVE_DISPATCHER] = native_dispatcher;
    pointers[ES_Native::TRAMPOLINE_POINTER_RECONSTRUCT_NATIVE_STACK] = cast.ptr;
    pointers[ES_Native::TRAMPOLINE_POINTER_STACK_LIMIT] = stack_limit;
#ifdef ES_FUNCTION_CONSTANT_DATA_SECTION
    pointers[ES_Native::TRAMPOLINE_POINTER_DATA_SECTION_REGISTER] = ncblock->GetDataSectionRegister();
#endif // ES_FUNCTION_CONSTANT_DATA_SECTION

    BOOL result = BytecodeToNativeTrampoline(pointers, arguments_count);

    need_adjust_register_usage = stored_need_adjust_register_usage;

    return result;
}

/* static */ void *ES_CALLING_CONVENTION
ES_Execution_Context::ExecuteBytecode(ES_Execution_Context *context, unsigned start_instruction_index, unsigned end_instruction_index)
{
    ES_NativeStackFrame *frame = context->native_stack_frame;

    context->reg = ES_NativeStackFrame::GetRegisterFrame(frame);
    context->code = ES_NativeStackFrame::GetCode(frame);

    BOOL is_constructor;

    if (end_instruction_index & 0x80000000u)
    {
        is_constructor = TRUE;
        end_instruction_index = ~end_instruction_index;
    }
    else
        is_constructor = FALSE;

    unsigned start_cw_index = context->code->data->instruction_offsets[start_instruction_index];
    unsigned end_cw_index = context->code->data->instruction_offsets[end_instruction_index];

#ifdef ES_DEBUG_NATIVE_BEHAVIOUR
    extern BOOL g_debug_native_behaviour;
    if (g_debug_native_behaviour)
    {
        const char *fnname = "<unknown>";

        TempBuffer buffer;
        OpString8 buffer8;

        if (context->code->type == ES_Code::TYPE_FUNCTION)
        {
            AppendFunctionNameL(buffer, static_cast<ES_FunctionCode *>(context->code));
            buffer8.SetL(buffer.GetStorage());
            fnname = buffer8.CStr();
        }

        printf("slow path: %s [%u, %u)\n", fnname, start_cw_index, end_cw_index);
    }

    extern unsigned g_total_native_recompiles;
    ++g_total_native_recompiles;
#endif // ES_DEBUG_NATIVE_BEHAVIOUR

    ES_CodeWord *start = context->code->data->codewords + start_cw_index, *stop = start + end_cw_index - start_cw_index;

    context->slow_path_flag = FALSE;
    context->ip = start;

    do
        context->ExecuteSingleInstruction(context->ip);
    while (context->ip != stop);

    if (context->slow_path_flag)
        context->code->slow_case_calls += end_cw_index - start_cw_index;

    if (is_constructor)
        end_cw_index += context->code->data->codewords_count;

    return UpdateNativeDispatcher(context, end_cw_index);
}

/* static */ void *
ES_Execution_Context::EvalFromMachineCode(ES_Execution_Context *context, unsigned cw_index)
{
    OP_ASSERT(context->native_stack_frame);

    ES_Code *code = ES_NativeStackFrame::GetCode(context->native_stack_frame);
    ES_CodeWord *ip = code->data->codewords + cw_index;

    OP_ASSERT(ip[0].instruction == ESI_EVAL);

    context->reg = ES_NativeStackFrame::GetRegisterFrame(context->native_stack_frame);
    context->code = NULL;

    ES_CodeWord::Index rel_frame_start = ip[1].index;
    ES_CodeWord::Index argc = ip[2].index & 0x7fffffffu;

    context->reg[rel_frame_start].SetObject(NULL);

    BOOL eval_success = ES_GlobalBuiltins::Eval(context, argc, context->reg + rel_frame_start + 2, context->reg + rel_frame_start, code, ip);

    OP_ASSERT(!context->arguments_object || context->arguments_object == ES_NativeStackFrame::GetArgumentsObject(context->native_stack_frame));
    OP_ASSERT(!context->variable_object || context->variable_object == ES_NativeStackFrame::GetVariableObject(context->native_stack_frame));

    /* These are stored in the native stack frame, and not in the context.
       Having them set in the context would confuse us when popping virtual
       stack frames. */
    context->arguments_object = NULL;
    context->variable_object = NULL;

    if (!eval_success)
        ThrowFromMachineCode(context);

    return NULL;
}

/* static */ void
ES_Execution_Context::DoCallFromMachineCode(OpPseudoThread *thread)
{
    ES_Execution_Context *context = static_cast<ES_Execution_Context *>(thread);
    context->stack_limit = context->StackLimit() + context->stack_per_recursion;
    context->native_dispatcher_success = context->PerformCallFromMachineCode(context->native_dispatcher, context->stored_cw_index);
}

/* virtual */ void
ES_AnalyzeAndGenerateNativeDispatcher::DoCall(ES_Execution_Context *context)
{
    TRAP(status, DoCallL(context));
}

void
ES_AnalyzeAndGenerateNativeDispatcher::DoCallL(ES_Execution_Context *context)
{
    ES_FunctionCode *code = function->GetFunctionCode();

#ifdef ES_DEBUG_NATIVE_BEHAVIOUR
    extern BOOL g_debug_native_behaviour;
    if (g_debug_native_behaviour)
    {
        const char *fnname = "<unknown>";

        TempBuffer buffer;
        OpString8 buffer8;

        AppendFunctionNameL(buffer, static_cast<ES_FunctionCode *>(code));
        buffer8.SetL(buffer.GetStorage());
        fnname = buffer8.CStr();

        printf("compiling native dispatcher: %s (slow case calls: %u, codewords: %u, entry point: %u)\n", fnname, code->slow_case_calls, code->data->codewords_count, prologue_entry_point_cw ? static_cast<unsigned>(prologue_entry_point_cw - code->data->codewords) : 0);

        fflush(stdout);
    }

    extern unsigned g_total_native_recompiles;
    ++g_total_native_recompiles;
#endif // ES_DEBUG_NATIVE_BEHAVIOUR

    code->data->FindInstructionOffsets(context);

    ES_Analyzer analyzer(context, code); ANCHOR(ES_Analyzer, analyzer);

    if (!code->data->optimization_data)
        code->data->optimization_data = analyzer.AnalyzeL();

    ES_Native nc(context, code); ANCHOR(ES_Native, nc);

    native_code_block = nc.CreateNativeDispatcher(prologue_entry_point_cw, &entry_point, TRUE);

    if (native_code_block)
    {
        if (code->native_code_block)
            context->heap->DeprecateNativeDispatcher(code->native_code_block);

        code->native_code_block = native_code_block;
        code->native_code_block->SetFunctionCode(code);
        code->native_dispatcher = code->native_code_block->GetAddress();
        code->slow_case_calls = 0;
    }
}


/* virtual */ void
ES_SuspendedAnalyzer::DoCall(ES_Execution_Context *context)
{
    ES_Analyzer analyzer(context, code);
    TRAP(status, data = analyzer.AnalyzeL());
}

#define ES_SUPPORTED_UNARY_BUILTIN(code) \
    ((code) == ES_BUILTIN_FN_sin || \
     (code) == ES_BUILTIN_FN_cos || \
     (code) == ES_BUILTIN_FN_tan || \
     (code) == ES_BUILTIN_FN_sqrt || \
     (code) == ES_BUILTIN_FN_floor || \
     (code) == ES_BUILTIN_FN_ceil || \
     (code) == ES_BUILTIN_FN_abs || \
     (code) == ES_BUILTIN_FN_log || \
     (code) == ES_BUILTIN_FN_parseInt || \
     (code) == ES_BUILTIN_FN_push || \
     (code) == ES_BUILTIN_FN_fromCharCode || \
     (code) == ES_BUILTIN_FN_charAt || \
     (code) == ES_BUILTIN_FN_charCodeAt)

BOOL
ES_Execution_Context::PerformCallFromMachineCode(void *&new_dispatcher, unsigned cw_index)
{
    reg = ES_NativeStackFrame::GetRegisterFrame(native_stack_frame);
    ES_Code *calling_code = code = ES_NativeStackFrame::GetCode(native_stack_frame);

    unsigned actual_cw_index = (cw_index < code->data->codewords_count ? cw_index : cw_index - code->data->codewords_count);
    ES_CodeWord *word = code->data->codewords + actual_cw_index;

    OP_ASSERT(word[0].instruction == ESI_CALL || word[0].instruction == ESI_EVAL);

    ES_CodeWord::Index rel_frame_start = word[1].index;
    ES_CodeWord::Index argc = word[2].index;

    ES_Value_Internal &function_val = reg[rel_frame_start + 1];
    ES_Object *function_obj;
    if (!function_val.IsObject() || !(function_obj = function_val.GetObject(this))->IsFunctionObject())
    {
        ThrowErrorBase(word, TYPE_ERROR, EXPECTED_FUNCTION);
        return FALSE;
    }

    if (--time_until_check == 0)
        CheckOutOfTime();

    ES_Value_Internal *this_vr = &reg[rel_frame_start];

    if (argc & 0x80000000u)
    {
        this_vr->SetUndefined();
        argc ^= 0x80000000u;
    }

    ES_Function *function = static_cast<ES_Function *>(function_obj);
    ES_FunctionCode *called_code;

    if (!function->IsHostObject() && (called_code = function->GetFunctionCode()) != NULL)
    {
        if (code->data->profile_data)
        {
            unsigned char *pd = code->data->profile_data + actual_cw_index;

            if (!pd[0] && pd[1] != 0)
            {
                pd[0] = 1;
                pd[1] = ES_BUILTIN_FN_DISABLE;
            }

            if (pd[0])
                code->slow_case_calls += ES_JIT_WEIGHT_INLINED_BUILTIN;
        }

        if (!called_code->data->is_strict_mode)
            if (!this_vr->ToObject(this, FALSE))
                this_vr->SetObject(calling_code->global_object);

        ip = &exit_programs[EXIT_PROGRAM];
        code = NULL;

        ABORT_IF_MEMORY_ERROR(frame_stack.Push(this));

        frame_stack.TopFrame()->true_ip = word + 3;

        code = calling_code;

        overlap = calling_code->data->register_frame_size - rel_frame_start;
        ABORT_IF_MEMORY_ERROR(register_blocks.Allocate(this, first_in_block, reg, MAX(argc + 2, called_code->data->register_frame_size), overlap, argc + 2));

        in_constructor = FALSE;
        SetupCallState(argc, function);

        Reserve(DoExecute, stack_per_recursion);
        stack_limit = StackLimit() + stack_per_recursion;

        OP_ASSERT(native_stack_frame);

        if (!current_exception.IsNoExceptionCookie())
            return FALSE;

        if (called_code->data->CanHaveNativeDispatcher())
        {
            called_code->slow_case_calls += called_code->data->codewords_count;

            if (called_code->ShouldGenerateNativeDispatcher())
            {
                ES_AnalyzeAndGenerateNativeDispatcher analyze_and_generate_native_dispatcher(this, function);

                if (OpStatus::IsMemoryError(analyze_and_generate_native_dispatcher.status))
                    AbortOutOfMemory();
            }
        }

        new_dispatcher = NULL;
    }
    else
    {
        ES_Function::BuiltIn *call;

        if (function->IsHostObject())
        {
            if (code->data->profile_data)
            {
                unsigned char *pd = code->data->profile_data + actual_cw_index;

                if (!pd[0] && pd[1] != 0)
                {
                    pd[0] = 1;
                    pd[1] = ES_BUILTIN_FN_DISABLE;
                }

                if (pd[0])
                    code->slow_case_calls += ES_JIT_WEIGHT_INLINED_BUILTIN;
            }

            call = ES_Host_Function::Call;
        }
        else
        {
            call = function->GetCall();

            if (!code->data->profile_data)
                AllocateProfileData();

            unsigned char *pd = code->data->profile_data + actual_cw_index;
            unsigned code = 0;

            if (function->IsBuiltInFunction() && (pd[1] == (code = function->GetFunctionID()) || pd[1] == ES_BUILTIN_FN_NONE))
            {
                unsigned arg_bits = 0;

                if (argc == 1 && ES_SUPPORTED_UNARY_BUILTIN(code))
                    arg_bits = reg[rel_frame_start + 2].TypeBits();
                else if (argc == 2 && code == ES_BUILTIN_FN_pow)
                    arg_bits = reg[rel_frame_start + 2].TypeBits() | reg[rel_frame_start + 3].TypeBits();
                else if (argc == 2 && code == ES_BUILTIN_FN_parseInt)
                    arg_bits = reg[rel_frame_start + 2].TypeBits();

                if (pd[1] == ES_BUILTIN_FN_NONE || (pd[2] | arg_bits) != pd[2])
                {
                    pd[0] = 1;
                    pd[2] |= arg_bits;
                }

                pd[1] = code;
            }
            else if (pd[1] != ES_BUILTIN_FN_DISABLE && pd[1] != ES_BUILTIN_FN_OTHER)
            {
                pd[0] = 1;
                pd[1] = ES_BUILTIN_FN_OTHER;
            }

            if (pd[0])
                this->code->slow_case_calls += ES_JIT_WEIGHT_INLINED_BUILTIN;
        }

        if (!call(this, argc, reg + rel_frame_start + 2, reg + rel_frame_start))
            return FALSE;

        OP_ASSERT(current_exception.IsNoExceptionCookie());

        stack_limit = StackLimit() + stack_per_recursion;

        new_dispatcher = UpdateNativeDispatcher(this, cw_index + 3);
    }

    return TRUE;
}

#undef ES_SUPPORTED_UNARY_BUILTIN

/* static */ void *
ES_Execution_Context::CallFromMachineCode(ES_Execution_Context *context, unsigned cw_index)
{
    OP_ASSERT(context->native_stack_frame);
#ifdef OPPSEUDOTHREAD_STACK_SWAPPING
    OP_ASSERT(context->stack_limit > context->StackLimit() && context->stack_limit <= context->StackTop());
#endif // OPPSEUDOTHREAD_STACK_SWAPPING

    if (context->frame_stack.TotalUsed() > ES_MAXIMUM_FUNCTION_RECURSION)
    {
        context->ThrowRangeError("Maximum recursion depth exceeded");
        ThrowFromMachineCode(context);
    }

    if (context->StackSpaceRemaining() < context->stack_per_recursion)
    {
        context->stored_cw_index = cw_index;
        context->Reserve(DoCallFromMachineCode, context->stack_per_recursion);
        context->stack_limit = context->StackLimit() + context->stack_per_recursion;

        if (!context->native_dispatcher_success)
            ThrowFromMachineCode(context);

        return context->native_dispatcher;
    }
    else
    {
        void *new_dispatcher;

        if (!context->PerformCallFromMachineCode(new_dispatcher, cw_index))
            ThrowFromMachineCode(context);

        return new_dispatcher;
    }
}

/* static */ void *
ES_Execution_Context::ConstructFromMachineCode(ES_Execution_Context *context, unsigned cw_index)
{
    OP_ASSERT(context->native_stack_frame);
#ifdef OPPSEUDOTHREAD_STACK_SWAPPING
    OP_ASSERT(context->stack_limit > context->StackLimit() && context->stack_limit <= context->StackTop());
#endif // OPPSEUDOTHREAD_STACK_SWAPPING

    if (context->frame_stack.TotalUsed() > ES_MAXIMUM_FUNCTION_RECURSION)
    {
        context->ThrowRangeError("Maximum recursion depth exceeded");
        ThrowFromMachineCode(context);
    }

    context->stored_cw_index = cw_index;
    context->Reserve(DoConstructFromMachineCode, context->stack_per_recursion);
    context->stack_limit = context->StackLimit() + context->stack_per_recursion;

    if (!context->native_dispatcher_success)
        if (context->is_aborting)
        {
            context->is_aborting = FALSE;
            context->AbortOutOfMemory();
        }
        else
            ThrowFromMachineCode(context);

    return NULL;
}

class ES_SuspendedCreateNativeConstructorDispatcher
    : public ES_SuspendedCall
{
public:
    ES_SuspendedCreateNativeConstructorDispatcher(ES_Native &nc, ES_Object *prototype, ES_Class *klass)
        : nc(nc),
          prototype(prototype),
          klass(klass)
    {
    }

    virtual void DoCall(ES_Execution_Context *context)
    {
        TRAP(status, block = nc.CreateNativeConstructorDispatcher(prototype, klass));
    }

    ES_Native &nc;
    ES_Object *prototype;
    ES_Class *klass;

    OP_STATUS status;
    ES_NativeCodeBlock *block;
};

/* static */ void
ES_Execution_Context::DoConstructFromMachineCode(OpPseudoThread *thread)
{
    ES_Execution_Context *context = static_cast<ES_Execution_Context *>(thread);

    OP_ASSERT(context->native_stack_frame);

    context->reg = ES_NativeStackFrame::GetRegisterFrame(context->native_stack_frame);
    context->code = ES_NativeStackFrame::GetCode(context->native_stack_frame);
    context->native_dispatcher_success = TRUE;

    unsigned cw_index = context->stored_cw_index, actual_cw_index = (cw_index < context->code->data->codewords_count ? cw_index : cw_index - context->code->data->codewords_count);
    ES_CodeWord *word = context->code->data->codewords + actual_cw_index;

    OP_ASSERT(word[0].instruction == ESI_CONSTRUCT);

    ES_CodeWord::Index rel_frame_start = word[1].index;
    ES_CodeWord::Index argc = word[2].index;

    ES_Value_Internal &function_val = context->reg[rel_frame_start + 1];
    if (!function_val.IsObject() || !function_val.GetObject(context)->IsFunctionObject())
    {
        context->ThrowErrorBase(word, TYPE_ERROR, EXPECTED_CONSTRUCTOR);
        context->native_dispatcher_success = FALSE;
        return;
    }

    if (--context->time_until_check == 0)
        context->CheckOutOfTime();

    ES_Function *function = static_cast<ES_Function *>(function_val.GetObject(context));
    ES_FunctionCode *called_code;

    if (!function->IsHostObject() && (called_code = function->GetFunctionCode()) != NULL)
    {
        ABORT_CONTEXT_IF_MEMORY_ERROR(context->frame_stack.Push(context));

        context->overlap = 0;
        context->ip = &context->exit_programs[EXIT_PROGRAM];

        ABORT_CONTEXT_IF_MEMORY_ERROR(context->frame_stack.Push(context));

        context->frame_stack.TopFrame()->true_ip = word + 3;

        context->overlap = context->code->data->register_frame_size - rel_frame_start;
        ABORT_CONTEXT_IF_MEMORY_ERROR(context->register_blocks.Allocate(context, context->first_in_block, context->reg, MAX(argc + 2, called_code->data->register_frame_size), context->overlap, argc + 2));

        context->in_constructor = TRUE;
        context->SetupCallState(argc, function);

        ES_Global_Object *global_object = context->GetGlobalObject();
        ES_Value_Internal prototype_value;
        ES_Object *prototype;
        GetResult result;

        if (function->Class() == global_object->GetNativeFunctionClass())
        {
            function->GetCachedAtIndex(ES_PropertyIndex(ES_Function::PROTOTYPE), prototype_value);

            if (prototype_value.IsSpecial())
                result = static_cast<ES_Special_Property *>(prototype_value.GetDecodedBoxed())->SpecialGetL(context, function, prototype_value, function);
            else
                result = PROP_GET_OK;
        }
        else
            result = function->GetL(context, RT_DATA.idents[ESID_prototype], prototype_value);

        ES_Object *used_prototype;

        if ((result == PROP_GET_OK || result == PROP_GET_OK_CAN_CACHE) && prototype_value.IsObject())
            used_prototype = prototype = prototype_value.GetObject(context);
        else
        {
            used_prototype = global_object->GetObjectPrototype();
            prototype = NULL;
        }

        ES_Class *klass = used_prototype->SetSubObjectClass(context);

        context->reg[0].SetObject(ES_Object::Make(context, klass));

        context->Reserve(DoExecute, context->stack_per_recursion);
        context->stack_limit = context->StackLimit() + context->stack_per_recursion;

        context->frame_stack.Pop(context);

        if (!context->current_exception.IsNoExceptionCookie())
        {
            context->native_dispatcher_success = FALSE;
            return;
        }

        if (!function->data.native.native_ctor_code_block && called_code->data->CanHaveNativeDispatcher())
        {
            if (!called_code->data->optimization_data)
            {
                called_code->data->FindInstructionOffsets(context);

                ES_SuspendedAnalyzer call(called_code);
                context->SuspendedCall(&call);
                if (OpStatus::IsError(call.status))
                    context->AbortOutOfMemory();

                called_code->data->optimization_data = call.data;
            }

            if (called_code->data->optimization_data)
            {
#ifdef ES_DEBUG_NATIVE_BEHAVIOUR
                extern BOOL g_debug_native_behaviour;
                if (g_debug_native_behaviour)
                {
                    const char *fnname = "<unknown>";

                    TempBuffer buffer;
                    OpString8 buffer8;

                    AppendFunctionNameL(buffer, static_cast<ES_FunctionCode *>(called_code));
                    buffer8.SetL(buffer.GetStorage());
                    fnname = buffer8.CStr();

                    printf("compiling native constructor dispatcher: %s (slow case calls: %u, codewords: %u)\n", fnname, called_code->slow_case_calls, called_code->data->codewords_count);
                    fflush(stdout);
                }

                extern unsigned g_total_native_recompiles;
                ++g_total_native_recompiles;
#endif // ES_DEBUG_NATIVE_BEHAVIOUR

                ES_Native nc(context, called_code);
                ES_SuspendedCreateNativeConstructorDispatcher call(nc, prototype, klass);

                context->SuspendedCall(&call);

                if (call.status == OpStatus::OK)
                {
                    function->data.native.native_ctor_dispatcher = call.block->GetAddress();
                    function->data.native.native_ctor_code_block = call.block;

                    function->object_bits |= ES_Object::MASK_IS_DISPATCHED_CTOR;

                    call.block->SetFunctionObject(function);

                    called_code->slow_case_calls = 0;
                }
                else
                {
                    /* Delayed abort to allow ES_Native to be destructed. */
                    context->is_aborting = TRUE;
                    context->native_dispatcher_success = FALSE;
                    return;
                }
            }
        }
    }
    else
    {
        ES_Function::BuiltIn *construct;
        if (function->IsHostObject())
            construct = ES_Host_Function::Construct;
        else
            construct = function->GetConstruct();

        if (!construct)
        {
            context->ThrowErrorBase(word, TYPE_ERROR, EXPECTED_CONSTRUCTOR);
            context->native_dispatcher_success = FALSE;
        }
        else if (!construct(context, argc, context->reg + rel_frame_start + 2, context->reg + rel_frame_start))
            context->native_dispatcher_success = FALSE;
    }
}

/* static */ ES_Object *ES_CALLING_CONVENTION
ES_Execution_Context::AllocateObjectFromMachineCodeSimple(ES_Execution_Context *context, ES_Class *klass)
{
    return ES_Object::Make(context, klass);
}

/* static */ ES_Object *ES_CALLING_CONVENTION
ES_Execution_Context::AllocateObjectFromMachineCodeComplex(ES_Execution_Context *context, ES_Object *constructor)
{
    ES_Global_Object *global_object = context->GetGlobalObject();
    ES_Value_Internal prototype_value;
    GetResult result;

    if (constructor->Class() == global_object->GetNativeFunctionClass())
    {
        constructor->GetCachedAtIndex(ES_PropertyIndex(ES_Function::PROTOTYPE), prototype_value);

        if (prototype_value.IsSpecial())
            result = static_cast<ES_Special_Property *>(prototype_value.GetDecodedBoxed())->SpecialGetL(context, constructor, prototype_value, constructor);
        else
            result = PROP_GET_OK;
    }
    else
    {
        context->reg = ES_NativeStackFrame::GetRegisterFrame(context->native_stack_frame);
        context->code = ES_NativeStackFrame::GetCode(context->native_stack_frame);

        result = constructor->GetL(context, RT_DATA.idents[ESID_prototype], prototype_value);
    }

    ES_Object *prototype;

    if (GET_OK(result) && prototype_value.IsObject())
        prototype = prototype_value.GetObject(context);
    else
        prototype = global_object->GetObjectPrototype();

    return ES_Object::Make(context, prototype->SetSubObjectClass(context));
}

/* static */ void *ES_CALLING_CONVENTION
ES_Execution_Context::GetNamedImmediate(ES_Execution_Context *context, unsigned cw_index)
{
    ES_Code *code = ES_NativeStackFrame::GetCode(context->native_stack_frame);
    ES_CodeWord *ip = code->data->codewords + (cw_index < code->data->codewords_count ? cw_index : cw_index - code->data->codewords_count);

    if (ip->instruction == ESI_GETN_IMM)
        context->IH_GETN_IMM(ip + 1);
    else
        context->IH_GET_LENGTH(ip + 1);

    if (SHOULD_UPDATE_NATIVE_DISPATCHER() || code->has_failed_inlined_function)
        return context->DoUpdateNativeDispatcher(cw_index + 5);
    else
        return NULL;
}

/* static */ void *ES_CALLING_CONVENTION
ES_Execution_Context::PutNamedImmediate(ES_Execution_Context *context, unsigned cw_index)
{
    ES_Code *code = ES_NativeStackFrame::GetCode(context->native_stack_frame);
    ES_CodeWord *ip = code->data->codewords + (cw_index < code->data->codewords_count ? cw_index : cw_index - code->data->codewords_count);

    if (ip->instruction == ESI_PUTN_IMM || ip->instruction == ESI_INIT_PROPERTY)
        context->IH_PUTN_IMM(ip + 1);
    else
        context->IH_PUT_LENGTH(ip + 1);

    if (SHOULD_UPDATE_NATIVE_DISPATCHER())
        return context->DoUpdateNativeDispatcher(cw_index + 5);
    else
        return NULL;
}

/* static */ void *ES_CALLING_CONVENTION
ES_Execution_Context::GetGlobalImmediate(ES_Execution_Context *context, unsigned cw_index)
{
    ES_Code *code = ES_NativeStackFrame::GetCode(context->native_stack_frame);
    ES_CodeWord *ip = code->data->codewords + (cw_index < code->data->codewords_count ? cw_index : cw_index - code->data->codewords_count);

    context->IH_GET_GLOBAL(ip + 1);

    if (SHOULD_UPDATE_NATIVE_DISPATCHER() || code->has_failed_inlined_function)
        return context->DoUpdateNativeDispatcher(cw_index + 4);
    else
        return NULL;
}

/* static */ void *ES_CALLING_CONVENTION
ES_Execution_Context::GetGlobal(ES_Execution_Context *context, unsigned cw_index)
{
    ES_Code *code = ES_NativeStackFrame::GetCode(context->native_stack_frame);
    ES_CodeWord *ip = code->data->codewords + (cw_index % code->data->codewords_count);

    context->IH_GET_GLOBAL(ip + 1);

    if (SHOULD_UPDATE_NATIVE_DISPATCHER())
        return context->DoUpdateNativeDispatcher(cw_index + 4);
    else
        return NULL;
}

/* static */ void *ES_CALLING_CONVENTION
ES_Execution_Context::PutGlobal(ES_Execution_Context *context, unsigned cw_index)
{
    ES_Code *code = ES_NativeStackFrame::GetCode(context->native_stack_frame);
    ES_CodeWord *ip = code->data->codewords + (cw_index % code->data->codewords_count);

    context->IH_PUT_GLOBAL(ip + 1);

    if (SHOULD_UPDATE_NATIVE_DISPATCHER())
        return context->DoUpdateNativeDispatcher(cw_index + 4);
    else
        return NULL;
}

/* static */ void ES_CALLING_CONVENTION
ES_Execution_Context::ReadLoopIO(ES_Execution_Context *context, ES_Code *code, ES_Value_Internal *reg)
{
    ES_Global_Object *global_object = code->global_object;

    for (ES_Code::LoopIO *loop_io = code->first_loop_io; loop_io; loop_io = loop_io->next)
        if (loop_io->type == ES_Code::LoopIO::TYPE_GLOBAL_IMM)
            reg[loop_io->alias_register] = global_object->GetVariableValue(loop_io->index);
        else
            global_object->GetCached(global_object->Class()->GetLayoutInfoAtIndex(ES_LayoutIndex(loop_io->index)), reg[loop_io->alias_register]);
}

/* static */ void ES_CALLING_CONVENTION
ES_Execution_Context::WriteLoopIO(ES_Execution_Context *context, ES_Code *code, ES_Value_Internal *reg)
{
    ES_Global_Object *global_object = code->global_object;

    for (ES_Code::LoopIO *loop_io = code->first_loop_io; loop_io; loop_io = loop_io->next)
        if (loop_io->type == ES_Code::LoopIO::TYPE_GLOBAL_IMM)
            global_object->GetVariableValue(loop_io->index) = reg[loop_io->alias_register];
        else
            global_object->UnsafePutL(context, loop_io->name, reg[loop_io->alias_register]);
}

/* static */ void
ES_Execution_Context::DoCallNativeDispatcher(OpPseudoThread *thread)
{
    ES_Execution_Context *context = static_cast<ES_Execution_Context *>(thread);
    context->native_dispatcher_success = context->CallBytecodeToNativeTrampoline(context->code, context->native_dispatcher_reg, context->native_code_block, context->native_dispatcher, context->native_dispatcher_argc);
}

/* static */ void ES_CALLING_CONVENTION
ES_Execution_Context::StackOrRegisterLimitExceeded(ES_Execution_Context *context, BOOL is_constructor)
{
    ES_Code *called_code = ES_NativeStackFrame::GetCode(context->native_stack_frame);
    ES_Value_Internal *reg = context->reg = ES_NativeStackFrame::GetRegisterFrame(context->native_stack_frame);
    unsigned rel_frame_start = ES_NativeStackFrame::GetRegisterFrameOffset(context->native_stack_frame);
    unsigned argc = ES_NativeStackFrame::GetArgumentsCount(context->native_stack_frame);

    unsigned allocated_size = 0, overlap = 0, reserved_stacksize;
    ES_NativeStackFrame *stored_native_stack_frame = context->native_stack_frame;

    OP_ASSERT(stored_native_stack_frame != ES_NativeStackFrame::GetNextFrame(stored_native_stack_frame));

    //printf("context->reg going in: %p (-> %p)\n", context->reg, context->reg_limit);

    if (context->frame_stack.TotalUsed() > ES_MAXIMUM_FUNCTION_RECURSION)
    {
        context->ThrowRangeError("Maximum recursion depth exceeded");
        ThrowFromMachineCode(context);
    }

    BOOL allocate_register_frame = reg + called_code->data->register_frame_size >= context->reg_limit;
    ES_FrameStackIterator frames(context);

    /* First frame (it's the native stack frame that called this function.) */
    frames.Next();
    /* Next frame, can be either native or virtual. */
    frames.Next();

    context->reg = frames.GetRegisterFrame();
    context->code = frames.GetCode();

    OP_ASSERT(static_cast<unsigned>(reg - context->reg) == rel_frame_start);

    if (allocate_register_frame)
    {
        allocated_size = MAX(argc + 2, called_code->data->register_frame_size);
        overlap = context->code->data->register_frame_size - rel_frame_start;
    }

    ABORT_CONTEXT_IF_MEMORY_ERROR(context->frame_stack.Push(context));

    if (allocate_register_frame)
    {
        ABORT_CONTEXT_IF_MEMORY_ERROR(context->register_blocks.Allocate(context, context->first_in_block, context->native_dispatcher_reg, allocated_size, overlap, argc + 2));
        reserved_stacksize = 2 * context->stack_per_recursion;
    }
    else
    {
        context->native_dispatcher_reg = reg;
        reserved_stacksize = context->stack_per_recursion;
    }

    if (!called_code->data->is_strict_mode && context->native_dispatcher_reg->IsNullOrUndefined())
        context->native_dispatcher_reg->SetObject(context->code->global_object);

    //printf("context->reg going in: %p\n", context->reg);

    /* NOTE: this does not necessary point to the previous register frame.  But
       context->reg here is used in the bytecode to native trampoline, and later
       in native dispatchers, to calculate the rel_frame_start of this call, so
       the important thing is that it is rel_frame_start less than the native
       dispatchers register frame, not that it is really the previous register
       frame. */
    context->reg = context->native_dispatcher_reg - rel_frame_start;

    context->native_dispatcher = is_constructor ? static_cast<ES_Function *>(context->native_dispatcher_reg[1].GetObject(context))->data.native.native_ctor_dispatcher : called_code->native_dispatcher;
    context->native_code_block = is_constructor ? static_cast<ES_Function *>(context->native_dispatcher_reg[1].GetObject(context))->data.native.native_ctor_code_block : called_code->native_code_block;
    context->native_dispatcher_argc = argc;

    context->Reserve(DoCallNativeDispatcher, reserved_stacksize);
    context->stack_limit = context->StackLimit() + context->stack_per_recursion;

    //printf("context->reg coming out: %p\n", context->reg);

    if (allocate_register_frame)
        context->register_blocks.Free(allocated_size, overlap, 1, context->first_in_block);

    context->frame_stack.Pop(context);

#ifdef _DEBUG
    context->reg = reinterpret_cast<ES_Value_Internal *>(0xfdfdfdfd);
    context->code = reinterpret_cast<ES_Code *>(0xfdfdfdfd);
#endif // _DEBUG

    if (!context->native_dispatcher_success)
        ThrowFromMachineCode(context);

    OP_ASSERT(stored_native_stack_frame != ES_NativeStackFrame::GetNextFrame(stored_native_stack_frame));

    context->native_stack_frame = ES_NativeStackFrame::GetNextFrame(stored_native_stack_frame);
}

void
ES_Execution_Context::AllocateProfileData()
{
    return AllocateProfileData(Code()->data);
}

void
ES_Execution_Context::AllocateProfileData(ES_CodeStatic *data)
{
    ES_Suspended_NEWA<unsigned char> snew(data->codewords_count);
    SuspendedCall(&snew);
    if (!snew.storage)
        AbortOutOfMemory();

    data->profile_data = snew.storage;
    if (!data->profile_data)
        AbortOutOfMemory();
    op_memset(data->profile_data, 0, data->codewords_count);
}

#ifdef _DEBUG

BOOL es_is_in_slow_path = FALSE;

void
ES_DebugEnteredSlowPath()
{
    es_is_in_slow_path = TRUE;
}

#endif // _DEBUG

/* static  */ void *ES_CALLING_CONVENTION
ES_Execution_Context::UpdateNativeDispatcher(ES_Execution_Context *context, unsigned cw_index)
{
#ifdef _DEBUG
    es_is_in_slow_path = FALSE;
#endif // _DEBUG

    ES_Code *code = ES_NativeStackFrame::GetCode(context->native_stack_frame);

    if (SHOULD_UPDATE_NATIVE_DISPATCHER())
        return context->DoUpdateNativeDispatcher(cw_index);
    else
        return NULL;
}

/* static  */ void *ES_CALLING_CONVENTION
ES_Execution_Context::ForceUpdateNativeDispatcher(ES_Execution_Context *context, unsigned cw_index)
{
    ES_Code *code = ES_NativeStackFrame::GetCode(context->native_stack_frame);

#ifdef _DEBUG
    es_is_in_slow_path = FALSE;

    OP_ASSERT(code->has_failed_preconditions == 1 || code->has_failed_inlined_function != 0);
#endif // _DEBUG

    if (code->has_failed_inlined_function)
        ++code->total_inline_failures;

    return context->DoUpdateNativeDispatcher(cw_index);
}

class ES_SuspendedUpdateNativeDispatcher
    : public ES_SuspendedCall
{
public:
    ES_SuspendedUpdateNativeDispatcher(unsigned cw_index)
        : cw_index(cw_index)
    {
    }

    virtual void DoCall(ES_Execution_Context *context)
    {
        TRAP(status, entry_point = context->ReallyDoUpdateNativeDispatcherL(cw_index));
        if (OpStatus::IsMemoryError(status))
            context->AbortOutOfMemory();
    }

    unsigned cw_index;
    OP_STATUS status;
    void *entry_point;
};

void *
ES_Execution_Context::DoUpdateNativeDispatcher(unsigned cw_index)
{
    ES_SuspendedUpdateNativeDispatcher suspended(cw_index);

    SuspendedCall(&suspended);

    if (suspended.status != OpStatus::OK)
        AbortOutOfMemory();

    return suspended.entry_point;
}

void *
ES_Execution_Context::ReallyDoUpdateNativeDispatcherL(unsigned cw_index)
{
    ES_Code *code = ES_NativeStackFrame::GetCode(native_stack_frame);
    BOOL is_constructor;

    if (cw_index >= code->data->codewords_count)
    {
        is_constructor = TRUE;
        cw_index -= code->data->codewords_count;
    }
    else
        is_constructor = FALSE;

#ifdef ES_DEBUG_NATIVE_BEHAVIOUR
    extern BOOL g_debug_native_behaviour;
    if (g_debug_native_behaviour)
    {
        const char *fnname;

        TempBuffer buffer1;
        OpString8 string1;

        ES_Code *actual_code = code->parent_code ? code->parent_code : code;

        if (code->type == ES_Code::TYPE_FUNCTION)
        {
            AppendFunctionNameL(buffer1, static_cast<ES_FunctionCode *>(code));
            string1.SetL(buffer1.GetStorage());
            fnname = string1.CStr();
        }
        else if (code->type == ES_Code::TYPE_EVAL_PLAIN)
            fnname = "<eval plain>";
        else if (code->type == ES_Code::TYPE_EVAL_ODD)
            fnname = "<eval odd>";
        else
            fnname = "<program>";

        OpString8 string2;

        if (code != actual_code)
        {
            string2.Set("<loop in ");
            string2.Append(fnname);
            string2.Append(">");

            fnname = string2.CStr();
        }

        printf("recompiling native %sdispatcher: %s (slow case calls: %u, codewords: %u, entry point: %u)\n", is_constructor ? "constructor " : "", fnname, code->slow_case_calls, code->data->codewords_count, cw_index);
        fflush(stdout);
    }

    extern unsigned g_total_native_recompiles;
    ++g_total_native_recompiles;
#endif // ES_DEBUG_NATIVE_BEHAVIOUR

    code->slow_case_calls = 0;

    ES_Native nc(this, code); ANCHOR(ES_Native, nc);

    void *entry_point;

    if (is_constructor)
    {
        ES_Value_Internal *registers = ES_NativeStackFrame::GetRegisterFrame(native_stack_frame);
        ES_Function *function = static_cast<ES_Function *>(registers[1].GetObject(this));
        ES_Global_Object *global_object = GetGlobalObject();

        ES_Value_Internal prototype_value;
        ES_Object *prototype;
        GetResult result;

        if (function->Class() == global_object->GetNativeFunctionClass())
        {
            function->GetCachedAtIndex(ES_PropertyIndex(ES_Function::PROTOTYPE), prototype_value);

            if (prototype_value.IsSpecial())
                result = static_cast<ES_Special_Property *>(prototype_value.GetDecodedBoxed())->SpecialGetL(this, function, prototype_value, function);
            else
                result = PROP_GET_OK;
        }
        else
            result = function->GetL(this, RT_DATA.idents[ESID_prototype], prototype_value);

        if ((result == PROP_GET_OK || result == PROP_GET_OK_CAN_CACHE) && prototype_value.IsObject())
            prototype = prototype_value.GetObject();
        else
            prototype = global_object->GetObjectPrototype();

        ES_Class *klass = prototype->SetSubObjectClass(this);

        ES_NativeCodeBlock *native_ctor_code_block = NULL;
        TRAPD(status, native_ctor_code_block = nc.CreateNativeConstructorDispatcher(prototype, klass, code->data->codewords + cw_index, &entry_point));

        if (OpStatus::IsMemoryError(status))
            LEAVE(OpStatus::ERR_NO_MEMORY);

        if (function->data.native.native_ctor_code_block)
            heap->DeprecateNativeDispatcher(function->data.native.native_ctor_code_block);

        function->data.native.native_ctor_code_block = native_ctor_code_block;
        function->data.native.native_ctor_code_block->SetFunctionObject(function);
        function->data.native.native_ctor_dispatcher = function->data.native.native_ctor_code_block->GetAddress();
    }
    else
    {
        if (code->native_code_block && code->native_code_block->IsLoop())
            nc.SetIsLoopDispatcher(code->native_code_block->GetLoopIndex());

        BOOL light_weight_allowed = TRUE;

        if (ES_NativeStackFrame::GetArgumentsObject(native_stack_frame) || ES_NativeStackFrame::GetVariableObject(native_stack_frame))
            light_weight_allowed = FALSE;

        ES_NativeCodeBlock *code_block = NULL;
        TRAPD(status, code_block = nc.CreateNativeDispatcher(code->data->codewords + cw_index, &entry_point, FALSE, light_weight_allowed));

        if (OpStatus::IsMemoryError(status))
            LEAVE(OpStatus::ERR_NO_MEMORY);

        if (code->native_code_block)
            heap->DeprecateNativeDispatcher(code->native_code_block);

        code->native_code_block = code_block;
        if (!code->native_code_block->IsLoop())
            code->native_code_block->SetFunctionCode(static_cast<ES_FunctionCode *>(code));

        code->native_dispatcher = code->native_code_block->GetAddress();
    }

    code->has_failed_inlined_function = 0;

    return entry_point;
}

/* static */ void *ES_CALLING_CONVENTION
ES_Execution_Context::LightWeightDispatcherFailure(ES_Execution_Context *context)
{
    BOOL register_space_exceeded = FALSE;

    context->register_blocks.AdjustUsed();

    ES_Block<ES_Value_Internal> *block = context->register_blocks.GetLastUsedBlock();
    if (block->Used() > block->Capacity())
        register_space_exceeded = TRUE;

    context->register_blocks.ReadjustUsed();

    ES_Code *code = ES_NativeStackFrame::GetCode(context->native_stack_frame);

#ifdef ES_DEBUG_NATIVE_BEHAVIOUR
    extern BOOL g_debug_native_behaviour;
    if (g_debug_native_behaviour)
    {
        const char *fnname = "<unknown>";

        TempBuffer buffer;
        OpString8 buffer8;

        if (code->type == ES_Code::TYPE_FUNCTION)
        {
            AppendFunctionNameL(buffer, static_cast<ES_FunctionCode *>(code));
            buffer8.SetL(buffer.GetStorage());
            fnname = buffer8.CStr();
        }

        printf("light-weight native dispatcher failed: %s\n", fnname);
        fflush(stdout);
    }

    extern unsigned g_total_native_recompiles;
    ++g_total_native_recompiles;
#endif // ES_DEBUG_NATIVE_BEHAVIOUR

    void *entry_point;

    ES_Suspended_CreateNativeDispatcher suspended(code, code->data->codewords, &entry_point, FALSE);

    context->SuspendedCall(&suspended);

    if (OpStatus::IsMemoryError(suspended.status))
        context->AbortOutOfMemory();

    if (code->native_code_block)
        context->heap->DeprecateNativeDispatcher(code->native_code_block);

    code->native_code_block = suspended.block;
    code->native_code_block->SetFunctionCode(static_cast<ES_FunctionCode *>(code));
    code->native_dispatcher = suspended.block->GetAddress();

    if (register_space_exceeded)
    {
        StackOrRegisterLimitExceeded(context, FALSE);

        return NULL;
    }
    else
        return entry_point;
}

/* static */ void *ES_CDECL_CALLING_CONVENTION
ES_Execution_Context::ReconstructNativeStack(ES_Execution_Context *context, void **stack_pointer)
{
    ES_NativeStackFrame *previous_nsf = context->native_stack_frame;

    /* Set up just now by the bytecode-to-native trampline, but it will confuse
       the stack frame iterator since there aren't any actual native stack
       frames yet. */
    context->native_stack_frame = NULL;

    void **stack_limit = reinterpret_cast<void **>(context->StackLimit() + context->stack_per_recursion);
    unsigned stack_used = 0, count = 0;

    ES_FrameStackIterator frames1(context);
    frames1.Next();

    BOOL first_in_block = context->first_in_block, skip_first_frame = FALSE;

    if (!context->native_dispatcher_reg)
    {
        frames1.Next();

        first_in_block = frames1.GetVirtualFrame()->first_in_block;
        skip_first_frame = TRUE;
    }

    BOOL include_return_address = skip_first_frame;

    while (frames1.GetCode() && frames1.GetCode()->native_dispatcher && (!include_return_address || ES_NativeStackFrame::GetReturnAddress(frames1.GetCode(), frames1.GetCodeWord())) && !frames1.InConstructor())
    {
        unsigned frame_size = ES_NativeStackFrame::GetFrameSize(frames1.GetCode(), include_return_address);

        if (stack_pointer - (stack_used + frame_size) / sizeof(void *) <= stack_limit)
            break;

        ++count;
        stack_used += frame_size;

        if (frames1.GetCode()->native_code_block->IsLoop() || first_in_block || !frames1.Next() || !frames1.GetVirtualFrame())
            break;

        first_in_block = frames1.GetVirtualFrame()->first_in_block;
        include_return_address = TRUE;
    }

    OP_ASSERT(count != 0);

#if 0
    /* Need to suspend to do this: this function is always called with rather
       little remaining stack space, so using printf() is a no-no. */
#ifdef ES_DEBUG_NATIVE_BEHAVIOUR
    extern BOOL g_debug_native_behaviour;
    if (g_debug_native_behaviour)
        printf("converting %u virtual frames into native frames\n", count);
#endif // ES_DEBUG_NATIVE_BEHAVIOUR
#endif // 0

    ES_FrameStackIterator frames2(context);
    frames2.Next();

    void **write_pointer = stack_pointer - stack_used / sizeof(void *), *nsf, **nsf_pointer = &nsf;
    unsigned index, overlap = context->overlap;
    first_in_block = context->first_in_block;

    if (skip_first_frame)
    {
        context->register_blocks.Free(MAX(2 + frames2.GetArgumentsCount(), frames2.GetCode()->data->register_frame_size), overlap, 0, first_in_block);

        frames2.Next();

        overlap = frames2.GetVirtualFrame()->register_overlap;
        first_in_block = frames2.GetVirtualFrame()->first_in_block;
    }

    include_return_address = skip_first_frame;

    for (index = 0;; ++index)
    {
        unsigned frame_size = ES_NativeStackFrame::GetFrameSize(frames2.GetCode(), include_return_address);

        write_pointer += frame_size / sizeof(void *);

        ES_NativeStackFrame::InitializeFrame(write_pointer, frames2, nsf_pointer, include_return_address);

        if (index == count - 1)
            break;

        context->register_blocks.Free(frames2.GetCode()->data->register_frame_size, overlap, 0, first_in_block);

        frames2.Next();

        overlap = frames2.GetVirtualFrame()->register_overlap;
        first_in_block = frames2.GetVirtualFrame()->first_in_block;
        include_return_address = TRUE;
    }

    unsigned next_frame_size = frames2.GetCode()->data->register_frame_size;

    frames2.Next();

    ES_NativeStackFrame::SetRegisterFrame(previous_nsf, frames2.GetRegisterFrame());

    frames2.GetVirtualFrame()->next_frame_size = next_frame_size;

    context->frame_stack.DropFrames(skip_first_frame ? count : count - 1);
    context->overlap = overlap;
    context->first_in_block = first_in_block;

    context->arguments_object = NULL;
    context->variable_object = NULL;

    *nsf_pointer = previous_nsf;

    context->native_stack_frame = static_cast<ES_NativeStackFrame *>(nsf);

#ifdef _DEBUG
    context->ValidateNativeStack();
#endif // _DEBUG

    return stack_pointer - stack_used / sizeof(void *);
}

/* static */ void ES_CALLING_CONVENTION
ES_Execution_Context::ThrowFromMachineCode(ES_Execution_Context *context)
{
    OP_ASSERT(context->native_stack_frame);

#ifdef OPPSEUDOTHREAD_STACK_SWAPPING
    OP_ASSERT(context->stack_ptr > context->StackLimit() && context->stack_ptr < context->StackTop());
#endif // OPPSEUDOTHREAD_STACK_SWAPPING

    while (ES_NativeStackFrame::GetCode(context->native_stack_frame))
    {
        if (ES_Arguments_Object *arguments = ES_NativeStackFrame::GetArgumentsObject(context->native_stack_frame))
            DetachArgumentsObject(context, arguments);
        if (ES_Object *variables = ES_NativeStackFrame::GetVariableObject(context->native_stack_frame))
            DetachVariableObject(context, variables);

        context->native_stack_frame = ES_NativeStackFrame::GetNextFrame(context->native_stack_frame);
    }

    void *pointers[ES_Native::THROW_POINTERS_COUNT];

    pointers[ES_Native::THROW_POINTER_CONTEXT] = context;
    pointers[ES_Native::THROW_POINTER_CONTEXT_NATIVE_STACK_FRAME] = &context->native_stack_frame;
    pointers[ES_Native::THROW_POINTER_CONTEXT_STACK_PTR] = &context->stack_ptr;

    context->ThrowFromMachineCodeTrampoline(pointers);
}

#ifdef _DEBUG

void
ES_Execution_Context::ValidateNativeStack()
{
    ES_NativeStackFrame *current = native_stack_frame;

    while (ES_NativeStackFrame *next = ES_NativeStackFrame::GetNextFrame(current))
    {
        if (ES_Code *code = ES_NativeStackFrame::GetCode(next))
        {
            /* Make sure register frames are strictly ordered. */
            OP_ASSERT(ES_NativeStackFrame::GetRegisterFrame(next) <= ES_NativeStackFrame::GetRegisterFrame(current));
            OP_ASSERT(ES_NativeStackFrame::GetRegisterFrame(next) + code->data->register_frame_size >= ES_NativeStackFrame::GetRegisterFrame(current));
            OP_ASSERT(code->native_code_block->ContainsAddress(ES_NativeStackFrame::GetReturnAddress(current)));
        }
        else
        {
            OP_ASSERT(!ES_NativeStackFrame::GetNextFrame(next));
            OP_ASSERT(ES_Native::IsAddressInBytecodeToNativeTrampoline(ES_NativeStackFrame::GetReturnAddress(current)));
        }

        current = next;
    }
}

#endif // _DEBUG

#ifdef ES_NATIVE_PROFILING

class ES_NativeProfilingRecord
{
public:
    ES_FunctionCode *code;
    struct timespec start, current;
};

ES_NativeProfilingRecord g_native_profiling_records[1024];
unsigned g_native_profiling_depth = 0;

static void
AddDifferenceTo(struct timespec &start, struct timespec &end, struct timespec &target)
{
    unsigned elapsed = 0;

    if (start.tv_sec < end.tv_sec)
        elapsed += (1000000000 - start.tv_nsec) + 1000000000 * (end.tv_sec - (start.tv_sec + 1)) + end.tv_nsec;
    else
        elapsed = end.tv_nsec - start.tv_nsec;

    //printf("elapsed: %u\n", elapsed);

    target.tv_sec += elapsed / 1000000000;

    elapsed %= 1000000000;

    target.tv_nsec += elapsed;

    if (target.tv_nsec > 1000000000)
    {
        ++target.tv_sec;
        target.tv_nsec -= 1000000000;
    }
}

/* static */ void ES_CALLING_CONVENTION
ES_Execution_Context::EnteringFunction(ES_FunctionCode *code)
{
    struct timespec now;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &now);

    unsigned index = g_native_profiling_depth++;

    if (index != 0)
        AddDifferenceTo(g_native_profiling_records[index].current, now, code->self);

    g_native_profiling_records[index].code = code;
    g_native_profiling_records[index].start = g_native_profiling_records[index].current = now;
}

/* static */ void ES_CALLING_CONVENTION
ES_Execution_Context::LeavingFunction(ES_FunctionCode *code)
{
    struct timespec now;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &now);

    unsigned index = g_native_profiling_depth--;

    AddDifferenceTo(g_native_profiling_records[index].current, now, code->self);
    AddDifferenceTo(g_native_profiling_records[index].start, now, code->inclusive);

    if (index != 0)
        g_native_profiling_records[index].current = now;
    else
    {
        int stop_here =0;
    }
}

#endif // ES_NATIVE_PROFILING
#endif // ES_NATIVE_SUPPORT

void
ES_Execution_Context::CachePropertyCache(ES_Code::PropertyCache *property_cache)
{
    property_cache->next = property_caches_cache;
    property_caches_cache = property_cache;
}

ES_Code::PropertyCache *
ES_Execution_Context::AllocatePropertyCache()
{
    if (property_caches_cache)
    {
        ES_Code::PropertyCache *to_return = property_caches_cache;
        property_caches_cache = property_caches_cache->next;
        *to_return = ES_Code::PropertyCache();
        return to_return;
    }
    else
    {
        if (IsUsingStandardStack())
            return OP_NEW(ES_Code::PropertyCache, ());
        else
        {
            ES_Suspended_NEW<ES_Code::PropertyCache> suspended;
            GetExecutionContext()->SuspendedCall(&suspended);
            return suspended.storage;
        }
    }
}

void
ES_Execution_Context::CaptureStackTrace(ES_Error *error)
{
    CaptureStackTrace(NULL, error);
}

void
ES_Execution_Context::CaptureStackTrace(ES_CodeWord *ip, ES_Error *error)
{
    ES_StackTraceElement *st = error ? error->GetStackTrace() : stacktrace;
    unsigned length = 0;

    ES_FrameStackIterator frames(this);
    ES_VirtualStackFrame::FrameType frame_type;
    BOOL more_frames = TRUE;

    frames.Next();

    if (ip)
    {
        st[0].code = Code();
        st[0].codeword = ip;

        ++length;

        if (!frames.Next())
            goto finished;
    }

    frame_type = ES_VirtualStackFrame::NORMAL;

    do
    {
#ifdef ES_NATIVE_SUPPORT
        if (!frames.IsFollowedByNative())
#endif // ES_NATIVE_SUPPORT
            if (ES_Code *code = frames.GetCode())
            {
                st[length].code = code;
                st[length].codeword = frames.GetCodeWord();

                /* The frame has the following instruction's codeword, but
                   stacktraces use the calling instruction. Precise adjustment
                   to the previous instruction's codeword (ESI_CALL most likely)
                   would be the ideal thing to do here. */
                if (length > 0 && st[length].codeword)
                    st[length].codeword--;
                st[length].frame_type = frame_type;
                ++length;

                frame_type = ES_VirtualStackFrame::NORMAL;
            }

        if (frames.GetFrameType() != ES_VirtualStackFrame::NORMAL)
            frame_type = frames.GetFrameType();
    }
    while (length != ES_StackTraceElement::STACK_TRACE_ELEMENTS && (more_frames = frames.Next()));

    if (!more_frames && is_top_level_call)
        --length;

finished:
#ifdef ECMASCRIPT_DEBUGGER
    current_exception_signalled = FALSE;
#endif // ECMASCRIPT_DEBUGGER
    if (error)
        error->SetStackTraceLength(length);
    else
        stacktrace_length = length;
}

BOOL
ES_Execution_Context::GenerateStackTraceL(JString *&string, ES_Execution_Context *error_context, ES_Error *error, ES_Error::StackTraceFormat format, unsigned prefix_linebreaks)
{
    OP_ASSERT(error);

    if (GenerateStackTraceL(stacktrace_buffer, error_context, error, format, prefix_linebreaks))
    {
        const uni_char *stacktrace = stacktrace_buffer.GetStorage();
        unsigned length = stacktrace_buffer.Length();

        while (length && stacktrace[length - 1] == '\n')
            --length;

        string = JString::Make(this, stacktrace, length);
        stacktrace_buffer.FreeStorage();
        return TRUE;
    }
    else
    {
        stacktrace_buffer.FreeStorage();
        return FALSE;
    }
}

BOOL
ES_Execution_Context::GenerateStackTraceL(TempBuffer &buffer, ES_Execution_Context *error_context, ES_Error *error, ES_Error::StackTraceFormat format, unsigned prefix_linebreaks)
{
    ES_StackTraceElement *st = error_context ? error_context->stacktrace : stacktrace;
    unsigned st_length = error_context ? error_context->stacktrace_length : stacktrace_length;
    unsigned repeat_following = 0;
    BOOL initial_location = FALSE;

    while (prefix_linebreaks--)
        buffer.AppendL("\n");

    if (st_length > 0)
    {
    again:
        for (unsigned index = 0; index < st_length; ++index)
        {
            ES_Code *code = st[index].code;
            ES_CodeStatic::DebugRecord *debug_record = NULL;
            ES_SourcePosition position;

            if (ES_CodeWord *codeword = st[index].codeword)
                if ((debug_record = code->data->FindDebugRecord(this, ES_CodeStatic::DebugRecord::TYPE_EXTENT_INFORMATION, codeword)) != NULL)
                    code->data->source.Resolve(debug_record->location, code->data->source_storage_owner, position);

            if (format == ES_Error::FORMAT_READABLE)
            {
                if (index == 0)
                {
                    if (!initial_location)
                        buffer.AppendL("Error thrown at ");
                }
                else
                    switch (st[index].frame_type)
                    {
                    case ES_VirtualStackFrame::VIA_TOPRIMITIVE:
                        buffer.AppendL("called via ToPrimitive() from ");
                        break;
                    case ES_VirtualStackFrame::VIA_FUNCTION_APPLY:
                        buffer.AppendL("called via Function.prototype.apply() from ");
                        break;
                    case ES_VirtualStackFrame::VIA_FUNCTION_CALL:
                        buffer.AppendL("called via Function.prototype.call() from ");
                        break;
                    case ES_VirtualStackFrame::VIA_FUNCTION_BIND:
                        buffer.AppendL("called as bound function from ");
                        break;
                    default:
                        buffer.AppendL("called from ");
                    }

                if (debug_record)
                {
                    buffer.AppendL("line ");
                    buffer.AppendUnsignedLongL(position.GetAbsoluteLine());
                    buffer.AppendL(", column ");
                    buffer.AppendUnsignedLongL(position.GetAbsoluteColumn());
                }
                else
                    buffer.AppendL("unknown location");

                buffer.AppendL(" in ");

                if (code->type == ES_Code::TYPE_FUNCTION)
                {
                    ES_FunctionCode *fncode = static_cast<ES_FunctionCode *>(code);
                    ES_FunctionCodeStatic *data = fncode->GetData();

                    AppendFunctionNameL(buffer, fncode);

                    if (data->formals_count != 0)
                    {
                        JString *formal = fncode->GetString(data->formals_and_locals[0]);

                        buffer.AppendL("(");
                        buffer.AppendL(Storage(this, formal), Length(formal));

                        for (unsigned formal_index = 1; formal_index < data->formals_count; ++formal_index)
                        {
                            JString *formal = fncode->GetString(data->formals_and_locals[formal_index]);

                            buffer.AppendL(", ");
                            buffer.AppendL(Storage(this, formal), Length(formal));
                        }

                        buffer.AppendL(")");
                    }
                    else
                        buffer.AppendL("()");

                    if (JString *url = code->url)
                    {
                        buffer.AppendL(" in ");
                        buffer.AppendL(Storage(this, url), Length(url));
                    }

                    buffer.AppendL(":\n");
                }
                else if (code->type == ES_Code::TYPE_EVAL_PLAIN || code->type == ES_Code::TYPE_EVAL_ODD)
                    buffer.AppendL("evaluated code:\n");
                else if (JString *url = code->url)
                {
                    buffer.AppendL(Storage(this, url), Length(url));
                    buffer.AppendL(":\n");
                }
                else
                    buffer.AppendL("program code:\n");

                buffer.AppendL("    ");

                if (debug_record)
                {
                    JString *extent = code->data->source.GetExtent(this, debug_record->location, TRUE);
                    buffer.AppendL(Storage(this, extent), Length(extent));
                }
                else
                    buffer.AppendL("/* no source available */");

                buffer.AppendL("\n");
            }
            else
            {
            repeat:
                if (code->type == ES_Code::TYPE_FUNCTION)
                {
                    ES_FunctionCode *fncode = static_cast<ES_FunctionCode *>(code);

                    AppendFunctionNameL(buffer, fncode);

                    buffer.AppendL("([arguments not available])");
                }
                else if (code->type == ES_Code::TYPE_EVAL_ODD || code->type == ES_Code::TYPE_EVAL_PLAIN)
                {
                    ++repeat_following;
                    continue;
                }

                buffer.AppendL("@");

                if (JString *url = code->url)
                {
                    buffer.AppendL(Storage(this, url), Length(url));
                    buffer.AppendL(":");

                    if (debug_record)
                        buffer.AppendUnsignedLongL(position.GetAbsoluteLine());
                    else
                        buffer.AppendL("0");
                }

                buffer.AppendL("\n");

                if (repeat_following > 0)
                {
                    --repeat_following;
                    goto repeat;
                }
            }
        }

        if (error && error->GetStackTraceLength() != 0)
        {
            st = error->GetStackTrace();
            st_length = error->GetStackTraceLength();
            initial_location = TRUE;

            if (error->ViaConstructor())
                buffer.AppendL("\nError created at ");
            else
                buffer.AppendL("\nError initially occurred at ");

            error = NULL;

            goto again;
        }

        return TRUE;
    }
    else if (error)
    {
        st = error->GetStackTrace();
        st_length = error->GetStackTraceLength();
        error = NULL;

        goto again;
    }
    else
        return FALSE;
}

unsigned
ES_Execution_Context::GetCallLine(ES_Code *code, ES_CodeWord *codeword)
{
    if (!codeword)
        return 0;

    ES_CodeStatic::DebugRecord::Type type = ES_CodeStatic::DebugRecord::TYPE_EXTENT_INFORMATION;
    ES_CodeStatic::DebugRecord *debug_record = code->data->FindDebugRecord(this, type, codeword);

    if (!debug_record)
        return 0;

    unsigned index0, line, column;
    code->data->source.Resolve(debug_record->location, index0, line, column);

    return line;
}

#ifdef ECMASCRIPT_DEBUGGER

unsigned int
ES_Execution_Context::FindBacktraceLine(ES_Code *code, ES_CodeWord *codeword, BOOL top_frame)
{
    if (!code)
        return 0;

    ES_DebugListener::EventType signal = GetDebuggerSignal();

    if (top_frame)
    {
        if (signal == ES_DebugListener::ESEV_ENTERFN)
            return code->data->start_location.Line();
        else if (signal == ES_DebugListener::ESEV_LEAVEFN)
            return code->data->end_location.Line();
    }

    return GetCallLine(code, codeword);
}

unsigned int
ES_Execution_Context::BacktraceForDebugger(unsigned int maximum)
{
    unsigned stack_size = MIN(frame_stack.TotalUsed(), maximum);

    if (rt_data->debugger_call_stack == NULL || rt_data->debugger_call_stack_length < stack_size)
    {
        OP_DELETEA(rt_data->debugger_call_stack);
        rt_data->debugger_call_stack_length = 0;
        rt_data->debugger_call_stack = OP_NEWA(ES_DebugStackElement, stack_size);
        if (rt_data->debugger_call_stack == NULL)
            return 0;
        rt_data->debugger_call_stack_length = stack_size;
    }

    ES_DebugStackElement* cs = rt_data->debugger_call_stack;

    unsigned index = 0;
    ES_FrameStackIterator frames(this);

    while (index != maximum && frames.Next())
    {
#ifdef ES_NATIVE_SUPPORT
        OP_ASSERT(!frames.IsFollowedByNative());
#endif // ES_NATIVE_SUPPORT
        if (ES_Code *code = frames.GetCode())
        {
            cs[index].this_obj = frames.GetThisObjectOrNull();
            cs[index].script_guid = code->GetScriptGuid();
            // The ip of the frame will sometimes be on the next line or statement
            // which causes the wrong line number to be reported. To avoid this we
            // subtract by 1 to get an ip that belongs to the previous opcode.
            cs[index].line_no = FindBacktraceLine(code, frames.GetCodeWord() - 1, (index == 0));

            cs[index].function = frames.GetFunctionObject();
            cs[index].arguments = frames.GetArgumentsObject();
            if (cs[index].arguments == NULL && code->type == ES_Code::TYPE_FUNCTION)
            {
#ifdef ES_NATIVE_SUPPORT
                static_cast<ES_Function *>(cs[index].function)->SetHasCreatedArgumentsArray(this);
#endif // ES_NATIVE_SUPPORT

                ES_Arguments_Object *arguments = ES_Arguments_Object::Make(this, static_cast<ES_Function *>(cs[index].function), frames.GetRegisterFrame(), frames.GetArgumentsCount());
                frames.SetArgumentsObject(arguments);
                arguments->SetUsed();
                cs[index].arguments = arguments;
            }

            cs[index].variables = frames.GetVariableObject();
            if (cs[index].variables == NULL)
                if (code->type == ES_Code::TYPE_FUNCTION)
                {
                    cs[index].variables = CreateVariableObject(frames.GetFunctionObject()->GetFunctionCode(), frames.GetRegisterFrame() + 2);
                    frames.SetVariableObject(cs[index].variables);
                }
                else
                    cs[index].variables = GetGlobalObject();

            ++index;
        }
    }

    // If we're currently signalling ESEV_CALLSTARTING to the debugger, then
    // don't report the frame for the function we're about to call.
    if (GetDebuggerSignal() == ES_DebugListener::ESEV_CALLSTARTING && index > 0)
        --index;

    return index;
}

void
ES_Execution_Context::SignalToDebuggerInternal(ES_DebugListener::EventType type, BOOL source_loc, ES_CodeWord *ip)
{
    unsigned line = 0;
    unsigned script_guid = 0;
    dbg_signal = type;
    ES_Code *code = Code();

    if (source_loc && code)
    {
        switch (type)
        {
        case ES_DebugListener::ESEV_NEWSCRIPT:
            line = 0;
            break;
        case ES_DebugListener::ESEV_ENTERFN:
            line = code->data->start_location.IsValid() ? code->data->start_location.Line() : 0;
            break;
        case ES_DebugListener::ESEV_LEAVEFN:
            line = code->data->end_location.IsValid() ? code->data->end_location.Line() : 0;
            break;
        case ES_DebugListener::ESEV_ERROR:
        case ES_DebugListener::ESEV_EXCEPTION:
            if (current_exception_signalled)
            {
                dbg_signal = ES_DebugListener::ESEV_NONE;
                return;
            }
            else
            {
                unsigned handler_ip;
                ES_CodeStatic::ExceptionHandler::Type handler_type;
                ES_FrameStackIterator frames(this);

                while (frames.Next())
                    if (ES_Code *frame_code = frames.GetCode())
                        if (ES_CodeWord *frame_ip = frames.GetCodeWord())
                            if (frame_code->data->FindExceptionHandler(frame_ip - frame_code->data->codewords, handler_ip, handler_type))
                                if (handler_type == ES_CodeStatic::ExceptionHandler::TYPE_CATCH)
                                {
                                    type = ES_DebugListener::ESEV_HANDLED_ERROR;
                                    break;
                                }

                current_exception_signalled = TRUE;
            }
        // fall through
        default:
            if (ES_CodeStatic::DebugRecord *debug_record = code->data->FindDebugRecord(this, ES_CodeStatic::DebugRecord::TYPE_EXTENT_INFORMATION, ip))
            {
                line = debug_record->location.Line();
                OP_ASSERT(line);
            }
            else if (type == ES_DebugListener::ESEV_RETURN)
                /* In case of the empty function, reported position
                   should point at function's end brace. */
                line = code->data->end_location.Line();
        }

        script_guid = code->GetScriptGuid();
    }

    ES_Suspended_Debugger_HandleEvent handle_event(type, script_guid, line);
    SuspendedCall(&handle_event);
    if (handle_event.return_value == ES_DebugListener::ESACT_SUSPEND)
        YieldExecution();

    dbg_signal = ES_DebugListener::ESEV_NONE;
}

void
ES_Execution_Context::SetCallDetails(const ES_Value_Internal &this_value, ES_Object *callee, int argc, ES_Value_Internal *argv)
{
    this_value.ExportL(this, call_details.this_value);

    call_details.callee = callee;
    call_details.argc = argc;

    if (argc)
    {
        if (call_details.argv_allocated < argc)
        {
            call_details.argv_allocated = 0;

            ES_Suspended_ResizeArray<ES_Value> suspended_resize(&call_details.argv, argc);
            SuspendedCall(&suspended_resize);
            if (!call_details.argv)
                AbortOutOfMemory();

            call_details.argv_allocated = argc;
        }

        for (int index = 0; index < argc; ++index)
            argv[index].ExportL(this, call_details.argv[index]);
    }
}

void
ES_Execution_Context::GetCallDetails(ES_Value &this_value, ES_Object *&callee, int &argc, ES_Value *&argv)
{
    this_value = call_details.this_value;
    callee = call_details.callee;
    argc = call_details.argc;
    argv = call_details.argv;
}

#endif // ECMASCRIPT_DEBUGGER

BOOL
ES_Execution_Context::GetStackLocationAtIndex(ES_Error *error, unsigned index, JString *&url, ES_SourcePosition &position)
{
    unsigned trace_length = stacktrace_length;
    ES_StackTraceElement *st = stacktrace;

    if (trace_length == 0)
    {
        st = error->GetStackTrace();
        trace_length = error->GetStackTraceLength();
    }

    if (index >= trace_length)
        return FALSE;

    if (ES_CodeWord *codeword = st[index].codeword)
    {
        ES_Code *code = st[index].code;

        url = code->url;

        if (ES_CodeStatic::DebugRecord *debug_record = code->data->FindDebugRecord(this, ES_CodeStatic::DebugRecord::TYPE_EXTENT_INFORMATION, codeword))
        {
            code->data->source.Resolve(debug_record->location, code->data->source_storage_owner, position);
            return TRUE;
        }
    }
    return FALSE;
}

/* virtual */ void
ES_StackPointerAnchor::GCTrace()
{
    GC_PUSH_BOXED(context->heap, ptr);
}

/* virtual */ void
ES_StackValueAnchor::GCTrace()
{
    GC_PUSH_VALUE(context->heap, value);
}

ES_FunctionCall::~ES_FunctionCall()
{
#ifdef ES_NATIVE_SUPPORT
    if (bytecode && native)
    {
        unsigned frame_size = MAX(2 + argc, bytecode->data->register_frame_size);
        context->FreeRegisters(frame_size);
        context->frame_stack.Pop(context);
    }
#endif // ES_NATIVE_SUPPORT

    if (builtin)
        context->FreeRegisters(2 + argc);
}

void
ES_FunctionCall::Initialize()
{
    context->code = context->Code();
    context->reg = context->Registers();

    if (function_object->IsHostObject())
    {
        builtin = ES_Host_Function::Call;
        return_value = registers = context->AllocateRegisters(2 + argc);
        argv = registers + 2;
    }
    else
    {
        ES_Function *function = static_cast<ES_Function *>(function_object);

        bytecode = function->GetFunctionCode();

        if (bytecode)
        {
#ifdef ES_NATIVE_SUPPORT
            unsigned frame_size = MAX(2 + argc, bytecode->data->register_frame_size);

            if (bytecode->native_dispatcher)
            {
                native = TRUE;

                ABORT_CONTEXT_IF_MEMORY_ERROR(context->frame_stack.Push(context));

                return_value = registers = context->AllocateRegisters(frame_size);
                argv = registers + 2;
            }
            else
#endif // ES_NATIVE_SUPPORT
                return_value = &return_value_storage;
        }
        else
        {
            builtin = function->GetCall();
            return_value = registers = context->AllocateRegisters(2 + argc);
            argv = registers + 2;
        }
    }
}

void
ES_FunctionCall::Setup(ES_Code *code)
{
    context->code = code;
#ifdef ES_NATIVE_SUPPORT
    if (bytecode && !native)
#else // ES_NATIVE_SUPPORT
    if (bytecode)
#endif // ES_NATIVE_SUPPORT
    {
        registers = context->SetupFunctionCall(function_object, argc, frame_type);
        argv = registers + 2;
    }

    registers[0] = thisArg;
    registers[1].SetObject(function_object);
}

void
ES_FunctionCall::Abort()
{
#ifdef ES_NATIVE_SUPPORT
    if (bytecode && !native)
#else // ES_NATIVE_SUPPORT
    if (bytecode)
#endif // ES_NATIVE_SUPPORT
        context->AbortFunctionCall(function_object, argc);
}

BOOL
ES_FunctionCall::Execute()
{
#ifdef ES_NATIVE_SUPPORT
    if (native)
    {
        context->reg = registers;
        if (!bytecode->data->is_strict_mode && registers[0].IsNullOrUndefined())
            registers[0].SetObject(context->GetGlobalObject());
        return context->CallBytecodeToNativeTrampoline(NULL, registers, bytecode->native_code_block, bytecode->native_dispatcher, argc, FALSE);
    }
#endif // ES_NATIVE_SUPPORT

    if (bytecode)
    {
        BOOL result = context->CallFunction(registers, argc, return_value);

#ifdef ES_NATIVE_SUPPORT
        if (bytecode->native_dispatcher)
        {
            native = TRUE;
            Initialize();
        }
#endif // ES_NATIVE_SUPPORT

        return result;
    }
    else
        return builtin(context, argc, argv, return_value);
}

