/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domstylesheets/mediaquerylist.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/style/css_collection.h"

/* static */ OP_STATUS
DOM_MediaQueryList::Make(DOM_MediaQueryList*& mediaquerylist, DOM_Runtime* runtime, CSSCollection* collection, const uni_char* media_string)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(mediaquerylist = OP_NEW(DOM_MediaQueryList, ()), runtime, runtime->GetPrototype(DOM_Runtime::MEDIAQUERYLIST_PROTOTYPE), "MediaQueryList"));
	CSS_MediaQueryList* css_mql = collection->AddMediaQueryList(media_string, mediaquerylist);
	if (css_mql)
	{
		mediaquerylist->m_mediaquerylist = css_mql;
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR_NO_MEMORY;
}

/* virtual */
DOM_MediaQueryList::~DOM_MediaQueryList()
{
	OP_DELETE(m_mediaquerylist);
}

/* virtual */ ES_GetState
DOM_MediaQueryList::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_matches)
	{
		DOMSetBoolean(value, m_mediaquerylist && m_mediaquerylist->Matches());
		return GET_SUCCESS;
	}
	else if (property_name == OP_ATOM_media)
	{
		if (value)
		{
			TempBuffer* buf = GetEmptyTempBuf();
			if (m_mediaquerylist)
				GET_FAILED_IF_ERROR(m_mediaquerylist->GetMedia(buf));
			DOMSetString(value, buf);
		}
		return GET_SUCCESS;
	}
	else
		return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_MediaQueryList::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_matches || property_name == OP_ATOM_media)
		return PUT_READ_ONLY;
	else
		return PUT_FAILED;
}

/* virtual */ void
DOM_MediaQueryList::GCTrace()
{
	for (UINT32 i = 0; i < m_handlers.GetCount(); i++)
		GCMark(m_handlers.Get(i));
}

/* static */ int
DOM_MediaQueryList::addListener(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(mediaquerylist, DOM_TYPE_MEDIAQUERYLIST, DOM_MediaQueryList);
	DOM_CHECK_ARGUMENTS("f");

	if (!mediaquerylist->OriginCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	ES_Object* handler = argv[0].value.object;

	for (UINT32 i = 0; i < mediaquerylist->m_handlers.GetCount(); i++)
	{
		if (mediaquerylist->m_handlers.Get(i) == handler)
			return ES_FAILED;
	}

	CALL_FAILED_IF_ERROR(mediaquerylist->m_handlers.Add(handler));

	return ES_FAILED;
}

/* static */ int
DOM_MediaQueryList::removeListener(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(mediaquerylist, DOM_TYPE_MEDIAQUERYLIST, DOM_MediaQueryList);
	DOM_CHECK_ARGUMENTS("f");

	if (!mediaquerylist->OriginCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	OpStatus::Ignore(mediaquerylist->m_handlers.RemoveByItem(argv[0].value.object));

	return ES_FAILED;
}

/* virtual */ void
DOM_MediaQueryList::OnChanged(const CSS_MediaQueryList* media_query_list)
{
	// Invoke all listeners.
	for (UINT32 i = 0; i < m_handlers.GetCount(); i++)
	{
		ES_Value arguments[1];
		DOMSetObject(&arguments[0], *this);
		OpStatus::Ignore(GetEnvironment()->GetAsyncInterface()->CallFunction(m_handlers.Get(i), *this, 1, arguments));
	}
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_MediaQueryList)
	DOM_FUNCTIONS_FUNCTION(DOM_MediaQueryList, DOM_MediaQueryList::addListener, "addListener", "f")
	DOM_FUNCTIONS_FUNCTION(DOM_MediaQueryList, DOM_MediaQueryList::removeListener, "removeListener", "f")
DOM_FUNCTIONS_END(DOM_MediaQueryList)
