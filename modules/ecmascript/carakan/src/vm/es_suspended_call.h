/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#ifndef ES_SUSPEND_H
#define ES_SUSPEND_H

class ES_Execution_Context;

class ES_SuspendedCall
{
public:
    virtual void DoCall(ES_Execution_Context *context) = 0;
};

/* Allocations */

template <class T>
class ES_Suspended_NEW : public ES_SuspendedCall
{
public:
    ES_Suspended_NEW()
    {
    }

    virtual void DoCall(ES_Execution_Context *context)
    {
        storage = OP_NEW(T, ());
    }

    T *storage;
};

template <class T, class ARG_T>
class ES_Suspended_NEW1 : public ES_SuspendedCall
{
public:
    ES_Suspended_NEW1(ARG_T arg)
        : arg(arg)
    {
    }

    virtual void DoCall(ES_Execution_Context *context)
    {
        storage = OP_NEW(T, (arg));
    }

    T *storage;
    ARG_T arg;
};

template <class T, class ARG_T1, class ARG_T2>
class ES_Suspended_NEW2 : public ES_SuspendedCall
{
public:
    ES_Suspended_NEW2(ARG_T1 arg1, ARG_T2 arg2)
        : arg1(arg1)
        , arg2(arg2)
    {
    }

    virtual void DoCall(ES_Execution_Context *context)
    {
        storage = OP_NEW(T, (arg1, arg2));
    }

    T *storage;
    ARG_T1 arg1;
    ARG_T2 arg2;
};

template <class T>
class ES_Suspended_NEWA : public ES_SuspendedCall
{
public:
    ES_Suspended_NEWA(unsigned count)
        : count(count)
    {
    }

    virtual void DoCall(ES_Execution_Context *context)
    {
        storage = OP_NEWA(T, (count));
    }

    T *storage;
    unsigned count;
};

template <class T>
class ES_Suspended_DELETE : public ES_SuspendedCall
{
public:
    ES_Suspended_DELETE(T *storage)
        : storage(storage)
    {
    }

    virtual void DoCall(ES_Execution_Context *context)
    {
        OP_DELETE(storage);
    }

    T *storage;
};

template <class T>
class ES_Suspended_DELETEA : public ES_SuspendedCall
{
public:
    ES_Suspended_DELETEA(T *storage)
        : storage(storage)
    {
    }

    virtual void DoCall(ES_Execution_Context *context);

    T *storage;
};

template <class T>
class ES_Suspended_ResizeArray : public ES_SuspendedCall
{
public:
    ES_Suspended_ResizeArray(T **storage, unsigned count)
        : storage(storage)
        , count(count)
    {
    }

    virtual void DoCall(ES_Execution_Context *context)
    {
        OP_DELETEA(*storage);
        *storage = OP_NEWA(T, (count));
    }

    T **storage;
    unsigned count;
};

#ifdef ES_OVERRUN_PROTECTION

template <class T>
class ES_Suspended_NEWA_OverrunProtected : public ES_SuspendedCall
{
public:
    ES_Suspended_NEWA_OverrunProtected(unsigned count)
        : count(count)
    {
    }

    virtual void DoCall(ES_Execution_Context *context);

    T *storage;
    unsigned count;
};

template <class T>
class ES_Suspended_DELETEA_OverrunProtected : public ES_SuspendedCall
{
public:
    ES_Suspended_DELETEA_OverrunProtected(T *storage, unsigned count)
        : storage(storage),
          count(count)
    {
    }

    virtual void DoCall(ES_Execution_Context *context);

    T *storage;
    unsigned count;
};

#endif // ES_OVERRUN_PROTECTION

class ES_SuspendedMalloc : public ES_SuspendedCall
{
public:
    ES_SuspendedMalloc(unsigned nbytes)
        : storage(NULL)
        , nbytes(nbytes)
    {
    }

    virtual void DoCall(ES_Execution_Context *context)
    {
        storage = op_malloc(nbytes);
    }

    void *storage;
    unsigned nbytes;
};

class ES_Parser;

class ES_SuspendedParseProgram : public ES_SuspendedCall
{
public:
    ES_SuspendedParseProgram(ES_Parser &parser, JString *body, ES_Value_Internal *value, bool is_strict_mode)
        : parser(parser)
        , body(body)
        , value(value)
        , is_strict_mode(is_strict_mode)
        , code(NULL)
        , status(OpStatus::OK)
        , success(FALSE)
        , message(NULL)
    {
    }

