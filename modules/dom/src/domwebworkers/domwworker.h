/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 2009-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef DOM_WW_WORKER_H
#define DOM_WW_WORKER_H

#ifdef DOM_WEBWORKERS_SUPPORT

#include "modules/dom/src/domobj.h"
#include "modules/dom/domtypes.h"
#include "modules/dom/src/domruntime.h"
#include "modules/dom/src/domevents/domevent.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domevents/domevent.h"
#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/ecmascript_utils/esenvironment.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/doc/frm_doc.h"

#include "modules/dom/src/domwebworkers/domwweventqueue.h"

class DOM_WebWorker_Loader;
class DOM_DedicatedWorkerObject;

class DOM_DedicatedWorkerGlobalScope
    : public DOM_Prototype
{
public:
    DOM_DedicatedWorkerGlobalScope();

    virtual void GCTrace();

#ifdef URL_UPLOAD_BASE64_SUPPORT
    enum { FUNCTIONS_ARRAY_SIZE = 8 };
#else
    enum { FUNCTIONS_ARRAY_SIZE = 6 };
#endif // URL_UPLOAD_BASE64_SUPPORT

    enum { FUNCTIONS_WITH_DATA_ARRAY_SIZE = 7 };
};

class DOM_SharedWorkerGlobalScope
    : public DOM_Prototype
{
public:
    DOM_SharedWorkerGlobalScope();

    virtual void GCTrace();

#ifdef URL_UPLOAD_BASE64_SUPPORT
    enum { FUNCTIONS_ARRAY_SIZE = 7 };
#else
    enum { FUNCTIONS_ARRAY_SIZE = 5 };
#endif // URL_UPLOAD_BASE64_SUPPORT

    enum { FUNCTIONS_WITH_DATA_ARRAY_SIZE = 7 };
};

/** The DOM_WebWorker represents an active web worker and its execution context.

    It handles the execution of the worker, providing the scope and APIs that
    the Web Worker code expects. Additionally, it handles interaction with the
    outside world and the objects (DOM_WebWorkerObject) that are connected
    to this worker. */
