/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT

#include "searchsuggestionsevent.h"
#include "modules/dom/src/domenvironmentimpl.h"

/**
 * This event class is used to insert search suggestions text (html) into an extisting document.
 * The code for receiving data can be as simple as
 * ..
 * <script type="text/javascript">
 * function some_func(event)
 * {
 *    document.getElementById('searchContainer').innerHTML = event.content;
 * }
 * window.addEventListener('SearchSuggestionsEvent', some_func, false);
 * </script>
 * ..
 * <div id="searchContainer"></div>
 * ..
 */

// static 
OP_STATUS DOM_SearchSuggestionsEvent::Make(DOM_SearchSuggestionsEvent*& event, const OpString& content, DOM_Runtime* runtime)
{
	event = OP_NEW(DOM_SearchSuggestionsEvent, ());
	if (!event)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS rc = event->m_content.Set(content);
	if (OpStatus::IsError(rc))
	{
		OP_DELETE(event);
		return rc;
	}

	rc = DOMSetObjectRuntime(event, runtime, runtime->GetPrototype(DOM_Runtime::SEARCHSUGGESTS_PROTOTYPE), "SearchSuggestionsEvent");
	if (OpStatus::IsError(rc))
	{
		// Event destroyed by DOMSetObjectRuntime on failure
		return rc;
	}

	event->InitEvent(SEARCHSUGGESTS, runtime->GetEnvironment()->GetWindow(), NULL);

	return rc;
}


ES_GetState DOM_SearchSuggestionsEvent::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_content)
	{
		DOMSetString(value, m_content.CStr());
		return GET_SUCCESS;
	}
	return DOM_Event::GetName(property_name, value, origining_runtime);
}



ES_PutState DOM_SearchSuggestionsEvent::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_content)
		return PUT_SUCCESS;

	return DOM_Event::PutName(property_name, value, origining_runtime);
}

#endif