    virtual void DoCall(ES_Execution_Context *context);

    ES_Parser &parser;
    JString *body;
    ES_Value_Internal *value;
    bool is_strict_mode;

    ES_Code *code;
    OP_STATUS status;
    BOOL success;

    JString *message;
};

class ES_SuspendedParseFunction : public ES_SuspendedCall
{
public:
    ES_SuspendedParseFunction(ES_Global_Object *global_object, const uni_char *formals, unsigned formals_length, const uni_char *body, unsigned body_length)
        : global_object(global_object)
        , formals(formals)
        , body(body)
        , formals_length(formals_length)
        , body_length(body_length)
        , code(NULL)
        , status(OpStatus::OK)
        , success(FALSE)
    {
    }

    virtual void DoCall(ES_Execution_Context *context);

    ES_Global_Object *global_object;
    const uni_char *formals;
    const uni_char *body;
    unsigned formals_length;
    unsigned body_length;

    ES_FunctionCode *code;
    OP_STATUS status;
    BOOL success;
};

class ES_Formatter;

class ES_SuspendedFormatProgram : public ES_SuspendedCall
{
public:
    ES_SuspendedFormatProgram(ES_Formatter &formatter, JString *&body)
        : formatter(formatter)
        , body(body)
        , status(OpStatus::OK)
        , success(FALSE)
    {
    }

    virtual void DoCall(ES_Execution_Context *context);

    ES_Formatter &formatter;
    JString *&body;

    OP_STATUS status;
    BOOL success;
};

/* Host calls */

class ES_SuspendedHostGetName : public ES_SuspendedCall
{
public:
    ES_SuspendedHostGetName(EcmaScript_Object *base, const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
        : state(GET_FAILED)
        , base(base)
        , property_name(property_name)
        , property_code(property_code)
        , value(value)
        , origining_runtime(origining_runtime)
        , is_restarted(FALSE)
        , restart_object(NULL)
    {
    }

    virtual void DoCall(ES_Execution_Context *context);

    ES_GetState state;
    EcmaScript_Object *base;
    const uni_char* property_name;
    int property_code;
    ES_Value* value;
    ES_Runtime* origining_runtime;
    BOOL is_restarted;
    ES_Object* restart_object;
};

class ES_SuspendedHostGetIndex : public ES_SuspendedCall
{
public:
    ES_SuspendedHostGetIndex(EcmaScript_Object *base, int property_index, ES_Value* value, ES_Runtime* origining_runtime)
        : state(GET_FAILED)
        , base(base)
        , property_index(property_index)
        , value(value)
        , origining_runtime(origining_runtime)
        , is_restarted(FALSE)
        , restart_object(NULL)
    {
    }

    virtual void DoCall(ES_Execution_Context *context);

    ES_GetState state;
    EcmaScript_Object *base;
    int property_index;
    ES_Value* value;
    ES_Runtime* origining_runtime;
    BOOL is_restarted;
    ES_Object* restart_object;
};

class ES_SuspendedHostPutName : public ES_SuspendedCall
{
public:
    ES_SuspendedHostPutName(EcmaScript_Object *base, const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
        : state(PUT_FAILED)
        , base(base)
        , property_name(property_name)
        , property_code(property_code)
        , value(value)
        , origining_runtime(origining_runtime)
        , is_restarted(FALSE)
        , restart_object(NULL)
    {
    }

    virtual void DoCall(ES_Execution_Context *context);

    ES_PutState state;
    EcmaScript_Object *base;
    const uni_char* property_name;
    int property_code;
    ES_Value* value;
    ES_Runtime* origining_runtime;
    BOOL is_restarted;
    ES_Object* restart_object;
};

class ES_SuspendedHostPutIndex : public ES_SuspendedCall
{
public:
    ES_SuspendedHostPutIndex(EcmaScript_Object *base, int property_index, ES_Value* value, ES_Runtime* origining_runtime)
        : state(PUT_FAILED)
        , base(base)
        , property_index(property_index)
        , value(value)
        , origining_runtime(origining_runtime)
        , is_restarted(FALSE)
        , restart_object(NULL)
    {
    }

    virtual void DoCall(ES_Execution_Context *context);