class DOM_WebWorker
    : public DOM_Object,
      public DOM_WebWorkerBase,
      public DOM_EventTargetOwner,
      public ES_Runtime::ErrorHandler,
      public ES_AsyncCallback
{
protected:
    DOM_WebWorker(BOOL is_dedicated);

    List<DOM_WebWorkerObject> connected_workers;
    /**< Web Workers have two instantiations - one residing within its
         execution context, the other in the browser context that begat it.
         This holds true for both objects for a dedicated worker and shared workers.

         DOM_WebWorker is the representation within the execution context, and
         it keeps track of the foreign DOM_WebWorkerObjects connected to it.
         For a dedicated worker, there's only one, for shared workers, a list.

         The tracking of external connections is required to handle propagation
         of errors and do clean shutdowns. */

    DOM_WebWorker *parent;
    /**< Reference to the parent web worker instance. Non-NULL if Worker
         was instantiated from within a Worker execution context. */

    List<AsListElement<DOM_WebWorker> > child_workers;
    /**< The list of workers that we've created and control. */

    DOM_WebWorkerDomain *domain;
    /**< The domain that this worker belongs to. */

    const uni_char *worker_name;
    /**< Shared Workers have a name along with a base URL. */

    DOM_MessagePort *port;
    /**< Keep track of the port that the worker is listening to on the
         inside, _or_ in the case of an external DOM object, the port to
         use to communicate with the worker.

         This port is identical to the .port property on the SharedWorker
         object. */

    BOOL is_dedicated;
    /**< Flag recording the kind, FALSE if a shared worker. */

    BOOL is_closed;
    /**< If TRUE, the Worker has been terminate()d / close()d. */

    BOOL is_enabled;
    /**< If TRUE, the Worker has successfully evaluated its toplevel script
         to completion and is ready for incoming messages.

         A Worker that is either terminated or close()d in the process
         of evaluating its script will not enter an enabled state. If FALSE,
         the Worker is currently executing its toplevel script (or will be)
         _or_ it has done so and been closed while doing it (see is_closed
         to determine this state.) */

    DOM_EventQueue connect_event_queue;
    /**< List of queued-up MessageEvents for connect.
         For shared workers only. */

    Head/*DOM_EventListener*/ delayed_listeners;

    /**< List of queued-up listener registrations (drained on
         enabling the worker.) */

    DOM_EventListener *connect_handler;
    /**< Currently registered .onmessage, .onerror and .onconnect
         handlers for this worker. */

    ES_Value loader_return_value;
    /**< Result of the last completed sync script load. */

    List<AsListElement<DOM_ErrorEvent> > processing_exceptions;
    /**< List of in-flight exceptions that the worker's execution context
         is processing. */

    int keep_alive_counter;
    /**< Counter for keep-alive properties on this global worker object. */

    class DOM_WebWorker_Loader_Element
       : public ListElement<DOM_WebWorker_Loader_Element>
    {
    public:
        DOM_WebWorker_Loader_Element(DOM_WebWorker_Loader *loader)
            : loader(loader)
        {
        }

        void GCTrace(DOM_Runtime *runtime);

        DOM_WebWorker_Loader *loader;
    };

    List<DOM_WebWorker_Loader_Element> active_loaders;
    /**< If a Worker or one of its nested invocations of importScripts()
         are blocked waiting for script content to complete loading
         (and executing), 'active_loaders' keeps track of their loader
         objects. As the loaders complete (or fail), they pop themselves
         off the stack.

         The purpose of the stack being to keep the loaders alive and
         have a way to forward async callbacks from the worker to the
         active loader (at TOS.) */

public:
    static OP_STATUS Construct(DOM_Object *this_object, DOM_WebWorker *&instance, DOM_WebWorkerDomain *&domain, BOOL &reused, const URL &origin, BOOL is_ded, const uni_char *name, ES_Value *return_value);
    /**< Set up a new worker instance and context, guided by the
         (url,is_dedicated, nm) triple.

         If a (shared) worker already exists for the given domain, and this
         is not for a dedicated worker, we simply 'connect' to the running
         instance. If not, a domain holding the new execution context of
         the worker is created, and the new worker instance is created
         and initialised within it.

         @param this_object The 'object' creating the worker (used when
                generating exceptions.)

         @param[out] instance The resulting worker implementation instance.
         @param[out] domain The domain the worker was instantiated within.
         @param[out] reused A flag indicating if the result is a connection
                     to an existing shared worker instance.

         @param origin The URL of the requested worker script.
         @param is_dedicated A flag indicating what kind of worker to construct.
         @param name The optional name for a shared worker. Workers are
                shared based on tag equality (within the same origin.)
         @param return_value Holds the value associated with any exception thrown.

         @return OpStatus::OK on successful creation and initialisation.
                 OpStatus::ERR_NO_MEMORY on OOM while creating new instance
                 and environment; OpStatus::ERR if validation of arguments fails. */

    static OP_STATUS NewExecutionContext(DOM_Object *this_object, DOM_WebWorker *&inner, DOM_WebWorkerDomain *&domain, DOM_WebWorker *parent, DOM_WebWorkerObject *worker_object, const URL &origin, const uni_char *name, ES_Value *return_value);
    /**< Constructor for the context and environment of a new Worker,
         setting up its execution environment.

         Called during construction of a DOM_WebWorkerObject to set up a running
         Web Worker execution context (or connect to an already running one.)

         The execution environment is created within the context of
         a DOM_WebWorkerDomain -- the domain is the party controlling the
         runtime and execution context of web workers. It can be shared.

         @param this_object The 'object' creating the worker (used when
                generating exceptions.)
         @param[out] inner  The new Worker instance.
         @param[out] domain The domain that the new instance is belonging to.
         @param parent If a nested worker, the parent worker.
         @param worker_object The Worker object that's creating/connecting
                to the new context.
         @param origin The URL pointing to the initial script to load
                and execute.
         @param name The name of a shared worker. NULL for dedicated workers.
         @param return_value Holds the value associated with any exception thrown.

         @return OpStatus::ERR_NO_MEMORY if OOM condition hits,
                 OpStatus::ERR if validation of creation request fails or
                 other error conditions. OpStatus::OK if new Worker context
                 and instance created successfully. */

    static OP_STATUS ConstructGlobalScope(BOOL is_dedicated, DOM_Runtime *runtime);
    /**< Initialize and set up a freshly created worker runtime with its
         corresponding global object and scope.

         @param is_dedicated The kind of global scope to create.
         @param runtime A new runtime, but with a global object missing its
                host object along with the names and objects of a web worker
                execution context.

         @return OpStatus::OK on successful creation,
                 OpStatus::ERR_NO_MEMORY on OOM. */

    virtual OP_STATUS EnableWorker();
    /**< If not already receptive to incoming messages, make the
         Worker change into that state.

         That is, if messages have been queued waiting for delivery,
         enable their delivery. If the Worker has been closed or
         terminated while in a non-enabled state, then it is
         not enabled.

         Operation fails if draining the waiting messages failed. */

    virtual void CloseWorker();
    /**< Perform the resource-release functionality of a terminate()
         operation on a Worker. Ceases operation and closes off all
         entangled ports. Upon completion it is safe to abort overall
         execution.

         Closing a Worker implies termination of a Worker's children. */

    virtual void TerminateWorker();
    /**< Implement the lower-level portions of the terminate() operation
         on a Worker object. Doesn't entirely cease operation -- queued
         up messages and timeouts may still be attempted run -- but closes
         down in preparation for exit. */

    FramesDocument *GetWorkerOwnerDocument();
    /**< Locate and return the FramesDocument of the browser context that
         created this worker. If a nested Worker, the method will chase up
         the parent chain until a FramesDocument can be located. */

    OP_STATUS DeliverMessage(DOM_MessageEvent *ev, BOOL is_message);
    /**< Add given message event to the queue of messages awaiting the
         registration of the first listener. Checks if a message listener
         now exists, in which case it fires the given event (and the other
         items in the queue) at the listener. The flag determines if the
         message event is a standard message event or a connect event fired
         at shared workers upon startup/connection.

        @return OpStatus::ERR if event couldn't be added or if the draining
                of queue failed. OpStatus::OK on successful addition to the
                queue. */

    OP_STATUS RegisterConnection(DOM_MessagePort *port);
    /**< When creating a shared worker instance, an explicit port is created
         to communicate with the (shared) worker on the inside. It being
         notified of new connections through 'connect' events.

         RegisterConnection() sets up the delivery of this event, but taking
         care to queue up initial 'connect' events if the worker hasn't yet
         registered a connect event target. A message port within the context
         of the worker is created, and entangled with the passed-in port.
         Should the worker choose to do so within its connect listener,
         it may then start communicating with the connected worker.

         @param [out]port The port for the shared worker instance to
                communicate on. The inner shared worker receives a 'connect'
                event having a port entangled with 'port'.

         @return OpStatus::ERR_NO_MEMORY, OpStatus::ERR on failure to set
                 up and forward the message port to the worker.
                 OpStatus::OK on success. */

    OP_STATUS ProcessingException(DOM_ErrorEvent *error);
    /**< Record the exception 'error' as being processed by the Worker.

         @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM. */

    void ProcessedException(DOM_ErrorEvent *e);
    /**< Flag that processing/propagation of the exception 'e' has finished. */

    BOOL IsProcessingException(const uni_char *message, unsigned line_number, const uni_char *url_str);
    /**< Returns TRUE if exception with the given 'signature' is currently
         being processed; FALSE otherwise. */

#ifdef ECMASCRIPT_DEBUGGER
    OP_STATUS GetWindows(OpVector<Window> &windows);
    /**< Adds the Windows associated with this worker.

         A dedicated worker will only have one associated
         Window, but a shared worker may have more than one.

         @param windows [out] Associated Windows are added to this vector.
         @return OpStatus::ERR_NO_MEMORY, otherwise OpStatus::OK. */
#endif // ECMASCRIPT_DEBUGGER

    virtual ~DOM_WebWorker();
    virtual void GCTrace();

    DOM_DedicatedWorkerObject *GetDedicatedWorker();
    /**< If the worker is operating in dedicated mode, return the
         corresponding dedicated worker object; otherwise NULL. */

    DOM_WebWorker *GetParentWorker() { return parent; }
    DOM_WebWorkerDomain *GetWorkerDomain() { return domain; }
    void SetWorkerDomain(DOM_WebWorkerDomain *domain){ this->domain = domain; }
    const uni_char *GetWorkerName() const { return worker_name; }

    BOOL IsClosed() { return is_closed; }
    BOOL IsDedicated() { return is_dedicated; }
    /**< Returns TRUE iff this is a dedicated Worker. */

    void ClearOtherWorker(DOM_WebWorkerObject *w);
    OP_STATUS SetParentWorker(DOM_WebWorker *p);

    OP_STATUS AddKeepAlive(DOM_Object *object, int *id);
    void RemoveKeepAlive(int id);

    /* DOM_EventTargetOwner */
    virtual DOM_Object *GetOwnerObject() { return this; }

    /* DOM_WebWorkerBase methods */
    virtual OP_STATUS PropagateErrorException(DOM_ErrorEvent *exception);
    virtual OP_STATUS InvokeErrorListeners(DOM_ErrorEvent  *exception, BOOL propagate_error);

    /* This chunk of methods dealing with errors and exception handling needs more work.. */
    OP_STATUS HandleWorkerError(const ES_Value &result);
    OP_STATUS HandleException(DOM_ErrorEvent *exception);

    virtual OP_STATUS HandleError(ES_Runtime *runtime, const ES_Runtime::ErrorHandler::ErrorInformation *information, const uni_char *msg, const uni_char *url, const ES_SourcePosition &position, const ES_ErrorData *data);
    /**< Implementation of ES_Runtime::ErrorHandler */

    virtual OP_STATUS HandleCallback(ES_AsyncOperation operation, ES_AsyncStatus status, const ES_Value &result);
    /**< Handle uncaught exceptions stemming from worker's script execution. */

    virtual OP_STATUS HandleError(const uni_char *message, unsigned line, unsigned offset) { return OpStatus::OK; }
    /**< Empty implementation to avoid warning about hiding this HandleError(). */

#ifdef DOM_WEBWORKERS_WORKER_STAT_SUPPORT
    OP_STATUS GetStatistics(DOM_WebWorkerInfo *&info);
    /**< Supply information on name and nature of this worker, including
         its children and the contexts known to be currently accessing it. */
#endif // DOM_WEBWORKERS_WORKER_STAT_SUPPORT

    /* DOM infrastructure methods - please consult their defining classes for information */
    virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
    virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

    DOM_DECLARE_FUNCTION_WITH_DATA(accessEventListener);
    /**< The Workers specification _may_ end up requiring a change in the behaviour of addEventListener() for
         the event types it introduces. To prepare for such a change, override the method for listener registration here. */

    DOM_DECLARE_FUNCTION(close);
    DOM_DECLARE_FUNCTION(importScripts);

private:
    friend class DOM_WebWorker_Loader;

    /* Updating operations used by the script loader */
    OP_STATUS PushActiveLoader(DOM_WebWorker_Loader *l);
    void PopActiveLoader();

    OP_STATUS SetLoaderReturnValue(const ES_Value &value);

    void RemoveActiveLoaders();
    /**< Clear out all active loaders. */

    /* These are internal operations for handling the delivery and propagation of errors, but ought to
       be documented just the same.. ToDo. */
    void UpdateEventHandlers(DOM_EventListener *l);

    void CloseChildren();

    OP_STATUS AddConnectedWorker(DOM_WebWorkerObject *w);

    void DetachConnectedWorkers();

    virtual OP_STATUS DrainEventQueues();

    static OP_STATUS AddChildWorker(DOM_WebWorker* parent, DOM_WebWorker* w);
    /**< Register a worker as a child of another; this is merely a
         book-keeping operation, with the worker having already been
         instantiated elsewhere.*/

    static BOOL RemoveChildWorker(DOM_WebWorker* parent, DOM_WebWorker* w);
    /**< Unregister a worker as belonging to another as a child.
         Upon shutdown or termination of a worker, it unregisters. */

    static void SetupGlobalScopeL(DOM_Prototype *worker_prototype, DOM_WebWorker *worker);
    /**< Initialise the global scope for a Worker.

         @param proto The prototype of the worker's global object.
         @param worker The worker being created.
         @return No result, but method will leave with
                 OpStatus::ERR_NO_MEMORY on OOM. */
};

