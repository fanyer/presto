/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 2009-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** @author sof
*/
#include "core/pch.h"

#ifdef DOM_WEBWORKERS_SUPPORT
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/domevents/domeventlistener.h"

#include "modules/dom/src/domwebworkers/domwebworkers.h"
#include "modules/dom/src/domwebworkers/domwwobject.h"

#include "modules/dom/src/domwebworkers/domcrossmessage.h"
#include "modules/dom/src/domwebworkers/domcrossutils.h"

#include "modules/dom/src/domwebworkers/domwwutils.h"

DOM_WebWorkerObject::~DOM_WebWorkerObject()
{
    /* Drop the connection to the worker execution context; safe if its lifetime extends ours.
       A DOM_WebWorker is finalised/destroyed via its DOM_WebWorkerDomain, which in turn are let go
       from the DOM_WebWorkerController of the environment that spawned the workers. It deletes the
       DOM_WebWorkerObjects of the environment before the DOM_WebWorkerDomains, hence it is safe to
       poke into the worker here and clear the back-pointer. */
    if (the_worker)
        the_worker->ClearOtherWorker(this);
    the_worker = NULL;

    DropEntangledPorts();
    port = NULL;
}

/* virtual */ void
DOM_WebWorkerObject::GCTrace()
{
    DOM_Runtime *runtime = GetRuntime();
    if (port && runtime->IsSameHeap(port->GetRuntime()))
        runtime->GCMark(port);

    for (DOM_MessagePort *p = entangled_ports.First(); p; p = p->Suc())
         if (p->GetRuntime() == runtime)
             runtime->GCMark(p);

    message_event_queue.GCTrace(runtime);
    error_event_queue.GCTrace(runtime);
    GCMark(FetchEventTarget());
}


/* static */ OP_STATUS
DOM_WebWorkerObject::Make(DOM_Object *this_object, DOM_WebWorkerObject *&instance, DOM_WebWorker *parent, const uni_char *url, const uni_char *name, ES_Value *return_value)
{
    BOOL is_dedicated = (name == NULL);

    /* Create representation of the Worker in the origining context */
    if (is_dedicated)
        instance = OP_NEW(DOM_DedicatedWorkerObject, ());
    else
        instance = OP_NEW(DOM_SharedWorkerObject, ());

    DOM_EnvironmentImpl *environment = this_object->GetEnvironment();
    DOM_Runtime *runtime = environment->GetDOMRuntime();
    ES_Object *proto = runtime->GetPrototype( is_dedicated ? DOM_Runtime::WEBWORKERS_DEDICATED_OBJECT_PROTOTYPE : DOM_Runtime::WEBWORKERS_SHARED_OBJECT_PROTOTYPE);
    RETURN_IF_ERROR(DOMSetObjectRuntime(instance, runtime, proto, ( is_dedicated ? "Worker" : "SharedWorker")));

    /* Create the worker and its execution environment; in case there already
       is a worker that can be reused, we share/recycle it. */
    DOM_WebWorker *inner = NULL;
    DOM_WebWorkerDomain *worker_domain = NULL;
    URL context_url = runtime->GetOriginURL();
    URL worker_url = g_url_api->GetURL(context_url, url);
    RETURN_IF_ERROR(DOM_WebWorker::NewExecutionContext(this_object, inner, worker_domain, parent, instance, worker_url, name, return_value));

    /* Tie the knot between the Worker instance in the origining/creating context and the one on
       the inside. The one within the Worker execution context could be shared and maintain multiple
       references outwards. If successful, the creation of the worker context will add the instance to
       its list. */
    instance->the_worker = inner;

    instance->SetLocationURL(worker_url);

    /* Record the Worker object on the controller for our owner environment; it keeps them alive */
    RETURN_IF_ERROR(instance->GetEnvironment()->GetWorkerController()->AddWebWorkerObject(instance));

    /* If a shared worker, create its port (available as .port on the object) and notify inner Worker object
       that another instance has connected by firing a 'connect' event. */
    if (!is_dedicated)
    {
        DOM_MessagePort *portIn = NULL;
        RETURN_IF_ERROR(DOM_MessagePort::Make(portIn, runtime));
        instance->port = portIn;

        /* Fire 'connect' event, communicating to the worker that 'port' is now available
         * for outgoing communication. Only for shared workers. */
        RETURN_IF_ERROR(inner->RegisterConnection(instance->port));

        /* Now entangled; record the port as being tied to its worker. */
        instance->AddEntangledPort(portIn);
    }
    return OpStatus::OK;
}

