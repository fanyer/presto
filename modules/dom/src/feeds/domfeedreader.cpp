/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#if defined(WEBFEEDS_BACKEND_SUPPORT) && defined(WEBFEEDS_EXTERNAL_READERS)

#include "modules/dom/src/feeds/domfeedreader.h"

#include "modules/webfeeds/webfeeds_api.h"

/* static */ OP_STATUS
DOM_FeedReaderList::Make(DOM_FeedReaderList *&list, DOM_Runtime *runtime)
{
	return DOMSetObjectRuntime(list = OP_NEW(DOM_FeedReaderList, ()), runtime,
			runtime->GetPrototype(DOM_Runtime::FEEDREADERLIST_PROTOTYPE), "FeedReaderList");
}

/* virtual */ ES_GetState
DOM_FeedReaderList::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_length)
	{
		DOMSetNumber(value, g_webfeeds_api->GetExternalReaderCount());
		return GET_SUCCESS;
	}

	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_FeedReaderList::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_length)
		return PUT_READ_ONLY;

	return PUT_FAILED;
}

/* virtual */ ES_GetState
DOM_FeedReaderList::GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (value)
	{
		if (property_index < 0)
			return GET_FAILED;

		if (OpStatus::IsMemoryError(GetItem(property_index, value)))
			return GET_NO_MEMORY;
	}

	return GET_SUCCESS;
}

/* static */ int
DOM_FeedReaderList::item(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(list, DOM_TYPE_FEEDREADERLIST, DOM_FeedReaderList);
	DOM_CHECK_ARGUMENTS("n");

	if (argv[0].value.number >= 0)
	{
		if (OpStatus::IsMemoryError(list->GetItem(static_cast<unsigned>(argv[0].value.number), return_value)))
			return ES_NO_MEMORY;
	}
	else
	{
		DOMSetNull(return_value);
	}

	return ES_VALUE;
}

OP_STATUS
DOM_FeedReaderList::GetItem(unsigned index, ES_Value *value)
{
	if (index < g_webfeeds_api->GetExternalReaderCount())
	{
		unsigned id = g_webfeeds_api->GetExternalReaderIdByIndex(index);
		DOM_FeedReader *dom_feedreader;
		RETURN_IF_ERROR(DOM_FeedReader::Make(dom_feedreader, id, GetRuntime()));

		DOMSetObject(value, dom_feedreader);
	}
	else
	{
		DOMSetNull(value);
	}

	return OpStatus::OK;
}

/* virtual */ ES_DeleteStatus
DOM_FeedReaderList::DeleteIndex(int property_index, ES_Runtime* origining_runtime)
{
#ifdef WEBFEEDS_ADD_REMOVE_EXTERNAL_READERS
	if (property_index >= 0 && static_cast<unsigned int>(property_index) < g_webfeeds_api->GetExternalReaderCount())
		DELETE_FAILED_IF_MEMORY_ERROR(g_webfeeds_api->DeleteExternalReader(g_webfeeds_api->GetExternalReaderIdByIndex(property_index)));
#endif // WEBFEEDS_ADD_REMOVE_EXTERNAL_READERS

	return DELETE_OK;
}

/* static */ OP_STATUS
DOM_FeedReader::Make(DOM_FeedReader *&feedreader, unsigned id, DOM_Runtime *runtime)
{
	return DOMSetObjectRuntime(feedreader = OP_NEW(DOM_FeedReader, (id)), runtime,
							   runtime->GetPrototype(DOM_Runtime::FEEDREADER_PROTOTYPE), "FeedReader");
}

/* virtual */ ES_GetState
DOM_FeedReader::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_name)
	{
		OpString name;
		GET_FAILED_IF_ERROR(g_webfeeds_api->GetExternalReaderName(m_id, name));

		TempBuffer* buffer = GetEmptyTempBuf();
		GET_FAILED_IF_ERROR(buffer->Append(name.CStr()));

		DOMSetString(value, buffer);
		return GET_SUCCESS;
	}

	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_FeedReader::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_name)
		return PUT_READ_ONLY;

	return PUT_FAILED;
}

/* static */ int
DOM_FeedReader::getTargetURL(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(reader, DOM_TYPE_FEEDREADER, DOM_FeedReader);
	DOM_CHECK_ARGUMENTS("s");

	URL feed_url = g_url_api->GetURL(argv[0].value.string);
	URL target_url;

	CALL_FAILED_IF_ERROR(g_webfeeds_api->GetExternalReaderTargetURL(reader->m_id, feed_url, target_url));

	OpString url_str;
	CALL_FAILED_IF_ERROR(target_url.GetAttribute(URL::KUniName_With_Fragment_Escaped, url_str));

	TempBuffer* buffer = GetEmptyTempBuf();
	CALL_FAILED_IF_ERROR(buffer->Append(url_str.CStr()));

	DOMSetString(return_value, buffer);

	return ES_VALUE;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_FeedReaderList)
	DOM_FUNCTIONS_FUNCTION(DOM_FeedReaderList, DOM_FeedReaderList::item, "item", "n-")
DOM_FUNCTIONS_END(DOM_FeedReaderList)

DOM_FUNCTIONS_START(DOM_FeedReader)
	DOM_FUNCTIONS_FUNCTION(DOM_FeedReader, DOM_FeedReader::getTargetURL, "getTargetURL", "s-")
DOM_FUNCTIONS_END(DOM_FeedReader)

#endif // defined(WEBFEEDS_BACKEND_SUPPORT) && defined(WEBFEEDS_EXTERNAL_READERS)
