/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef WEBFEEDS_BACKEND_SUPPORT

#include "modules/dom/src/feeds/domwebfeeds.h"
#include "modules/dom/src/feeds/domfeed.h"
#include "modules/dom/src/feeds/domfeedentry.h"
#include "modules/webfeeds/webfeeds_api.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/dom/src/domenvironmentimpl.h"


/* static */ OP_STATUS
DOM_FeedList::Make(DOM_FeedList *&list, DOM_Runtime *runtime, BOOL subscribed_only)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(list = OP_NEW(DOM_FeedList, (subscribed_only)), runtime,
		runtime->GetPrototype(DOM_Runtime::FEEDLIST_PROTOTYPE), "FeedList"));

	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_FeedList::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_length)
	{
		UINT len = 0;

		UINT *dummy_map = g_webfeeds_api->GetAllFeedIds(len, m_subscribed_only);

		DOMSetNumber(value, len);

		OP_DELETEA(dummy_map);

		return GET_SUCCESS;
	}

	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_FeedList::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_length)
		return PUT_READ_ONLY;

	return PUT_FAILED;
}

OP_STATUS
DOM_FeedList::GetItem(UINT idx, ES_Value *value)
{
	UINT len = 0;
	UINT* map = g_webfeeds_api->GetAllFeedIds(len, m_subscribed_only);

	if (idx < len)
	{
		if (OpFeed *feed = g_webfeeds_api->GetFeedById(map[idx]))
		{
			OP_DELETEA(map);

			DOM_HOSTOBJECT_SAFE(dom_feed, feed->GetDOMObject(GetEnvironment()), DOM_TYPE_FEED, DOM_Feed);
			if (!dom_feed)
			{
				OP_STATUS oom = DOM_Feed::Make(dom_feed, feed, GetRuntime());
				if (OpStatus::IsError(oom))
					return oom;
			}

			DOMSetObject(value, dom_feed);
			return OpStatus::OK;
		}
	}

	OP_DELETEA(map);

	DOMSetNull(value);
	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_FeedList::GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (!value)
		return GET_SUCCESS;

	if (property_index >= 0)
	{
		if (OpStatus::IsMemoryError(GetItem(property_index, value)))
			return GET_NO_MEMORY;
		else
			return GET_SUCCESS;
	}
	else
		return GET_FAILED;
}

/* static */ int
DOM_FeedList::item(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(list, DOM_TYPE_FEEDLIST, DOM_FeedList);
	DOM_CHECK_ARGUMENTS("n");

	if (argv[0].value.number >= 0)
	{
		UINT idx = static_cast<UINT>(argv[0].value.number);

		if (OpStatus::IsMemoryError(list->GetItem(idx, return_value)))
			return ES_NO_MEMORY;

		return ES_VALUE;
	}

	DOMSetNull(return_value);
	return ES_VALUE;
}


class DOM_FeedListener : public Link, public OpFeedListener
{
private:
	ES_Object*		m_object;
	DOM_Runtime*	m_runtime;
	BOOL			m_loading_one_feed;

public:
	DOM_FeedListener(ES_Object* listener_obj, DOM_Runtime* runtime, BOOL loading_one_feed_only)
		: m_object(listener_obj), m_runtime(runtime),
		m_loading_one_feed(loading_one_feed_only) {}
	virtual ~DOM_FeedListener() { Out(); }

	void	Mark(DOM_Object *owner) { owner->GCMark(m_runtime, m_object); }
	void	Detach() { Out(); m_object = NULL; }

	void 	OnUpdateFinished();
	void	OnFeedLoaded(OpFeed* feed, OpFeed::FeedStatus status);
	void	OnEntryLoaded(OpFeedEntry* entry, OpFeedEntry::EntryStatus status);
	void	OnNewEntryLoaded(OpFeedEntry* entry, OpFeedEntry::EntryStatus) {}
};

void
DOM_FeedListener::OnUpdateFinished()
{
	if (m_object)
	{
		ES_AsyncInterface *asyncif = m_runtime->GetEnvironment()->GetAsyncInterface();

		if (op_strcmp(ES_Runtime::GetClass(m_object), "Function") == 0)
			asyncif->CallFunction(m_object, NULL, 0, NULL, NULL, NULL);
		else
			asyncif->CallMethod(m_object, UNI_L("updateFinished"), 0, NULL, NULL, NULL);
	}

	OP_DELETE(this); // the listener will no longer be needed when this has been called
}

