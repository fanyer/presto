/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 2009-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#ifdef DOM_WEBWORKERS_SUPPORT
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/domevents/domeventlistener.h"
#include "modules/dom/src/domcore/domerror.h"
#include "modules/dom/src/domcore/domexception.h"

#include "modules/dom/src/domwebworkers/domwebworkers.h"
#include "modules/dom/src/domwebworkers/domwworker.h"

#include "modules/dom/src/domwebworkers/domcrossmessage.h"
#include "modules/dom/src/domwebworkers/domcrossutils.h"

#include "modules/dom/src/domwebworkers/domwwutils.h"
#include "modules/dom/src/domwebworkers/domwwloader.h"

#include "modules/dom/src/domevents/domeventsource.h"
#include "modules/dom/src/domfile/domfileerror.h"
#include "modules/dom/src/domfile/domfileexception.h"
#include "modules/dom/src/domfile/domfilereadersync.h"
#include "modules/dom/src/opera/domhttp.h"
#include "modules/dom/src/opera/domformdata.h"
#include "modules/dom/src/js/js_console.h"
#include "modules/dom/src/js/location.h"
#include "modules/dom/src/js/navigat.h"
#include "modules/dom/src/js/dombase64.h"

#ifdef CANVAS_SUPPORT
# include "modules/dom/src/canvas/domcontext2d.h"
#endif // CANVAS_SUPPORT

#ifdef WEBSOCKETS_SUPPORT
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/dom/src/websockets/domwebsocket.h"
#endif // WEBSOCKETS_SUPPORT

#ifdef DOM_WEBWORKERS_WORKER_STAT_SUPPORT
#include "modules/dom/src/domwebworkers/domwwinfo.h"
#endif // DOM_WEBWORKERS_WORKER_STAT_SUPPORT

#ifdef PROGRESS_EVENTS_SUPPORT
#include "modules/dom/src/domevents/domprogressevent.h"
#endif // PROGRESS_EVENTS_SUPPORT

#include "modules/prefs/prefsmanager/collections/pc_js.h"

#include "modules/dom/src/domglobaldata.h"

DOM_DedicatedWorkerGlobalScope::DOM_DedicatedWorkerGlobalScope()
#ifdef DOM_NO_COMPLEX_GLOBALS
    : DOM_Prototype(g_DOM_globalData->DOM_DedicatedWorkerGlobalScope_functions, g_DOM_globalData->DOM_DedicatedWorkerGlobalScope_functions_with_data)
#else // DOM_NO_COMPLEX_GLOBALS
    : DOM_Prototype(DOM_DedicatedWorkerGlobalScope_functions, DOM_DedicatedWorkerGlobalScope_functions_with_data)
#endif // DOM_NO_COMPLEX_GLOBALS
{
}

/* virtual */ void
DOM_DedicatedWorkerGlobalScope::GCTrace()
{
    GetEnvironment()->GCTrace();
    DOM_Prototype::GCTrace();
}

DOM_SharedWorkerGlobalScope::DOM_SharedWorkerGlobalScope()
#ifdef DOM_NO_COMPLEX_GLOBALS
    : DOM_Prototype(g_DOM_globalData->DOM_SharedWorkerGlobalScope_functions, g_DOM_globalData->DOM_SharedWorkerGlobalScope_functions_with_data)
#else // DOM_NO_COMPLEX_GLOBALS
    : DOM_Prototype(DOM_SharedWorkerGlobalScope_functions, DOM_SharedWorkerGlobalScope_functions_with_data)
#endif // DOM_NO_COMPLEX_GLOBALS
{
}

/* virtual */ void
DOM_SharedWorkerGlobalScope::GCTrace()
{
    GetEnvironment()->GCTrace();
    DOM_Prototype::GCTrace();
}

/*-------------------------------------------------------------------------------------- */

DOM_WebWorker::DOM_WebWorker(BOOL is_dedicated)
    : parent(NULL),
      domain(NULL),
      worker_name(NULL),
      port(NULL),
      is_dedicated(is_dedicated),
      is_closed(FALSE),
      is_enabled(FALSE),
      connect_handler(NULL),
      keep_alive_counter(1024)
{
}

/** Initialise the global scope for a Worker.

    @param proto The prototype of the worker's global object.
    @param worker The worker being created.
    @return No result, but method will leave with
            OpStatus::ERR_NO_MEMORY on OOM. */
