/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_FEED_H
#define DOM_FEED_H

#ifdef WEBFEEDS_BACKEND_SUPPORT

#include "modules/dom/src/domobj.h"

class OpFeed;
class DOM_Environment;

class DOM_FeedEntryList : public DOM_Object
{
private:
	OpFeed*		m_feed;

	OP_STATUS	GetItem(UINT idx, ES_Value *value);

public:
			DOM_FeedEntryList(OpFeed *feed) : m_feed(feed) {}

	static OP_STATUS	Make(DOM_FeedEntryList *&list, OpFeed *feed, DOM_Runtime *runtime);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_GetState	GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL		IsA(int type) { return type == DOM_TYPE_FEEDENTRYLIST || DOM_Object::IsA(type); }

	DOM_DECLARE_FUNCTION(item);

	enum { FUNCTIONS_ARRAY_SIZE = 2 };
};


class DOM_Feed
	: public DOM_Object
{
private:
	OpFeed*	m_feed; ///< The real feed object from the WebFeed backend

	DOM_Feed() : m_feed(NULL) {}

public:
	static OP_STATUS Make(DOM_Feed *&out_feed, OpFeed *feed, DOM_Runtime *runtime);
	static OP_STATUS Make(DOM_Feed *&out_feed, const URL& feed_url, DOM_Runtime *runtime);

	virtual ~DOM_Feed();

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_FEED || DOM_Object::IsA(type); }

	static void	ConstructFeedObjectL(ES_Object *object, DOM_Runtime *runtime);

	DOM_DECLARE_FUNCTION(subscribe);
	DOM_DECLARE_FUNCTION(unsubscribe);
	DOM_DECLARE_FUNCTION(getProperty);

#ifdef DOM_WEBFEEDS_SUBSCRIBE_NATIVE
	DOM_DECLARE_FUNCTION(subscribeNative);
#endif // DOM_WEBFEEDS_SUBSCRIBE_NATIVE

	enum
	{
		FUNCTIONS_BASIC = 3,
#ifdef DOM_WEBFEEDS_SUBSCRIBE_NATIVE
		FUNCTIONS_subscribeNative,
#endif // DOM_WEBFEEDS_SUBSCRIBE_NATIVE
		FUNCTIONS_ARRAY_SIZE
	};
};

#endif // WEBFEEDS_BACKEND_SUPPORT

#endif // DOM_FEED_H
