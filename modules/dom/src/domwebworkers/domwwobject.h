/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 2009-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef DOM_WW_OBJECT_H
#define DOM_WW_OBJECT_H

#ifdef DOM_WEBWORKERS_SUPPORT

/** The DOM_WebWorkerObject type is the representation of a dedicated or shared worker
    as returned by a call to the DOM-level constructors. Via it, the outside world can interact
    with the connected web worker instance, sending and receiving messages and events, and otherwise
    controlling the operation of the worker. */
class DOM_WebWorkerObject
    : public DOM_Object,
      public DOM_WebWorkerBase,
      public DOM_EventTargetOwner,
      public ListElement<DOM_WebWorkerObject>
{
protected:
    DOM_WebWorker *the_worker;
    /**< Worker objects have two instantiations - one residing within its execution context,
         the other in the browser context that begat it. 'the_worker' points to the real worker object.
         This holds true for both objects for a dedicated worker, but only the external one
         for shared workers. The object residing within the worker context for shared ones
         is..shared..hence it keeps a list of everyone connected. This is so that we
         can correctly handle notifications of shutdowns. */

    DOM_MessagePort *port;
    /**< Keep track of the port that the worker is listening to on the inside, _or_
         in the case of an external DOM object, the port to use to communicate with
         the worker. This port is identical to the .port property on the SharedWorker
         object. */

    DOM_WebWorkerObject()
        : the_worker(NULL),
          port(NULL)
    {
    }

public:
    static OP_STATUS Make(DOM_Object          *this_object,
                          DOM_WebWorkerObject *&instance,
                          DOM_WebWorker       *parent,
                          const uni_char      *url,
                          const uni_char      *name,
                          ES_Value            *return_value);
    /**< Constructor for a new Shared / Dedicated Worker, either from within the context of
         an existing Worker (i.e., creating a nested Worker), or from a browser context. A Worker
         is given an initial script to load&evaluate, as given by the 'url' argument. The script
         is handled within the domain of the given environment.

         @param[out] instance the reference to the resulting Worker instance.
         @param      environ  the context from which the Worker constructor is called. Either the
                              browser context or the environment of an existing Worker.
         @param      url      the URL to the initial script to load and execute. In case a Shared
                              Worker instance is created, a Worker instance may already exist for that
                              resource, in which the new instance merely connects to it (and the
                              script isn't reloaded.)
         @param      name     In case of a Shared Worker, its name. NULL if a Dedicated Worker is requested.

         @return OpStatus::ERR_NO_MEMORY on OOM, OpStatus::ERR on failure to allocate and initialise the Worker.
                 OpStatus::OK on success, returning the new Worker instance via the 'instance' reference. */

    virtual void TerminateWorker();
    /**< Implement the lower-level portions of the terminate() operation on a Worker object.
         Doesn't entirely cease operation -- queued up messages and timeouts may still be
         attempted run -- but closes down in preparation for exit. */

    OP_STATUS DeliverMessage(DOM_MessageEvent *ev);
    /**< Add given message event to the queue of messages awaiting the registration of
         the first listener. Checks if a message listener now exists, in which case it
         fires the given event (and the other items in the queue) at the listener. The flag
         determines if the message event is a standard message event or a connect event fired at
         shared workers upon startup/connection.

        @return OpStatus::ERR if event couldn't be added or if the draining of queue failed.
                OpStatus::OK on successful addition to the queue. Use GetMsgDrainFlag() to
                determine if the operation caused the queue to be drained in the process of
                adding the event. */

    virtual ~DOM_WebWorkerObject();
    virtual void GCTrace();

    void ClearWorker() { the_worker = NULL; }
    DOM_WebWorker *GetWorker() { return the_worker; }
    BOOL IsClosed() { return (!the_worker || the_worker->IsClosed()); }

    /* DOM_EventTargetOwner */
    virtual DOM_Object *GetOwnerObject() { return this; }

    /* DOM_WebWorkerBase methods: */
    virtual OP_STATUS InvokeErrorListeners(DOM_ErrorEvent  *exception, BOOL propagate_error);
    virtual OP_STATUS PropagateErrorException(DOM_ErrorEvent *exception);

    /* DOM infrastructure methods - please consult their defining classes for information. */

    virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
    virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

    DOM_DECLARE_FUNCTION_WITH_DATA(accessEventListener);
    /**< The Workers specification _may_ end up requiring a change in the behaviour of addEventListener() for
         the event types it introduces. To prepare for such a change, override the method for listener registration here. */

    /* For convenience, we define DOM functions on the parent (for both worker objects and scopes);
       The DOM-exposed subclasses handles the exposure of the subset they support; DOM_WebWorker is not directly visible. */

    DOM_DECLARE_FUNCTION(terminate);
};

