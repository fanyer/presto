/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_FEEDENTRY_H
#define DOM_FEEDENTRY_H

#ifdef WEBFEEDS_BACKEND_SUPPORT

#include "modules/dom/src/domobj.h"

class OpFeedEntry;
class DOM_Environment;

class DOM_FeedEntry
	: public DOM_Object
{
private:
	OpFeedEntry*	m_entry; ///< The real feedentry object from the WebFeed backend

	DOM_FeedEntry() : m_entry(NULL) {}

public:
	static OP_STATUS	Make(DOM_FeedEntry *&out_feedentry, OpFeedEntry *entry, DOM_Runtime *runtime);

	virtual				~DOM_FeedEntry();

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL	IsA(int type) { return type == DOM_TYPE_FEEDENTRY || DOM_Object::IsA(type); }

	static void		ConstructFeedEntryObjectL(ES_Object *object, DOM_Runtime *runtime);

	DOM_DECLARE_FUNCTION(getProperty);

	enum { FUNCTIONS_ARRAY_SIZE = 2 };
};

#endif // WEBFEEDS_BACKEND_SUPPORT
#endif // DOM_FEEDENTRY_H
