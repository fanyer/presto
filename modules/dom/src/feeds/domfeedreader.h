/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_FEEDREADER_H
#define DOM_FEEDREADER_H

#if defined(WEBFEEDS_BACKEND_SUPPORT) && defined(WEBFEEDS_EXTERNAL_READERS)

#include "modules/dom/src/domobj.h"

class DOM_FeedReaderList : public DOM_Object
{
public:
	static OP_STATUS	Make(DOM_FeedReaderList *&list, DOM_Runtime *runtime);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_GetState	GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_DeleteStatus DeleteIndex(int property_index, ES_Runtime* origining_runtime);

	virtual BOOL		IsA(int type) { return type == DOM_TYPE_FEEDREADERLIST || DOM_Object::IsA(type); }

	DOM_DECLARE_FUNCTION(item);

	enum { FUNCTIONS_ARRAY_SIZE = 2 };

private:
	OP_STATUS GetItem(unsigned index, ES_Value *value);
};

class DOM_FeedReader : public DOM_Object
{
public:
	static OP_STATUS	Make(DOM_FeedReader *&feedreader, unsigned id, DOM_Runtime *runtime);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL		IsA(int type) { return type == DOM_TYPE_FEEDREADER || DOM_Object::IsA(type); }

	DOM_DECLARE_FUNCTION(getTargetURL);

	enum { FUNCTIONS_ARRAY_SIZE = 2 };

private:
	DOM_FeedReader(unsigned id) : m_id(id) {}

	unsigned m_id;
};

#endif // defined(WEBFEEDS_BACKEND_SUPPORT) && defined(WEBFEEDS_EXTERNAL_READERS)

#endif // DOM_FEEDREADER_H