class DOM_DedicatedWorkerObject
    : public DOM_WebWorkerObject
{
public:
    virtual BOOL IsA(int type) { return (type == DOM_TYPE_WEBWORKERS_WORKER_OBJECT_DEDICATED || type == DOM_TYPE_WEBWORKERS_WORKER_OBJECT || DOM_Object::IsA(type)); }

    virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
    virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

    DOM_DECLARE_FUNCTION(postMessage);
    /**< Perform a synchronous call out from this dedicated worker execution context to an outside worker object, passing a data value
         and an optional ports array. Both these pieces of data are validated and cloned as
         set out in the cross-document messaging specification. For the ports array, the message ports
         are replicated in the context of the target worker, but entangling them so that they refer back
         to the ports they are entangled with in its origin.

         @param argv[0] = value  value to send as 'data' in the MessageEvent. Must be cloneable into the context of the receiving party.
         @param argv[1] = ports  an optional native array of DOM_MessagePorts to hand over to the Worker. */

    enum { FUNCTIONS_ARRAY_SIZE = 4 };
    enum { FUNCTIONS_WITH_DATA_BASIC = 2,
#ifdef DOM3_EVENTS
       FUNCTIONS_WITH_DATA_addEventListenerNS,
       FUNCTIONS_WITH_DATA_removeEventListenerNS,
#endif // DOM3_EVENTS
       FUNCTIONS_WITH_DATA_ARRAY_SIZE
         };
};

class DOM_SharedWorkerObject
    : public DOM_WebWorkerObject
{
public:
    virtual BOOL IsA(int type) { return (type == DOM_TYPE_WEBWORKERS_WORKER_OBJECT_SHARED || type == DOM_TYPE_WEBWORKERS_WORKER_OBJECT || DOM_Object::IsA(type)); }

    virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
    virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

    /* Inherits terminate() from DOM_WebWorkerObject */
    enum { FUNCTIONS_ARRAY_SIZE = 3 };
    enum { FUNCTIONS_WITH_DATA_BASIC = 2,
#ifdef DOM3_EVENTS
       FUNCTIONS_WITH_DATA_addEventListenerNS,
       FUNCTIONS_WITH_DATA_removeEventListenerNS,
#endif // DOM3_EVENTS
       FUNCTIONS_WITH_DATA_ARRAY_SIZE
         };

};

/** The 'constructor' wrapper classes -- one for each that Web Workers exposes at the DOM level. */

class DOM_DedicatedWorkerObject_Constructor
    : public DOM_BuiltInConstructor
{
public:
    DOM_DedicatedWorkerObject_Constructor()
        : DOM_BuiltInConstructor(DOM_Runtime::WEBWORKERS_DEDICATED_OBJECT_PROTOTYPE)
    {
    }

    virtual int Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime);
};

class DOM_SharedWorkerObject_Constructor
    : public DOM_BuiltInConstructor
{
public:
    DOM_SharedWorkerObject_Constructor()
        : DOM_BuiltInConstructor(DOM_Runtime::WEBWORKERS_SHARED_OBJECT_PROTOTYPE)
    {
    }

    virtual int Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime);
};

#endif // DOM_WEBWORKERS_SUPPORT
#endif // DOM_WW_OBJECT_H
