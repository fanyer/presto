/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 2009-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef DOM_WW_BASE_H
#define DOM_WW_BASE_H

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

/** The base class for web worker context and object object. Both execution context and worker object
    representations have the notion of active set of ports that they are entangled with & need to
    keep track of these. The base class also shares some of the functionality for reporting and propagating
    error exceptions between worker (and their contexts, i.e., any parent worker + the user.) */
class DOM_WebWorkerBase
{
protected:
    List<DOM_MessagePort> entangled_ports;
    /**< The list of ports that this worker is entangled with. We need to track this in
         order to handle closing down of a worker (=> all its listened-to ports become disentangled.) */

    DOM_EventQueue message_event_queue;
    /**< Locally queueable MessageEvents (queue drained on start() or .onmessage registration.) */

    DOM_EventQueue error_event_queue;
    /**< Queued-up Error events. */

    DOM_EventListener *message_handler;
    DOM_EventListener *error_handler;

    URL location_url;
    /**< The URL of the script that invoked this Worker. */

    DOM_WebWorkerBase()
        : message_handler(NULL),
          error_handler(NULL)
    {
    }

public:
    virtual ~DOM_WebWorkerBase();

    const URL &GetLocationURL() { return location_url; }
    /**< The origin for this worker; the absolute URL of the script used
         to create this worker. Redirects have been followed (within same
         origin), and value is exposed via the WorkerLocation interface. */

    void SetLocationURL(const URL& u) { location_url = u; }
    /**< Set the absolute URL for the script used to create this worker. */

    void AddEntangledPort(DOM_MessagePort *p);
    /**< In order to implement the behaviour required by the close() and terminate() operations
         on Workers, we need to keep track of what ports a Worker has currently entangled as part
         of creating message channels and as an active party in postMessage() calls. AddEntangledPort()
         performs the necessary bookkeeping.

         @return void; no current observable failure conditions. */

    void DropEntangledPorts();
    /**< Disentangle from all the ports that a Worker is known to be entangled with. Called
         during shutdown operations.

         @return void - no current observable failure conditions. */

    virtual OP_STATUS DeliverError(DOM_Object *target_object, DOM_Event *ev);
    /**< Add given error event to the queue of errors/exceptions awaiting the registration of
         the first .onerror listener. Checks if such a listener now exists, in which case it
         fires the given event (and the other items in the queue) at the listener.

         @return OpStatus::ERR if event couldn't be added or if the draining of queue failed.
                 OpStatus::OK on successful addition to the queue. */

    OP_STATUS DrainEventQueues(DOM_Object *target_object);
    /**< 'error' and 'message' events have guaranteed delivery, hence they have
          to be queued on a DOM_WebWorker that's starting but with no event handlers yet set up.
          When handlers are registered by the worker, call DrainEventQueues() to fire the waiting
          events. */

    virtual OP_STATUS PropagateErrorException(DOM_ErrorEvent *exception) = 0;
    virtual OP_STATUS InvokeErrorListeners(DOM_ErrorEvent  *exception, BOOL propagate_error) = 0;
};

#endif // DOM_WEBWORKERS_SUPPORT
#endif // DOM_WW_BASE_H