void
DOM_WebWorkerObject::TerminateWorker()
{
    OP_ASSERT(GetWorker());
    /* Perform termination of a Worker and its execution environment; extends to its progeny */
    if (GetWorker())
        GetWorker()->CloseWorker();

    error_handler = NULL;
    message_handler = NULL;
    DropEntangledPorts();
}

/* static */ int
DOM_DedicatedWorkerObject::postMessage(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
    /* Entry point for script code that called .postMessage() on a dedicated worker object.
       NOTE: this is not supported on shared workers, you have to go via the .port property..a common gotcha to forget this. */
    DOM_THIS_OBJECT(worker, DOM_TYPE_WEBWORKERS_WORKER_OBJECT_DEDICATED, DOM_DedicatedWorkerObject);

    if (argc < 0)
        return ES_FAILED;

    DOM_CHECK_ARGUMENTS("-|O");

    /* If we're unconnected to a target Worker, or it is closed or a shared worker; bail...noisily. */
    if (!worker->GetWorker() || worker->GetWorker()->IsClosed() || !worker->GetWorker()->IsDedicated())
        return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);

    /* the origin url of the _sender_. */
    URL origin_sender = worker->GetLocationURL();
    /* the DOM environment we are injecting the message into. */
    DOM_EnvironmentImpl *target_environment = worker->GetWorker()->GetEnvironment();
    /* Construct a MessageEvent holding the incoming content, which is then simply sent on and delivered via std DOM mechanisms. */
    int result;
    DOM_MessageEvent *message_event = NULL;
    if ((result = DOM_MessageEvent::Create(message_event, worker/*this_object*/, target_environment->GetDOMRuntime(), NULL/*no source port*/, NULL/*no target port*/, origin_sender, argv, argc, return_value)) == ES_FAILED && message_event)
    {
        /* Send event to the pointed-to entity:
            - If from a browser context, to the listeners on the inside object representing the worker.
            - If from within a worker, to the browser context object's listeners.

           Hence the switch-over in environment + setting of targets. */
        message_event->SetTarget(worker->GetWorker());
        message_event->SetSynthetic();
        CALL_FAILED_IF_ERROR(worker->GetWorker()->DeliverMessage(message_event, TRUE));
        return (ES_RESTART | ES_SUSPEND);
    }

    return result;
}

/* static */ int
DOM_WebWorkerObject::terminate(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
    DOM_THIS_OBJECT(worker, DOM_TYPE_WEBWORKERS_WORKER_OBJECT, DOM_WebWorkerObject);

    /* "Inform" caller of repeated terminations. */
    if (worker->IsClosed())
        return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);


    /* Expressly terminated, drop the reference from the owning controller. */
    worker->GetEnvironment()->GetWorkerController()->RemoveWebWorkerObject(worker);

    /* terminate() is called on the Worker object, now go inside and tell it to cease operation. */
    worker->TerminateWorker(); // returns void.
    return ES_FAILED;
}

/* virtual */ ES_GetState
DOM_SharedWorkerObject::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
    switch(property_name)
    {
    case OP_ATOM_port:
        DOMSetObject(value, port);
        return GET_SUCCESS;
    default:
        return DOM_WebWorkerObject::GetName(property_name, value, origining_runtime);
    }
}

/* virtual */ ES_PutState
DOM_SharedWorkerObject::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
    switch(property_name)
    {
    case OP_ATOM_port:
        return PUT_SUCCESS;
    default:
        return DOM_Object::PutName(property_name, value, origining_runtime);
    }
}