/* static */ void
DOM_WebWorker::SetupGlobalScopeL(DOM_Prototype *worker_prototype, DOM_WebWorker *worker)
{
    worker_prototype->InitializeL();

    DOM_Runtime *runtime = worker_prototype->GetRuntime();
    DOM_BuiltInConstructor *constructor = OP_NEW(DOM_BuiltInConstructor, (worker_prototype));
    LEAVE_IF_ERROR(DOM_Object::DOMSetFunctionRuntime(constructor, runtime, "WorkerGlobalScope"));

    ES_Value value;
    DOM_Object::DOMSetObject(&value, constructor);
    worker_prototype->PutL("WorkerGlobalScope", value, PROP_DONT_ENUM);
    worker_prototype->PutL(worker->IsDedicated() ? "DedicatedWorkerGlobalScope" : "SharedWorkerGlobalScope", value, PROP_DONT_ENUM);
    worker_prototype->PutL("constructor", value, PROP_DONT_ENUM);

    DOM_Event::ConstructEventObjectL(*worker->PutConstructorL("Event", DOM_Runtime::EVENT_PROTOTYPE), runtime);
    worker->PutConstructorL("ErrorEvent", DOM_Runtime::ERROREVENT_PROTOTYPE);
    DOM_DOMException::ConstructDOMExceptionObjectL(*worker->PutConstructorL("DOMException", DOM_Runtime::DOMEXCEPTION_PROTOTYPE, TRUE), runtime);

    DOM_Object *object;
    worker->PutObjectL("DOMError", object = OP_NEW(DOM_Object, ()), "DOMError", PROP_DONT_ENUM);
    DOM_DOMError::ConstructDOMErrorObjectL(*object, runtime);

#ifdef DOM_HTTP_SUPPORT
    DOM_XMLHttpRequest_Constructor *xhr_constructor = OP_NEW_L(DOM_XMLHttpRequest_Constructor, ());
    worker->PutFunctionL("XMLHttpRequest", xhr_constructor, "XMLHttpRequest", NULL);
    DOM_XMLHttpRequest::ConstructXMLHttpRequestObjectL(*xhr_constructor, runtime);

    DOM_AnonXMLHttpRequest_Constructor *anon_xhr_constructor = OP_NEW_L(DOM_AnonXMLHttpRequest_Constructor, ());
    worker->PutFunctionL("AnonXMLHttpRequest", anon_xhr_constructor, "AnonXMLHttpRequest", NULL);
    worker->PutFunctionL("FormData", OP_NEW(DOM_FormData_Constructor, ()), "FormData", NULL);
# ifdef PROGRESS_EVENTS_SUPPORT
    worker->PutFunctionL("XMLHttpRequestUpload", OP_NEW(DOM_Object, ()), "XMLHttpRequestUpload");
# endif // PROGRESS_EVENTS_SUPPORT
#endif // DOM_HTTP_SUPPORT

#ifdef CANVAS_SUPPORT
    worker->PutConstructorL("ImageData", DOM_Runtime::CANVASIMAGEDATA_PROTOTYPE);
#endif // CANVAS_SUPPORT

    worker->PutFunctionL("FileReader", OP_NEW(DOM_FileReaderSync_Constructor, ()), "FileReader");
    DOM_FileError::ConstructFileErrorObjectL(*worker->PutConstructorL("FileError", DOM_Runtime::FILEERROR_PROTOTYPE), runtime);
    worker->PutFunctionL("FileReaderSync", OP_NEW(DOM_FileReaderSync_Constructor, ()), "FileReaderSync");
    DOM_FileException::ConstructFileExceptionObjectL(*worker->PutConstructorL("FileException", DOM_Runtime::FILEEXCEPTION_PROTOTYPE), runtime);
    worker->PutConstructorL("File", DOM_Runtime::FILE_PROTOTYPE);
    worker->PutFunctionL("Blob", OP_NEW(DOM_Blob_Constructor, ()), "Blob");

    // Console object. Mirror Window, and provide it inside Workers too.
    DOM_Object::DOMSetObjectRuntimeL(object = OP_NEW(JS_Console, ()), runtime, runtime->GetPrototype(DOM_Runtime::CONSOLE_PROTOTYPE), "Console");

    DOM_Object::DOMSetObject(&value, object);
    worker->PutL("console", value);

    DOM_Object::DOMSetObjectRuntimeL(object = OP_NEW(JS_Location_Worker, ()), runtime, runtime->GetPrototype(DOM_Runtime::WEBWORKERS_LOCATION_PROTOTYPE), "WorkerLocation");
    worker->PutPrivateL(DOM_PRIVATE_location, object);

    DOM_Object::DOMSetObjectRuntimeL(object = OP_NEW(JS_Navigator_Worker, ()), runtime, runtime->GetPrototype(DOM_Runtime::WEBWORKERS_NAVIGATOR_PROTOTYPE), "WorkerNavigator");
    worker->PutPrivateL(DOM_PRIVATE_navigator, object);

    worker->PutFunctionL("Worker", OP_NEW(DOM_DedicatedWorkerObject_Constructor,()),"Worker","s");
    worker->PutFunctionL("SharedWorker", OP_NEW(DOM_SharedWorkerObject_Constructor,()),"SharedWorker","ss-");

#ifdef DOM_CROSSDOCUMENT_MESSAGING_SUPPORT
    worker->PutFunctionL("MessageChannel",  OP_NEW(DOM_MessageChannel_Constructor,()),"MessageChannel", NULL);
    worker->PutFunctionL("MessageEvent", OP_NEW(DOM_MessageEvent_Constructor,()),"MessageEvent","-O");
    worker->PutConstructorL("MessagePort", DOM_Runtime::CROSSDOCUMENT_MESSAGEPORT_PROTOTYPE);
#endif // DOM_CROSSDOCUMENT_MESSAGING_SUPPORT

#ifdef EVENT_SOURCE_SUPPORT
    worker->PutFunctionL("EventSource", OP_NEW(DOM_EventSource_Constructor, ()), "EventSource", "s{withCredentials:b}-");
#endif // EVENT_SOURCE_SUPPORT

#ifdef PROGRESS_EVENTS_SUPPORT
    DOM_ProgressEvent_Constructor::AddConstructorL(worker);
#endif // PROGRESS_EVENTS_SUPPORT

    /* Known interface object omissions: ApplicationCache, EventTarget, Database, FileList. */

#ifdef WEBSOCKETS_SUPPORT
    if (g_pcdoc->GetIntegerPref(PrefsCollectionDoc::EnableWebSockets, runtime->GetOriginURL()))
    {
        DOM_WebSocket_Constructor::AddConstructorL(worker);
        DOM_CloseEvent_Constructor::AddConstructorL(worker);
    }
#endif // WEBSOCKETS_SUPPORT
}

/* static */ OP_STATUS
DOM_WebWorker::ConstructGlobalScope(BOOL is_dedicated, DOM_Runtime *runtime)
{
    DOM_WebWorker *worker;
    if (is_dedicated)
        worker = OP_NEW(DOM_DedicatedWorker, ());
    else
        worker = OP_NEW(DOM_SharedWorker, ());

    if (!worker)
        return OpStatus::ERR_NO_MEMORY;

    DOM_Prototype *worker_prototype;
    if (is_dedicated)
        worker_prototype = OP_NEW(DOM_DedicatedWorkerGlobalScope, ());
    else
        worker_prototype = OP_NEW(DOM_SharedWorkerGlobalScope, ());

    if (!worker_prototype)
    {
        OP_DELETE(worker);
        return OpStatus::ERR_NO_MEMORY;
    }

    RETURN_IF_ERROR(runtime->SetHostGlobalObject(worker, worker_prototype));
    runtime->GetEnvironment()->GetWorkerController()->SetWorkerObject(worker);

    RETURN_IF_LEAVE(DOM_WebWorker::SetupGlobalScopeL(worker_prototype, worker));
    return OpStatus::OK;
}

DOM_WebWorker::~DOM_WebWorker()
{
    /* If OOM hits before this (host) global object has been attached, nothing to do. */
    if (!GetRuntime())
        return;

    /* Drop the connection to any _remaining_ connected workers; when a controllern shuts down
     * this DOM_WebWorker, the connected DOM_WebWorkerObject that it has registered will be let
     * go of first & deleted, which will cause them to unregister with this worker. Hence, any
     * remaining ones are still alive and can be safely closed off here. */
    DetachConnectedWorkers();
    RemoveActiveLoaders();
    DropEntangledPorts();

    is_closed = TRUE;
    port = NULL;
    if (worker_name)
        OP_DELETEA(worker_name);

    while (DOM_EventListener *listener = static_cast<DOM_EventListener*>(delayed_listeners.First()))
    {
        listener->Out();
        DOM_EventListener::DecRefCount(listener);
    }

    processing_exceptions.Clear();

    DOM_Object::DOMFreeValue(loader_return_value);

    if (GetEnvironment()->GetWorkerController() && GetEnvironment()->GetWorkerController()->GetWorkerObject() == this)
        GetEnvironment()->GetWorkerController()->SetWorkerObject(NULL);
}

DOM_DedicatedWorkerObject*
DOM_WebWorker::GetDedicatedWorker()
{
    if (!is_dedicated || connected_workers.Empty())
        return NULL;
    else
        return static_cast<DOM_DedicatedWorkerObject*>(connected_workers.First());
}

/* static */ OP_STATUS
DOM_WebWorker::AddChildWorker(DOM_WebWorker *parent, DOM_WebWorker *w)
{
    AsListElement<DOM_WebWorker> *element = OP_NEW(AsListElement<DOM_WebWorker>, (w));
    if (!element)
        return OpStatus::ERR_NO_MEMORY;

    element->Into(&parent->child_workers);
    return OpStatus::OK;
}

/* static */ BOOL
DOM_WebWorker::RemoveChildWorker(DOM_WebWorker *parent, DOM_WebWorker *worker)
{
    for (AsListElement<DOM_WebWorker> *element = parent->child_workers.First(); element; element = element->Suc())
        if (element->ref == worker)
        {
            element->Out();
            OP_DELETE(element);
            return TRUE;
        }

    return FALSE;
}

OP_STATUS
DOM_WebWorker::PushActiveLoader(DOM_WebWorker_Loader *l)
{
    DOM_WebWorker_Loader_Element *element = OP_NEW(DOM_WebWorker_Loader_Element, (l));
    if (!element)
        return OpStatus::ERR_NO_MEMORY;

    element->IntoStart(&active_loaders);
    return OpStatus::OK;
}