void
DOM_FeedListener::OnFeedLoaded(OpFeed* feed, OpFeed::FeedStatus status)
{
	if (m_object && feed)
	{
		DOM_HOSTOBJECT_SAFE(dom_feed, feed->GetDOMObject(m_runtime->GetEnvironment()), DOM_TYPE_FEED, DOM_Feed);

		if (!dom_feed)
			RETURN_VOID_IF_ERROR(DOM_Feed::Make(dom_feed, feed, m_runtime));

		OP_ASSERT(dom_feed);

		ES_AsyncInterface *asyncif = m_runtime->GetEnvironment()->GetAsyncInterface();

		ES_Value argv[2];
		DOM_Object::DOMSetObject(&argv[0], dom_feed);
		DOM_Object::DOMSetNumber(&argv[1], status);

		if (op_strcmp(ES_Runtime::GetClass(m_object), "Function") == 0)
			asyncif->CallFunction(m_object, NULL, 2, argv, NULL, NULL);
		else
			asyncif->CallMethod(m_object, UNI_L("feedLoaded"), 2, argv, NULL, NULL);
	}

	if (m_loading_one_feed) // we will not get an OnUpdateFinished from loadFeed()
		OP_DELETE(this);
}

void
DOM_FeedListener::OnEntryLoaded(OpFeedEntry* entry, OpFeedEntry::EntryStatus status)
{
	if (m_object)
	{
		DOM_HOSTOBJECT_SAFE(dom_entry, entry->GetDOMObject(m_runtime->GetEnvironment()), DOM_TYPE_FEEDENTRY, DOM_FeedEntry);

		if (!dom_entry)
			RETURN_VOID_IF_ERROR(DOM_FeedEntry::Make(dom_entry, entry, m_runtime));

		OP_ASSERT(dom_entry);

		ES_AsyncInterface *asyncif = m_runtime->GetEnvironment()->GetAsyncInterface();

		ES_Value argv[2];
		DOM_Object::DOMSetObject(&argv[0], dom_entry);
		DOM_Object::DOMSetNumber(&argv[1], status);

		if (op_strcmp(ES_Runtime::GetClass(m_object), "Function") == 0)
			asyncif->CallFunction(m_object, NULL, 2, argv, NULL, NULL);
		else
			asyncif->CallMethod(m_object, UNI_L("entryLoaded"), 2, argv, NULL, NULL);
	}
}

/* virtual */ DOM_WebFeeds::~DOM_WebFeeds()
{
	while (DOM_FeedListener *tmp = (DOM_FeedListener *)m_listeners.First())
		tmp->Detach();
}

/* static */ OP_STATUS
DOM_WebFeeds::Make(DOM_WebFeeds *&feeds, DOM_Runtime *runtime)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(feeds = OP_NEW(DOM_WebFeeds, ()), runtime,
		runtime->GetPrototype(DOM_Runtime::WEBFEEDS_PROTOTYPE), "WebFeeds"));

	return OpStatus::OK;
}

/* virtual */ void
DOM_WebFeeds::GCTrace()
{
	DOM_FeedListener *tmp = (DOM_FeedListener *)m_listeners.First();
	while (tmp)
	{
		tmp->Mark(this);
		tmp = (DOM_FeedListener *)tmp->Suc();
	}
}

/* virtual */ ES_GetState
DOM_WebFeeds::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_maxAge:
		DOMSetNumber(value, g_webfeeds_api->GetDefMaxAge());
		return GET_SUCCESS;

	case OP_ATOM_maxEntries:
		DOMSetNumber(value, g_webfeeds_api->GetDefMaxEntries());
		return GET_SUCCESS;

	case OP_ATOM_maxSize:
		DOMSetNumber(value, g_webfeeds_api->GetMaxSize());
		return GET_SUCCESS;

	case OP_ATOM_showImages:
		DOMSetBoolean(value, g_webfeeds_api->GetDefShowImages());
		return GET_SUCCESS;

	case OP_ATOM_subscribedFeeds:
	case OP_ATOM_allFeeds:
		{
			DOM_PrivateName variable = property_name == OP_ATOM_subscribedFeeds ? DOM_PRIVATE_subscribedFeeds : DOM_PRIVATE_allFeeds;
			ES_GetState state = DOMSetPrivate(value, variable);

			if (state == GET_FAILED)
			{
				DOM_FeedList *list;

				GET_FAILED_IF_ERROR(DOM_FeedList::Make(list, GetRuntime(), (property_name == OP_ATOM_subscribedFeeds)));
				GET_FAILED_IF_ERROR(PutPrivate(variable, *list));

				DOMSetObject(value, list);
				return GET_SUCCESS;
			}
			else
				return state;
		}

	case OP_ATOM_updateInterval:
		DOMSetNumber(value, g_webfeeds_api->GetDefUpdateInterval());
		return GET_SUCCESS;
	}

	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_WebFeeds::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_maxSize:
	case OP_ATOM_maxAge:
	case OP_ATOM_maxEntries:
	case OP_ATOM_updateInterval:
		if (value->type != VALUE_NUMBER)
			return PUT_NEEDS_NUMBER;
		else
		{
			UINT int_value;
			if (value->value.number < 0)
				int_value = 0;
			else
				int_value = (UINT)value->value.number;

			if (property_name == OP_ATOM_maxSize)
				g_webfeeds_api->SetMaxSize(int_value);
			else if (property_name == OP_ATOM_maxAge)
			{
				UINT old_max = g_webfeeds_api->GetDefMaxAge();
				g_webfeeds_api->SetDefMaxAge(int_value);
				// delete the excess entries if new value is less than old
				if (int_value != 0 && (int_value < old_max || old_max == 0))
					g_webfeeds_api->CheckAllFeedsForExpiredEntries();
			}
			else if (property_name == OP_ATOM_maxEntries)
			{
				UINT old_max = g_webfeeds_api->GetDefMaxEntries();
				g_webfeeds_api->SetDefMaxEntries(int_value);
				// delete the excess entries if new value is less than old
				if (int_value != 0 && (int_value < old_max || old_max == 0))
					g_webfeeds_api->CheckAllFeedsForNumEntries();
			}
			else
				g_webfeeds_api->SetDefUpdateInterval(int_value);
		}
		return PUT_SUCCESS;

	case OP_ATOM_showImages:
		if (value->type != VALUE_BOOLEAN)
			return PUT_NEEDS_BOOLEAN;
		else
			g_webfeeds_api->SetDefShowImages(value->value.boolean);
		return PUT_SUCCESS;

	case OP_ATOM_allFeeds:
	case OP_ATOM_subscribedFeeds:
		return PUT_READ_ONLY;
	}

	return PUT_FAILED;
}

