//
// $Id$
//
// Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//

#ifndef AUTODELETE_H
#define AUTODELETE_H

#include "adjunct/m2/src/glue/mh.h"
#include "adjunct/desktop_util/adt/opqueue.h"

#ifdef _DEBUG
# define autodelete(object) MessageEngine::GetInstance()->GetAutoDelete()->DeleteDbg(object, __FILE__, __LINE__)
#else
#define autodelete(object) MessageEngine::GetInstance()->GetAutoDelete()->Delete(object)
#endif

/**
 * Implements a delayed delete of any kind of object.
 *
 * An important advantage here is that you can actually delete
 * an object in a callback function coming from that very
 * same instance. Example:
 *
 * Object::Close()
 * {
 *     Cleanup();
 *     m_observer->Finished(this);
 * }
 *
 * Observer::Finished(Object *object)
 * {
 *     autodelete(object);
 *     m_object = NULL;
 * }
 *
 * Attention: All objects to be deleted with autodelete must
 * inherit from base classes that implement the Autodeletable
 * interface (with a public destructor).
 *
 * If the AutoDelete object is deleted, any remaining
 * objects in the delete queue will also be deleted.
 * Normally, objects in the delete queue will be deleted
 * during the next round of the message loop.
 *
 * @author Johan Borg, johan@opera.com
 */

class Autodeletable : public OpQueueItem<Autodeletable>
{
public:
	Autodeletable() {
#ifdef _DEBUG
		m_ok_to_delete = TRUE;
#endif
	}

	virtual ~Autodeletable() {
#ifdef _DEBUG
		OP_ASSERT(m_ok_to_delete);
#endif
    };

#ifdef _DEBUG
	BOOL		m_ok_to_delete;
	OpString8	m_autodelete_file;
	int			m_autodelete_line;
#endif
};

class AutoDelete : public MessageLoop::Target
{
public:
	AutoDelete();
	virtual ~AutoDelete();

	/** Delete all messages waiting in the queue
	  * also performed in the destructor
	  */
	void FlushQueue();

	/** Put an object in the queue for future deletion
	  */
#ifdef _DEBUG
	void DeleteDbg(Autodeletable *object, const OpStringC8& autodelete_file, int autodelete_line);
#else
	void Delete(Autodeletable *object);
#endif

	// From Target:
	OP_STATUS Receive(OpMessage message);
private:
	MessageLoop *m_loop;
	OpQueue<Autodeletable> m_queue;
};

#endif // AUTODELETE_H