void
DOM_WebWorker::PopActiveLoader()
{
    if (DOM_WebWorker_Loader_Element *element = active_loaders.First())
    {
        element->loader->Shutdown();
        element->Out();
        OP_DELETE(element);
    }
}

void
DOM_WebWorker::RemoveActiveLoaders()
{
    while (DOM_WebWorker_Loader_Element *element = active_loaders.First())
    {
        element->loader->Shutdown();
        element->Out();
        OP_DELETE(element);
    }
}

void
DOM_WebWorker::DOM_WebWorker_Loader_Element::GCTrace(DOM_Runtime *runtime)
{
    runtime->GCMark(loader);
}

OP_STATUS
DOM_WebWorker::SetParentWorker(DOM_WebWorker *p)
{
    OP_STATUS status = OpStatus::OK;
    if (p)
    {
        parent = p;
        /* ..and back again; maintain a connection from the parent (e.g., for termination requests.) */
        if (OpStatus::IsError(status = AddChildWorker(p, this)))
            /* Failure, reset attachment. */
            parent = NULL;
    }
    return status;
}

OP_STATUS
DOM_WebWorker::AddKeepAlive(DOM_Object *object, int *id)
{
    RETURN_IF_ERROR(PutPrivate(keep_alive_counter, *object));

    if (id)
        *id = keep_alive_counter;

    ++keep_alive_counter;
    return OpStatus::OK;
}

void
DOM_WebWorker::RemoveKeepAlive(int id)
{
    OpStatus::Ignore(DeletePrivate(id));
}

OP_STATUS
DOM_WebWorker::AddConnectedWorker(DOM_WebWorkerObject *worker)
{
    /* "There can only be one." Clear out previous. */
    if (is_dedicated)
        connected_workers.RemoveAll();

    worker->Into(&connected_workers);
    return OpStatus::OK;
}

void
DOM_WebWorker::ClearOtherWorker(DOM_WebWorkerObject *w)
{
    OP_ASSERT(w && connected_workers.HasLink(w));

    w->Out();
    w->ClearWorker();
}

/* static */ OP_STATUS
DOM_WebWorker::NewExecutionContext(DOM_Object *this_object, DOM_WebWorker *&inner, DOM_WebWorkerDomain *&worker_domain, DOM_WebWorker *parent, DOM_WebWorkerObject *worker_object, const URL &origin, const uni_char *name, ES_Value *return_value)
{
    OP_STATUS status;
    BOOL is_dedicated = (name == NULL);

    worker_domain = NULL;
    BOOL existing_worker = FALSE;

    inner = NULL;
    /* Create worker in some execution context/domain (worker_domain.) */
    RETURN_IF_ERROR(DOM_WebWorker::Construct(this_object, inner, worker_domain, existing_worker, origin, is_dedicated, name, return_value));
    RETURN_IF_ERROR(inner->AddConnectedWorker(worker_object));

    DOM_EnvironmentImpl *worker_exec_env = worker_domain->GetEnvironment();
    worker_exec_env->GetRuntime()->SetErrorHandler(inner);
    worker_exec_env->GetWorkerController()->SetWorkerObject(inner);

    /* For a shared worker, we simply share the running worker instance and won't reload any scripts or
     * perform other initialisation actions. The connection of a new 'user' of the worker is announced
     * to the worker instance by firing a 'connect' event.
     * (see the creation of the worker object in DOM_WebWorker::Make() for how this is done.)
     */
    if (existing_worker)
        return OpStatus::OK;

    inner->SetLocationURL(origin);
    inner->port = NULL;
    inner->is_dedicated = is_dedicated;
    if (!is_dedicated)
        inner->worker_name = UniSetNewStr(name);

    /* And the incoming parent for the (outer) instance. */
    RETURN_IF_ERROR(inner->SetParentWorker(parent));

    /* Create the port that resides inside the shared worker */
    if (!is_dedicated)
    {
        DOM_MessagePort *port = NULL;
        RETURN_IF_ERROR(DOM_MessagePort::Make(port, worker_exec_env->GetDOMRuntime()));
        inner->port = port;
    }

    /* Kick off the new worker and its instance by issuing a load/import of its script; followed by evaluation. */
    status = OpStatus::OK;
    URL *resolved_url = NULL;
    OpAutoVector<URL> *import_urls = NULL;

    /* Attach the Web Worker to the controlling domain + configure its toplevel scope/execution context. */
    RETURN_IF_ERROR(worker_domain->AddWebWorker(inner));

    resolved_url = OP_NEW(URL, ());
    if (!resolved_url)
    {
        status = OpStatus::ERR_NO_MEMORY;
        goto failed_import_script;
    }

    if (!DOM_WebWorker_Utils::CheckImportScriptURL(origin))
    {
        OP_DELETE(resolved_url);
        status = OpStatus::ERR;
        goto failed_import_script;
    }
    else
        *resolved_url = origin;

    import_urls = OP_NEW(OpAutoVector<URL>, ());
    if (!import_urls)
    {
        OP_DELETE(resolved_url);
        status = OpStatus::ERR_NO_MEMORY;
        goto failed_import_script;
    }

    if (OpStatus::IsError(status = import_urls->Add(resolved_url)))
    {
        OP_DELETE(resolved_url);
        OP_DELETE(import_urls);
        goto failed_import_script;
    }

    if (OpStatus::IsError(status = DOM_WebWorker_Loader::LoadScripts(this_object, inner, worker_object, import_urls, return_value, NULL/*interrupt_thread*/)))
    {
failed_import_script:
        inner->TerminateWorker();
        return status;
    }

    /* Enable the processing of listener registration -- the spec is in two minds here,
     * stating in one place (4.7.3, step 12/13) that message queues are enabled as the last step in
     * initialising a Worker object, while in another (4.5, step 8) stating that message queues
     * are only enabled after the script has finished processing its toplevel.
     *
     * => taking height for the situation that the spec may go either way, we keep around the notion
     *    of enabling a worker (cf. EnableWorker()), but turn on the enabled state right away here.
     *    If the spec goes with the other option, simply uncomment the next line.
     */
    inner->is_enabled = TRUE;
    return OpStatus::OK;
}

/* virtual */ void
DOM_WebWorker::GCTrace()
{
    DOM_Runtime *runtime = GetRuntime();
    if (port && runtime->IsSameHeap(port->GetRuntime()))
        runtime->GCMark(port);

    for (DOM_WebWorker_Loader_Element *loader = active_loaders.First(); loader; loader = loader->Suc())
        loader->GCTrace(runtime);

    GCMark(loader_return_value);

    for (AsListElement<DOM_WebWorker> *element = child_workers.First(); element; element = element->Suc())
        if (runtime->IsSameHeap(element->ref->GetRuntime()))
            runtime->GCMark(element->ref);

    for (DOM_MessagePort *p = entangled_ports.First(); p; p = p->Suc())
    {
        OP_ASSERT(runtime->IsSameHeap(p->GetRuntime()));
        runtime->GCMark(p);
    }

    for (AsListElement<DOM_ErrorEvent> *error_event = processing_exceptions.First();
         error_event;
         error_event = error_event->Suc())
        if (runtime->IsSameHeap(error_event->ref->GetRuntime()))
            runtime->GCMark(error_event->ref);

    for (DOM_EventListener *listener = static_cast<DOM_EventListener*>(delayed_listeners.First());
         listener;
         listener = static_cast<DOM_EventListener*>(listener->Suc()))
        listener->GCTrace(runtime);

    message_event_queue.GCTrace(runtime);
    error_event_queue.GCTrace(runtime);
    connect_event_queue.GCTrace(runtime);
    GCMark(FetchEventTarget());
}