    ES_PutState state;
    EcmaScript_Object *base;
    int property_index;
    ES_Value* value;
    ES_Runtime* origining_runtime;
    BOOL is_restarted;
    ES_Object* restart_object;
};

class ES_SuspendedHostHasPropertyName : public ES_SuspendedCall
{
public:
    ES_SuspendedHostHasPropertyName(EcmaScript_Object *base, const uni_char* property_name, int property_code, ES_Runtime* origining_runtime)
        : result(GET_FAILED)
        , is_restarted(FALSE)
        , base(base)
        , property_name(property_name)
        , property_code(property_code)
        , origining_runtime(origining_runtime)
    {
    }

    virtual void DoCall(ES_Execution_Context *context);

    ES_GetState result;
    BOOL is_restarted;
    EcmaScript_Object *base;
    const uni_char* property_name;
    int property_code;
    ES_Runtime* origining_runtime;
};

class ES_SuspendedHostHasPropertyIndex : public ES_SuspendedCall
{
public:
    ES_SuspendedHostHasPropertyIndex(EcmaScript_Object *base, unsigned property_index, ES_Runtime* origining_runtime)
        : result(GET_FAILED)
        , is_restarted(FALSE)
        , base(base)
        , property_index(property_index)
        , origining_runtime(origining_runtime)
    {
    }

    virtual void DoCall(ES_Execution_Context *context);

    ES_GetState result;
    BOOL is_restarted;
    EcmaScript_Object *base;
    unsigned property_index;
    ES_Runtime* origining_runtime;
};

class ES_SuspendedHostSecurityCheck : public ES_SuspendedCall
{
public:
    ES_SuspendedHostSecurityCheck(EcmaScript_Object *base, ES_Runtime* origining_runtime)
        : result(FALSE)
        , base(base)
        , origining_runtime(origining_runtime)
    {
    }

    virtual void DoCall(ES_Execution_Context *context);

    BOOL result;
    EcmaScript_Object *base;
    ES_Runtime* origining_runtime;
};

class ES_SuspendedHostFetchProperties : public ES_SuspendedCall
{
public:
    ES_SuspendedHostFetchProperties(EcmaScript_Object *base, ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime)
        : result(GET_SUCCESS)
        , base(base)
        , enumerator(enumerator)
        , origining_runtime(origining_runtime)
    {
    }

    virtual void DoCall(ES_Execution_Context *context);

    ES_GetState result;
    EcmaScript_Object *base;
    ES_PropertyEnumerator *enumerator;
    ES_Runtime *origining_runtime;
};

class ES_SuspendedHostGetIndexedPropertiesLength : public ES_SuspendedCall
{
public:
    ES_SuspendedHostGetIndexedPropertiesLength(EcmaScript_Object *base, ES_Runtime* origining_runtime)
        : result(GET_SUCCESS)
        , return_value(0)
        , base(base)
        , origining_runtime(origining_runtime)
    {
    }

    virtual void DoCall(ES_Execution_Context *context);

    ES_GetState result;
    unsigned return_value;
    EcmaScript_Object *base;
    ES_Runtime* origining_runtime;
};

class ES_SuspendedHostCall : public ES_SuspendedCall
{
public:
    ES_SuspendedHostCall(EcmaScript_Object *base, ES_Object* this_native_object, ES_Value* argv,
                         int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
        : restarted(FALSE)
        , conversion_restarted(FALSE)
        , result(ES_FAILED)
        , base(base)
        , this_native_object(this_native_object)
        , argv(argv)
        , argc(argc)
        , return_value(return_value)
        , origining_runtime(origining_runtime)
    {
    }

    virtual void DoCall(ES_Execution_Context *context);

    BOOL restarted;
    BOOL conversion_restarted;
    int result;
    EcmaScript_Object *base;
    ES_Object* this_native_object;
    ES_Value* argv;
    int argc;
    ES_Value* return_value;
    ES_Runtime* origining_runtime;
};

class ES_SuspendedHostConstruct : public ES_SuspendedCall
{
public:
    ES_SuspendedHostConstruct(EcmaScript_Object *base, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
        : restarted(FALSE)
        , conversion_restarted(FALSE)
        , result(ES_FAILED)
        , base(base)
        , argv(argv)
        , argc(argc)
        , return_value(return_value)
        , origining_runtime(origining_runtime)
    {
    }

    virtual void DoCall(ES_Execution_Context *context);

    BOOL restarted;
    BOOL conversion_restarted;
    int result;
    EcmaScript_Object *base;
    ES_Value* argv;
    int argc;
    ES_Value* return_value;
    ES_Runtime* origining_runtime;
};

class ES_SuspendedHostAllowPropertyOperationFor : public ES_SuspendedCall
{
public:
    ES_SuspendedHostAllowPropertyOperationFor(EcmaScript_Object *base, ES_Runtime* origining_runtime, const uni_char *property_name, EcmaScript_Object::PropertyOperation op)
        : result(FALSE)
        , base(base)
        , origining_runtime(origining_runtime)
        , property_name(property_name)
        , op(op)
    {
    }

