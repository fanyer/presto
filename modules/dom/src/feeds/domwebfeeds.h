/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_WEBFEEDS_H
#define DOM_WEBFEEDS_H

#ifdef WEBFEEDS_BACKEND_SUPPORT

#include "modules/dom/src/domobj.h"

class DOM_FeedListener;


class DOM_FeedList : public DOM_Object
{
private:
	BOOL		m_subscribed_only;

	DOM_FeedList(BOOL subscribed_only) : m_subscribed_only(subscribed_only) {}
	OP_STATUS	GetItem(UINT idx, ES_Value *value);

public:
	static OP_STATUS	Make(DOM_FeedList *&list, DOM_Runtime *runtime, BOOL subscribed_only);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_GetState	GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL		IsA(int type) { return type == DOM_TYPE_FEEDLIST || DOM_Object::IsA(type); }

	DOM_DECLARE_FUNCTION(item);

	enum { FUNCTIONS_ARRAY_SIZE = 2 };
};


class DOM_WebFeeds
	: public DOM_Object
{
private:
	Head	m_listeners; ///< List of active feed listeners

public:
	virtual ~DOM_WebFeeds();

	static OP_STATUS	Make(DOM_WebFeeds *&feeds, DOM_Runtime *runtime);
	virtual void		GCTrace();

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL		IsA(int type) { return type == DOM_TYPE_WEBFEEDS || DOM_Object::IsA(type); }

	DOM_FeedListener*	AddListener(ES_Object* listener_obj, DOM_Runtime* runtime, BOOL loading_one_feed_only);

	DOM_DECLARE_FUNCTION(loadFeed);
	DOM_DECLARE_FUNCTION(updateFeeds);
	DOM_DECLARE_FUNCTION(getFeedById);

	enum { FUNCTIONS_ARRAY_SIZE = 4 };
};

#endif // WEBFEEDS_BACKEND_SUPPORT
#endif // DOM_WEBFEEDS_H