/* static */ OP_STATUS
DOM_WebWorker::Construct(DOM_Object *this_object, DOM_WebWorker *&worker_instance, DOM_WebWorkerDomain *&domain_instance, BOOL &existing_worker, const URL &origin, BOOL is_dedicated, const uni_char *name, ES_Value *return_value)
{
    worker_instance = NULL;

    if (!g_webworkers->FindWebWorkerDomain(domain_instance, origin, name))
    {
        if (!g_webworkers->CanCreateWorker())
        {
            RETURN_IF_ERROR(DOM_WebWorker_Utils::ReportQuotaExceeded(this_object, FALSE, g_webworkers->GetMaxWorkersPerSession()));
            this_object->CallDOMException(QUOTA_EXCEEDED_ERR, return_value); // ignore ES_EXCEPTION;
            return OpStatus::ERR;
        }
        RETURN_IF_ERROR(DOM_WebWorkerDomain::Make(domain_instance, this_object, origin, is_dedicated, return_value));
        worker_instance = domain_instance->GetEnvironment()->GetWorkerController()->GetWorkerObject();
        g_webworkers->AddWorkerDomain(domain_instance);
    }

    if (is_dedicated)
    {
        worker_instance = domain_instance->GetEnvironment()->GetWorkerController()->GetWorkerObject();
        return OpStatus::OK;
    }

    if (DOM_WebWorker *shared_worker = domain_instance->FindSharedWorker(origin, name))
    {
        /* a shared worker instance's already active, attach to it. */
        worker_instance = shared_worker;
        DOM_EnvironmentImpl *owner = this_object->GetEnvironment();
        domain_instance = worker_instance->GetWorkerDomain();
        /* Register new shared worker connection/reuse with its domain + record it with the instatiating environment's
           controller, so that we can unregister upon closing down. */
        RETURN_IF_ERROR(domain_instance->AddDomainOwner(owner));
        RETURN_IF_ERROR(owner->GetWorkerController()->AddWebWorkerDomain(domain_instance));
        existing_worker = TRUE;
    }

    return (worker_instance ? OpStatus::OK : OpStatus::ERR);
}

OP_STATUS
DOM_WebWorker::InvokeErrorListeners(DOM_ErrorEvent *exception, BOOL propagate_if_not_handled)
{
    DOM_ErrorEvent *event = NULL;
    RETURN_IF_ERROR(DOM_ErrorException_Utils::CopyErrorEvent(this, event, exception, GetLocationURL(), propagate_if_not_handled));
    /* Sigh - we want onerror handlers _at the worker global scope_ to be of the
       same shape (i.e., taking three arguments) as window.onerror, so flag this
       error event as a window event to have the event delivery code do the
       necessary unbundling. We _only_ do this when firing the event at the object
       representing the worker inside its execution context. If not, it's handled
       like a normal ErrorEvent. */
    event->SetWindowEvent();
    return DeliverError(this, event);
}

/* virtual */ OP_STATUS
DOM_WebWorker::HandleException(DOM_ErrorEvent *exception)
{
    if (error_handler)
        /* Handle the error by firing a local error event; none is already in flight */
        return InvokeErrorListeners(exception, TRUE);
    else
        /* Either there are no local listeners or we are already processing an unhandled
           exception/error: how do we know if the exception happened as a result of
           processing it?

           We don't really (until we figure out how to inject an exception handler around
           'onerror' executions..), so all subsequent exceptions are treated as stemming
           from within these exception handlers. */
        return PropagateErrorException(exception);
}

OP_STATUS
DOM_WebWorker::ProcessingException(DOM_ErrorEvent *error_event)
{
    AsListElement<DOM_ErrorEvent> *element = OP_NEW(AsListElement<DOM_ErrorEvent>, (error_event));
    if (!element)
        return OpStatus::ERR_NO_MEMORY;

    element->Into(&processing_exceptions);
    return OpStatus::OK;
}

void
DOM_WebWorker::ProcessedException(DOM_ErrorEvent *exception)
{
    for (AsListElement<DOM_ErrorEvent> *element = processing_exceptions.First(); element; element = element->Suc())
    {
        DOM_ErrorEvent *error = element->ref;
        if (error == exception || exception->GetMessage() && error->GetMessage() && uni_str_eq(error->GetMessage(), exception->GetMessage()))
        {
            element->Out();
            OP_DELETE(element);
            return;
        }
    }
}

BOOL
DOM_WebWorker::IsProcessingException(const uni_char *message, unsigned line_number, const uni_char *url_str)
{
    OP_ASSERT(message && url_str);
    for (AsListElement<DOM_ErrorEvent> *element = processing_exceptions.First(); element; element = element->Suc())
    {
        DOM_ErrorEvent *error = element->ref;
        if (error->GetMessage() && uni_str_eq(error->GetMessage(), message) && error->GetResourceLineNumber() == line_number && uni_str_eq(error->GetResourceUrl(), url_str))
            return TRUE;
    }
    return FALSE;
}

#ifdef ECMASCRIPT_DEBUGGER
OP_STATUS
DOM_WebWorker::GetWindows(OpVector<Window> &windows)
{
    DOM_WebWorker *worker = this;

    /* If this is a child webworker, move all the way up the parent
       chain until we reach a worker instantiated by a real browsing
       context. */
    while (worker->parent)
        worker = worker->parent;

    if (worker->GetWorkerDomain())
        return worker->GetWorkerDomain()->GetWindows(windows);

    return OpStatus::OK;
}
#endif // ECMASCRIPT_DEBUGGER

/* virtual */ OP_STATUS
DOM_WebWorker::PropagateErrorException(DOM_ErrorEvent *exception)
{
    /* If this worker is shared, then the error should be reported to the user
       (Section 4.7, second para.) */
    if (!IsDedicated())
    {
        /* Reset exception handling flags; we're done. */
        ProcessedException(exception);
        return DOM_ErrorException_Utils::ReportErrorEvent(exception, GetLocationURL(), GetEnvironment());
    }
    else
    {
        /* Check if parent is receptive.. */
        DOM_WebWorker* parent_worker = GetParentWorker();
        if (parent_worker && !parent_worker->IsDedicated())
        {
            /* Parent is shared, report to user (Section 4.7, 4th para.) */
            ProcessedException(exception);
            return DOM_ErrorException_Utils::ReportErrorEvent(exception, parent_worker->GetLocationURL(), parent_worker->GetEnvironment());
        }
        else if (parent_worker)
        {
            /* Hand off the exception to the parent. */
            ProcessedException(exception);
            return parent_worker->InvokeErrorListeners(exception, TRUE);
        }
        else if (GetDedicatedWorker() && GetDedicatedWorker()->GetEventTarget() && GetDedicatedWorker()->GetEventTarget()->HasListeners(ONERROR, UNI_L("error"), ES_PHASE_ANY))
        {
            /* Fire the event at Worker object on the other side */
            ProcessedException(exception);
            return GetDedicatedWorker()->InvokeErrorListeners(exception, TRUE);
        }
        else
        {  /* All else failed (for dedicated worker.); report to user...but also
              queue it up for delivery at both worker sites. Either may have
              an .onerror listener registration queued up, so be helpful and
              notify them of prev. error when/if they come online. */
            OpStatus::Ignore(InvokeErrorListeners(exception, FALSE));
            if (GetDedicatedWorker())
                OpStatus::Ignore(GetDedicatedWorker()->InvokeErrorListeners(exception, FALSE));
            ProcessedException(exception);
            return DOM_ErrorException_Utils::ReportErrorEvent(exception, GetLocationURL(), GetEnvironment());
        }
    }
}

