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

#include "modules/dom/src/feeds/domfeedentry.h"
#include "modules/dom/src/feeds/domfeedcontent.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/webfeeds/webfeeds_api.h"

/* virtual */
DOM_FeedEntry::~DOM_FeedEntry()
{
	if (m_entry)
	{
		OP_ASSERT(m_entry->GetDOMObject(GetEnvironment()) == *this);

		m_entry->SetDOMObject(NULL, GetEnvironment());
		m_entry->DecRef();
	}
}

/* static */ OP_STATUS
DOM_FeedEntry::Make(DOM_FeedEntry *&out_entry, OpFeedEntry *entry, DOM_Runtime *runtime)
{
	// Check that there isn't already a DOM object for
	// this OpFeedEntry. One is more than enough.
	OP_ASSERT(!entry->GetDOMObject(runtime->GetEnvironment()));

	RETURN_IF_ERROR(DOMSetObjectRuntime(out_entry = OP_NEW(DOM_FeedEntry, ()), runtime, runtime->GetPrototype(DOM_Runtime::FEEDENTRY_PROTOTYPE), "FeedEntry"));

	entry->IncRef();
	out_entry->m_entry = entry;
	entry->SetDOMObject(*out_entry, runtime->GetEnvironment());

	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_FeedEntry::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_author:
		DOMSetString(value, m_entry->GetAuthor());
		return GET_SUCCESS;

	case OP_ATOM_id:
		DOMSetNumber(value, m_entry->GetId());
		return GET_SUCCESS;

	case OP_ATOM_keep:
		DOMSetBoolean(value, m_entry->GetKeep());
		return GET_SUCCESS;

	case OP_ATOM_publicationDate:
		{
			ES_Value date_value;
			// double arithmetics
			DOMSetNumber(&date_value, m_entry->GetPublicationDate());
			ES_Object* date_obj;
			GET_FAILED_IF_ERROR(GetRuntime()->CreateNativeObject(date_value,
																ENGINE_DATE_PROTOTYPE,
																&date_obj));
			DOMSetObject(value, date_obj);
		}
		return GET_SUCCESS;

	case OP_ATOM_title:
	case OP_ATOM_content:
		{
			OpFeedContent *content = property_name == OP_ATOM_title ? m_entry->GetTitle() : m_entry->GetContent();
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

	case OP_ATOM_status:
		DOMSetNumber(value, m_entry->GetReadStatus());
		return GET_SUCCESS;

	case OP_ATOM_uri:
		{
			URL pri_link = m_entry->GetPrimaryLink();
			DOMSetString(value, pri_link.GetAttribute(URL::KUniName_With_Fragment_Escaped).CStr());
		}
		return GET_SUCCESS;
	}

	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_FeedEntry::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_author:
	case OP_ATOM_content:
	case OP_ATOM_id:
	case OP_ATOM_publicationDate:
	case OP_ATOM_title:
	case OP_ATOM_uri:
		return PUT_READ_ONLY;

	case OP_ATOM_keep:
		if (value->type != VALUE_BOOLEAN)
			return PUT_NEEDS_BOOLEAN;

		m_entry->SetKeep(value->value.boolean);
		return PUT_SUCCESS;

	case OP_ATOM_status:
		if (value->type != VALUE_NUMBER)
			return PUT_NEEDS_NUMBER;

		int intvalue = (int) value->value.number;

		if (intvalue >= OpFeedEntry::STATUS_UNREAD && intvalue <= OpFeedEntry::STATUS_DELETED)
			m_entry->SetReadStatus((OpFeedEntry::ReadStatus) intvalue);

		return PUT_SUCCESS;
	}

	return PUT_FAILED;
}

/* static */ int
DOM_FeedEntry::getProperty(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(entry, DOM_TYPE_FEEDENTRY, DOM_FeedEntry);
	DOM_CHECK_ARGUMENTS("s");

	DOMSetString(return_value, entry->m_entry->GetProperty(argv[0].value.string));

	return ES_VALUE;
}

/* static */ void
DOM_FeedEntry::ConstructFeedEntryObjectL(ES_Object *object, DOM_Runtime *runtime)
{
	PutNumericConstantL(object, "STATUS_UNREAD", OpFeedEntry::STATUS_UNREAD, runtime);
	PutNumericConstantL(object, "STATUS_READ", OpFeedEntry::STATUS_READ, runtime);
	PutNumericConstantL(object, "STATUS_DELETED", OpFeedEntry::STATUS_DELETED, runtime);
}


#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_FeedEntry)
	DOM_FUNCTIONS_FUNCTION(DOM_FeedEntry, DOM_FeedEntry::getProperty, "getProperty", "s-")
DOM_FUNCTIONS_END(DOM_FeedEntry)

#endif // WEBFEEDS_BACKEND_SUPPORT
