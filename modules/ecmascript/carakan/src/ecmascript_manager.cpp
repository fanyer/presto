/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  1995 - 2011
 *
 * EcmaScript_Manager -- API for access to the ECMAScript engine
 * Lars T Hansen
 */
#include "core/pch.h"

#include "modules/ecmascript/ecmascript.h"
#include "modules/ecmascript/carakan/ecmascript_parameters.h"
#include "modules/ecmascript/carakan/src/ecmascript_manager_impl.h"
#include "modules/ecmascript/carakan/src/es_program_cache.h"
#include "modules/ecmascript/carakan/src/ecma_pi.h"
#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/util/adt/bytebuffer.h"
#include "modules/util/tempbuf.h"
#include "modules/pi/OpSystemInfo.h"
#ifndef _STANDALONE
#include "modules/util/opfile/opfile.h"
#include "modules/url/url_man.h"
#include "modules/dom/domenvironment.h"
#endif // _STANDALONE
#include "modules/hardcore/cpuusagetracker/cpuusagetrackeractivator.h"

#ifndef ARRAY_SIZE
#  define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#endif

EcmaScript_Manager::RuntimeOrContext::RuntimeOrContext(ES_Runtime *runtime)
    : runtime(runtime),
      context(NULL)
{
    OP_ASSERT(runtime);
}
EcmaScript_Manager::RuntimeOrContext::RuntimeOrContext(ES_Context *context)
    : runtime(NULL),
      context(context)
{
    OP_ASSERT(context);
}

/* static */ EcmaScript_Manager*
EcmaScript_Manager::MakeL()
{
    EcmaScript_Manager* em = OP_NEW_L(EcmaScript_Manager_Impl, ());
#ifndef _STANDALONE
    LEAVE_IF_ERROR(g_main_message_handler->SetCallBack(em, MSG_ES_COLLECT, 0));
#endif // _STANDALONE
    return em;
}

EcmaScript_Manager::EcmaScript_Manager()
    : pending_collections(FALSE)
    , maintenance_gc_running(FALSE)
    , first_host_object_clone_handler(NULL)
#ifdef ECMASCRIPT_DEBUGGER
    , reformat_runtime(NULL)
    , want_reformat(FALSE)
#endif // ECMASCRIPT_DEBUGGER
    , property_name_translator(NULL)
#ifdef ES_HEAP_DEBUGGER
    , heap_id_counter(0)
#endif // ES_HEAP_DEBUGGER
{
    // If we use a DLL it is initialized on demand, not here.
}

/* virtual */
EcmaScript_Manager::~EcmaScript_Manager()
{
#ifndef _STANDALONE
    g_main_message_handler->RemoveCallBacks(this, 0);
#endif // _STANDALONE

    OP_DELETE(property_name_translator);
    property_name_translator = NULL;

    ES_HostObjectCloneHandler *handler = first_host_object_clone_handler;
    while (handler)
    {
        first_host_object_clone_handler = handler->next;
        OP_DELETE(handler);
        handler = first_host_object_clone_handler;
    }
}

EcmaScript_Manager_Impl::EcmaScript_Manager_Impl()
    : EcmaScript_Manager()
{
}

/* virtual */
EcmaScript_Manager_Impl::~EcmaScript_Manager_Impl()
{
    GarbageCollect();
    ESRT::Shutdown(g_esrt);
}

#ifndef _STANDALONE
/* virtual */
void EcmaScript_Manager_Impl::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
    TRACK_CPU_PER_TAB(g_ecma_gc_cputracker);

    OP_ASSERT(msg == MSG_ES_COLLECT);

    if (par2)
        MaintenanceGarbageCollect();
    else
        GarbageCollect(TRUE, par1 ? ~0 : 0);
}
#endif // _STANDALONE

void
EcmaScript_Manager::PurgeDestroyedHeaps()
{
    for (Link *heap = destroy_heaps.First(); heap != NULL;)
    {
        ES_Heap *to_remove = static_cast<ES_Heap *>(heap);
        heap = heap->Suc();
        ES_Heap::Destroy(to_remove);
    }
}

