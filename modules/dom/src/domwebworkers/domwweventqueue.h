/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 2009-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef DOM_WW_EVENTQUEUE_H
#define DOM_WW_EVENTQUEUE_H

class DOM_EventQueue_Event;

/** Simple wait queue of events, i.e., a queue that is in an initial recording mode,
    batching up events until the queue is explicitly drained. Events sent/added after
    that are then forwarded directly. */
class DOM_EventQueue
{
private:
    BOOL drained_queue;
    List<DOM_EventQueue_Event> event_queue;

public:
    DOM_EventQueue()
        : drained_queue(FALSE)
    {
    }

    ~DOM_EventQueue();

    void GCTrace(ES_Runtime *runtime);

    BOOL HaveDrainedEvents();
    /**< Returns TRUE if queue has been drained. */

    OP_STATUS DeliverEvent(DOM_Event *event, DOM_EnvironmentImpl *environment);
    /**< Attempt delivering the event; if in recording mode, the event will simply
         be added to a queue until it is enabled and drained. */

    OP_STATUS DrainEventQueue(DOM_EnvironmentImpl *environment);
    /**< Empty the queue of events, forwarding any of the events it may have recorded.
         If event delivery fails for any of the queued events, it's error condition
         will be returned. Unless that error is OOM, the draining operation will attempt
         to complete the delivery of all events in the queue.

         If successful, the drained flag for the event queue will be set to TRUE. Calling
         this method on an already drained queue is the identity operation. */

    OP_STATUS CopyEventQueue(DOM_EventQueue *targetQueue, DOM_Object *targetObject);
    /**< Transfer the content to another queue, updating the target of the queued events.
         The local event queue is emptied & drained as a result.

         Returns OpStatus::ERR_NO_MEMORY on OOM; OpStatus::OK otherwise. */
};

#endif // DOM_WW_EVENTQUEUE_H
