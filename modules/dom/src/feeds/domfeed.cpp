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

#include "modules/dom/src/feeds/domfeed.h"

#include "modules/doc/frm_doc.h"
#include "modules/dochand/win.h"
#include "modules/dom/src/feeds/domfeedentry.h"
#include "modules/dom/src/feeds/domfeedcontent.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/webfeeds/webfeeds_api.h"
#include "modules/windowcommander/src/WindowCommander.h"


/* static */ OP_STATUS
DOM_FeedEntryList::Make(DOM_FeedEntryList *&list, OpFeed *feed, DOM_Runtime *runtime)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(list = OP_NEW(DOM_FeedEntryList, (feed)), runtime,
		runtime->GetPrototype(DOM_Runtime::FEEDENTRYLIST_PROTOTYPE), "FeedEntryList"));

	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_FeedEntryList::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_length)
	{
		DOMSetNumber(value, m_feed->GetTotalCount());
		return GET_SUCCESS;
	}

	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_FeedEntryList::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_length)
		return PUT_READ_ONLY;

	return PUT_FAILED;
}

OP_STATUS
DOM_FeedEntryList::GetItem(UINT idx, ES_Value *value)
{
	if (idx < m_feed->GetTotalCount())
	{
		OpFeedEntry *entry = m_feed->GetFirstEntry();
		while (entry && idx > 0)
		{
			entry = entry->GetNext(FALSE);
			idx--;
		}

		if (entry)
		{
			DOM_HOSTOBJECT_SAFE(dom_feedentry, entry->GetDOMObject(GetEnvironment()), DOM_TYPE_FEEDENTRY, DOM_FeedEntry);

			if (!dom_feedentry)
			{
				OP_STATUS oom = DOM_FeedEntry::Make(dom_feedentry, entry, GetRuntime());
				if (OpStatus::IsError(oom))
					return oom;
			}

			DOMSetObject(value, dom_feedentry);
			return OpStatus::OK;
		}
	}

	DOMSetNull(value);
	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_FeedEntryList::GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
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
DOM_FeedEntryList::item(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(list, DOM_TYPE_FEEDENTRYLIST, DOM_FeedEntryList);
	DOM_CHECK_ARGUMENTS("n");

	if (argv[0].value.number >= 0)
	{
		UINT idx = static_cast<UINT>(argv[0].value.number);
		if (OpStatus::IsMemoryError(list->GetItem(idx, return_value)))
			return ES_NO_MEMORY;
	}
	else
		DOMSetNull(return_value);

	return ES_VALUE;
}

/* virtual */
DOM_Feed::~DOM_Feed()
{
	if (m_feed)
	{
		OP_ASSERT(m_feed->GetDOMObject(GetEnvironment()) == *this);
		m_feed->SetDOMObject(NULL, GetEnvironment());
		m_feed->DecRef();
	}
}

/* static */ OP_STATUS
DOM_Feed::Make(DOM_Feed *&out_feed, OpFeed *feed, DOM_Runtime *runtime)
{
	// Check that there isn't already a DOM object for
	// this OpFeed. One is more than enough.
	OP_ASSERT(!feed->GetDOMObject(runtime->GetEnvironment()));

	RETURN_IF_ERROR(DOMSetObjectRuntime(out_feed = OP_NEW(DOM_Feed, ()), runtime, runtime->GetPrototype(DOM_Runtime::FEED_PROTOTYPE), "Feed"));

	feed->IncRef();
	out_feed->m_feed = feed;
	feed->SetDOMObject(*out_feed, runtime->GetEnvironment());

	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_Feed::Make(DOM_Feed *&out_feed, const URL& feed_url, DOM_Runtime *runtime)
{
	OpFeed* feed = g_webfeeds_api->GetFeedByUrl(feed_url);

	if (!feed)
		return OpStatus::ERR_NO_SUCH_RESOURCE;

	return Make(out_feed, feed, runtime);
}

/* virtual */ ES_GetState
DOM_Feed::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_author:
		DOMSetString(value, m_feed->GetAuthor());
		return GET_SUCCESS;

	case OP_ATOM_entries:
	{
		ES_GetState state = DOMSetPrivate(value, DOM_PRIVATE_entries);

		if (state == GET_FAILED)
		{
			DOM_FeedEntryList *list;

			GET_FAILED_IF_ERROR(DOM_FeedEntryList::Make(list, m_feed, GetRuntime()));
			GET_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_entries, *list));

			DOMSetObject(value, list);
			return GET_SUCCESS;
		}
		else
			return state;
	}

	case OP_ATOM_icon:
		DOMSetString(value, m_feed->GetIcon());
		return GET_SUCCESS;

	case OP_ATOM_id:
		DOMSetNumber(value, m_feed->GetId());
		return GET_SUCCESS;

	case OP_ATOM_isSubscribed:
		DOMSetBoolean(value, m_feed->IsSubscribed());
		return GET_SUCCESS;

	case OP_ATOM_lastUpdate:
		{
			ES_Value date_value;
			// double arithmetics
			DOMSetNumber(&date_value, m_feed->LastUpdated());
			ES_Object* date_obj;
			GET_FAILED_IF_ERROR(GetRuntime()->CreateNativeObject(date_value,
																ENGINE_DATE_PROTOTYPE,
																&date_obj));
			DOMSetObject(value, date_obj);
		}
		return GET_SUCCESS;

	case OP_ATOM_logo:
		DOMSetString(value, m_feed->GetLogo());
		return GET_SUCCESS;

	case OP_ATOM_maxAge:
		DOMSetNumber(value, m_feed->GetMaxAge());
		return GET_SUCCESS;

	case OP_ATOM_maxEntries:
		DOMSetNumber(value, m_feed->GetMaxEntries());
		return GET_SUCCESS;

	case OP_ATOM_minUpdateInterval:
		DOMSetNumber(value, m_feed->GetMinUpdateInterval());
		return GET_SUCCESS;

	case OP_ATOM_prefetchPrimaryLink:
		DOMSetBoolean(value, m_feed->PrefetchPrimaryWhenNewEntries());
		return GET_SUCCESS;

	case OP_ATOM_showImages:
		DOMSetBoolean(value, m_feed->GetShowImages());
		return GET_SUCCESS;

	case OP_ATOM_size:
		DOMSetNumber(value, m_feed->GetSizeUsed());
		return GET_SUCCESS;

	case OP_ATOM_status:
		DOMSetNumber(value, m_feed->GetStatus());
		return GET_SUCCESS;

	case OP_ATOM_title:
		{
			OpFeedContent *content = m_feed->GetTitle();
			if (content)
			{
				DOM_HOSTOBJECT_SAFE(dom_content, content->GetDOMObject(GetEnvironment()), DOM_TYPE_FEEDCONTENT, DOM_FeedContent);

				if (!dom_content)
					GET_FAILED_IF_ERROR(DOM_FeedContent::Make(dom_content, content, GetRuntime()));

				DOMSetObject(value, dom_content);
				return GET_SUCCESS;
			}

			DOMSetNull(value);
		}
		return GET_SUCCESS;

	case OP_ATOM_total:
		DOMSetNumber(value, m_feed->GetTotalCount());
		return GET_SUCCESS;

	case OP_ATOM_unread:
		DOMSetNumber(value, m_feed->GetUnreadCount());
		return GET_SUCCESS;

	case OP_ATOM_updateInterval:
		DOMSetNumber(value, m_feed->GetUpdateInterval());
		return GET_SUCCESS;

	case OP_ATOM_uri:
		{
			OpString url;
			GET_FAILED_IF_ERROR(m_feed->GetURL().GetAttribute(URL::KUniName_With_Fragment_Escaped, url));
			TempBuffer *buffer = GetEmptyTempBuf();
			GET_FAILED_IF_ERROR(buffer->Append(url.CStr()));
			DOMSetString(value, buffer);

			return GET_SUCCESS;
		}

	case OP_ATOM_userDefinedTitle:
		OpString title;
		GET_FAILED_IF_ERROR(m_feed->GetUserDefinedTitle(title));
		TempBuffer *buffer = GetEmptyTempBuf();
		GET_FAILED_IF_ERROR(buffer->Append(title.CStr()));
		DOMSetString(value, buffer);
		return GET_SUCCESS;
	}

	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_Feed::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_author:
	case OP_ATOM_entries:
	case OP_ATOM_icon:
	case OP_ATOM_id:
	case OP_ATOM_isSubscribed:
	case OP_ATOM_lastUpdate:
	case OP_ATOM_logo:
	case OP_ATOM_minUpdateInterval:
	case OP_ATOM_size:
	case OP_ATOM_status:
	case OP_ATOM_title:
	case OP_ATOM_total:
	case OP_ATOM_unread:
	case OP_ATOM_uri:
		return PUT_READ_ONLY;

	case OP_ATOM_maxAge:
	case OP_ATOM_maxEntries:
	case OP_ATOM_updateInterval:
		if (value->type != VALUE_NUMBER)
			return PUT_NEEDS_NUMBER;

		UINT int_value;
		if (value->value.number < 0)
			int_value = 0;
		else
			int_value = (UINT)value->value.number;

		if (property_name == OP_ATOM_maxAge)
		{
			UINT old_max = m_feed->GetMaxAge();
			m_feed->SetMaxAge(int_value);
			// delete the excess entries if new value is less than old
			if (int_value < old_max || old_max == 0)
				m_feed->CheckFeedForExpiredEntries();
		}
		else if (property_name == OP_ATOM_maxEntries)
		{
			UINT old_max = m_feed->GetMaxEntries();
			m_feed->SetMaxEntries(int_value);
			// delete the excess entries if new value is less than old
			if (int_value < old_max || old_max == 0)
				m_feed->CheckFeedForNumEntries();
		}
		else
			m_feed->SetUpdateInterval(int_value);
		return PUT_SUCCESS;

	case OP_ATOM_prefetchPrimaryLink:
		if (value->type != VALUE_BOOLEAN)
			return PUT_NEEDS_BOOLEAN;

		m_feed->SetPrefetchPrimaryWhenNewEntries(value->value.boolean, TRUE);
		return PUT_SUCCESS;

	case OP_ATOM_showImages:
		if (value->type != VALUE_BOOLEAN)
			return PUT_NEEDS_BOOLEAN;

		m_feed->SetShowImages(value->value.boolean);
		return PUT_SUCCESS;

	case OP_ATOM_userDefinedTitle:
		if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;

		if (OpStatus::IsMemoryError(m_feed->SetUserDefinedTitle(value->value.string)))
			return PUT_NO_MEMORY;
		return PUT_SUCCESS;
	}

	return PUT_FAILED;
}