void
EcmaScript_Manager::GarbageCollect(BOOL unconditional/*=TRUE*/, unsigned int timeout/*=0*/)
{
#ifdef _STANDALONE
    if (timeout == 0)
    {
        for (Link *heap = destroy_heaps.First(); heap != NULL;)
        {
            ES_Heap *to_remove = static_cast<ES_Heap *>(heap);
            heap = heap->Suc();
            ES_Heap::Destroy(to_remove);
        }

        for (Link *heap = active_heaps.First(); heap != NULL; heap = heap->Suc())
            static_cast<ES_Heap *>(heap)->ForceCollect(NULL, GC_REASON_EXTERN);

        for (Link *heap = inactive_heaps.First(); heap != NULL; heap = heap->Suc())
            static_cast<ES_Heap *>(heap)->ForceCollect(NULL, GC_REASON_EXTERN);
    }
#else // _STANDALONE
    if (unconditional)
    {
        if (timeout == 0 || timeout == ~0U && pending_collections == TRUE)
        {
            PurgeDestroyedHeaps();

            for (Link *heap = active_heaps.First(); heap != NULL; heap = heap->Suc())
                static_cast<ES_Heap *>(heap)->ForceCollect(NULL, GC_REASON_EXTERN);

            for (Link *heap = inactive_heaps.First(); heap != NULL; heap = heap->Suc())
                static_cast<ES_Heap *>(heap)->ForceCollect(NULL, GC_REASON_EXTERN);

            pending_collections = FALSE;
        }
        else if (timeout != ~0U)
        {
            pending_collections = TRUE;
            ES_ImportedAPI::PostGCMessage(timeout);
        }
    }
    else if (!pending_collections)
    {
        for (Link *heap = active_heaps.First(); heap != NULL; heap = heap->Suc())
            static_cast<ES_Heap *>(heap)->MaybeCollect(NULL);

        for (Link *heap = inactive_heaps.First(); heap != NULL; heap = heap->Suc())
            static_cast<ES_Heap *>(heap)->MaybeCollect(NULL);
    }
#endif // _STANDALONE
}

void
EcmaScript_Manager::ScheduleGarbageCollect()
{
#ifndef _STANDALONE
	ES_ImportedAPI::PostGCMessage();
#endif // _STANDALONE
}

void
EcmaScript_Manager::StartMaintenanceGC()
{
#ifndef _STANDALONE
    if (!maintenance_gc_running)
    {
        maintenance_gc_running = TRUE;
        ES_ImportedAPI::PostMaintenanceGCMessage(ES_PARM_MAINTENANCE_GC_INTERVAL);
    }
#endif // _STANDALONE
}

void
EcmaScript_Manager::StopMaintenanceGC()
{
#ifndef _STANDALONE
    OP_ASSERT(maintenance_gc_running);
    maintenance_gc_running = FALSE;
#endif // _STANDALONE
}

void
EcmaScript_Manager::MaintenanceGarbageCollect()
{
#ifndef _STANDALONE
    unsigned current_time = g_op_time_info->GetRuntimeTickMS();
    Head selected_heaps;
    unsigned selected_heap_count = 0;
    BOOL continue_running = FALSE;

    OP_ASSERT(maintenance_gc_running);

    PurgeDestroyedHeaps();

    for (ES_Heap *heap = static_cast<ES_Heap *>(inactive_heaps.First()), *suc; heap != NULL; heap = suc)
    {
        suc = static_cast<ES_Heap *>(heap->Suc());

        if (heap->NeedsMaintenanceGC(current_time))
        {
            selected_heap_count++;

            ES_Heap *sheap = NULL;
            for (sheap = static_cast<ES_Heap *>(selected_heaps.First()); sheap != NULL; sheap = static_cast<ES_Heap *>(sheap->Suc()))
                if (heap->LastGC() <= sheap->LastGC())
                {
                    heap->Out();
                    heap->Precede(sheap);
                    selected_heap_count++;
                    break;
                }

            if (!sheap)
            {
                heap->Out();
                heap->Into(&selected_heaps);
            }
        }

        if (heap->ActiveSinceLastGC())
            continue_running = TRUE;
    }

    // Limit number of heaps to run gc on
    unsigned max_heaps_to_gc = selected_heap_count / ES_PARM_MAINTENANCE_GC_RATION + 1;

    if (selected_heap_count && max_heaps_to_gc != selected_heap_count)
        continue_running = TRUE;

    for (ES_Heap *heap = static_cast<ES_Heap *>(selected_heaps.First()); heap != NULL && max_heaps_to_gc-- > 0; heap = static_cast<ES_Heap *>(heap->Suc()))
        heap->ForceCollect(NULL, GC_REASON_MAINTENANCE);

    inactive_heaps.Append(&selected_heaps);

#ifdef ES_NATIVE_SUPPORT
    // Remove all machine code on inactive heaps
    for (ES_Heap *heap = static_cast<ES_Heap *>(inactive_heaps.First()); heap != NULL; heap = static_cast<ES_Heap *>(heap->Suc()))
        if (!heap->ActiveRecently(current_time))
            heap->FlushNativeDispatchers();
        else
            continue_running = TRUE;
#endif // ES_NATIVE_SUPPORT

    if (!continue_running)
        StopMaintenanceGC();

    if (maintenance_gc_running)
        ES_ImportedAPI::PostMaintenanceGCMessage(ES_PARM_MAINTENANCE_GC_INTERVAL);
#endif // _STANDALONE
}

