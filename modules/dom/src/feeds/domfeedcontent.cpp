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

#include "modules/dom/src/feeds/domfeedcontent.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/webfeeds/webfeeds_api.h"

/* virtual */
DOM_FeedContent::~DOM_FeedContent()
{
	if (m_content)
	{
		OP_ASSERT(m_content->GetDOMObject(GetEnvironment()) == *this);
		m_content->SetDOMObject(NULL, GetEnvironment());
		m_content->DecRef();
	}
}

/* static */ OP_STATUS
DOM_FeedContent::Make(DOM_FeedContent *&out_content, OpFeedContent *content, DOM_Runtime *runtime)
{
	OP_ASSERT(!content->GetDOMObject(runtime->GetEnvironment()));

	RETURN_IF_ERROR(DOMSetObjectRuntime(out_content = OP_NEW(DOM_FeedContent, ()), runtime, runtime->GetPrototype(DOM_Runtime::FEEDCONTENT_PROTOTYPE), "FeedContent"));
	out_content->m_content = content;
	content->IncRef();
	content->SetDOMObject(*out_content, runtime->GetEnvironment());

	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_FeedContent::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_data:
		{
			if (m_content->IsBinary())
			{
				const unsigned char *buf = NULL;
				UINT len = 0;
				m_content->Data(buf, len);
				// TODO: figure out how to return binary data
				DOMSetNull(value);
			}
			else
			{
				DOMSetString(value, m_content->Data());
			}
		}
		return GET_SUCCESS;

	case OP_ATOM_type:
		DOMSetString(value, m_content->MIME_Type());
		return GET_SUCCESS;

	case OP_ATOM_isPlainText:
		DOMSetBoolean(value, m_content->IsPlainText());
		return GET_SUCCESS;

	case OP_ATOM_isBinary:
		DOMSetBoolean(value, m_content->IsBinary());
		return GET_SUCCESS;

	case OP_ATOM_isMarkup:
		DOMSetBoolean(value, m_content->IsMarkup());
		return GET_SUCCESS;
	}

	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_FeedContent::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_data:
	case OP_ATOM_type:
	case OP_ATOM_isPlainText:
	case OP_ATOM_isBinary:
	case OP_ATOM_isMarkup:
		return PUT_READ_ONLY;
	}

	return PUT_FAILED;
}

#endif // WEBFEEDS_BACKEND_SUPPORT