/* virtual */ OP_STATUS
DOM_WebWorker::HandleError(ES_Runtime *runtime, const ES_Runtime::ErrorHandler::ErrorInformation *information, const uni_char *message, const uni_char *url, const ES_SourcePosition &position, const ES_ErrorData *data)
{
    unsigned line_number = position.GetAbsoluteLine();

    if (!url)
        url = UNI_L("");
    if (information->type == ES_Runtime::ErrorHandler::RUNTIME_ERROR && !IsProcessingException(message, line_number, url))
    {
        DOM_ErrorEvent *event = NULL;

        RETURN_IF_ERROR(DOM_ErrorException_Utils::BuildErrorEvent(this, event, url, message, line_number, TRUE/*propagate, if needs be*/));
        RETURN_IF_ERROR(ProcessingException(event));
        RETURN_IF_ERROR(HandleException(event));
    }

    if (data)
        runtime->IgnoreError(data);

    return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_WebWorker::HandleWorkerError(const ES_Value &exception)
{
    /* This is the exception case of ES_AsyncCallback::HandleCallback() */
    ES_Value message_value;

    unsigned line_number = 1;
    const uni_char *msg = UNI_L("");

    /* Extrapolate the arguments required by HandleException() */
    if (exception.type == VALUE_STRING)
        msg = exception.value.string;
    else if (exception.type == VALUE_OBJECT)
    {
        DOM_Object *host_object = DOM_HOSTOBJECT(exception.value.object, DOM_Object);
        if (host_object && host_object->IsA(DOM_TYPE_ERROREVENT))
        {
            DOM_ErrorEvent *error_event = static_cast<DOM_ErrorEvent*>(host_object);
            msg = error_event->GetMessage();
            line_number = error_event->GetResourceLineNumber();
        }
        else if (host_object)
        {
            ES_Runtime *runtime = host_object->GetEnvironment()->GetRuntime();
            /* Arguably trying way too hard... Q: is there a DOM_TYPE for DOMExceptions? */
            if (OpStatus::IsSuccess(host_object->GetName(OP_ATOM_message, &message_value, runtime)) && message_value.type == VALUE_STRING)
                    msg = message_value.value.string;
            else if (OpStatus::IsSuccess(host_object->Get(UNI_L("message"), &message_value)) && message_value.type == VALUE_STRING)
                    msg = message_value.value.string;
            if (OpStatus::IsSuccess(host_object->GetName(OP_ATOM_lineno, &message_value, runtime)) && message_value.type == VALUE_NUMBER)
                    line_number = static_cast<unsigned>(message_value.value.number);
            else if (OpStatus::IsSuccess(host_object->Get(UNI_L("lineno"), &message_value)) && message_value.type == VALUE_NUMBER)
                    line_number = static_cast<unsigned>(message_value.value.number);
            else if (OpStatus::IsSuccess(host_object->Get(UNI_L("code"), &message_value)) && message_value.type == VALUE_NUMBER)
                    line_number = static_cast<unsigned>(message_value.value.number);
        }
        else
        {
            ES_Object *error_object = exception.value.object;
            ES_Runtime *runtime = GetRuntime(); /* For Carakan friendliness; may elicit a warning about non-ref'ed variable w/ Futhark. */
            if (OpStatus::IsSuccess(runtime->GetName(error_object, UNI_L("message"), &message_value)) && message_value.type == VALUE_STRING)
                msg = message_value.value.string;
            if (OpStatus::IsSuccess(runtime->GetName(error_object, UNI_L("lineno"), &message_value)) &&  message_value.type == VALUE_NUMBER)
                line_number = static_cast<unsigned>(message_value.value.number);
        }
    }

    DOM_ErrorEvent *event = NULL;
    OpString url_str;
    RETURN_IF_ERROR(GetLocationURL().GetAttribute(URL::KUniName_With_Fragment, url_str));
    RETURN_IF_ERROR(DOM_ErrorException_Utils::BuildErrorEvent(this, event, url_str.CStr(), msg, line_number, TRUE/*propagate, if needs be*/));
    return HandleException(event);
}

/* virtual */ OP_STATUS
DOM_WebWorker::HandleCallback(ES_AsyncOperation operation, ES_AsyncStatus status, const ES_Value &result)
{
    if (DOM_WebWorker_Loader_Element *element = active_loaders.First())
        OpStatus::Ignore(element->loader->HandleCallback(status, result));

    switch(status)
    {
    case ES_ASYNC_SUCCESS:
        /* If we've reached the end of evaluation and "enable" the worker (=> start message reception.) */
        OpStatus::Ignore(EnableWorker());
        return OpStatus::OK;
    case ES_ASYNC_EXCEPTION:
        return HandleWorkerError(result/*this is the exception object*/);

    /* 'result' is of an undefined nature for these; propagate an internal error event for now. */
    case ES_ASYNC_FAILURE:
    case ES_ASYNC_NO_MEMORY:
    {
        DOM_ErrorEvent *event = NULL;
        const uni_char *msg;
        if (status == ES_ASYNC_FAILURE)
            msg = UNI_L("Internal error");
        else
            msg = UNI_L("Out of memory");
        unsigned line_number = 0;
        OpString url_str;
        RETURN_IF_ERROR(GetLocationURL().GetAttribute(URL::KUniName_With_Fragment, url_str));
        RETURN_IF_ERROR(DOM_ErrorException_Utils::BuildErrorEvent(this, event, url_str.CStr(), msg, line_number, TRUE/*propagate, if needs be*/));
        return HandleException(event);
    }

    case ES_ASYNC_CANCELLED:
    default:
        return OpStatus::OK;
    }
}

static BOOL
IsEventHandlerFor(DOM_EventTarget *target, DOM_EventListener *listener, DOM_EventType event_type, const uni_char *event_name)
{
    ES_Object *handler;
    return (target->HasListeners(event_type, event_name, ES_PHASE_ANY) &&
            target->FindOldStyleHandler(event_type, &handler) == OpBoolean::IS_TRUE &&
            listener->GetNativeHandler() == handler);
}

void
DOM_WebWorker::UpdateEventHandlers(DOM_EventListener *l)
{
    DOM_EventTarget *target = GetEventTarget();
    /* Did we just add an .onXXX handler, and didn't have one before? */
    if (!error_handler && IsEventHandlerFor(target, l, ONERROR, UNI_L("error")))
        error_handler = l;
    if (!message_handler && IsEventHandlerFor(target, l, ONMESSAGE, UNI_L("message")))
        message_handler = l;
    if (!connect_handler && IsEventHandlerFor(target, l, ONCONNECT, UNI_L("connect")))
        connect_handler = l;
}

/* Terminate/close distinction - the former is from the outside, the latter triggered by explicit Worker call to "close()" */

void
DOM_WebWorker::CloseChildren()
{
    /* Summarily inform the children */
    while (AsListElement<DOM_WebWorker> *element = child_workers.First())
         element->ref->TerminateWorker();

    child_workers.Clear();
}

/* virtual */ OP_STATUS
DOM_WebWorker::DrainEventQueues()
{
    return DOM_WebWorkerBase::DrainEventQueues(this);
}

/* virtual */ OP_STATUS
DOM_SharedWorker::DrainEventQueues()
{
    if (!connect_event_queue.HaveDrainedEvents() && GetEventTarget() && GetEventTarget()->HasListeners(ONCONNECT, UNI_L("connect"), ES_PHASE_ANY))
        OpStatus::Ignore(connect_event_queue.DrainEventQueue(GetEnvironment()));
    return DOM_WebWorkerBase::DrainEventQueues(this);
}

/* virtual */ ES_PutState
DOM_SharedWorker::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
    switch(property_name)
    {
    case OP_ATOM_applicationCache:
    case OP_ATOM_location:
    case OP_ATOM_name:
    case OP_ATOM_self:
        return PUT_SUCCESS;

    case OP_ATOM_onconnect:
        PUT_FAILED_IF_ERROR(DOM_Object::CreateEventTarget());
        return DOM_CrossMessage_Utils::PutEventHandler(this, UNI_L("connect"), ONCONNECT, connect_handler, &connect_event_queue, value, origining_runtime);
    default:
        return DOM_WebWorker::PutName(property_name, value, origining_runtime);
    }
}