BOOL
EcmaScript_Manager::Protect(ES_Object *obj)
{
    OP_ASSERT(!"Deprecated! Use ES_Runtime::Protect().");

    /* Workaround: this works for host objects, at least. */
    if (obj->IsHostObject())
        return static_cast<ES_Host_Object *>(obj)->GetHostObject()->GetRuntime()->Protect(obj);

    return TRUE;
}

void
EcmaScript_Manager::Unprotect(ES_Object *obj)
{
    OP_ASSERT(!"Deprecated! Use ES_Runtime::Unrotect().");

    /* Workaround: this works for host objects, at least. */
    if (obj->IsHostObject())
        return static_cast<ES_Host_Object *>(obj)->GetHostObject()->GetRuntime()->Unprotect(obj);
}

long
EcmaScript_Manager::GetCurrHeapSize()
{
    unsigned total_size = 0;

    for (ES_Heap *heap = static_cast<ES_Heap *>(active_heaps.First()); heap; heap = static_cast<ES_Heap *>(heap->Suc()))
        total_size += heap->GetBytesInHeap();

    for (ES_Heap *heap = static_cast<ES_Heap *>(inactive_heaps.First()); heap; heap = static_cast<ES_Heap *>(heap->Suc()))
        total_size += heap->GetBytesInHeap();

    return total_size;
}

BOOL
EcmaScript_Manager::IsDebugging(ES_Runtime *runtime)
{
    if (!runtime)
        return FALSE;

#ifdef ECMASCRIPT_DEBUGGER
    return GetDebugListener() && GetDebugListener()->EnableDebugging(runtime);
#else // ECMASCRIPT_DEBUGGER
    return FALSE;
#endif // ECMASCRIPT_DEBUGGER
}

#ifdef ECMASCRIPT_DEBUGGER

/* virtual */
ES_DebugListener::~ES_DebugListener()
{
    /* Nothing, just here to kill GCC warnings */
}

void
EcmaScript_Manager::SetDebugListener(ES_DebugListener *listener)
{
    if (!listener)
    {
        reformat_function.Unprotect();
        if (reformat_runtime)
        {
            reformat_runtime->Detach();
            reformat_runtime = NULL;
        }
    }

    g_esrt->debugger_listener = listener;
}

ES_DebugListener *
EcmaScript_Manager::GetDebugListener()
{
    return g_esrt->debugger_listener;
}

void
EcmaScript_Manager::IgnoreError(ES_Context *context, ES_Value value)
{
    // TODO: Implement
    /*
    ES_DebugControl ctl;

    ctl.op = ES_DebugControl::IGNORE_ERROR;
    ctl.u.ignore_error.context = context;
    ctl.u.ignore_error.value = &value;
    ES_DebuggerOp(&ctl);
    */
}

static ES_Object **
CopyScopeChain(ES_Object **dst, ES_Object **src, unsigned length)
{
    for (unsigned i = 0; i < length; i++)
    {
        *dst = src[i];
        dst++;
    }

    return dst;
}

static ES_Object **
CopyScopeChainReversed(ES_Object **dst, ES_Object **src, unsigned length)
{
    for (int i = length - 1; i >= 0; i--)
    {
        *dst = src[i];
        dst++;
    }

    return dst;
}

