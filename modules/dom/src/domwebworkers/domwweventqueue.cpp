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

#if defined(DOM_WEBWORKERS_SUPPORT) || defined(DOM_CROSSDOCUMENT_MESSAGING_SUPPORT)
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domevents/domevent.h"

#include "modules/dom/src/domwebworkers/domwweventqueue.h"
#include "modules/dom/src/domwebworkers/domcrossmessage.h"
#include "modules/dom/src/domwebworkers/domcrossutils.h"
#include "modules/dom/src/domwebworkers/domwwutils.h"

class DOM_EventQueue_Event
    : public ListElement<DOM_EventQueue_Event>
{
public:
    DOM_Event *event;

    DOM_EventQueue_Event(DOM_Event *e)
        : event(e)
    {
    }

    void GCTrace(ES_Runtime *runtime)
    {
        if (event && runtime->IsSameHeap(event->GetRuntime()))
            runtime->GCMark(event);
    }
};

DOM_EventQueue::~DOM_EventQueue()
{
    event_queue.Clear();
}

void
DOM_EventQueue::GCTrace(ES_Runtime *runtime)
{
    for (DOM_EventQueue_Event *qev = event_queue.First(); qev; qev = qev->Suc())
        qev->GCTrace(runtime);
}

BOOL
DOM_EventQueue::HaveDrainedEvents()
{
    return drained_queue;
}

OP_STATUS
DOM_EventQueue::DeliverEvent(DOM_Event *event, DOM_EnvironmentImpl *environment)
{
    OP_ASSERT(event->GetRuntime()->IsSameHeap(environment->GetRuntime()));

    if (drained_queue)
        return environment->SendEvent(event);

    /* Keeping track of unbounded amounts of events in the hope that a handler will eventually
       be registered has unacceptable overheads; cap it. (but at what?) */
    if (event_queue.Cardinal() >= 5)
    {
        DOM_EventQueue_Event *qev = event_queue.First();
        qev->Out();
        OP_DELETE(qev);
    }

    DOM_EventQueue_Event *qev = OP_NEW(DOM_EventQueue_Event, (event));
    if (!qev)
        return OpStatus::ERR_NO_MEMORY;

    qev->Into(&event_queue);
    return OpStatus::OK;
}

OP_STATUS
DOM_EventQueue::DrainEventQueue(DOM_EnvironmentImpl *environment)
{
    OP_STATUS ret_status = OpStatus::OK;
    OP_STATUS status;

    if (drained_queue)
        return OpStatus::OK;

    while (DOM_EventQueue_Event *qev = event_queue.First())
    {
        qev->Out();
        status = environment->SendEvent(qev->event);
        OP_DELETE(qev);

        /* Unless OOM, blaze on.*/
        if (OpBoolean::IsMemoryError(status))
            return status;
        else if (OpBoolean::IsError(status))
            ret_status = status;
    }

    if (OpStatus::IsSuccess(ret_status))
        drained_queue = TRUE;
    return ret_status;
}

OP_STATUS
DOM_EventQueue::CopyEventQueue(DOM_EventQueue *target_queue, DOM_Object *target_object)
{
    target_queue->drained_queue = drained_queue;
    if (drained_queue)
    {
        target_queue->event_queue.Clear();
        return OpStatus::OK;
    }
    while(DOM_EventQueue_Event *qev = event_queue.First())
    {
        qev->Out();
        DOM_Event *target_event = NULL;
        /* Hmm. We've got two types of events to deal with here, so won't come up with some more generic cloning story */
        RETURN_IF_ERROR(DOM_ErrorException_Utils::CloneDOMEvent(qev->event, target_object, target_event));
        qev->event = target_event;
        qev->event->SetTarget(target_object);
        qev->Into(&target_queue->event_queue);
    }
    return OpStatus::OK;
}

#endif // DOM_WEBWORKERS_SUPPORT || DOM_CROSSDOCUMENT_MESSAGING_SUPPORT