/* virtual */ ES_PutState
DOM_DedicatedWorker::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
    switch(property_name)
    {
    case OP_ATOM_location:
    case OP_ATOM_self:
        return PUT_SUCCESS;
    case OP_ATOM_onmessage:
        PUT_FAILED_IF_ERROR(DOM_Object::CreateEventTarget());
        return DOM_CrossMessage_Utils::PutEventHandler(this, UNI_L("message"), ONMESSAGE, message_handler, &message_event_queue, value, origining_runtime);
    case OP_ATOM_onerror:
        PUT_FAILED_IF_ERROR(DOM_Object::CreateEventTarget());
        return DOM_CrossMessage_Utils::PutEventHandler(this, UNI_L("error"), ONERROR, error_handler, &error_event_queue, value, origining_runtime);
    default:
        return DOM_WebWorker::PutName(property_name, value, origining_runtime);
    }
}

/* virtual */ ES_PutState
DOM_WebWorker::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
    switch(property_name)
    {
    case OP_ATOM_onerror:
        PUT_FAILED_IF_ERROR(DOM_Object::CreateEventTarget());
        return DOM_CrossMessage_Utils::PutEventHandler(this, UNI_L("error"), ONERROR, error_handler, &error_event_queue, value, origining_runtime);
    default:
        return DOM_Object::PutName(property_name, value, origining_runtime);
    }
}

OP_STATUS
DOM_WebWorker::DeliverMessage(DOM_MessageEvent *message_event, BOOL is_message)
{
    if (is_message)
        RETURN_IF_ERROR(message_event_queue.DeliverEvent(message_event, GetEnvironment()));
    else
        RETURN_IF_ERROR(connect_event_queue.DeliverEvent(message_event, GetEnvironment()));

    /* Somewhat unfortunate place, but do check that we haven't got a
     * a listener set up after all.. this can happen behind our back for
     * toplevel assignments to "onmessage = ..." for dedicated workers.
     */
    return DrainEventQueues();
}

OP_STATUS
DOM_WebWorker::RegisterConnection(DOM_MessagePort *port_out)
{
    DOM_MessageEvent *connect_event;
    RETURN_IF_ERROR(DOM_MessageEvent::MakeConnect(connect_event, TRUE, GetLocationURL(), this, port_out, TRUE/* with .ports alias */, this));

    AddEntangledPort(port_out->GetTarget());
    return DeliverMessage(connect_event, FALSE);
}

void
DOM_WebWorker::TerminateWorker()
{
    /* Perform termination of a Worker and its execution environment; extends to its progeny */
    error_handler = NULL;
    message_handler = NULL;
    connect_handler = NULL;
    CloseWorker();
}

void
DOM_WebWorker::DetachConnectedWorkers()
{
    for (DOM_WebWorkerObject *worker = connected_workers.First(); worker; worker = worker->Suc())
         worker->ClearWorker();

    connected_workers.RemoveAll();
}

void
DOM_WebWorker::CloseWorker()
{
    is_closed = TRUE;
    /* Note: this includes handling downward termination notifications to children */
    CloseChildren();
    if (parent)
        RemoveChildWorker(parent, this);

    DetachConnectedWorkers();
    DropEntangledPorts();
    RemoveActiveLoaders();

    if (domain)
        domain->RemoveWebWorker(this);

    domain = NULL;
}

/* virtual */ OP_STATUS
DOM_WebWorker::EnableWorker()
{
    if (is_closed || is_enabled)
        return OpStatus::OK;
    is_enabled = TRUE;

    RETURN_IF_ERROR(DOM_Object::CreateEventTarget());
    while (DOM_EventListener *listener = static_cast<DOM_EventListener*>(delayed_listeners.First()))
    {
        listener->Out();
        GetEventTarget()->AddListener(listener);
        UpdateEventHandlers(listener);
    }

    OpStatus::Ignore(connect_event_queue.DrainEventQueue(GetEnvironment()));
    OpStatus::Ignore(message_event_queue.DrainEventQueue(GetEnvironment()));
    OpStatus::Ignore(error_event_queue.DrainEventQueue(GetEnvironment()));
    return OpStatus::OK;
}

/* static */ int
DOM_WebWorker::close(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
    DOM_THIS_OBJECT(worker, DOM_TYPE_WEBWORKERS_WORKER, DOM_WebWorker);

    worker->is_closed = TRUE;
    /* Summarily inform the children - a close is a termination event for them. */
    worker->CloseChildren();
    return ES_FAILED;
}

OP_STATUS
DOM_WebWorker::SetLoaderReturnValue(const ES_Value &value)
{
    DOM_Object::DOMFreeValue(loader_return_value);
    return DOM_Object::DOMCopyValue(loader_return_value, value);
}