/* static */ int
DOM_Feed::subscribe(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(feed, DOM_TYPE_FEED, DOM_Feed);

	feed->m_feed->Subscribe();

	return ES_FAILED;
}

/* static */ int
DOM_Feed::unsubscribe(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(feed, DOM_TYPE_FEED, DOM_Feed);

	feed->m_feed->UnSubscribe();

	return ES_FAILED;
}

/* static */ int
DOM_Feed::getProperty(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(feed, DOM_TYPE_FEED, DOM_Feed);
	DOM_CHECK_ARGUMENTS("s");

	DOMSetString(return_value, feed->m_feed->GetProperty(argv[0].value.string));

	return ES_VALUE;
}

#ifdef DOM_WEBFEEDS_SUBSCRIBE_NATIVE
/* static */ int
DOM_Feed::subscribeNative(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(feed, DOM_TYPE_FEED, DOM_Feed);

	OpString url;
	CALL_FAILED_IF_ERROR(feed->m_feed->GetURL().GetAttribute(URL::KUniName_Username_Password_NOT_FOR_UI, url));

	WindowCommander* commander = origining_runtime->GetFramesDocument()->GetWindow()->GetWindowCommander();
	commander->GetDocumentListener()->OnSubscribeFeed(commander, url.CStr());

	return ES_FAILED;
}
#endif // DOM_WEBFEEDS_SUBSCRIBE_NATIVE

