/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2009-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 */
#include "core/pch.h"

#ifdef DOM_CROSSDOCUMENT_MESSAGING_SUPPORT
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domcore/domerror.h"
#include "modules/dom/src/domcore/domexception.h"
#include "modules/dom/src/domevents/domeventlistener.h"
#include "modules/dom/src/opera/domhttp.h"
#include "modules/dom/src/js/js_console.h"
#include "modules/dom/src/js/location.h"
#include "modules/dom/src/js/navigat.h"

#include "modules/dom/src/domwebworkers/domcrossmessage.h"
#include "modules/dom/src/domwebworkers/domcrossutils.h"

#ifdef DOM_WEBWORKERS_SUPPORT
#include "modules/dom/src/domwebworkers/domwwutils.h"
#endif // DOM_WEBWORKERS_SUPPORT

/* static */ ES_PutState
DOM_CrossMessage_Utils::PutEventHandler(DOM_Object *owner, const uni_char *event_name, DOM_EventType event_type, DOM_EventListener *&the_handler, DOM_EventQueue *event_queue, ES_Value *value, ES_Runtime *origining_runtime)
{
    OP_ASSERT(event_queue);
    DOM_EnvironmentImpl *environment = owner->GetEnvironment();

    if (value && value->type == VALUE_OBJECT && op_strcmp(ES_Runtime::GetClass(value->value.object), "Function") == 0)
    {
        DOM_EventListener *listener = OP_NEW(DOM_EventListener, ());
        if (!listener || OpStatus::IsMemoryError(listener->SetNative(environment, event_type, event_name, FALSE, TRUE, NULL, value->value.object)))
        {
            if (listener)
                DOM_EventListener::DecRefCount(listener);
            return PUT_NO_MEMORY;
        }

#ifdef ECMASCRIPT_DEBUGGER
        // If debugging is enabled we store the caller which registered the event listener.
        ES_Context *context = DOM_Object::GetCurrentThread(origining_runtime)->GetContext();
        if (origining_runtime->IsContextDebugged(context))
        {
            ES_Runtime::CallerInformation call_info;
            OP_STATUS status = ES_Runtime::GetCallerInformation(context, call_info);
            if (OpStatus::IsSuccess(status))
                listener->SetCallerInformation(call_info.script_guid, call_info.line_no);
        }
#endif // ECMASCRIPT_DEBUGGER

        if (the_handler)
            owner->GetEventTarget()->RemoveListener(the_handler);

        the_handler = listener;
        owner->GetEventTarget()->AddListener(listener);

        /* Check if there are queued up events; drain if so. */
        OP_STATUS status = event_queue->DrainEventQueue(environment);
        if (OpStatus::IsError(status))
        {
            the_handler = NULL;
            DOM_EventListener::DecRefCount(listener);
            PUT_FAILED_IF_ERROR(status);
        }
        return PUT_SUCCESS;
    }
    else if (value && (value->type == VALUE_NULL || value->type == VALUE_UNDEFINED || value->type == VALUE_NUMBER && value->value.number == 0))
    {
        if (the_handler)
        {
            owner->GetEventTarget()->RemoveListener(the_handler);
            the_handler = NULL;
        }
        return PUT_SUCCESS;
    }
    else
    {
        OpString error_message;
        PUT_FAILED_IF_ERROR(error_message.Append(UNI_L("Non-function value used for '")));
        PUT_FAILED_IF_ERROR(error_message.Append(event_name));
        PUT_FAILED_IF_ERROR(error_message.Append(UNI_L("' event handler")));

        ES_Object *type_error;
        ES_Runtime *runtime = environment->GetRuntime();
        PUT_FAILED_IF_ERROR(runtime->CreateNativeErrorObject(&type_error, ES_Native_TypeError, error_message.CStr()));
        DOM_Object::DOMSetObject(value, type_error);
        return PUT_EXCEPTION;
    }
}

/*-------------------------------------------------------------------------------------- */

/* static */ OP_STATUS
DOM_ErrorException_Utils::ReportErrorEvent(DOM_ErrorEvent *error_event, const URL &url, DOM_EnvironmentImpl *environment)
{
    const uni_char *context = UNI_L("Web Worker exception");
    ES_ErrorInfo error_info(context);
    OpString url_str;

    RETURN_IF_ERROR(url.GetAttribute(URL::KUniName_With_Fragment, url_str));
    uni_strncpy(error_info.error_text, error_event->GetMessage(), ARRAY_SIZE(error_info.error_text));
    error_info.error_text[ARRAY_SIZE(error_info.error_text)-1] = 0;
    return DOM_EnvironmentImpl::PostError(environment, error_info, context, url_str.CStr());
}