/* virtual */ ES_GetState
DOM_DedicatedWorkerObject::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
    switch (property_name)
    {
    case OP_ATOM_onmessage:
        /* Implement the HTML5-prescribed event target behaviour of giving back a 'null' rather
           than 'undefined' for targets without any listeners. We don't yet have a consistent story
           throughout dom/ for these event targets, hence local "hack". */
        DOMSetObject(value, message_handler);
        return GET_SUCCESS;
    default:
        return DOM_WebWorkerObject::GetName(property_name, value, origining_runtime);
    }
}

/* virtual */ ES_GetState
DOM_WebWorkerObject::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
    switch (property_name)
    {
    case OP_ATOM_onerror:
        DOMSetObject(value, error_handler);
        return GET_SUCCESS;
    default:
        return DOM_Object::GetName(property_name, value, origining_runtime);
    }
}

/* virtual */ ES_PutState
DOM_DedicatedWorkerObject::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
    switch (property_name)
    {
    case OP_ATOM_onmessage:
        PUT_FAILED_IF_ERROR(DOM_Object::CreateEventTarget());
        return DOM_CrossMessage_Utils::PutEventHandler(this, UNI_L("message"), ONMESSAGE, message_handler, &message_event_queue, value, origining_runtime);
    default:
        return DOM_WebWorkerObject::PutName(property_name, value, origining_runtime);
    }
}


/* virtual */ ES_PutState
DOM_WebWorkerObject::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
    switch (property_name)
    {
    case OP_ATOM_onerror:
        PUT_FAILED_IF_ERROR(DOM_Object::CreateEventTarget());
        return DOM_CrossMessage_Utils::PutEventHandler(this, UNI_L("error"), ONERROR, error_handler, &error_event_queue, value, origining_runtime);
    default:
        return DOM_Object::PutName(property_name, value, origining_runtime);
    }
}

/* Resisting the temptation to unify this one with the DOM_WebWorker version...for now :) */

/* static */ int
DOM_WebWorkerObject::accessEventListener(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
    int status = DOM_Node::accessEventListener(this_object, argv, argc, return_value, origining_runtime, data);
    /* Check if adding a listener on the Worker object may allow us to drain queued-up events. */
    if ((status == ES_FAILED || status == ES_VALUE) && argc >= 1 && (data == 0 || data == 2))
    {
        DOM_THIS_OBJECT(worker, DOM_TYPE_WEBWORKERS_WORKER_OBJECT, DOM_WebWorkerObject);
        OpStatus::Ignore(worker->DrainEventQueues(worker));
    }
    return status;
}


OP_STATUS
DOM_WebWorkerObject::DeliverMessage(DOM_MessageEvent *message_event)
{
    RETURN_IF_ERROR(message_event_queue.DeliverEvent(message_event, GetEnvironment()));

    /* Somewhat unfortunate place, but do check that we haven't got a
       a listener set up after all.. this can happen behind our back for
       toplevel assignments to "onmessage = ..." for dedicated workers. */
    return DrainEventQueues(this);
}

OP_STATUS
DOM_WebWorkerObject::InvokeErrorListeners(DOM_ErrorEvent *exception, BOOL propagate_if_not_handled)
{
    DOM_ErrorEvent *event = NULL;
    RETURN_IF_ERROR(DOM_ErrorException_Utils::CopyErrorEvent(this, event, exception, GetLocationURL(), propagate_if_not_handled));
    /* break off further (re-)delivery */
    event->SetStopPropagation(TRUE);
    return DeliverError(this, event);
}

/* virtual */ OP_STATUS
DOM_WebWorkerObject::PropagateErrorException(DOM_ErrorEvent *exception)
{
    /* The inner worker is the better spot to handle propagation, forward if possible. */
    if (GetWorker())
        return GetWorker()->PropagateErrorException(exception);
    else
    {
        OpStatus::Ignore(InvokeErrorListeners(exception, FALSE));
        return DOM_ErrorException_Utils::ReportErrorEvent(exception, GetLocationURL(), GetEnvironment());
    }
}

