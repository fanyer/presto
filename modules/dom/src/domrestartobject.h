/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DOM_DOMRESTARTOBJECT_H
#define DOM_DOMRESTARTOBJECT_H

#include "modules/dom/src/domobj.h"
#include "modules/ecmascript_utils/esthread.h"


/** Helper class used for blocking/restarting function calls in host objects. */
class DOM_RestartObject
	: public DOM_Object
	, public ES_ThreadListener
{
public:
	inline int         BlockCall(ES_Value* return_value, ES_Runtime* origining_runtime);
	/**< Blocks the current thread for the origining_runtime and puts itself into return value.
	     @return ES_SUSPEND | ES_RESTART */
	inline ES_PutState BlockPut(ES_Value* restart_value, ES_Runtime* origining_runtime);
	/**< Blocks the current thread for the origining_runtime and puts itself into return value.
	     @return GET_SUSPEND */
	inline ES_GetState BlockGet(ES_Value* restart_value, ES_Runtime* origining_runtime);
	/**< Blocks the current thread for the origining_runtime and puts itself into return value.
	     @return PUT_SUSPEND */
protected:
	DOM_RestartObject()
		: thread(NULL)
	{
	}

	virtual void OnAbort() {}
	/**< Called when the thread that was suspended by former call to BlockXXX is aborted.
	     The descendant classes may choose to overload this method to abort the underlaying
		 operation. In any case the restart object will be kept alive until Resume is called. */

	OP_STATUS KeepAlive();
	/**< Protects the restart object until it is resumed. MUST be called before Block.
	     @return OK or ERR_NO_MEMORY. */

	void Resume();
	/**< Resumes the current thread(if it is still alive) and unprotects the restart object. */
private:
	/* Implementation of OpThreadListener */
	virtual OP_STATUS Signal(ES_Thread* thread, ES_ThreadSignal signal);

	void Block(ES_Value* restart_value, ES_Runtime* origining_runtime);
	/**< Blocks the current thread for the origining_runtime and puts itself into return value.
	     It also starts listening for thread events to send OnAbort if the thread is aborted. */

	ES_ObjectReference keep_alive;
	ES_Thread* thread;
};

inline int DOM_RestartObject::BlockCall(ES_Value* restart_value, ES_Runtime* origining_runtime)
{
	Block(restart_value, origining_runtime);
	return ES_SUSPEND | ES_RESTART;
}

inline ES_PutState DOM_RestartObject::BlockPut(ES_Value* restart_value, ES_Runtime* origining_runtime)
{
	Block(restart_value, origining_runtime);
	return PUT_SUSPEND;
}

inline ES_GetState DOM_RestartObject::BlockGet(ES_Value* restart_value, ES_Runtime* origining_runtime)
{
	Block(restart_value, origining_runtime);
	return GET_SUSPEND;
}

#endif // DOM_DOMRESTARTOBJECT_H