void
EcmaScript_Manager::GetScope(ES_Context *context, unsigned int frame_index, unsigned *length, ES_Object ***scope)
{
    unsigned frame_length = *length;
    *length = 0;
    frame_index = frame_length - frame_index - 1;
    *scope = NULL;

    ES_FrameStackIterator frames(context->GetExecutionContext());

    /* Look for "frame_index"th major frame; stopping at the one equal or closest to that count. */
    while (frames.NextMajorFrame() && frame_index > 0)
        frame_index--;

    ES_Code *code = frames.GetCode();
    ES_Object *variable_object = frames.GetVariableObject();
    unsigned scope_chain_length = 0;
    unsigned function_scope_chain_length = 0;
    ES_Object **function_scope_chain = NULL;
    ES_Object *global_object = code->global_object;


    if (code && code->type == ES_Code::TYPE_FUNCTION)
    {
        ES_Function *current_function = frames.GetFunctionObject();

        if (!variable_object)
        {
            variable_object = context->GetExecutionContext()->CreateVariableObject(current_function->GetFunctionCode(), frames.GetRegisterFrame() + 2);
            frames.SetVariableObject(variable_object);
        }

        function_scope_chain = current_function->GetScopeChain();
        function_scope_chain_length = current_function->GetScopeChainLength();
    }

    scope_chain_length = (variable_object ? 1 : 0) + function_scope_chain_length + code->scope_chain_length + context->GetExecutionContext()->GetExternalScopeChainLength();

    // + 1 for global object (never NULL).
    ++scope_chain_length;

    if (context->rt_data->debugger_scope_chain == NULL || context->rt_data->debugger_scope_chain_length < scope_chain_length)
    {
        OP_DELETEA(context->rt_data->debugger_scope_chain);
        context->rt_data->debugger_scope_chain_length = 0;
        context->rt_data->debugger_scope_chain = OP_NEWA(ES_Object *, scope_chain_length);
        if (context->rt_data->debugger_scope_chain == NULL)
            return;

        context->rt_data->debugger_scope_chain_length = scope_chain_length;
    }

    ES_Object **dst = context->rt_data->debugger_scope_chain;

    dst = CopyScopeChain(dst, &variable_object, variable_object ? 1 : 0);

    dst = CopyScopeChain(dst, function_scope_chain, function_scope_chain_length);

    // The following two copies might be wrong. Might want to flip Reversed.
    dst = CopyScopeChainReversed(dst, code->scope_chain, code->scope_chain_length);

    dst = CopyScopeChain(dst, context->GetExecutionContext()->GetExternalScopeChain(), context->GetExecutionContext()->GetExternalScopeChainLength());

    dst = CopyScopeChain(dst, &global_object, 1);

    OP_ASSERT(dst == context->rt_data->debugger_scope_chain + scope_chain_length);
    *scope = context->rt_data->debugger_scope_chain;
    *length = scope_chain_length;
}

void
EcmaScript_Manager::GetStack(ES_Context *context, unsigned int maximum, unsigned int *length, ES_DebugStackElement **stack)
{
    *length = context->GetExecutionContext()->BacktraceForDebugger(maximum);
    *stack = context->rt_data->debugger_call_stack;
}

void
EcmaScript_Manager::GetExceptionValue(ES_Context *context, ES_Value *exception_value)
{
    context->GetExecutionContext()->GetCurrentException().Export(context, *exception_value);
}

void
EcmaScript_Manager::GetReturnValue(ES_Context *context, ES_Value *return_value)
{
    context->GetExecutionContext()->ReturnValue().Export(context, *return_value);
}

ES_Object*
EcmaScript_Manager::GetCallee(ES_Context *context)
{
    return context->GetExecutionContext()->GetCallee();
}

void
EcmaScript_Manager::GetCallDetails(ES_Context *context, ES_Object **this_object, ES_Object **callee, int *argc, ES_Value **argv)
{
    ES_Value this_value;

    context->GetExecutionContext()->GetCallDetails(this_value, *callee, *argc, *argv);

    if (this_value.type == VALUE_OBJECT)
        *this_object = this_value.value.object;
    else
        *this_object = NULL;
}

