/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2008
 *
 * @author Daniel Spang
 */

#ifndef ES_EXECUTION_CONTEXT_H
#define ES_EXECUTION_CONTEXT_H

#include "modules/ecmascript/carakan/src/compiler/es_code.h"
#include "modules/ecmascript/carakan/src/object/es_function.h"
#include "modules/ecmascript/carakan/src/kernel/es_value.h"
#include "modules/ecmascript/oppseudothread/oppseudothread.h"
#include "modules/ecmascript/carakan/src/vm/es_context.h"
#include "modules/ecmascript/carakan/src/vm/es_frame.h"
#include "modules/ecmascript/carakan/src/vm/es_register_blocks.h"
#include "modules/ecmascript/carakan/src/vm/es_suspended_call.h"
#include "modules/ecmascript/carakan/src/util/es_bclogger.h"
#include "modules/ecmascript/carakan/src/compiler/es_native_arch.h"
#include "modules/ecmascript/carakan/src/object/es_error_object.h"

#if defined ES_DIRECT_THREADING || defined ES_NATIVE_SUPPORT
#define IH_RETURNTYPE_DECL void
#define IH_PARAM_DECL ES_CodeWord *lip
#define IH_PARAM(ip) (ip)
#define IH_RETURN return
#define IH_RETURN_REDIRECTED(x) (x); return
#define IH_SET_CONTEXT_IP(x) this->ip = (x)
#define IH_GET_CONTEXT_IP(x) (x) = this->ip
#define IH_SPECIFIER IH_RETURNTYPE_DECL ES_CALLING_CONVENTION
#define IH_FUNCTION_PTR(name) IH_RETURNTYPE_DECL (ES_CALLING_CONVENTION ES_Execution_Context::*name)(IH_PARAM_DECL)
#define IH_SPECIAL_FUNCTION_PTR(returntype, name, paramtypes) returntype (ES_CALLING_CONVENTION *name)paramtypes
#define SLOW_CASE_RETURN_VALUE BOOL
#else
#define IH_SPECIFIER ALWAYS_INLINE void
#define IH_PARAM_DECL
#define IH_PARAM(ip)
#define IH_RETURN return
#define IH_RETURN_REDIRECTED(x) x;              \
    return
#define IH_SET_CONTEXT_IP(x) this->ip = (x)
#define IH_GET_CONTEXT_IP(x)
#define SLOW_CASE_RETURN_VALUE BOOL
#endif

#define ES_MAXIMUM_FUNCTION_RECURSION 16384

#define ES_REDIRECTED_CALL_MAXIMUM_ARGUMENTS 8

//class ES_Function;

class ES_FunctionCall;

#ifdef ES_BYTECODE_DEBUGGER
class ES_BytecodeDebugger;
class ES_BytecodeDebuggerData;
#endif // ES_BYTECODE_DEBUGGER

class ES_StackGCRoot
{
public:
    ES_StackGCRoot(ES_Execution_Context *context);
    virtual ~ES_StackGCRoot();

    virtual void GCTrace() = 0;

protected:
    friend class ES_Execution_Context;

    ES_Execution_Context *context;
private:
    ES_StackGCRoot *previous;
};

class ES_Execution_State
{
public:
    ES_Execution_State()
        : reg(NULL),
          implicit_bool(0),
          time_until_check(0),
#ifdef ES_NATIVE_SUPPORT
          reg_limit(NULL),
          native_stack_frame(NULL),
          stack_ptr(NULL),
          stack_limit(NULL),
          need_adjust_register_usage(FALSE),
#endif // ES_NATIVE_SUPPORT
          ip(NULL),
          code(NULL),
          overlap(0),
          in_constructor(0),
          first_in_block(FALSE),
          variable_object(NULL),
          arguments_object(NULL),
          argc(0)
    {
    }

    ES_Value_Internal *reg; /**< Register pointer */
    unsigned implicit_bool; /**< Used in comparison operators instead of storing the result in a register. */
    unsigned time_until_check;

#ifdef ES_NATIVE_SUPPORT
    ES_Value_Internal *reg_limit; /**< Current register block limit. */

    ES_NativeStackFrame *native_stack_frame;
    /**< Pointer to current native stack frame. */
    unsigned char *stack_ptr;
    /**< CPU stack pointer saved by bytecode->native trampoline. */
    unsigned char *stack_limit;
    /**< Current CPU stack limit.  When a native code function, after allocating
         its stack frame, passes this limit, it instead calls a slow case.  This
         is not the actual beginning of the stack, it's that plus a safety
         margin (at least enough space for the slow case to do its job.) */
    BOOL need_adjust_register_usage;
    /**< TRUE if a register allocation first needs to adjust current usage based
         on the current top (native) stack frame. */
#endif // ES_NATIVE_SUPPORT

    ES_CodeWord *ip; /**< Instruction pointer */
    ES_Code *code;
    unsigned overlap;
    int in_constructor; /**< If true: this frame belongs to a call to a constructor. */
    BOOL first_in_block;
    ES_Object *variable_object;
    ES_Arguments_Object *arguments_object;
    unsigned argc;

    ES_Value_Internal *Registers()
    {
#ifdef ES_NATIVE_SUPPORT
        if (native_stack_frame)
            return ES_NativeStackFrame::GetRegisterFrame(native_stack_frame);
        else
#endif // ES_NATIVE_SUPPORT
            return reg;
    }

    ES_Code *Code()
    {
#ifdef ES_NATIVE_SUPPORT
        if (native_stack_frame)
            return ES_NativeStackFrame::GetCode(native_stack_frame);
        else
#endif // ES_NATIVE_SUPPORT
            return code;
    }

