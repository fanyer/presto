/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/vm/es_suspended_call.h"
#include "modules/ecmascript/carakan/src/compiler/es_parser.h"
#include "modules/ecmascript/carakan/src/util/es_formatter.h"
#include "modules/util/opstring.h"

void
ES_SuspendedParseProgram::DoCall(ES_Execution_Context *context)
{
    TRAP(status, success = parser.ParseProgram(code, body, value, is_strict_mode));
    if (OpStatus::IsError(status))
        success = FALSE;

#ifdef ECMASCRIPT_DEBUGGER
    parser.DebuggerSignalNewScript(parser.GetContext());
#endif // ECMASCRIPT_DEBUGGER

    if (!success && status != OpStatus::ERR_NO_MEMORY)
    {
        unsigned body_length = Length(body);
        ES_ParseErrorInfo info;

        parser.GetError(info);

        OpString message_string;

        if (info.line == 1)
            if (body_length < 128)
            {
                message_string.AppendFormat(UNI_L("at index %u in \""), info.index);
                message_string.Append(Storage(parser.GetContext(), body), body_length);
                message_string.Append(UNI_L("\": "));
            }
            else
                message_string.AppendFormat(UNI_L("at index %u: "), info.index);
        else
        {
            unsigned column = 0;
            while (info.index != 0 && !ES_Lexer::IsLinebreak(Storage(parser.GetContext(), body)[info.index - 1]))
                ++column, --info.index;
            message_string.AppendFormat(UNI_L("at line %u, column %u: "), info.line, column);
        }
        ES_CollectorLock gclock(parser.GetContext());
        message = JString::Make(parser.GetContext(), message_string.CStr());
        Append(parser.GetContext(), message, info.message);

#if 0
#ifdef ECMASCRIPT_DEBUGGER

        ES_ErrorInfo err(UNI_L(""));

        uni_sprintf(err.error_text, UNI_L("Syntax error at line %u %s:\n  "), info.line, err.error_context);

        unsigned length = uni_strlen(err.error_text);

        err.error_length = es_minu(length + Length(message), (sizeof err.error_text / sizeof err.error_text[0]) - 1);

        uni_strncpy(err.error_text + length, Storage(parser.GetContext(), message), err.error_length - length);
        err.error_text[err.error_length] = 0;

        err.error_line = info.line;
        err.error_offset = info.index;
        err.script_guid = parser.GetScriptGuid();

        parser.DebuggerSignalParseError(&err);
#endif // ECMASCRIPT_DEBUGGER
#endif // 0
    }
}

void
ES_SuspendedParseFunction::DoCall(ES_Execution_Context *context)
{
    ES_Parser parser(context, global_object, FALSE);
    TRAP(status, success = parser.ParseFunction(code, formals, formals_length, body, body_length));
    if (OpStatus::IsError(status))
        success = FALSE;
}

void
ES_SuspendedFormatProgram::DoCall(ES_Execution_Context *context)
{
    TRAP(status, success = formatter.FormatProgram(body));
    if (OpStatus::IsError(status))
        success = FALSE;
}

void
ES_SuspendedHostGetName::DoCall(ES_Execution_Context *context)
{
    ES_CollectorLock gclock(context);
    if (!is_restarted)
        state = base->GetName(property_name, property_code, value, origining_runtime);
    else
        state = base->GetNameRestart(property_name, property_code, value, origining_runtime, restart_object);
}

void
ES_SuspendedHostGetIndex::DoCall(ES_Execution_Context *context)
{
    ES_CollectorLock gclock(context);
    if (!is_restarted)
        state = base->GetIndex(property_index, value, origining_runtime);
    else
        state = base->GetIndexRestart(property_index, value, origining_runtime, restart_object);
}

void
ES_SuspendedHostPutName::DoCall(ES_Execution_Context *context)
{
    ES_CollectorLock gclock(context);
    if (!is_restarted)
        state = base->PutName(property_name, property_code, value, origining_runtime);
    else
        state = base->PutNameRestart(property_name, property_code, value, origining_runtime, restart_object);
}

void
ES_SuspendedHostPutIndex::DoCall(ES_Execution_Context *context)
{
    ES_CollectorLock gclock(context);
    if (!is_restarted)
        state = base->PutIndex(property_index, value, origining_runtime);
    else
        state = base->PutIndexRestart(property_index, value, origining_runtime, restart_object);
}

void
ES_SuspendedHostHasPropertyName::DoCall(ES_Execution_Context *context)
{
    ES_CollectorLock gclock(context);
    result = base->HasPropertyName(property_name, property_code, is_restarted, origining_runtime);
}

void
ES_SuspendedHostHasPropertyIndex::DoCall(ES_Execution_Context *context)
{
    ES_CollectorLock gclock(context);
    result = base->HasPropertyIndex(property_index, is_restarted, origining_runtime);
}

void
ES_SuspendedHostSecurityCheck::DoCall(ES_Execution_Context *context)
{
    ES_CollectorLock gclock(context);
    result = base->SecurityCheck(origining_runtime);
}

void
ES_SuspendedHostFetchProperties::DoCall(ES_Execution_Context *context)
{
    ES_CollectorLock gclock(context);
    TRAPD(status, result = base->FetchPropertiesL(enumerator, origining_runtime));
    if (OpStatus::IsError(status))
        result = GET_NO_MEMORY;
}

