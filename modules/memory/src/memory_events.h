/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef MEMORY_EVENTS_H
#define MEMORY_EVENTS_H

#ifdef ENABLE_MEMORY_DEBUGGING

#include "modules/memory/src/memory_opallocinfo.h"
#include "modules/memory/src/memory_memguard.h"

/** \file
 *
 * \brief Include file for OpMemEvent object
 *
 * The OpMemEvent object carries information about memory events.
 * The events may be regular allocation/deallocation events, or it may
 * be error or warning events.
 *
 * Different subsystems may want to listen to different events, e.g.
 * the scope module is interested in warning/error events (mostly),
 * while a memory map viewer will plot all allocations.
 *
 * A memory database would track all allocations/deallocations and
 * build a database based on the information.
 */

/**
 * \brief Class to signal a memory event of some sort
 *
 * Objects of this type will be delivered through the OpMemEventListener
 * class.  The object will only be valid for the duration of the delivery,
 * and the delivery will happen from within the allocation functions.
 */
class OpMemEvent
{
public:
	OpMemEvent(UINT32 eventclass, OpAllocInfo* ai, OpMemGuardInfo* mgi = 0,
			   OpMemGuardInfo* realloc_mgi = 0, size_t oom_size = 0) :
		eventclass(eventclass),
		ai(ai),
		mgi(mgi),
		realloc_mgi(realloc_mgi),
		oom_size(oom_size)
	{
	}

	/**
	 * \brief Eventclass identifying what memory operation the event is for
	 *
	 * This value is one of the MEMORY_EVENTCLASS_* define values.
	 */
	const UINT32 eventclass;

	/**
	 * \brief Information concerning the allocation operation
	 *
	 * This object, if present, contains information about the allocation
	 * or deallocation operation, like allocation site, size of arrays,
	 * call-stack etc.
	 */
	OpAllocInfo* ai;

	/**
	 * \brief Memory Guard Information for the allocation operation
	 *
	 * This object, if present, contains the details of the allocation
	 * in question.  This information is similar to that of the
	 * \c OpAllocInfo \c ai object, but may contains more history of
	 * the object in question.
	 */
	const OpMemGuardInfo* mgi;

	/**
	 * \brief Secondary Memory Guard Information for reallocations
	 *
	 * This object will only be present in case of a realloc operation.
	 *
	 * A reallocate operation is signalled as an event of type
	 * MEMORY_EVENTCLASS_ALLOCATE with <code>realloc_mgi != 0.</code>
	 * If the reallocation fails, \c realloc_mgi will also be set for the
	 * MEMORY_EVENTCLASS_OOM event. The \c realloc_mgi object
	 * contains the previous object that was vacated by the realloc
	 * operation (or not vacated in case of OOM).
	 *
	 * Note: Calling \c op_realloc(0, size) to allocate memory will
	 * not cause a \c realloc_mgi object to be provided.
	 */
	const OpMemGuardInfo* realloc_mgi;

	/**
	 * \brief Requested allocation size that failed
	 *
	 * This value is only valid for MEMORY_EVENTCLASS_OOM,
	 * for other events the size can be obtained from the corresponding
	 * OpMemGuardInfo member.
	 */
	const size_t oom_size;
};

/**
 * \brief Class to provide events of type OpMemEvent
 *
 * This listener will deliver all memory events once created.
 * The events will be delivered from within the memory allocation
 * functions, which will severely restrict how much can be done
 * when the event is delivered.
 *
 * Allocating memory with op_malloc etc. directly or indirectly
 * will instantly drop you off in a recursion well, and must be
 * avoided.
 */
class OpMemEventListener : private Link
{
protected:
	/**
	 * \brief Constructor for the memory event listener
	 *
	 * This constructor takes an argument, which is the logical or'ing of
	 * all the memory event-classes that is of interrest.  The event
	 * classes are identified through the MEMORY_EVENTCLASS_* set of
	 * definitions in \c memory_memguard.h .
	 *
	 * \param eventclasses Bit-wise or of all event-classes of interest
	 */
	OpMemEventListener(UINT32 eventclasses);
	virtual ~OpMemEventListener(void);

	/**
	 * \brief The method called when there is a memory event
	 *
	 * This method should be implemented by the class that
	 * derives from \c OpMemEventListener and wants to be
	 * informed of memory events.
	 *
	 * Note: The memory events are delivered synchronously with
	 * memory operations, so this method should not assume
	 * Opera to be in any particular state when called.
	 *
	 * \param event The memory event structure delivered
	 */
	virtual void MemoryEvent(OpMemEvent* event) = 0;

private:
	UINT32 eventclass_mask;

public:
	static void DeliverMemoryEvent(OpMemEvent* event);
};

#endif // ENABLE_MEMORY_DEBUGGING

#endif // MEMORY_EVENTS_H