void
EcmaScript_Manager::GetCallDetails(ES_Context *context, ES_Value *this_value, ES_Object **callee, int *argc, ES_Value **argv)
{
    context->GetExecutionContext()->GetCallDetails(*this_value, *callee, *argc, *argv);
}

/* static */ OP_STATUS
EcmaScript_Manager::GetFunctionPosition(ES_Object *function_object, unsigned &script_guid, unsigned &line_no)
{
    if (!function_object || !function_object->IsNativeFunctionObject())
        return OpStatus::ERR;

    ES_Function *function = static_cast<ES_Function *>(function_object);
    ES_FunctionCode *code = function->GetFunctionCode();
    ES_FunctionCodeStatic *data = code->GetData();
    script_guid = data->source.GetScriptGuid();
    line_no = data->start_location.Line();
    return OpStatus::OK;
}


static void
GetObjectPropertiesImpl(ES_Context *context, ES_Object *object, ES_PropertyFilter *filter, BOOL skip_non_enum, uni_char ***names, ES_Value **values, GetNativeStatus **getstatus)
{
    ES_Object *obj = ES_Value_Internal(object).GetObject(context);
    ES_Indexed_Properties *property_names = NULL;
    ES_Class *klass = NULL;
    unsigned size = 0;

    if (!obj->IsScopeObject())
    {
        property_names = obj->GetOwnPropertyNamesSafeL(context, NULL, !skip_non_enum);
        size = ES_Indexed_Properties::GetUsed(property_names);
    }
    else
    {
        klass = obj->Class();
        size = klass->Count();
    }

    uni_char **lnames = *names = OP_NEWA(uni_char *, size + 1);
    ES_Value *lvalues = *values = OP_NEWA(ES_Value, size);
    GetNativeStatus *lgetstatuses = *getstatus = OP_NEWA(GetNativeStatus, size);

    ANCHOR_ARRAY(uni_char *, lnames);
    ANCHOR_ARRAY(ES_Value, lvalues);
    ANCHOR_ARRAY(GetNativeStatus, lgetstatuses);

    if (!lnames || !lvalues || !lgetstatuses)
        return; //TODO : Check return value from caller

    lnames[size] = NULL;

    ES_CollectorLock gclock(context);

    if (klass)
    {
        // The object is a proper scope object, which is guaranteed to only have class properties
        for (unsigned i = 0; i < size; ++i)
        {
            ES_Identifier_List *klass_table = klass->GetPropertyTable()->properties;

            JString *string;
            ES_Value_Internal result;

            klass_table->Lookup(i, string);

            if (!GET_OK(obj->GetL(context->GetExecutionContext(), string, result)))
                result.SetUndefined();

            if (OpStatus::IsError(result.Export(context, *lvalues)))
                return;

            uni_char *name = StorageZ(context, string);

            if (filter && filter->Exclude(name, *lvalues))
                continue;

            if (!(*lnames = uni_strdup(name)))
                return;

            *lgetstatuses = GETNATIVE_SUCCESS;
            lnames++, lvalues++, lgetstatuses++;
            *lnames = NULL;
        }
    }
    else if (property_names)
    {
        /* The object is set through the 'with' statement, and the ordinary kind
           of property enumeration is used;
         */
        ES_Indexed_Property_Iterator iterator(context, NULL, property_names);

        unsigned index;
        while (iterator.Next(index))
        {
            *lgetstatuses = GETNATIVE_SUCCESS;
            ES_Value_Internal value, result;
            BOOL res;

            iterator.GetValue(value);
            uni_char srcstr[16] = {0}; // ARRAY OK 2011-01-06 andre
            if (value.IsUInt32())
            {
                UINT32 i = value.GetNumAsUInt32();
                uni_itoa(i, srcstr, 10);
                res = obj->GetNativeL(context, i, result);
                if (!res)
                    *lgetstatuses = GETNATIVE_NOT_OWN_NATIVE_PROPERTY;
            }
            else
                res = obj->GetNativeL(context, value.GetString(), result, NULL, lgetstatuses);

            if (!res)
                result.SetUndefined();

            if (OpStatus::IsError(result.Export(context, *lvalues)))
                return;

            uni_char *name = *srcstr ? srcstr : StorageZ(context, value.GetString());

            if (res && filter && filter->Exclude(name, *lvalues))   // we don't necessarily have the value here, so we need to do the exclution when we have
                continue;

            if (!(*lnames = uni_strdup(name)))
                return;

            lnames++, lvalues++, lgetstatuses++;
            *lnames = NULL;
        }
    }

    ANCHOR_ARRAY_RELEASE(lnames);
    ANCHOR_ARRAY_RELEASE(lvalues);
    ANCHOR_ARRAY_RELEASE(lgetstatuses);
}