void
ES_SuspendedHostGetIndexedPropertiesLength::DoCall(ES_Execution_Context *context)
{
    ES_CollectorLock gclock(context);
    result = base->GetIndexedPropertiesLength(return_value,origining_runtime);
}

void
ES_SuspendedHostCall::DoCall(ES_Execution_Context *context)
{
    ES_CollectorLock gclock(context);
    if (restarted)
        argv = NULL, argc = -1;
    else if (conversion_restarted)
        argc = -(2 + argc);

    result = base->Call(this_native_object, argv, argc, return_value, origining_runtime);

    restarted = TRUE;
}

void
ES_SuspendedHostConstruct::DoCall(ES_Execution_Context *context)
{
    ES_CollectorLock gclock(context);
    if (restarted)
        argv = NULL, argc = -1;
    else if (conversion_restarted)
        argc = -(2 + argc);

    result = base->Construct(argv, argc, return_value, origining_runtime);

    restarted = TRUE;
}

void
ES_SuspendedHostAllowPropertyOperationFor::DoCall(ES_Execution_Context *context)
{
    ES_CollectorLock gclock(context);
    result = base->SecurityCheck(origining_runtime) && base->AllowOperationOnProperty(property_name, op);
}

void
ES_SuspendedHostIdentity::DoCall(ES_Execution_Context *context)
{
    /*
     * The lock had to go...
     * Locking the heap here means that we can't do gc:s when creating
     * an es environment.  Not doing a gc has a very bad effect on
     * performance when a new es environment is created on the same
     * heap.
     */
    result = base->Identity(&loc);
}

void
ES_SuspendedHostDeleteName::DoCall(ES_Execution_Context *context)
{
    status = base->DeleteName(property_name, origining_runtime);
}

void
ES_SuspendedHostDeleteIndex::DoCall(ES_Execution_Context *context)
{
    status = base->DeleteIndex(property_index, origining_runtime);
}

ES_SuspendedClearHead::ES_SuspendedClearHead(ES_Execution_Context *context, Head *head)
    : head(head)
{
    context->SuspendedCall(this);
}

void
ES_SuspendedClearHead::DoCall(ES_Execution_Context *context)
{
    head->Clear();
}

#ifdef ECMASCRIPT_DEBUGGER
void
ES_Suspended_Debugger_HandleEvent::DoCall(ES_Execution_Context *context)
{
    ES_CollectorLock gclock(context);
    if (g_ecmaManager->GetDebugListener())
        return_value = g_ecmaManager->GetDebugListener()->HandleEvent(event_type, script_guid, line);
    else
        return_value = ES_DebugListener::ESACT_CONTINUE;
}
#endif // ECMASCRIPT_DEBUGGER

#ifdef ES_OVERRUN_PROTECTION
# include <sys/mman.h>

template <class T>
void
ES_Suspended_NEWA_OverrunProtected<T>::DoCall(ES_Execution_Context *context)
{
    enum { PAGESIZE = 4096 };

    unsigned mapsize = PAGESIZE, arraysize = count * sizeof *storage;

    while (mapsize < arraysize)
        mapsize += PAGESIZE;

    if (mapsize == arraysize)
    {
        char *mapped = static_cast<char *>(mmap(NULL, PAGESIZE + mapsize + PAGESIZE, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));

        if (mapped != MAP_FAILED)
        {
            mprotect(mapped + PAGESIZE, mapsize, PROT_READ | PROT_WRITE);
            storage = reinterpret_cast<T *>(mapped + PAGESIZE);
        }
        else
            storage = NULL;
    }
    else
    {
        char *mapped = static_cast<char *>(mmap(NULL, mapsize + PAGESIZE, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));

        if (mapped != MAP_FAILED)
        {
            mprotect(mapped, mapsize, PROT_READ | PROT_WRITE);
            storage = reinterpret_cast<T *>(mapped + mapsize - arraysize);
        }
        else
            storage = NULL;
    }
}

template <class T>
void
ES_Suspended_DELETEA_OverrunProtected<T>::DoCall(ES_Execution_Context *context)
{
    enum { PAGESIZE = 4096 };

    unsigned mapsize = PAGESIZE, arraysize = count * sizeof *storage;

    while (mapsize < arraysize)
        mapsize += PAGESIZE;

    if (mapsize == arraysize)
        munmap(reinterpret_cast<char *>(storage) - PAGESIZE, PAGESIZE + mapsize + PAGESIZE);
    else
        munmap(reinterpret_cast<char *>(storage) + arraysize - mapsize, mapsize + PAGESIZE);
}

template class ES_Suspended_NEWA_OverrunProtected<ES_Value_Internal>;
template class ES_Suspended_DELETEA_OverrunProtected<ES_Value_Internal>;

template class ES_Suspended_NEWA_OverrunProtected<ES_VirtualStackFrame>;
template class ES_Suspended_DELETEA_OverrunProtected<ES_VirtualStackFrame>;

template class ES_Suspended_NEWA_OverrunProtected<ES_RegisterBlocksAdjustment>;
template class ES_Suspended_DELETEA_OverrunProtected<ES_RegisterBlocksAdjustment>;

#endif // ES_OVERRUN_PROTECTION