/* static */ void
DOM_Feed::ConstructFeedObjectL(ES_Object *object, DOM_Runtime *runtime)
{
	PutNumericConstantL(object, "STATUS_OK", OpFeed::STATUS_OK, runtime);
	PutNumericConstantL(object, "STATUS_ABORTED", OpFeed::STATUS_ABORTED, runtime);
	PutNumericConstantL(object, "STATUS_REFRESH_POSTPONED", OpFeed::STATUS_REFRESH_POSTPONED, runtime);
	PutNumericConstantL(object, "STATUS_NOT_MODIFIED", OpFeed::STATUS_NOT_MODIFIED, runtime);
	PutNumericConstantL(object, "STATUS_OOM", OpFeed::STATUS_OOM, runtime);
	PutNumericConstantL(object, "STATUS_SERVER_TIMEOUT", OpFeed::STATUS_SERVER_TIMEOUT, runtime);
	PutNumericConstantL(object, "STATUS_LOADING_ERROR", OpFeed::STATUS_LOADING_ERROR, runtime);
	PutNumericConstantL(object, "STATUS_PARSING_ERROR", OpFeed::STATUS_PARSING_ERROR, runtime);
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_FeedEntryList)
	DOM_FUNCTIONS_FUNCTION(DOM_FeedEntryList, DOM_FeedEntryList::item, "item", "n-")
DOM_FUNCTIONS_END(DOM_FeedEntryList)

DOM_FUNCTIONS_START(DOM_Feed)
	DOM_FUNCTIONS_FUNCTION(DOM_Feed, DOM_Feed::subscribe, "subscribe", "-")
	DOM_FUNCTIONS_FUNCTION(DOM_Feed, DOM_Feed::unsubscribe, "unSubscribe", "-")
	DOM_FUNCTIONS_FUNCTION(DOM_Feed, DOM_Feed::getProperty, "getProperty", "s-")
#ifdef DOM_WEBFEEDS_SUBSCRIBE_NATIVE
	DOM_FUNCTIONS_FUNCTION(DOM_Feed, DOM_Feed::subscribeNative, "subscribeNative", "-")
#endif // DOM_WEBFEEDS_SUBSCRIBE_NATIVE
DOM_FUNCTIONS_END(DOM_Feed)

#endif // WEBFEEDS_BACKEND_SUPPORT