    virtual void DoCall(ES_Execution_Context *context);

    BOOL result;
    EcmaScript_Object *base;
    ES_Runtime* origining_runtime;
    const uni_char *property_name;
    EcmaScript_Object::PropertyOperation op;
};

class ES_SuspendedHostIdentity : public ES_SuspendedCall
{
public:
    ES_SuspendedHostIdentity(EcmaScript_Object *base)
        : base(base)
        , loc(NULL)
        , result(OpStatus::OK)
    {
    }

    virtual void DoCall(ES_Execution_Context *context);

    EcmaScript_Object *base;
    ES_Object *loc;
    OP_STATUS result;
};

class ES_SuspendedHostDeleteName : public ES_SuspendedCall
{
public:
    ES_SuspendedHostDeleteName(EcmaScript_Object *base, const uni_char* property_name, ES_Runtime* origining_runtime)
        : status(DELETE_OK)
        , base(base)
        , property_name(property_name)
        , origining_runtime(origining_runtime)
    {
    }

    virtual void DoCall(ES_Execution_Context *context);

    ES_DeleteStatus status;
    EcmaScript_Object *base;
    const uni_char* property_name;
    ES_Runtime* origining_runtime;
};

class ES_SuspendedHostDeleteIndex : public ES_SuspendedCall
{
public:
    ES_SuspendedHostDeleteIndex(EcmaScript_Object *base, int property_index, ES_Runtime* origining_runtime)
        : status(DELETE_OK)
        , base(base)
        , property_index(property_index)
        , origining_runtime(origining_runtime)
    {
    }

    virtual void DoCall(ES_Execution_Context *context);

    ES_DeleteStatus status;
    EcmaScript_Object *base;
    int property_index;
    ES_Runtime* origining_runtime;
};

class ES_SuspendedMergeHeapWith : public ES_SuspendedCall
{
public:
    ES_SuspendedMergeHeapWith(ES_Heap *heap1, ES_Heap *heap2)
        : heap1(heap1)
        , heap2(heap2)
        , result(OpStatus::OK)
    {
    }

    virtual void DoCall(ES_Execution_Context *context)
    {
        result = heap1->MergeWith(heap2);
    }

    ES_Heap *heap1;
    ES_Heap *heap2;
    OP_STATUS result;
};

class ES_SuspendedClearHead
    : public ES_SuspendedCall
{
public:
    ES_SuspendedClearHead(ES_Execution_Context *context, Head *head);

    Head *head;

private:
    virtual void DoCall(ES_Execution_Context *context);
};

template <class T>
class ES_SuspendedStackAutoPtr : public OpStackAutoPtr<T>
{
private:
    ES_Execution_Context *context;
public:
    ES_SuspendedStackAutoPtr(ES_Execution_Context *ctx, T *_ptr)
        : OpStackAutoPtr<T>(_ptr)
        , context(ctx)
    {
    }

    virtual ~ES_SuspendedStackAutoPtr();
};

/* Debugger */

#ifdef ECMASCRIPT_DEBUGGER
class ES_Suspended_Debugger_HandleEvent : public ES_SuspendedCall
{
public:
    ES_Suspended_Debugger_HandleEvent(ES_DebugListener::EventType event_type, unsigned script_guid, unsigned line)
        : event_type(event_type)
        , script_guid(script_guid)
        , line(line)
    {
    }

    virtual void DoCall(ES_Execution_Context *context);

    ES_DebugListener::EventAction return_value;
    ES_DebugListener::EventType event_type;
    unsigned script_guid, line;
};
#endif // ECMASCRIPT_DEBUGGER

#endif // ES_SUSPEND_H