/* static */ int
DOM_WebWorker::importScripts(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
    DOM_THIS_OBJECT(worker, DOM_TYPE_WEBWORKERS_WORKER, DOM_WebWorker);

    if (argc < 0)
    {
        CALL_FAILED_IF_ERROR(DOM_Object::DOMCopyValue(*return_value, worker->loader_return_value));

        int result = (worker->loader_return_value.type == VALUE_UNDEFINED ? ES_FAILED : ES_EXCEPTION);

        DOM_Object::DOMFreeValue(worker->loader_return_value);
        DOM_Object::DOMSetUndefined(&worker->loader_return_value);
        worker->PopActiveLoader();

        return result;
    }
    else if (argc == 0)
        return ES_FAILED;
    else
    {
        DOM_CHECK_ARGUMENTS("s");
        URL base_url = worker->GetLocationURL();
        OP_STATUS status;
        OpAutoVector<URL> *import_urls = OP_NEW(OpAutoVector<URL>, ());
        if (!import_urls)
            return ES_FAILED;
        for (int i = 0 ; i < argc; i++)
        {
            URL *resolved_url = OP_NEW(URL, ());
            if (!resolved_url)
                return ES_FAILED;
            if (OpStatus::IsError(status = import_urls->Add(resolved_url)))
            {
                OP_DELETE(resolved_url);
                OP_DELETE(import_urls);
                CALL_FAILED_IF_ERROR(status);
            }

            BOOL is_import_url_ok = FALSE;
            BOOL type_mismatch = argv[i].type != VALUE_STRING;
            if (!type_mismatch)
            {
                URL effective_url = g_url_api->GetURL(base_url, argv[i].value.string);
                is_import_url_ok = DOM_WebWorker_Utils::CheckImportScriptURL(effective_url);
                if (is_import_url_ok)
                    *resolved_url = effective_url;
            }
            if (type_mismatch || !is_import_url_ok)
            {
                OpString error_message;
                const uni_char *error_header = (type_mismatch  ? UNI_L("Expecting string argument") :
                                               (!is_import_url_ok ? UNI_L("Security error importing script: ") :
                                                                    UNI_L("Unable to import script: ")));

                if (OpStatus::IsError(status = error_message.Append(error_header)) ||
                    !type_mismatch && OpStatus::IsError(status = error_message.Append(argv[i].value.string)))
                {
                    OP_DELETE(import_urls);
                    CALL_FAILED_IF_ERROR(status);
                }

               OpString url_str;
               CALL_FAILED_IF_ERROR(worker->GetLocationURL().GetAttribute(URL::KUniName_With_Fragment, url_str));

               DOM_ErrorEvent *error_event = NULL;
               CALL_FAILED_IF_ERROR(DOM_ErrorException_Utils::BuildErrorEvent(worker, error_event, url_str.CStr(), error_message.CStr(), 0, FALSE));
               DOM_Object::DOMSetObject(return_value, error_event);
               OP_DELETE(import_urls);
               return ES_EXCEPTION;
            }
        }

        ES_Thread *interrupt_thread = origining_runtime->GetESScheduler()->GetCurrentThread();
        if (OpStatus::IsError(DOM_WebWorker_Loader::LoadScripts(this_object, worker, NULL, import_urls, return_value, interrupt_thread)))
        {
            const uni_char *header = UNI_L("Unable to import script");
            OpString url_str;
            if (worker)
                CALL_FAILED_IF_ERROR(worker->GetLocationURL().GetAttribute(URL::KUniName_With_Fragment, url_str));

            DOM_ErrorEvent *error_event = NULL;
            CALL_FAILED_IF_ERROR(DOM_ErrorException_Utils::BuildErrorEvent(worker, error_event, url_str.CStr(), header, 0, FALSE));
            DOM_Object::DOMSetObject(return_value, error_event);
            return ES_EXCEPTION;
        }
        return (ES_RESTART | ES_SUSPEND);
    }
}

/* virtual */ ES_GetState
DOM_DedicatedWorker::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
    switch(property_name)
    {
    case OP_ATOM_onmessage:
        DOMSetObject(value, message_handler);
        return GET_SUCCESS;
    default:
        return DOM_WebWorker::GetName(property_name, value, origining_runtime);
    }
}

/* virtual */ ES_GetState
DOM_SharedWorker::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
    switch(property_name)
    {
    case OP_ATOM_name:
        OP_ASSERT(GetWorkerName());
        DOMSetString(value, GetWorkerName());
        return GET_SUCCESS;
    case OP_ATOM_applicationCache:
        // FIXME: Once AppCache is integrated with Web Workers; integrate and hook up here.
        DOMSetNull(value);
        return GET_SUCCESS;
    case OP_ATOM_onconnect:
        DOMSetObject(value, connect_handler);
        return GET_SUCCESS;
    default:
        return DOM_WebWorker::GetName(property_name, value, origining_runtime);
    }
}

/* virtual */ ES_GetState
DOM_WebWorker::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
    switch (property_name)
    {
    case OP_ATOM_self:
        DOMSetObject(value, this);
        return GET_SUCCESS;
    case OP_ATOM_location:
        return DOMSetPrivate(value, DOM_PRIVATE_location);
    case OP_ATOM_navigator:
        return DOMSetPrivate(value, DOM_PRIVATE_navigator);

    case OP_ATOM_onerror:
        /* Implement the HTML5-prescribed event target behaviour of giving back a 'null' rather
           than 'undefined' for targets without any listeners. We don't yet have a consistent story
           throughout dom/ for these event targets, hence local "hack". */
         DOMSetObject(value, error_handler);
        return GET_SUCCESS;
    default:
        return DOM_Object::GetName(property_name, value, origining_runtime);
    }
}

/* static */ int
DOM_DedicatedWorker::postMessage(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
    DOM_THIS_OBJECT(worker, DOM_TYPE_WEBWORKERS_WORKER_DEDICATED, DOM_DedicatedWorker);
    if (argc < 0)
        return ES_FAILED;
    else if (argc == 0)
        return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);
    else
    {
        DOM_WebWorkerObject *target_worker = worker->GetDedicatedWorker();
        if (!target_worker)
            return ES_FAILED;

        if (worker->IsClosed())
        {
            OpString url_str;
            CALL_FAILED_IF_ERROR(worker->GetLocationURL().GetAttribute(URL::KUniName_With_Fragment, url_str));

            DOM_ErrorEvent *error_event = NULL;
            const uni_char* exception_string = UNI_L("postMessage() on closed worker");
            CALL_FAILED_IF_ERROR(DOM_ErrorException_Utils::BuildErrorEvent(worker, error_event, url_str.CStr(), exception_string, 0, FALSE));
            DOM_Object::DOMSetObject(return_value, error_event);
            return ES_EXCEPTION;
        }

        DOMSetNull(return_value);
        URL url = worker->GetLocationURL();

        ES_Value message_argv[2];
        message_argv[0] = argv[0];
        if (argc == 1)
            DOMSetNull(&message_argv[1]);
        else
            message_argv[1] = argv[1];

        ES_Value message_event_value;
        DOM_MessageEvent *message_event;
        int result;
        if ((result = DOM_MessageEvent::Create(message_event, worker, target_worker->GetEnvironment()->GetDOMRuntime(), NULL, NULL, url, message_argv, ARRAY_SIZE(message_argv), &message_event_value)) != ES_FAILED)
        {
            if (return_value)
                *return_value = message_event_value;
            return result;
        }

         message_event->SetTarget(target_worker);
         message_event->SetSynthetic();
         CALL_FAILED_IF_ERROR(target_worker->DeliverMessage(message_event));
         /* Have postMessage() suspend and restart as a way of giving the receiving
            context a chance to service the message right away. Not doing so leads
            to worker code that pumps out messages to run full-on for their timeslice,
            clogging up the system with messages that the listeners on the other
            side won't be able to drain. Laggy code and increased memory usage
            is the result. Hence, we force a context-switch here. It'll all be
            different when there are multiple threads of control running...

            (same comment re: restarting applies to DOM_DedicatedWorker::postMessage()) */
         return (ES_RESTART | ES_SUSPEND);
    }
}

/* static */ int
DOM_WebWorker::accessEventListener(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
    DOM_THIS_OBJECT(worker, DOM_TYPE_WEBWORKERS_WORKER, DOM_WebWorker);
    OP_ASSERT(origining_runtime == worker->GetEnvironment()->GetRuntime());
    int status = DOM_Node::accessEventListener(this_object, argv, argc, return_value, origining_runtime, data);
    /* Check if adding a listener on the Worker object may allow us to
       drain queued-up events. */
    if (argc >= 1 && (data == 0 || data == 2))
        OpStatus::Ignore(worker->DrainEventQueues());
    return status;
}

FramesDocument *
DOM_WebWorker::GetWorkerOwnerDocument()
{
    return DOM_WebWorker_Utils::GetWorkerFramesDocument(this, NULL);
}