static OP_BOOLEAN
WorkerSecurityCheck(URL &first_url, const uni_char *worker_script_url_str, URL &resolved_url, BOOL is_dedicated)
{
    /* Empty string could legally be passed, but never NULL. */
    OP_ASSERT(worker_script_url_str);

    resolved_url = g_url_api->GetURL(first_url, worker_script_url_str);
    if (resolved_url.IsEmpty() || !resolved_url.IsValid() || DOM_Utils::IsOperaIllegalURL(resolved_url))
        return OpBoolean::ERR;
    if (!DOM_Object::OriginCheck(first_url, resolved_url))
    {
#ifdef ALLOW_WORKER_DATA_URLS
        if (is_dedicated && resolved_url.Type() == URL_DATA)
            return OpBoolean::IS_TRUE;
#endif // ALLOW_WORKER_DATA_URLS
        return OpBoolean::IS_FALSE;
    }
    return OpBoolean::IS_TRUE;
}

/* virtual */ int
DOM_DedicatedWorkerObject_Constructor::Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime)
{
    /* This constructor may be called upon from inside a worker or from
       an external browser context. The difference between the two being
       in the DOM_Environment passed in -- the former is without a FramesDocument.

       If a new worker task has to be created to satisfy this constructor request,
       we create a second DOM_DedicatedWorker object for the insides of that new
       execution environment (accessible via 'self.worker'.)*/
    DOM_CHECK_ARGUMENTS("s");
    const uni_char *worker_script_url = argv[0].value.string;

    /* Check and resolve the worker URL at the outset.*/
    URL origin = GetEnvironment()->GetDOMRuntime()->GetOriginURL();
    URL resolved_url;

    OP_BOOLEAN checkStatus = WorkerSecurityCheck(origin, worker_script_url, resolved_url, TRUE);
    if (OpBoolean::IS_FALSE == checkStatus)
        return CallDOMException(DOM_Object::SECURITY_ERR, return_value);
    if (OpStatus::IsError(checkStatus))
        return CallDOMException(DOM_Object::SYNTAX_ERR, return_value);

    DOM_WebWorker *parent = NULL;
    if (!GetEnvironment()->GetFramesDocument())
        parent = GetEnvironment()->GetWorkerController()->GetWorkerObject();

    OP_STATUS status;
    DOM_WebWorkerObject *ww;
    OpString url_str;
    CALL_FAILED_IF_ERROR(resolved_url.GetAttribute(URL::KUniName_With_Fragment_Escaped, url_str));

    if (OpStatus::IsError(status = DOM_WebWorkerObject::Make(this, ww, parent, url_str.CStr(), NULL, return_value)))
    {
        if (OpStatus::IsMemoryError(status))
            return ES_NO_MEMORY;
        /* Last chance..fill in with error value (if none.) */
        if (return_value->type != VALUE_OBJECT)
            return CallDOMException(DOM_Object::SYNTAX_ERR, return_value);
        return ES_EXCEPTION;
    }
    DOMSetObject(return_value, ww);
    return ES_VALUE;
}