class DOM_DedicatedWorker
    : public DOM_WebWorker
{
public:
    DOM_DedicatedWorker()
        : DOM_WebWorker(TRUE)
    {
    }

    virtual BOOL IsA(int type) { return (type == DOM_TYPE_WEBWORKERS_WORKER_DEDICATED || type == DOM_TYPE_WEBWORKERS_WORKER || DOM_Object::IsA(type)); }

    virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
    virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

    DOM_DECLARE_FUNCTION(postMessage);
    /* On DOM_WebWorker: */
    /* DOM_DECLARE_FUNCTION(close); */
    /* DOM_DECLARE_FUNCTION(importScripts); */
    /* DOM_DECLARE_FUNCTION(dispatchEvent); */

};

class DOM_SharedWorker
    : public DOM_WebWorker
{
public:
    DOM_SharedWorker()
        : DOM_WebWorker(FALSE)
    {
    }

    virtual BOOL IsA(int type) { return (type == DOM_TYPE_WEBWORKERS_WORKER_SHARED || type == DOM_TYPE_WEBWORKERS_WORKER || DOM_Object::IsA(type)); }

    virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
    virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

    virtual OP_STATUS DrainEventQueues();

    /* On DOM_WebWorker: */
    /* DOM_DECLARE_FUNCTION(close); */
    /* DOM_DECLARE_FUNCTION(importScripts); */
    /* DOM_DECLARE_FUNCTION(dispatchEvent); */
};

#endif // DOM_WEBWORKERS_SUPPORT
#endif // DOM_WW_WORKER_H