#ifdef DOM_WEBWORKERS_WORKER_STAT_SUPPORT
OP_STATUS
DOM_WebWorker::GetStatistics(DOM_WebWorkerInfo *&info)
{
    info->is_dedicated = is_dedicated;
    info->worker_name = worker_name;
    if (GetWorkerDomain())
        info->worker_url = &GetOriginURL();
    info->ports_active = entangled_ports.Cardinal();

    if (GetDedicatedWorker())
    {
        DOM_WebWorkerInfo *other_wwi = OP_NEW(DOM_WebWorkerInfo, ());
        if (!other_wwi)
        {
            OP_DELETE(info);
            info = NULL;
            return OpStatus::ERR_NO_MEMORY;
        }
        other_wwi->is_dedicated = is_dedicated;
        other_wwi->worker_name = GetWorkerName();
        other_wwi->ports_active = 0;
        other_wwi->worker_url = info->worker_url;
        other_wwi->Into(&info->clients);
    }
    else if (!connected_workers.Empty())
    {
        DOM_WebWorkerInfo *other_wwi = OP_NEW(DOM_WebWorkerInfo, ());
        if (!other_wwi)
            return OpStatus::ERR_NO_MEMORY;
        other_wwi->Into(&info->clients);

        for (DOM_WebWorkerObject *worker = connected_workers.First(); worker; worker = worker->Suc())
        {
            if (worker->GetWorker())
            {
                other_wwi->is_dedicated = IsDedicated();
                other_wwi->worker_name = GetWorkerName();
                other_wwi->ports_active = 0;
                other_wwi->worker_url = &worker->GetOriginURL();
            }
            if (worker->Suc())
            {
                DOM_WebWorkerInfo *nxt = OP_NEW(DOM_WebWorkerInfo, ());
                if (!nxt)
                {
                    OP_DELETE(info);
                    info = NULL;
                    return OpStatus::ERR_NO_MEMORY;
                }
                nxt->Follow(other_wwi);
            }
        }
    }
    if (!child_workers.Empty())
    {
        DOM_WebWorkerInfo *child = OP_NEW(DOM_WebWorkerInfo, ());
        if (!child)
        {
            OP_DELETE(info);
            info = NULL;
            return OpStatus::ERR_NO_MEMORY;
        }

        DOM_WebWorkerInfo *current_wi = child;
        child->Into(&info->children);
        OP_STATUS status;
        for (AsListElement<DOM_WebWorker> *element = child_workers.First(); element; element = element->Suc())
        {
            DOM_WebWorker *w = element->ref;
            if (OpStatus::IsError(status = w->GetStatistics(current_wi)))
            {
                OP_DELETE(info);
                info = NULL;
                return status;
            }
            if (element->Suc())
            {
                DOM_WebWorkerInfo *nxt = OP_NEW(DOM_WebWorkerInfo, ());
                if (!nxt)
                {
                    OP_DELETE(info);
                    info = NULL;
                    return OpStatus::ERR_NO_MEMORY;
                }
                nxt->Follow(current_wi);
                current_wi = nxt;
            }
        }
    }
    return OpStatus::OK;
}
#endif // DOM_WEBWORKERS_WORKER_STAT_SUPPORT

#include "modules/dom/src/domglobaldata.h"

/* Global functions / toplevel methods on the dedicated Worker global scope object: */
DOM_FUNCTIONS_START(DOM_DedicatedWorkerGlobalScope)
    DOM_FUNCTIONS_FUNCTION(DOM_DedicatedWorkerGlobalScope, DOM_DedicatedWorker::postMessage, "postMessage", NULL)
    DOM_FUNCTIONS_FUNCTION(DOM_DedicatedWorkerGlobalScope, DOM_WebWorker::close, "close", NULL)
    DOM_FUNCTIONS_FUNCTION(DOM_DedicatedWorkerGlobalScope, DOM_WebWorker::importScripts, "importScripts", "s-")
    DOM_FUNCTIONS_FUNCTION(DOM_DedicatedWorkerGlobalScope, DOM_Node::dispatchEvent, "dispatchEvent", NULL)
#ifdef URL_UPLOAD_BASE64_SUPPORT
    DOM_FUNCTIONS_FUNCTION(DOM_DedicatedWorkerGlobalScope, DOM_Base64::atob, "atob", "Z-")
    DOM_FUNCTIONS_FUNCTION(DOM_DedicatedWorkerGlobalScope, DOM_Base64::btoa, "btoa", "Z-")
#endif // URL_UPLOAD_BASE64_SUPPORT
DOM_FUNCTIONS_END(DOM_DedicatedWorkerGlobalScope)

DOM_FUNCTIONS_WITH_DATA_START(DOM_DedicatedWorkerGlobalScope)
    DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_DedicatedWorkerGlobalScope, JS_Window::setIntervalOrTimeout, 0, "setTimeout", "-n-")
    DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_DedicatedWorkerGlobalScope, JS_Window::setIntervalOrTimeout, 1, "setInterval", "-n-")
    DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_DedicatedWorkerGlobalScope, JS_Window::clearIntervalOrTimeout, 0, "clearInterval", "n-")
    DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_DedicatedWorkerGlobalScope, JS_Window::clearIntervalOrTimeout, 1, "clearTimeout", "n-")
    DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_DedicatedWorkerGlobalScope, DOM_WebWorker::accessEventListener, 0, "addEventListener", "s-b-")
    DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_DedicatedWorkerGlobalScope, DOM_WebWorker::accessEventListener, 1, "removeEventListener", "s-b-")
DOM_FUNCTIONS_WITH_DATA_END(DOM_DedicatedWorkerGlobalScope)

/* Global functions / toplevel methods on the shared Worker global scope object: */
DOM_FUNCTIONS_START(DOM_SharedWorkerGlobalScope)
    DOM_FUNCTIONS_FUNCTION(DOM_SharedWorkerGlobalScope, DOM_WebWorker::close, "close", NULL)
    DOM_FUNCTIONS_FUNCTION(DOM_SharedWorkerGlobalScope, DOM_WebWorker::importScripts, "importScripts", "s-")
    DOM_FUNCTIONS_FUNCTION(DOM_SharedWorkerGlobalScope, DOM_Node::dispatchEvent, "dispatchEvent", NULL)
#ifdef URL_UPLOAD_BASE64_SUPPORT
    DOM_FUNCTIONS_FUNCTION(DOM_SharedWorkerGlobalScope, DOM_Base64::atob, "atob", "Z-")
    DOM_FUNCTIONS_FUNCTION(DOM_SharedWorkerGlobalScope, DOM_Base64::btoa, "btoa", "Z-")
#endif // URL_UPLOAD_BASE64_SUPPORT
DOM_FUNCTIONS_END(DOM_SharedWorkerGlobalScope)

DOM_FUNCTIONS_WITH_DATA_START(DOM_SharedWorkerGlobalScope)
    DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_SharedWorkerGlobalScope, JS_Window::setIntervalOrTimeout, 0, "setTimeout", "-n-")
    DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_SharedWorkerGlobalScope, JS_Window::setIntervalOrTimeout, 1, "setInterval", "-n-")
    DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_SharedWorkerGlobalScope, JS_Window::clearIntervalOrTimeout, 0, "clearInterval", "n-")
    DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_SharedWorkerGlobalScope, JS_Window::clearIntervalOrTimeout, 1, "clearTimeout", "n-")
    DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_SharedWorkerGlobalScope, DOM_WebWorker::accessEventListener, 0, "addEventListener", "s-b-")
    DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_SharedWorkerGlobalScope, DOM_WebWorker::accessEventListener, 1, "removeEventListener", "s-b-")
DOM_FUNCTIONS_WITH_DATA_END(DOM_SharedWorkerGlobalScope)

#endif // DOM_WEBWORKERS_SUPPORT
