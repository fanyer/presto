//
// $Id$
//
// Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "autodelete.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/glue/mh.h"


AutoDelete::AutoDelete()
{
	m_loop = MessageEngine::GetInstance()->GetGlueFactory()->CreateMessageLoop();
	if (m_loop)
	{
		m_loop->SetTarget(this);
	}
}

AutoDelete::~AutoDelete()
{
	FlushQueue();
	MessageEngine::GetInstance()->GetGlueFactory()->DeleteMessageLoop(m_loop);
	m_loop = NULL;
}

void AutoDelete::FlushQueue()
{
	while (!m_queue.IsEmpty())
	{
		Autodeletable *object = (Autodeletable*)(m_queue.Dequeue());

#ifdef _DEBUG
		if (object) object->m_ok_to_delete=TRUE;
#endif

		OP_DELETE(object);
	}
}

#ifdef _DEBUG
void AutoDelete::DeleteDbg(Autodeletable *object, const OpStringC8& autodelete_file, int autodelete_line)
#else
void AutoDelete::Delete(Autodeletable *object)
#endif
{
#ifdef _DEBUG
	if (object)
{
		object->m_ok_to_delete=FALSE;
		OpStatus::Ignore(object->m_autodelete_file.Set(autodelete_file));
		object->m_autodelete_line = autodelete_line;
	}
#endif

	if (m_loop)
	{
		m_queue.Enqueue(object);
		m_loop->Post(MSG_M2_AUTODELETE);
	}
}

OP_STATUS AutoDelete::Receive(OpMessage message)
{
	if (message == MSG_M2_AUTODELETE)
	{
		if (!m_queue.IsEmpty())
		{
			Autodeletable *object = m_queue.Dequeue();

#ifdef _DEBUG
			if (object) object->m_ok_to_delete=TRUE;
#endif

			OP_DELETE(object);
			return OpStatus::OK;
		}
	}
	return OpStatus::ERR_NULL_POINTER;
}

#endif //M2_SUPPORT