/* virtual */ int
DOM_SharedWorkerObject_Constructor::Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime)
{
    DOM_CHECK_ARGUMENTS("s|s");

    const uni_char *worker_script_url = argv[0].value.string;
    const uni_char *worker_shared_name = UNI_L("");
    if (argc >= 2)
        worker_shared_name = argv[1].value.string;

    /* Check and resolve the worker URL at the outset.*/
    URL origin = GetEnvironment()->GetDOMRuntime()->GetOriginURL();
    URL resolved_url;
    OP_BOOLEAN checkStatus = WorkerSecurityCheck(origin, worker_script_url, resolved_url, FALSE);
    if (OpBoolean::IS_FALSE == checkStatus)
        return CallDOMException(DOM_Object::SECURITY_ERR, return_value);
    if (OpStatus::IsError(checkStatus))
        return CallDOMException(DOM_Object::SYNTAX_ERR, return_value);

    DOM_WebWorker *parent = NULL;
    if (!GetEnvironment()->GetFramesDocument())
        parent = GetEnvironment()->GetWorkerController()->GetWorkerObject();

    OP_STATUS status;
    DOM_WebWorkerObject *ww = NULL;
    OpString url_str;
    CALL_FAILED_IF_ERROR(resolved_url.GetAttribute(URL::KUniName_With_Fragment, url_str));

    if (OpStatus::IsError(status = DOM_WebWorkerObject::Make(this, ww, parent, url_str.CStr(), worker_shared_name, return_value)))
    {
        if (OpStatus::IsMemoryError(status))
            return ES_NO_MEMORY;
        /* Last chance..fill in with error value (if none.) */
        if (return_value->type != VALUE_OBJECT)
            return CallDOMException(DOM_Object::SYNTAX_ERR, return_value);
        return ES_EXCEPTION;
    }
    DOMSetObject(return_value, ww);
    return ES_VALUE;
}

#include "modules/dom/src/domglobaldata.h"

/* Methods on the Worker object: */
DOM_FUNCTIONS_START(DOM_DedicatedWorkerObject)
    DOM_FUNCTIONS_FUNCTION(DOM_DedicatedWorkerObject, DOM_WebWorkerObject::terminate, "terminate", "-")
    DOM_FUNCTIONS_FUNCTION(DOM_DedicatedWorkerObject, DOM_DedicatedWorkerObject::postMessage, "postMessage", NULL)
    DOM_FUNCTIONS_FUNCTION(DOM_DedicatedWorkerObject, DOM_Node::dispatchEvent, "dispatchEvent", NULL)
DOM_FUNCTIONS_END(DOM_DedicatedWorkerObject)

DOM_FUNCTIONS_WITH_DATA_START(DOM_DedicatedWorkerObject)
    DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_DedicatedWorkerObject, DOM_WebWorkerObject::accessEventListener, 0, "addEventListener", "s-b-")
    DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_DedicatedWorkerObject, DOM_WebWorkerObject::accessEventListener, 1, "removeEventListener", "s-b-")
#ifdef DOM3_EVENTS
    DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_DedicatedWorkerObject, DOM_WebWorkerObject::accessEventListener, 2, "addEventListenerNS", "ss-b-")
    DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_DedicatedWorkerObject, DOM_WebWorkerObject::accessEventListener, 3, "removeEventListenerNS", "ss-b-")
#endif // DOM3_EVENTS
DOM_FUNCTIONS_WITH_DATA_END(DOM_DedicatedWorkerObject)

/* Methods on the shared Worker object: */
DOM_FUNCTIONS_START(DOM_SharedWorkerObject)
    DOM_FUNCTIONS_FUNCTION(DOM_SharedWorkerObject, DOM_WebWorkerObject::terminate, "terminate", "-")
    DOM_FUNCTIONS_FUNCTION(DOM_SharedWorkerObject, DOM_Node::dispatchEvent, "dispatchEvent", NULL)
DOM_FUNCTIONS_END(DOM_SharedWorkerObject)

DOM_FUNCTIONS_WITH_DATA_START(DOM_SharedWorkerObject)
    DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_SharedWorkerObject, DOM_WebWorkerObject::accessEventListener, 0, "addEventListener", "s-b-")
    DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_SharedWorkerObject, DOM_WebWorkerObject::accessEventListener, 1, "removeEventListener", "s-b-")
#ifdef DOM3_EVENTS
    DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_SharedWorkerObject, DOM_WebWorkerObject::accessEventListener, 2, "addEventListenerNS", "ss-b-")
    DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_SharedWorkerObject, DOM_WebWorkerObject::accessEventListener, 3, "removeEventListenerNS", "ss-b-")
#endif // DOM3_EVENTS
DOM_FUNCTIONS_WITH_DATA_END(DOM_SharedWorkerObject)

#endif // DOM_WEBWORKERS_SUPPORT