    JString *String(unsigned index)
    {
        return Code()->GetString(index);
    }
};

class ES_Execution_Context
    : public ES_Execution_State,
      public ES_Context,
      public ES_GCRoot,
      public OpPseudoThread
{
public:
    ES_Execution_Context(ESRT_Data *rt_data, ES_Global_Object *global_object, ES_Object *this_object, ES_Runtime *runtime, ES_Heap *heap);
    ~ES_Execution_Context();

    OP_STATUS Initialize();
    /**< Second phase initialization where all used data is allocated. Must be
       called after the contructor but before the execution context is used. */

    void SetExternalOutOfTime(BOOL (*ExternalOutOfTime)(void *), void *external_out_out_time_data);

    OP_STATUS SetupExecution(ES_Code *program, ES_Object **scope_chain, unsigned scope_chain_length);
    ES_Eval_Status ExecuteProgram(BOOL want_string_result);

    ES_Value_Internal *SetupFunctionCall(ES_Object *function, unsigned argc, ES_VirtualStackFrame::FrameType frame_type = ES_VirtualStackFrame::NORMAL, BOOL is_construct = FALSE);
    /**< Allocate argc+2 registers to use for calling 'function' from a builtin,
         and return a pointer to the first of those registers.  (Since the
         callee is available, the whole register frame can of course be
         allocated, but the caller of this function will only assume that argc+2
         registers are actually available.)

         @return Pointer to register frame. */

    void AbortFunctionCall(ES_Object *function, unsigned argc);
    /**< Abort a function call previous set up by SetupFunctionCall() if
         CallFunction() will not be called. */

    BOOL CallFunction(ES_Value_Internal *registers, unsigned argc, ES_Value_Internal *return_value = NULL, BOOL is_construct = FALSE);
    /**< Call a function based on what's in the registers previously returned by
         call to SetupFunctionCall() with the same 'argc' argument.  It is
         assumed that the stack looks the same when CallFunction() is called as
         it did when the corresponding SetupFunctionCall() is called, but nested
         SetupFunctionCall()/CallFunction() calls can occur in between.

         @return TRUE if the call returned normally, FALSE if an exception was
                 thrown. */

    void PushCall(ES_Object* function, unsigned argc, const ES_Value* argv);

    BOOL Evaluate(ES_Code *code, ES_Value_Internal *return_value, ES_Object *this_object);
    /**< Used by eval().  Run 'code' on the current stack frame (by allocating a
         larger register frame completely overlapping the old one.) */

    ES_Object *CreateVariableObject(ES_FunctionCode *code, ES_Value_Internal *properties);
    /**< Creates a variable object. */

    ES_Object *CreateAndPropagateVariableObject(ES_FrameStackIterator &current_frames, ES_FunctionCode *code, ES_Value_Internal *properties);
    /**< Creates a variable object and sets it on all frames down to current_frames. */

    ES_Function *NewFunction(ES_FunctionCode *&function_code, ES_CodeStatic::InnerScope *scope);
    /**< Create a new function based on function_code and the current scope
         chain (if applicable). */

    ES_Value_Internal *AllocateRegisters(unsigned count);
    /**< Allocate custom registers. Each call to AllocateRegisters
     * must be reversed with a similar call to FreeRegisters with the
     * same count. */

    void FreeRegisters(unsigned count);
    /**< Free registers previously allocated by AllocateRegisters(). */

    ES_Value_Internal *SaveScratchRegisters();
    /**< Saves all scratch registers. Used for recursive execution (e.g., ToPrimitive()). */

    void RestoreScratchRegisters(ES_Value_Internal *register_storage);
    /**< Restores scratch registers. */

    void StartUsingScratchRegisters(ES_Value_Internal *&storage)
    {
        storage = scratch_values_used ? SaveScratchRegisters() : NULL;
        scratch_values_used = TRUE;
    }

    void EndUsingScratchRegisters(ES_Value_Internal *storage)
    {
        if (storage)
            RestoreScratchRegisters(storage);
        else
            scratch_values_used = FALSE;
    }

    void ThrowValue(const ES_Value_Internal &exception_value, ES_CodeWord *ip = NULL);
    /**< Throw an ES_Value_Internal. */

    void ThrowValue(const ES_Value &exception_value);
    /**< Import and throw an ES_Value. */

    enum ErrorType { TYPE_ERROR, REFERENCE_ERROR };
    enum ExpectedType { EXPECTED_OBJECT, EXPECTED_FUNCTION, EXPECTED_CONSTRUCTOR, EXPECTED_LVALUE };

    void ThrowErrorBase(ES_CodeWord *ip, ErrorType error_type, ExpectedType expected_type);

    void ThrowTypeError(const char *msg, ES_CodeWord *ip = NULL);
    /**< Create and throw a TypeError exception. */

    void ThrowTypeError(const char *msg, const uni_char *name, unsigned length, ES_CodeWord *ip = NULL);
    /**< Create and throw a TypeError exception. */

    void ThrowRangeError(const char *msg, ES_CodeWord *ip = NULL);
    /**< Create and throw a RangeError exception. */

    void ThrowReferenceError(const char *msg, const uni_char *name = NULL, unsigned length = ~0u, ES_CodeWord *ip = NULL);
    /**< Create and throw a ReferenceError exception. */

    void ThrowSyntaxError(const char *msg, ES_CodeWord *ip = NULL);
    /**< Create and throw a SyntaxError exception. */

    void ThrowSyntaxError(JString *msg, ES_CodeWord *ip = NULL);
    /**< Create and throw a SyntaxError exception. */

    void ThrowURIError(const char *msg, ES_CodeWord *ip = NULL);
    /**< Create and throw a URIError exception. */

    void ThrowEvalError(const char *msg, ES_CodeWord *ip = NULL);
    /**< Create and throw a EvalError exception. */

    void ThrowInternalError(const char *msg, ES_CodeWord *ip = NULL);
    /**< Create and throw an internal error exception. */

    BOOL UncaughtException() { return uncaught_exception; }
    /** @return TRUE if the reason we aborted the execution was that
     * an exception was not caught */

    ES_Value_Internal GetCurrentException() { return current_exception; }

    void SetNoExceptionCookieOnCurrentException() { current_exception.SetNoExceptionCookie(); }

    ES_Function *GetCurrentFunction()
    {
        if (ES_Code *code = Code())
            if (code->type == ES_Code::TYPE_FUNCTION)
            {
                OP_ASSERT(Registers()[1].GetObject(this)->IsFunctionObject());
                return static_cast<ES_Function *>(Registers()[1].GetObject(this));
            }

        return NULL;
    }

#ifdef ES_BYTECODE_DEBUGGER
    void EnableBytecodeDebugger() { bytecode_debugger_enabled = TRUE; }
#endif // ES_BYTECODE_DEBUGGER

#ifdef ES_BYTECODE_LOGGER
    ES_BytecodeLogger* GetLogger() { return &bytecode_logger; }
#endif // ES_BYTECODE_LOGGER

    BOOL IsDebugged() { return is_debugged; }

    void SetIsDebugged(BOOL debugged) { is_debugged = debugged; }

    ES_Code *GetCallingCode(BOOL skip_plain_eval = FALSE);
    /**< Used from builtins: Returns the code that called the builtin. */

    ES_Value_Internal *GetCallingRegisterFrame() { return Registers(); }
    /**< Used from builtins: Returns the register frame of the code that called
         the builtin. */

    void SuspendExecution();
    /**< Mark the context as suspending.  This method is normally called from
         client code while ES_Execution_Context::Execute is active, and will
         cause the context to yield execution when the client code returns.
         @param blocking (in) If TRUE, then ES_BLOCKED is returned from
         Execute(), otherwise  ES_SUSPENDED is returned.  */

    ES_Value_Internal& ReturnValue();

    void MigrateTo(ES_Global_Object* global_object, ES_Runtime *runtime);
    /**< Migrate the context to another runtime. */

    ES_Execution_Context *GetExecutionContext() { return this; }

    BOOL GetIsExternal() { return Code() && Code()->data->is_external; }
    BOOL GetIsInIdentifierExpression() { return in_identifier_expression; }

    OP_STATUS GetCallerInformation(ES_Runtime::CallerInformation &call_info);
    OP_STATUS GetErrorStackCallerInformation(ES_Object* error_object, unsigned stack_position, BOOL resolve, ES_Runtime::CallerInformation& call_info);

    static void DoSuspendedCall(OpPseudoThread *thread);

    void SuspendedCall(ES_SuspendedCall *call);

    void ReservedCall(ES_SuspendedCall *call, unsigned amount = 0);

    virtual BOOL IsUsingStandardStack() { return is_using_standard_stack; }
    /**< Returns TRUE if the context is running on the standard stack (compared
       to a separate (limited) stack). Used to determine if we should suspend
       before calling functions that we don't know the stack usage for.  */

#ifdef ES_NATIVE_SUPPORT
    BOOL CallBytecodeToNativeTrampoline(ES_Code *calling_code, ES_Value_Internal *register_frame, ES_NativeCodeBlock *ncblock, void *native_dispatcher, unsigned arguments_count, BOOL reconstruct_native_stack = FALSE, BOOL prologue_entry_point = TRUE);
    BOOL DoCallBytecodeToNativeTrampoline(ES_Code *calling_code, ES_Value_Internal *register_frame, ES_NativeCodeBlock *ncblock, void *native_dispatcher, unsigned arguments_count, BOOL reconstruct_native_stack, BOOL prologue_entry_point);

    static void *ES_CALLING_CONVENTION ExecuteBytecode(ES_Execution_Context *context, unsigned start_cw_index, unsigned end_cw_index) REALIGN_STACK;
    /**< Executes bytecode instructions starting with the instruction starting
         at 'start_cw_index' and ending without executing the instruction
         starting at 'end_cw_index'. */

    static void *ES_CALLING_CONVENTION EvalFromMachineCode(ES_Execution_Context *context, unsigned cw_index) REALIGN_STACK;

    static void *ES_CALLING_CONVENTION CallFromMachineCode(ES_Execution_Context *context, unsigned cw_index) REALIGN_STACK;
    /**< Call function from machine code.  If called funciton is also machine
         code, return address to call, otherwise perform call directly (before
         returning) and return NULL. */

    static void *ES_CALLING_CONVENTION ConstructFromMachineCode(ES_Execution_Context *context, unsigned cw_index) REALIGN_STACK;
    /**< Construct object from machine code.  If called funciton is also machine
         code, return address to call, otherwise perform call directly (before
         returning) and return NULL. */

    static ES_Object *ES_CALLING_CONVENTION AllocateObjectFromMachineCodeSimple(ES_Execution_Context *context, ES_Class *klass) REALIGN_STACK;
    static ES_Object *ES_CALLING_CONVENTION AllocateObjectFromMachineCodeComplex(ES_Execution_Context *context, ES_Object *constructor) REALIGN_STACK;
    /**< Allocate object at the beginning of call to constructor's [[Construct]]. */

    static void *ES_CALLING_CONVENTION GetNamedImmediate(ES_Execution_Context *context, unsigned cw_index) REALIGN_STACK;
    /**< Combined slow-case for ESI_GETN_IMM and its cached variants.  Can
         return an updated native dispatcher. */

    static void *ES_CALLING_CONVENTION PutNamedImmediate(ES_Execution_Context *context, unsigned cw_index) REALIGN_STACK;
    /**< Combined slow-case for ESI_INIT_PROPERTY, ESI_PUTN_IMM and its cached variants.  Can
         return an updated native dispatcher. */

    static void *ES_CALLING_CONVENTION GetGlobalImmediate(ES_Execution_Context *context, unsigned cw_index) REALIGN_STACK;
    /**< Slow-case for ESI_GET_GLOBAL_IMM, only used in special cases such as
         when the inlining of a global function turns out to be invalid. */

    static void *ES_CALLING_CONVENTION GetGlobal(ES_Execution_Context *context, unsigned cw_index) REALIGN_STACK;
    static void *ES_CALLING_CONVENTION PutGlobal(ES_Execution_Context *context, unsigned cw_index) REALIGN_STACK;

    static void ES_CALLING_CONVENTION ReadLoopIO(ES_Execution_Context *context, ES_Code *code, ES_Value_Internal *reg) REALIGN_STACK;
    static void ES_CALLING_CONVENTION WriteLoopIO(ES_Execution_Context *context, ES_Code *code, ES_Value_Internal *reg) REALIGN_STACK;

    static void DoCallNativeDispatcher(OpPseudoThread *thread);

    static void ES_CALLING_CONVENTION StackOrRegisterLimitExceeded(ES_Execution_Context *context, BOOL is_constructor) REALIGN_STACK;

    void AllocateProfileData();
    void AllocateProfileData(ES_CodeStatic *data);

    static void *ES_CALLING_CONVENTION UpdateNativeDispatcher(ES_Execution_Context *context, unsigned cw_index) REALIGN_STACK;
    static void *ES_CALLING_CONVENTION ForceUpdateNativeDispatcher(ES_Execution_Context *context, unsigned cw_index) REALIGN_STACK;

    static void *ES_CALLING_CONVENTION LightWeightDispatcherFailure(ES_Execution_Context *context) REALIGN_STACK;
    static void *ES_CDECL_CALLING_CONVENTION ReconstructNativeStack(ES_Execution_Context *context, void **stack_pointer) REALIGN_STACK;

    static void ES_CALLING_CONVENTION ThrowFromMachineCode(ES_Execution_Context *context) REALIGN_STACK;

#ifdef _DEBUG
    void ValidateNativeStack();
#endif // _DEBUG

    void *DoUpdateNativeDispatcher(unsigned cw_index);
    void *ReallyDoUpdateNativeDispatcherL(unsigned cw_index);

#ifdef USE_CUSTOM_DOUBLE_OPS
    double (*AddDoubles)(double lhs, double rhs);
    double (*SubtractDoubles)(double lhs, double rhs);
    double (*MultiplyDoubles)(double lhs, double rhs);
    double (*DivideDoubles)(double lhs, double rhs);
    double (*Sin)(double arg);
    double (*Cos)(double arg);
    double (*Tan)(double arg);
#endif // USE_CUSTOM_DOUBLE_OPS

#ifdef ES_NATIVE_PROFILING
    static void ES_CALLING_CONVENTION EnteringFunction(ES_FunctionCode *code) REALIGN_STACK;
    static void ES_CALLING_CONVENTION LeavingFunction(ES_FunctionCode *code) REALIGN_STACK;
#endif // ES_NATIVE_PROFILING
#endif // ES_NATIVE_SUPPORT

    BOOL EqSlow(ES_Value_Internal *src1, ES_Value_Internal *src2);
    static int ES_CALLING_CONVENTION EqStrict(ES_Execution_Context *context, ES_Value_Internal *src1, ES_Value_Internal *src2) REALIGN_STACK;
#ifdef ES_NATIVE_SUPPORT
    static int ES_CALLING_CONVENTION EqFromMachineCode(ES_Execution_Context *context, ES_Value_Internal *src1, ES_Value_Internal *src2, unsigned cw_index) REALIGN_STACK;
    static void *ES_CALLING_CONVENTION UpdateComparison(ES_Execution_Context *context, unsigned cw_index) REALIGN_STACK;
#endif // ES_NATIVE_SUPPORT

    static void ES_CALLING_CONVENTION CreateArgumentsObject(ES_Execution_Context *context, ES_Object *function, ES_Value_Internal *registers, unsigned argc) REALIGN_STACK;
    static void ES_CALLING_CONVENTION DetachVariableObject(ES_Execution_Context *context, ES_Object *variables) REALIGN_STACK;
    static void ES_CALLING_CONVENTION PopVariableObject(ES_Execution_Context *context, ES_Object *variables) REALIGN_STACK;
    static void ES_CALLING_CONVENTION DetachArgumentsObject(ES_Execution_Context *context, ES_Arguments_Object *arguments) REALIGN_STACK;

    BOOL GenerateStackTraceL(JString *&string, ES_Execution_Context *error_context, ES_Error *error, ES_Error::StackTraceFormat format, unsigned prefix_linebreaks);
    BOOL GenerateStackTraceL(TempBuffer &buffer, ES_Execution_Context *error_context, ES_Error *error, ES_Error::StackTraceFormat format = ES_Error::FORMAT_READABLE, unsigned prefix_linebreaks = 1);
    BOOL GetStackLocationAtIndex(ES_Error *error, unsigned index, JString *&url, ES_SourcePosition &position);
    unsigned GetCallLine(ES_Code *code, ES_CodeWord *codeword);

#ifdef ECMASCRIPT_DEBUGGER
    unsigned int FindBacktraceLine(ES_Code *code, ES_CodeWord *codeword, BOOL top_frame);
    unsigned int BacktraceForDebugger(unsigned int maximum);
    void SignalToDebuggerInternal(ES_DebugListener::EventType type, BOOL source_loc, ES_CodeWord *ip);
    void SignalToDebugger(ES_DebugListener::EventType type, ES_CodeWord *ip) { if (IsDebugged()) SignalToDebuggerInternal(type, TRUE, ip); }
    void SignalToDebugger(ES_DebugListener::EventType type) { if (IsDebugged()) SignalToDebuggerInternal(type, FALSE, NULL); }

    class CallDetails
    {
    public:
        CallDetails()
            : callee(NULL)
            , argc(0)
            , argv_allocated(0)
            , argv(NULL)
        {
        }

        ~CallDetails()
        {
            OP_DELETEA(argv);
        }

        ES_Value this_value;
        ES_Object *callee;
        int argc, argv_allocated;
        ES_Value *argv;
    };
    CallDetails call_details;
    /**< Details of the last called function. */

    void SetCallDetails(const ES_Value_Internal &this_value, ES_Object *callee, int argc = 0, ES_Value_Internal *argv = 0);
    void GetCallDetails(ES_Value &this_value, ES_Object *&callee, int &argc, ES_Value *&argv);
    ES_Object *GetCallee() { return call_details.callee; }
#endif // ECMASCRIPT_DEBUGGER

    ES_Global_Object *&GetGlobalObject()
    {
        if (ES_Code *code = Code())
            return code->global_object;
        else
            return GetGlobalObjectSlow();
    }
    /**< Returns the global object in scope for the currently executing code. */

    ES_Global_Object *GetContextGlobalObject() { return context_global_object; }
    /**< Returns the global object that this context was constructed with.  Note
         that this global object should never be used in a way that affects the
         execution of code; the one returned by GetGlobalObject() should be used
         in such situations. */

    void RequestTimeoutCheck() { time_until_check = 1; }
    void CheckOutOfTime();
#ifdef ES_NATIVE_SUPPORT
    static void ES_CALLING_CONVENTION CheckOutOfTimeFromMachineCode(ES_Execution_Context *context) REALIGN_STACK;
#endif // ES_NATIVE_SUPPORT

    JString *GetNumberToStringCache(double number) { return number == numbertostring_cache_number ? numbertostring_cache_string : NULL; }
    void SetNumberToStringCache(double number, JString *string) { numbertostring_cache_number = number; numbertostring_cache_string = string; }

    GetResult GetToString(ES_Object *object, ES_Value_Internal &value) { return cached_toString.GetL(this, object, value); }
    GetResult GetValueOf(ES_Object *object, ES_Value_Internal &value) { return cached_valueOf.GetL(this, object, value); }

    BOOL IsEnumeratedPropertyName(JString *name, unsigned &class_id, unsigned &cached_index, unsigned &cached_type)
    {
        if (name == enumerated_string)
        {
            class_id = enumerated_class_id;
            cached_index = enumerated_cached_offset;
            cached_type = enumerated_cached_type;
            return TRUE;
        }
        else
            return FALSE;
    }

    void CachePropertyCache(ES_Code::PropertyCache *property_cache);
    ES_Code::PropertyCache *AllocatePropertyCache();

    unsigned GetExternalScopeChainLength() { return external_scope_chain_length; }
    ES_Object **GetExternalScopeChain() { return external_scope_chain; }

    void CaptureStackTrace(ES_Error *error);

protected:
    friend class ES_Host_Object;
    friend class ES_Host_Function;
    friend class ES_FrameStackIterator;

    inline void YieldImpl();

    void MaybeYield();
    /**< Yields execution if someone has told us to suspend. Otherwise nop. */

    void YieldExecution();
    /**< Yields execution and sets suspended status in the context.
       The context will typically be used again in a while. */

    void Block();
    /**< Yields execution and sets blocked status in the context.
       The context will typically be used again in a while. */

    void SetNormal() { eval_status = ES_NORMAL; }
    void SetSuspended() { eval_status = ES_SUSPENDED; }
    void SetBlocked() { eval_status = ES_BLOCKED; }
    void SetThrewException() { OP_ASSERT(eval_status == ES_NORMAL); eval_status = ES_THREW_EXCEPTION; }

    void GetHostArguments(ES_Value *&values, ES_ValueString *&value_strings, unsigned argc)
    {
        if (argc > host_arguments_length)
            AllocateHostArguments(argc);

        values = host_arguments;
        value_strings = host_argument_strings;

        host_arguments_used = argc;
    }

    void ReleaseHostArguments() { host_arguments_used = 0; }

    void AllocateHostArguments(unsigned argc);
    void FreeHostArguments();

private:
    friend class ES_Native;
    friend class ES_FunctionCall;

    virtual void GCTrace();
    virtual void NewHeap(ES_Heap *new_heap);

    static void DoExecute(OpPseudoThread *thread);
    static void DoCallBuiltIn(OpPseudoThread *thread);

    void Reset();

    ES_Value_Internal Execute();

    ES_Global_Object *&GetGlobalObjectSlow();

    friend void ES_SetupFunctionHandlers();

    IH_SPECIFIER ExecuteSingleInstruction(IH_PARAM_DECL);

    IH_SPECIFIER IH_LOAD_STRING(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_LOAD_DOUBLE(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_LOAD_INT32(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_LOAD_NULL(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_LOAD_UNDEFINED(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_LOAD_TRUE(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_LOAD_FALSE(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_LOAD_GLOBAL_OBJECT(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_COPY(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_COPYN(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_TYPEOF(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_TONUMBER(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_TOOBJECT(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_TOPRIMITIVE(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_TOPRIMITIVE1(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_IS_NULL_OR_UNDEFINED(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_IS_NOT_NULL_OR_UNDEFINED(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_ADD(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_ADD_IMM(IH_PARAM_DECL) REALIGN_STACK;
#ifdef ES_COMBINED_ADD_SUPPORT
    IH_SPECIFIER IH_ADDN(IH_PARAM_DECL) REALIGN_STACK;
#endif // ES_COMBINED_ADD_SUPPORT
    IH_SPECIFIER IH_FORMAT_STRING(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_SUB(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_SUB_IMM(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_MUL(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_MUL_IMM(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_DIV(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_DIV_IMM(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_REM(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_REM_IMM(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_LSHIFT(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_LSHIFT_IMM(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_RSHIFT_SIGNED(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_RSHIFT_SIGNED_IMM(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_RSHIFT_UNSIGNED(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_RSHIFT_UNSIGNED_IMM(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_NEG(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_COMPL(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_INC(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_DEC(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_BITAND(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_BITAND_IMM(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_BITOR(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_BITOR_IMM(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_BITXOR(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_BITXOR_IMM(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_LOGAND(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_LOGOR(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_NOT(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_EQ(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_EQ_IMM(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_NEQ(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_NEQ_IMM(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_EQ_STRICT(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_EQ_STRICT_IMM(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_NEQ_STRICT(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_NEQ_STRICT_IMM(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_LT(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_LT_IMM(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_LTE(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_LTE_IMM(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_GT(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_GT_IMM(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_GTE(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_GTE_IMM(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_CONDITION(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_JUMP(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_JUMP_INDIRECT(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_JUMP_IF_TRUE(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_JUMP_IF_FALSE(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_START_LOOP(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_STORE_BOOLEAN(IH_PARAM_DECL) REALIGN_STACK;
    BOOL IH_GETN_IMM_UNCACHED(ES_CodeWord *lip, ES_CodeWord::Index dst, ES_CodeWord::Index obj, ES_Code::PropertyCache *reuse_cache);
    IH_SPECIFIER IH_GETN_IMM(IH_PARAM_DECL) REALIGN_STACK;
    BOOL IH_PUTN_IMM_UNCACHED(ES_CodeWord *lip, ES_CodeWord::Index obj, ES_CodeWord::Index value, ES_Code::PropertyCache *reuse_cache);
    IH_SPECIFIER IH_PUTN_IMM(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_GET_LENGTH(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_PUT_LENGTH(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_GETI_IMM(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_PUTI_IMM(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_GET(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_PUT(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_DEFINE_GETTER(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_DEFINE_SETTER(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_GET_SCOPE(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_GET_SCOPE_REF(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_PUT_SCOPE(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_DELETE_SCOPE(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_GET_GLOBAL(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_PUT_GLOBAL(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_GET_LEXICAL(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_PUT_LEXICAL(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_DELETEN_IMM(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_DELETEI_IMM(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_DELETE(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_DECLARE_GLOBAL(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_HASPROPERTY(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_INSTANCEOF(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_ENUMERATE(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_NEXT_PROPERTY(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_EVAL(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_CALL(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_REDIRECTED_CALL(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_APPLY(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_CONSTRUCT(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_RETURN_VALUE(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_RETURN_NO_VALUE(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_NEW_OBJECT(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_CONSTRUCT_OBJECT(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_NEW_ARRAY(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_CONSTRUCT_ARRAY(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_NEW_FUNCTION(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_NEW_REGEXP(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_TABLE_SWITCH(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_CATCH(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_CATCH_SCOPE(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_THROW(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_THROW_BUILTIN(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_RETHROW(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_EXIT_FINALLY(IH_PARAM_DECL) REALIGN_STACK;
    IH_SPECIFIER IH_DEBUGGER_STOP(IH_PARAM_DECL) REALIGN_STACK;

    void PerformFunctionCall(ES_CodeWord *lip, const ES_Value_Internal &function_val, unsigned rel_frame_start, unsigned argc, BOOL new_in_constructor);

    SLOW_CASE_RETURN_VALUE AddSlow(ES_Value_Internal *dst, ES_Value_Internal *src1, ES_Value_Internal *src2, ES_Value_Internal *scratch_value_storage);
    SLOW_CASE_RETURN_VALUE SubSlow(ES_Value_Internal *dst, ES_Value_Internal *src1, ES_Value_Internal *src2, ES_Value_Internal *scratch_value_storage);
    SLOW_CASE_RETURN_VALUE MulSlow(ES_Value_Internal *dst, ES_Value_Internal *src1, ES_Value_Internal *src2, ES_Value_Internal *scratch_value_storage);
    inline BOOL Eq(ES_Value_Internal *src1, ES_Value_Internal *src2);

    void RelationalNumberComparision(unsigned &result, ES_Value_Internal src1, ES_Value_Internal src2, BOOL inverted);
    BOOL RelationalComparision(unsigned &result, ES_Value_Internal *src1, ES_Value_Internal *src2, BOOL left_first, BOOL inverted);
    /**<
     * @param inverted Caller atempts to invert the result so we need
     *        to compensate for the NaN case (NaN < anything == false,
     *        but since caller will invert we need to set it to true).
     *
     * @return FALSE if the caller needs to throw an exception.
     */
    void HandleThrow();

    void HandleFinally(unsigned target);
    /**< Look up the nearest enclosing finally handler that would be crossed by
         jumping to target and set the ip to this and set the implicit boolean
         register to TRUE, otherwise do nothing. */

    void SetupCallState(ES_CodeWord::Index argc, ES_Function *function);

    BOOL ShouldYield() { return eval_status == ES_SUSPENDED || eval_status == ES_BLOCKED; }

    void SetIsInIdentifierExpression(BOOL b) { in_identifier_expression = b; }

    ES_Value_Internal src_val1, src_val2, src_val3;
    /**< Scratch values used instead of stack allocated values in the
       instruction handlers. */
    BOOL scratch_values_used;

    unsigned enumerated_class_id, enumerated_cached_offset, enumerated_cached_type, enumerated_index, enumerated_limit;
    JString *enumerated_string;

    ES_FrameStack frame_stack;
    ES_RegisterBlocks register_blocks; /**< Underlying structure from where we allocate registers */

    enum
    {
        EXIT_TO_BUILTIN_PROGRAM = 0,
        EXIT_TO_EVAL_PROGRAM,
        EXIT_PROGRAM
    };

    ES_CodeWord exit_programs[EXIT_PROGRAM + 1]; /**< Array of last codeword executed. A new frame is pushed on the frame stack at beginning of execution with this instruction. */

    BOOL uncaught_exception;
    ES_Value_Internal current_exception;
#ifdef ECMASCRIPT_DEBUGGER
    BOOL current_exception_signalled;
    /**< Whether last exception was signalled to the debugger. Prevents same
         exception to be signalled multiple times. */
#endif // ECMASCRIPT_DEBUGGER

    ES_Global_Object *context_global_object;
    ES_Object *this_object;

    ES_Object **external_scope_chain;
    unsigned external_scope_chain_length;

    double numbertostring_cache_number;
    JString *numbertostring_cache_string;

    ES_Object::CachedPropertyAccess cached_toString;
    ES_Object::CachedPropertyAccess cached_valueOf;

    ES_Code::PropertyCache *property_caches_cache;

#ifdef ES_NATIVE_SUPPORT
    BOOL (*BytecodeToNativeTrampoline)(void **, unsigned);
    void (*ThrowFromMachineCodeTrampoline)(void **);

    static void DoCallFromMachineCode(OpPseudoThread *thread);
    static void DoConstructFromMachineCode(OpPseudoThread *thread);

    BOOL PerformCallFromMachineCode(void *&new_dispatcher, unsigned cw_index);

    void *native_dispatcher;
    ES_NativeCodeBlock *native_code_block;
    ES_Code *native_dispatcher_code;
    ES_Value_Internal *native_dispatcher_reg;
    unsigned native_dispatcher_argc;
    BOOL native_dispatcher_success;
    unsigned stored_cw_index;

    BOOL slow_path_flag;
#endif // ES_NATIVE_SUPPORT

    ES_Value_Internal return_value;

    ES_SuspendedCall *suspended_call; /**< Container containing information about a suspended call. */

    BOOL started;
    BOOL is_aborting; // flag: TRUE if context must be aborted.
#ifdef _DEBUG
    BOOL yielded;
#endif // _DEBUG
    BOOL generate_result;
    BOOL convert_result_to_string;

    BOOL (*ExternalOutOfTime)(void *);
    void *external_out_out_time_data;
    unsigned time_quota;

    BOOL in_identifier_expression;

    friend class ES_StackGCRoot;
    ES_StackGCRoot *stack_root_objs;

    ES_Value *host_arguments;
    ES_ValueString *host_argument_strings;
    unsigned host_arguments_length, host_arguments_used;

    ES_StackTraceElement stacktrace[ES_StackTraceElement::STACK_TRACE_ELEMENTS];
    unsigned stacktrace_length;

    void CaptureStackTrace(ES_CodeWord *ip, ES_Error *error = NULL);

#ifdef ES_BASE_REFERENCE_INFORMATION
    ES_Object *base_reference_object;
    JString *base_reference_name;
    unsigned base_reference_register;
#endif // ES_BASE_REFERENCE_INFORMATION

    TempBuffer stacktrace_buffer;

#ifdef ECMASCRIPT_DEBUGGER
    ES_DebugListener::EventType GetDebuggerSignal() const { return dbg_signal; }
    /**< Get the current signal to the debugger.

         The current debugger signal will be returned if there currently is
         a call to SignalDebugger(). If there is no such call,
         ES_DebugListener::ESEV_NONE will be returned.

         @return The current signal to the debugger. */

    ES_DebugListener::EventType dbg_signal;
#endif // ECMASCRIPT_DEBUGGER

#ifdef ES_BYTECODE_DEBUGGER
    friend class ES_BytecodeDebugger;
    BOOL bytecode_debugger_enabled;
    ES_BytecodeDebuggerData *bytecode_debugger_data;
#endif // ES_BYTECODE_DEBUGGER

#ifdef ES_BYTECODE_LOGGER
    ES_BytecodeLogger bytecode_logger;
#endif // ES_BYTECODE_LOGGER

    BOOL is_debugged;       // flag: TRUE iff this context has been reported to the debug backend
    BOOL is_top_level_call; // flag: TRUE iff PushCall() rather than PushProgram()

    unsigned stack_per_recursion;
};

/** Class to automatically free registers on function exit */
class AutoRegisters
{
private:
    ES_Execution_Context *context;
    ES_Value_Internal *registers;
    unsigned count;

public:
    AutoRegisters(ES_Execution_Context *context, unsigned count)
        : context(context),
          registers(context ? context->AllocateRegisters(count) : NULL),
          count(count)
    {
    }

    ~AutoRegisters()
    {
        if (context)
            context->FreeRegisters(count);
    }

    ES_Value_Internal &operator [](unsigned index)
    {
        OP_ASSERT(context && index < count);
        return registers[index];
    }

    ES_Value_Internal *GetRegisters()
    {
        return registers;
    }
};

#ifdef ES_MOVABLE_OBJECTS
# define GC_STACK_ANCHOR(context, object)        \
    ES_StackPointerAnchor object##_stack_anchor(static_cast<ES_Execution_Context*>(context), CAST_TO_BOXED_REF(object));
#else // ES_MOVABLE_OBJECTS
# define GC_STACK_ANCHOR(context, object)
#endif // ES_MOVABLE_OBJECTS

class ES_StackPointerAnchor
    : public ES_StackGCRoot
{
public:
    ES_StackPointerAnchor(ES_Execution_Context *context, ES_Boxed *ptr) : ES_StackGCRoot(context), ptr(ptr) {}

    virtual void GCTrace();

private:
    ES_Boxed *ptr;
};

class ES_StackValueAnchor
    : public ES_StackGCRoot
{
public:
    ES_StackValueAnchor(ES_Execution_Context *context, ES_Value_Internal &value) : ES_StackGCRoot(context), value(value) {}

    virtual void GCTrace();

private:
    ES_Value_Internal &value;
};

/*virtual*/ void
ES_Execution_Context::YieldImpl()
{
    if (IsUsingStandardStack())
    {
        OP_ASSERT(eval_status == ES_ERROR_NO_MEMORY);
        LEAVE(OpStatus::ERR_NO_MEMORY);
    }
    else
    {
#ifdef _DEBUG
        yielded = TRUE;
#endif // _DEBUG

        Yield();
        OP_ASSERT(eval_status == ES_BLOCKED || eval_status == ES_SUSPENDED);
        eval_status = ES_NORMAL;

#ifdef ES_NATIVE_SUPPORT
        stack_limit = StackLimit() + stack_per_recursion;
#endif // ES_NATIVE_SUPPORT
    }
}

class ES_FunctionCall
{
public:
    ES_FunctionCall(ES_Execution_Context *context, const ES_Value_Internal &thisArg, ES_Object *function_object, unsigned argc)
        : context(context)
        , thisArg(thisArg)
        , function_object(function_object)
        , argc(argc)
        , frame_type(ES_VirtualStackFrame::NORMAL)
        , bytecode(NULL)
#ifdef ES_NATIVE_SUPPORT
        , native(FALSE)
#endif // ES_NATIVE_SUPPORT
        , builtin(NULL)
    {
    }

    ~ES_FunctionCall();

    void SetFrameType(ES_VirtualStackFrame::FrameType frame_type0) { frame_type = frame_type0; }

    void Initialize();
    void Setup(ES_Code *code);
    void Abort();
    BOOL Execute();

    ES_Value_Internal *return_value, *argv;

private:
    ES_Execution_Context *context;
    ES_Value_Internal thisArg;
    ES_Object *function_object;
    unsigned argc;
    ES_VirtualStackFrame::FrameType frame_type;

    ES_FunctionCode *bytecode;
    ES_Value_Internal *registers, return_value_storage;
#ifdef ES_NATIVE_SUPPORT
    BOOL native;
#endif // ES_NATIVE_SUPPORT
    ES_Function::BuiltIn *builtin;
};

inline
ES_StackGCRoot::ES_StackGCRoot(ES_Execution_Context *context)
    : context(context),
      previous(NULL)
{
    if (context)
    {
        previous = context->stack_root_objs;
        context->stack_root_objs = this;
    }
}

#ifdef ES_NATIVE_SUPPORT

class ES_AnalyzeAndGenerateNativeDispatcher
    : public ES_SuspendedCall
{
public:
    ES_AnalyzeAndGenerateNativeDispatcher(ES_Execution_Context *context, ES_Function *function, ES_CodeWord *prologue_entry_point_cw = NULL)
        : function(function),
          prologue_entry_point_cw(prologue_entry_point_cw)
    {
        context->SuspendedCall(this);
    }

    virtual void DoCall(ES_Execution_Context *context);

    OP_STATUS status;
    void *entry_point;
    ES_NativeCodeBlock *native_code_block;

private:
    void DoCallL(ES_Execution_Context *context);

    ES_Function *function;
    ES_CodeWord *prologue_entry_point_cw;
};
class ES_SuspendedAnalyzer : public ES_SuspendedCall
{
public:
    ES_SuspendedAnalyzer(ES_Code *code)
        : code(code), data(NULL)
    {
    }

    virtual void DoCall(ES_Execution_Context *context);

    ES_Code *code;
    ES_CodeOptimizationData *data;
    OP_STATUS status;
};

#endif // ES_NATIVE_SUPPORT
#endif // ES_EXECUTION_CONTEXT_H