void
EcmaScript_Manager::GetObjectProperties(const RuntimeOrContext &runtime_or_context, ES_Object *object, ES_PropertyFilter *filter, BOOL skip_non_enum, uni_char ***names, ES_Value **values, GetNativeStatus **getstatus)
{
    if (runtime_or_context.context)
        GetObjectPropertiesImpl(runtime_or_context.context, object, filter, skip_non_enum, names, values, getstatus);
    else
    {
        ES_Allocation_Context allocation_context(runtime_or_context.runtime);
        GetObjectPropertiesImpl(&allocation_context, object, filter, skip_non_enum, names, values, getstatus);
    }
}

/**
 * Look for a certain object in the prototype chain of another object.
 *
 * @param object Look in this object's prototype chain.
 * @param prototype The prototype object to look for.
 * @return TRUE if the prototype is present, 'FALSE' otherwise.
 */
static BOOL
HasPrototype(ES_Object *object, ES_Object *prototype)
{
    OP_ASSERT(prototype);

    while (object)
        if (object == prototype)
            return TRUE;
        else
            object = object->Class()->Prototype();

    return FALSE;
}

void
EcmaScript_Manager::GetObjectAttributes(ES_Runtime *runtime, ES_Object *object, ES_DebugObjectElement *attr)
{
    ES_Allocation_Context context(runtime);
    ES_Object *obj = ES_Value_Internal(object).GetObject(&context);

    attr->prototype = obj->Class()->Prototype();
    attr->classname = obj->Class()->ObjectName();
    if (obj->IsFunctionObject())
    {
        attr->u.function.alias = NULL;
        if (obj->IsHostObject())
        {
            ES_Host_Function *host_function = static_cast<ES_Host_Function *>(obj);

            if (HasPrototype(obj, runtime->GetFunctionPrototype()))
            {
                attr->type = OBJTYPE_HOST_FUNCTION;
                attr->u.function.nformals = host_function->GetParameterTypesLength();
                attr->u.function.name = StorageZ(&context, host_function->GetHostFunctionName());
            }
            else
                attr->type = OBJTYPE_HOST_CALLABLE_OBJECT;
        }
        else
        {
            attr->type = OBJTYPE_NATIVE_FUNCTION;
            ES_Function *function = static_cast<ES_Function *>(obj);
            attr->u.function.nformals = function->GetLength();
            if (function->GetFunctionCode())
            {
                JString *debug_name = function->GetFunctionCode()->GetDebugName();
                if (debug_name)
                    attr->u.function.alias = StorageZ(&context, debug_name);
            }
            attr->u.function.name = StorageZ(&context, function->GetName(&context));
        }
    }
    else if (obj->IsHostObject())
        attr->type = OBJTYPE_HOST_OBJECT;
    else
        attr->type = OBJTYPE_NATIVE_OBJECT;
}

void
EcmaScript_Manager::ObjectTrackedByDebugger(ES_Object *object, BOOL tracked)
{
    object->TrackedByDebugger(tracked);
}

OP_STATUS
EcmaScript_Manager::SetReformatConditionFunction(const uni_char *source)
{
    reformat_function.Unprotect();

    /* NULL or empty string disables the function. */
    if (!source || !*source)
        return OpStatus::OK;

    /* Create runtime in which function will be compiled and code evaluated. */
    if (!reformat_runtime)
    {
        OpAutoPtr<ES_Runtime> runtime(OP_NEW(ES_Runtime, ()));
        if (!runtime.get())
            return OpStatus::ERR_NO_MEMORY;

        RETURN_IF_ERROR(runtime->Construct(NULL, NULL, TRUE));
        reformat_runtime = runtime.release();
    }

    /* Compile the function with a 'scriptData' argument declared. */
    ES_Runtime::CreateFunctionOptions options;
    options.prevent_debugging = TRUE;
    options.generate_result = TRUE;
    options.context = UNI_L("Script reformat function compilation");

    const uni_char *arg = UNI_L("scriptData");

    ES_Object *function;
    RETURN_IF_ERROR(reformat_runtime->CreateFunction(NULL, 0, source, uni_strlen(source), &function, 1, &arg, options));

    return reformat_function.Protect(reformat_runtime, function);
}