/* static */ OP_STATUS
DOM_ErrorException_Utils::BuildErrorEvent(DOM_Object *target_object, DOM_ErrorEvent *&event, const uni_char *url_str, const uni_char *message, unsigned int line_number, BOOL propagate_default)
{
    OP_ASSERT(target_object);
    DOM_Runtime *runtime = target_object->GetEnvironment()->GetDOMRuntime();
#ifdef DOM_WEBWORKERS_SUPPORT
    event = OP_NEW(DOM_WebWorker_ErrorEventDefault, (propagate_default));
#else
    event = OP_NEW(DOM_ErrorEvent, ());
#endif // DOM_WEBWORKERS_SUPPORT
    RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(event, runtime, runtime->GetPrototype(DOM_Runtime::ERROREVENT_PROTOTYPE), "ErrorEvent"));

    ES_Value return_value;
    ES_Value argv[3];

    DOM_Object::DOMSetString(&argv[0], UNI_L("error"));
    DOM_Object::DOMSetBoolean(&argv[1], FALSE); /* cannot bubble */
    DOM_Object::DOMSetBoolean(&argv[2], TRUE); /* is cancelable */
    /* Note: the return value will hold any DOM exception due to a non-event type; cannot happen here, so (somewhat) OK to drop. */
    RETURN_IF_ERROR(event->initEvent(event, argv, ARRAY_SIZE(argv), &return_value, runtime));
    RETURN_IF_ERROR(event->InitErrorEvent(message, url_str, (int)line_number));
    event->SetSynthetic();   // void.
    event->SetTarget(target_object); // void.
    return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_ErrorException_Utils::CopyErrorEvent(DOM_Object *target_object, DOM_ErrorEvent *&target, DOM_ErrorEvent *error, const URL &url, BOOL propagate_up)
{
    OP_ASSERT(error);

    /* Copying within same execution context is easy! */
    if (error->GetEnvironment() == target_object->GetEnvironment())
    {
        target = error;
        return OpStatus::OK;
    }
    return CloneErrorEvent(target_object, target, error, url, propagate_up);
}

/* static */ OP_STATUS
DOM_ErrorException_Utils::CloneErrorEvent(DOM_Object *target_object, DOM_ErrorEvent *&target, DOM_ErrorEvent *source, const URL &origin_url, BOOL propagate_up)
{
    OP_ASSERT(source);
    unsigned int line_number = source->GetResourceLineNumber();
    const uni_char *message = source->GetMessage();

    OpString url_str;
    /* The URL of the worker gets preference -- ignoring errors and attempt to build ErrorEvent regardless. */
    RETURN_IF_ERROR(origin_url.GetAttribute(URL::KUniName_With_Fragment, url_str));
    return BuildErrorEvent(target_object, target, url_str.CStr(), message, line_number, propagate_up);
}

/* static */ OP_STATUS
DOM_ErrorException_Utils::CloneDOMEvent(DOM_Event *event, DOM_Object *target_object, DOM_Event *&result_target_event)
{
    if (event->IsA(DOM_TYPE_ERROREVENT))
    {
        DOM_ErrorEvent *error_event = static_cast<DOM_ErrorEvent*>(event);
        DOM_ErrorEvent *target_event = NULL;
        RETURN_IF_ERROR(BuildErrorEvent(target_object, target_event, error_event->GetResourceUrl(), error_event->GetMessage(), error_event->GetResourceLineNumber(), FALSE));
        result_target_event = target_event;
        return OpStatus::OK;
    }
    else if (event->IsA(DOM_TYPE_MESSAGEEVENT))
    {
        DOM_MessageEvent *msg_event = static_cast<DOM_MessageEvent*>(event);
        DOM_Runtime      *target_runtime = target_object->GetRuntime();
        DOM_MessageEvent *target_event = OP_NEW(DOM_MessageEvent, ());
        RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(target_event, target_runtime, target_runtime->GetPrototype(DOM_Runtime::CROSSDOCUMENT_MESSAGEEVENT_PROTOTYPE), "MessageEvent"));

        if (!target_event)
            return OpStatus::ERR_NO_MEMORY;

        ES_Value argv[5];
        DOM_Object::DOMCopyValue(argv[0], msg_event->GetData());
        DOM_Object::DOMSetString(&argv[1], msg_event->GetOrigin());
        DOM_Object::DOMSetString(&argv[2], msg_event->GetLastEventId());
        /* Cannot reliably propagate .source */
        DOM_Object::DOMSetNull(&argv[3]);
        DOM_Object::DOMSetObject(&argv[4], msg_event->GetPorts());

        ES_Value return_value;
        /* Note: we leave both the source and target ports as empty here -- the communication is
         * happening across these (but are about setting that up..) */
        RETURN_IF_ERROR(DOM_MessageEvent::Make(target_event, target_object, NULL/*source port*/, NULL/* no target port*/, target_object->GetEnvironment(), target_runtime->GetOriginURL(), argv, ARRAY_SIZE(argv), &return_value));

        target_event->InitEvent(msg_event->GetKnownType(), target_object);
        target_event->SetSynthetic();
        target_event->SetEventPhase(ES_PHASE_ANY);
        result_target_event = target_event;
        return OpStatus::OK;
    }
    else
        return OpStatus::ERR;
}

#endif // DOM_CROSSDOCUMENT_MESSAGING_SUPPORT
