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
#include "modules/dom/src/domwebworkers/domcrossmessage.h"
#include "modules/dom/src/domwebworkers/domwwutils.h"
#include "modules/dom/src/domwebworkers/domwwloader.h"

#include "modules/prefs/prefsmanager/collections/pc_js.h"


/* ++++: DOM_WebWorkerBase */

DOM_WebWorkerBase::~DOM_WebWorkerBase()
{
}

void
DOM_WebWorkerBase::DropEntangledPorts()
{
    while (DOM_MessagePort *p = entangled_ports.First())
    {
        p->Out();
        p->Disentangle();
        /* Note: the worker still has access to the ports, so releasing them completely is not on. */
    }
}

void
DOM_WebWorkerBase::AddEntangledPort(DOM_MessagePort *p)
{
    p->Out();
    p->Into(&entangled_ports);
}

OP_STATUS
DOM_WebWorkerBase::DeliverError(DOM_Object *target_object, DOM_Event *event)
{
    RETURN_IF_ERROR(error_event_queue.DeliverEvent(event, target_object->GetEnvironment()));
    if (!error_event_queue.HaveDrainedEvents() && target_object->GetEventTarget() && target_object->GetEventTarget()->HasListeners(ONERROR, UNI_L("error"), ES_PHASE_ANY))
        RETURN_IF_ERROR(error_event_queue.DrainEventQueue(target_object->GetEnvironment()));
    return OpStatus::OK;
}

OP_STATUS
DOM_WebWorkerBase::DrainEventQueues(DOM_Object *target_object)
{
    /* Check to see if events are waiting for the new listener; if so, forward them right away. */
    if (!message_event_queue.HaveDrainedEvents() && target_object->GetEventTarget() && target_object->GetEventTarget()->HasListeners(ONMESSAGE, UNI_L("message"), ES_PHASE_ANY))
        OpStatus::Ignore(message_event_queue.DrainEventQueue(target_object->GetEnvironment()));

    if (!error_event_queue.HaveDrainedEvents() && target_object->GetEventTarget() && target_object->GetEventTarget()->HasListeners(ONERROR, UNI_L("error"), ES_PHASE_ANY))
        OpStatus::Ignore(error_event_queue.DrainEventQueue(target_object->GetEnvironment()));
    return OpStatus::OK;
}

#endif // DOM_WEBWORKERS_SUPPORT