static BOOL
ES_TimesliceExpiredCallback(void *)
{
    /* Abort the script if did not finish within one timeslice. */
    return TRUE;
}

BOOL
EcmaScript_Manager::GetWantReformatScript(ES_Runtime *runtime, const uni_char *program_text, unsigned program_text_length)
{
    BOOL reformat = want_reformat && IsDebugging(runtime);

    /* If program text is set and we have a reformat function compiled, run
       the function that will determine whether this particular script should
       be reformatted. */
    if (reformat && program_text && reformat_function.GetObject())
    {
        ES_Context *context = reformat_runtime->CreateContext(reformat_runtime->GetGlobalObject(), TRUE);
        if (!context)
            return FALSE;

        ES_ValueString program_string;
        program_string.string = program_text;
        program_string.length = program_text_length;

        ES_Value program_string_value;
        program_string_value.type = VALUE_STRING_WITH_LENGTH;
        program_string_value.value.string_with_length = &program_string;

        /* Push function and original script source as an argument. */
        reformat_runtime->PushCall(context, reformat_function.GetObject(), 1, &program_string_value);

        int iter = 0;
        ES_Eval_Status eval_result;
        eval_result = reformat_runtime->ExecuteContext(context, iter, FALSE, FALSE, &ES_TimesliceExpiredCallback);

        ES_Value value;
        reformat_runtime->GetReturnValue(context, &value);

        /* Reformat the script only when program evaluates to 'true'. */
        if (eval_result == ES_NORMAL_AFTER_VALUE && value.type == VALUE_BOOLEAN)
            reformat = value.value.boolean;
        else
            reformat = FALSE;

        reformat_runtime->DeleteContext(context);
    }

    return reformat;
}

#endif // ECMASCRIPT_DEBUGGER

void
EcmaScript_Manager::RegisterHostObjectCloneHandler(ES_HostObjectCloneHandler *handler)
{
    handler->next = first_host_object_clone_handler;
    first_host_object_clone_handler = handler;
}

void
EcmaScript_Manager::UnregisterHostObjectCloneHandler(ES_HostObjectCloneHandler *handler)
{
    ES_HostObjectCloneHandler **ptr = &first_host_object_clone_handler;
    while (*ptr)
        if (*ptr == handler)
        {
            *ptr = handler->next;
            break;
        }
        else
            ptr = &(*ptr)->next;
}

BOOL
EcmaScript_Manager::IsScriptSupported(const uni_char *type, const uni_char *language)
{
    BOOL handle_script = FALSE;

    int n = 0;

    if (type && *type)
    {
        // Recognized MIME types for "JavaScript":
        //
        //   application/ecmascript
        //   application/javascript
        //   application/x-ecmascript
        //   application/x-javascript
        //   text/ecmascript
        //   text/javascript
        //   text/javascript1.[0-5]
        //   text/jscript
        //   text/livescript
        //   text/x-ecmascript
        //   text/x-javascript
        //
        // All other MIME types are rejected. For recognized MIME types,
        // all parameters are considered unknown and the MIME type
        // is as a consequence rejected. Including 'version'.
        //
        // http://www.whatwg.org/specs/web-apps/current-work/multipage/scripting-1.html#scriptingLanguages

        while (*type == ' ')
            ++type;

        if (uni_strni_eq(type, "TEXT/", 5))
        {
            type += 5;

check_script_media_type:
            BOOL with_x = FALSE;

            if (uni_strni_eq(type, "X-", 2))
            {
                type += 2;
                with_x = TRUE;
            }

            if (uni_strni_eq(type, "JSCRIPT", 7))
            {
                n = 7;
                handle_script = !with_x;
            }
            else if (uni_strni_eq(type, "JAVASCRIPT", 10))
            {
                handle_script = TRUE;
                type += 10;
                if (!with_x && type[0] == '1' && type[1] == '.' && type[2] >= '0' && type[2] <= '5' && (!type[3] || type[3] == ' '))
                    n = 3;
            }
            else if (uni_strni_eq(type, "ECMASCRIPT", 10))
            {
                n = 10;
                handle_script = TRUE;
            }
            else if (uni_strni_eq(type, "LIVESCRIPT", 10))
            {
                n = 10;
                handle_script = !with_x;
            }
        }
        else if (uni_strni_eq(type, "APPLICATION/", 12))
        {
            type += 12;

            if (uni_strni_eq(type, "X-", 2))
                type += 2;

            if (uni_strni_eq(type, "JAVASCRIPT", 10))
            {
                n = 10;
                handle_script = TRUE;
            }
            else if (uni_strni_eq(type, "ECMASCRIPT", 10))
            {
                n = 10;
                handle_script = TRUE;
            }
        }

        type += n;
        while (*type == ' ')
            ++type;

        if (*type)
            handle_script = FALSE;
    }
    else if (language && *language)
    {
        // Language types supported are any of the text/.. types above
        // (but without the text/ bit.)
        type = language;
        goto check_script_media_type;
    }
    else
        handle_script = TRUE;

    return handle_script;
}