DOM_FeedListener*
DOM_WebFeeds::AddListener(ES_Object* listener_obj, DOM_Runtime* runtime, BOOL loading_one_feed_only)
{
	DOM_FeedListener *listener = OP_NEW(DOM_FeedListener, (listener_obj, runtime, loading_one_feed_only));
	if (listener)
		listener->Into(&m_listeners);

	return listener;
}

/* static */ int
DOM_WebFeeds::loadFeed(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(feeds, DOM_TYPE_WEBFEEDS, DOM_WebFeeds);
	DOM_CHECK_ARGUMENTS("so");

	URL feed_url = g_url_api->GetURL(argv[0].value.string);
	if (!feed_url.IsEmpty())
	{
		DOM_FeedListener *listener = feeds->AddListener(argv[1].value.object, feeds->GetRuntime(), TRUE);
		if (!listener)
			return ES_NO_MEMORY;

		CALL_FAILED_IF_ERROR(g_webfeeds_api->LoadFeed(feed_url, listener));
	}

	return ES_FAILED;
}

/* static */ int
DOM_WebFeeds::updateFeeds(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(feeds, DOM_TYPE_WEBFEEDS, DOM_WebFeeds);
	DOM_CHECK_ARGUMENTS("o");

	DOM_FeedListener *listener = feeds->AddListener(argv[0].value.object, feeds->GetRuntime(), FALSE);
	if (!listener)
		return ES_NO_MEMORY;

	g_webfeeds_api->UpdateFeeds(listener);

	return ES_FAILED;
}

/* static */ int
DOM_WebFeeds::getFeedById(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(feeds, DOM_TYPE_WEBFEEDS, DOM_WebFeeds);
	DOM_CHECK_ARGUMENTS("n");

	OpFeed *feed = g_webfeeds_api->GetFeedById((UINT)argv[0].value.number);
	if (feed)
	{
		DOM_HOSTOBJECT_SAFE(dom_feed, feed->GetDOMObject(origining_runtime->GetEnvironment()), DOM_TYPE_FEED, DOM_Feed);

		if (!dom_feed)
			CALL_FAILED_IF_ERROR(DOM_Feed::Make(dom_feed, feed, feeds->GetRuntime()));

		DOMSetObject(return_value, dom_feed);
		return ES_VALUE;
	}

	return ES_FAILED;
}


#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_FeedList)
	DOM_FUNCTIONS_FUNCTION(DOM_FeedList, DOM_FeedList::item, "item", "n-")
DOM_FUNCTIONS_END(DOM_FeedList)

DOM_FUNCTIONS_START(DOM_WebFeeds)
	DOM_FUNCTIONS_FUNCTION(DOM_WebFeeds, DOM_WebFeeds::loadFeed, "loadFeed", "so-")
	DOM_FUNCTIONS_FUNCTION(DOM_WebFeeds, DOM_WebFeeds::updateFeeds, "updateFeeds", "o-")
	DOM_FUNCTIONS_FUNCTION(DOM_WebFeeds, DOM_WebFeeds::getFeedById, "getFeedById", "n-")
DOM_FUNCTIONS_END(DOM_WebFeeds)

#endif // WEBFEEDS_BACKEND_SUPPORT
