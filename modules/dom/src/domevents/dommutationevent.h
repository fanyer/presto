/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_MUTATIONEVENT_H
#define DOM_MUTATIONEVENT_H

#ifdef DOM2_MUTATION_EVENTS

#include "modules/dom/src/domevents/domevent.h"

class DOM_Attr;

class DOM_MutationEvent
	: public DOM_Event
{
public:
	enum AttrChangeType { MODIFICATION = 1, ADDITION, REMOVAL };

protected:
	uni_char *attr_name;
	AttrChangeType attr_change;
	uni_char *prev_value;
	uni_char *new_value;

	DOM_Node *related_node;

	static OP_STATUS CreateEvent(DOM_MutationEvent *&mutation_event, DOM_EnvironmentImpl *environment, DOM_EventType type);
	static OP_STATUS SendEvent(DOM_MutationEvent *mutation_event, DOM_EnvironmentImpl *environment, ES_Thread *interrupt_thread);

public:
	DOM_MutationEvent();
	virtual ~DOM_MutationEvent();

	static OP_STATUS Make(DOM_MutationEvent *&mutation_event, DOM_EnvironmentImpl *environment, DOM_EventType type);

	virtual ES_GetState	GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_MUTATIONEVENT || DOM_Event::IsA(type); }
	virtual void GCTrace();

	const uni_char *GetPrevValue() { return prev_value; }
	const uni_char *GetNewValue() { return new_value; }
	const uni_char *GetAttrName() { return attr_name; }
	AttrChangeType GetAttrChange() { return attr_change; }

	/** Initialize the global variable "MutationEvent". */
	static void ConstructMutationEventObjectL(ES_Object *object, DOM_Runtime *runtime);

	static OP_STATUS SendSubtreeModified(ES_Thread *interrupt_thread, DOM_Node *target);
	static OP_STATUS SendNodeInserted(ES_Thread *interrupt_thread, DOM_Node *target, DOM_Node *parent);
	static OP_STATUS SendNodeRemoved(ES_Thread *interrupt_thread, DOM_Node *target, DOM_Node *parent);
	static OP_STATUS SendNodeRemovedFromDocument(ES_Thread *interrupt_thread, DOM_Node *target);
	static OP_STATUS SendNodeInsertedIntoDocument(ES_Thread *interrupt_thread, DOM_Node *target);
	static OP_STATUS SendAttrModified(ES_Thread *interrupt_thread, DOM_Node *target, DOM_Attr *attr, AttrChangeType type, const uni_char *attrName, const uni_char *prevValue, const uni_char *newValue);
	static OP_STATUS SendCharacterDataModified(ES_Thread *interrupt_thread, DOM_Node *target, const uni_char *prevValue, const uni_char *newValue);

	DOM_DECLARE_FUNCTION(initMutationEvent);
	enum { FUNCTIONS_ARRAY_SIZE = 2 };
};

#endif // DOM2_MUTATION_EVENTS
#endif // DOM_MUTATIONEVENT_H
