/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef USERJSEVENT_H
#define USERJSEVENT_H

#ifdef USER_JAVASCRIPT

#include "modules/dom/src/domevents/domevent.h"
#include "modules/logdoc/datasrcelm.h"

class DOM_UserJSManager;
class DOM_EnvironmentImpl;
class DOM_Element;
class DOM_EventListener;
class ES_JavascriptURLThread;

class DOM_UserJSEvent
	: public DOM_Event
{
protected:
	DOM_UserJSEvent(DOM_UserJSManager *manager);

	DOM_UserJSManager *manager;
	DOM_Node *node;
	DOM_Event *event;
	ES_Object *listener;
	ES_JavascriptURLThread *thread;
	DataSrc script_source;
	ES_Value source, returnValue;
	BOOL returnValueSet, was_stopped_before;

public:
	static OP_STATUS Make(DOM_UserJSEvent *&userjsevent, DOM_UserJSManager *manager, DOM_Event *event, const uni_char *type_string, ES_Object *listener = NULL);
	static OP_STATUS Make(DOM_UserJSEvent *&userjsevent, DOM_UserJSManager *manager, DOM_Node *element, const uni_char *type_string);
	static OP_STATUS Make(DOM_UserJSEvent *&userjsevent, DOM_UserJSManager *manager, ES_JavascriptURLThread *thread, const uni_char *type_string);

	virtual ~DOM_UserJSEvent();

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_USERJSEVENT || DOM_Event::IsA(type); }
	virtual void GCTrace();

	virtual OP_STATUS DefaultAction(BOOL cancelled);

	DOM_Node *GetNode() { return node; }
	DOM_Event *GetEvent() { return event; }
	ES_Object *GetListener() { return listener; }
	ES_JavascriptURLThread *GetJavascriptURLThread() { return thread; }
	void ResetJavascriptURLThread() { thread = NULL; }
	BOOL WasStoppedBefore() { return was_stopped_before; }

	OP_STATUS GetEventScriptSource(TempBuffer *buffer);
	/**< Fetch the script source from the script element associated
	     with this event. The script may either be external or inline.

	     @param buffer The buffer to append the script source to.
	     @return OpStatus::OK.I f an OOM condition is encountered,
	     OpStatus::ERR_NO_MEMORY. */

	static OP_BOOLEAN GetScriptSource(ES_Runtime *runtime, TempBuffer *buffer);
	/**< If 'runtime' is actively delivering a UserJS event, fetch
	     the script source associated with that UserJS event. That is,
	     invoke GetEventScriptSource() on the UserJS event associated
	     with a UserJS event thread currently being evaluated.

	     @param runtime The runtime to query. Must be a runtime that
	     is currently active in delivering a UserJS event.
	     @param [out]buffer On success, contains the script source of
	     the UserJS event. Otherwise unmodified.
	     @return If the runtime is currently executing a UserJS event
	     thread and its script source can be located, returns OpBoolean::IS_TRUE.
	     OpBoolean::IS_FALSE if the runtime is not active delivering a
	     UserJS event. OOM signalled as OpStatus::ERR_NO_MEMORY. */

private:
	static OP_STATUS Make(DOM_UserJSEvent *&userjsevent, DOM_UserJSManager *manager, const uni_char *type);
};

#endif // USER_JAVASCRIPT
#endif // USERJSEVENT_H