static OP_STATUS sanitize(OP_STATUS r)
{
    if (OpStatus::IsMemoryError(r) || OpStatus::IsSuccess(r))
        return r;
    else
        return OpStatus::ERR;
}

/* static */ OP_STATUS
EcmaScript_Manager_Impl::Initialise()
{
	OP_STATUS r = 0;

	g_esrt = NULL;
	TRAP(r, g_esrt = ESRT::Initialize());
	if (OpStatus::IsError(r))
	{
		if (g_esrt)
			ESRT::Shutdown(g_esrt);
		return sanitize(r);
	}

#ifndef CONSTANT_DATA_IS_EXECUTABLE
	OpPseudoThread::InitCodeVectors();
#endif // CONSTANT_DATA_IS_EXECUTABLE

#ifdef ES_NATIVE_SUPPORT
	extern void ES_SetupFunctionHandlers();
	ES_SetupFunctionHandlers();
#endif // ES_NATIVE_SUPPORT

    return OpStatus::OK;
}


void
EcmaScript_Manager::HandleOutOfMemory()
{
    if (g_esrt && g_esrt->program_cache)
        g_esrt->program_cache->Clear();
}

void
EcmaScript_Manager::AddHeap(ES_Heap *heap)
{
    heap->Into(&inactive_heaps);
}

void
EcmaScript_Manager::RemoveHeap(ES_Heap *heap)
{
    heap->Out();
}

void
EcmaScript_Manager::MoveHeapToDestroyList(ES_Heap *heap)
{
    heap->Out();
    heap->Into(&destroy_heaps);
}

void
EcmaScript_Manager::MoveHeapToInactiveList(ES_Heap *heap)
{
    heap->Out();
    heap->Into(&inactive_heaps);
}

void
EcmaScript_Manager::MoveHeapToActiveList(ES_Heap *heap)
{
    heap->Out();
    heap->Into(&active_heaps);
}

#ifdef ES_HEAP_DEBUGGER

ES_Heap *
EcmaScript_Manager::GetHeapById(unsigned id)
{
    ES_Heap *heap;

    for (heap = static_cast<ES_Heap *>(active_heaps.First()); heap; heap = static_cast<ES_Heap *>(heap->Suc()))
        if (heap->Id() == id)
            return heap;

    for (heap = static_cast<ES_Heap *>(inactive_heaps.First()); heap; heap = static_cast<ES_Heap *>(heap->Suc()))
        if (heap->Id() == id)
            return heap;

    for (heap = static_cast<ES_Heap *>(destroy_heaps.First()); heap; heap = static_cast<ES_Heap *>(heap->Suc()))
        if (heap->Id() == id)
            return heap;

    return NULL;
}

void
EcmaScript_Manager::ForceGCOnHeap(ES_Heap *heap)
{
    heap->ForceCollect(NULL, GC_REASON_SHUTDOWN);
}

ES_Runtime *
EcmaScript_Manager::GetNextRuntimePerHeap(ES_Runtime *runtime)
{
    return runtime->next_runtime_per_heap;
}

#endif // ES_HEAP_DEBUGGER
