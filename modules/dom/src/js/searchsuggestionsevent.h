/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT

#ifndef SEARCH_SUGGESTIONS_EVENT_H
#define SEARCH_SUGGESTIONS_EVENT_H

#include "modules/dom/src/domevents/domevent.h"

class DOM_SearchSuggestionsEvent : public DOM_Event
{
public:
	~DOM_SearchSuggestionsEvent() {}

	static OP_STATUS Make(DOM_SearchSuggestionsEvent*& event, const OpString& content, DOM_Runtime* runtime);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_SEARCH_SUGGESTIONS_EVENT || DOM_Event::IsA(type); }

private:
	DOM_SearchSuggestionsEvent() {}

private:
	OpString m_content;
};

#endif

#endif // DOC_SEARCH_SUGGESTIONS_SUPPORT
