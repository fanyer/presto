/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domstylesheets/stylesheet.h"
#include "modules/dom/src/domcore/element.h"
#include "modules/dom/src/domstylesheets/medialist.h"
#include "modules/dom/src/domcss/cssrule.h"

#include "modules/logdoc/htm_elm.h"

DOM_MediaList::DOM_MediaList(DOM_Object* owner)
	: m_owner(owner)
{
}

/* static */ OP_STATUS
DOM_MediaList::Make(DOM_MediaList*& medialist, DOM_StyleSheet* stylesheet)
{
	DOM_Runtime *runtime = stylesheet->GetRuntime();
	RETURN_IF_ERROR(DOMSetObjectRuntime(medialist = OP_NEW(DOM_MediaList, (stylesheet)), runtime, runtime->GetPrototype(DOM_Runtime::MEDIALIST_PROTOTYPE), "MediaList"));
	CSS_DOMMediaList* css_list = OP_NEW(CSS_DOMMediaList, (runtime->GetEnvironment(), stylesheet->GetOwnerNode()->GetThisElement()));
	if (!css_list)
		return OpStatus::ERR_NO_MEMORY;
	else
	{
		medialist->m_medialist = css_list;
		return OpStatus::OK;
	}
}

/* static */ OP_STATUS
DOM_MediaList::Make(DOM_MediaList*& medialist, DOM_CSSRule* rule)
{
	DOM_Runtime *runtime = rule->GetRuntime();
	RETURN_IF_ERROR(DOMSetObjectRuntime(medialist = OP_NEW(DOM_MediaList, (rule)), runtime, runtime->GetPrototype(DOM_Runtime::MEDIALIST_PROTOTYPE), "MediaList"));
	CSS_DOMMediaList* css_list = OP_NEW(CSS_DOMMediaList, (runtime->GetEnvironment(), rule->GetCSS_DOMRule()));
	if (!css_list)
		return OpStatus::ERR_NO_MEMORY;
	else
	{
		medialist->m_medialist = css_list;
		return OpStatus::OK;
	}
}

/* virtual */
DOM_MediaList::~DOM_MediaList()
{
	OP_DELETE(m_medialist);
}

/* virtual */ ES_GetState
DOM_MediaList::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_length)
	{
		DOMSetNumber(value, m_medialist->GetMediaCount());
		return GET_SUCCESS;
	}
	else if (property_name == OP_ATOM_mediaText)
	{
		TempBuffer* buf = GetEmptyTempBuf();
		GET_FAILED_IF_ERROR(m_medialist->GetMedia(buf));
		DOMSetString(value, buf);
		return GET_SUCCESS;
	}
	else
		return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_MediaList::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_length)
		return PUT_READ_ONLY;
	else if (property_name == OP_ATOM_mediaText)
		if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;
		else
		{
			CSS_DOMException exception;
			OP_STATUS stat = m_medialist->SetMedia(value->value.string, exception);
			if (stat == OpStatus::ERR)
				if (exception == CSS_DOMEXCEPTION_SYNTAX_ERR)
					DOM_PUTNAME_DOMEXCEPTION(SYNTAX_ERR);
			PUT_FAILED_IF_ERROR(stat);
			return PUT_SUCCESS;
		}
	else
		return PUT_FAILED;
}

/* virtual */ void
DOM_MediaList::GCTrace()
{
	GCMark(m_owner);
}

/* static */ int
DOM_MediaList::appendMedium(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(medialist, DOM_TYPE_MEDIALIST, DOM_MediaList);
	DOM_CHECK_ARGUMENTS("s");

	if (!medialist->OriginCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	CSS_DOMException exception;
	OP_STATUS stat = medialist->m_medialist->AppendMedium(argv[0].value.string, exception);
	if (stat == OpStatus::ERR)
		if (exception == CSS_DOMEXCEPTION_INVALID_CHARACTER_ERR)
			return DOM_CALL_DOMEXCEPTION(INVALID_CHARACTER_ERR);
	CALL_FAILED_IF_ERROR(stat);
	return ES_FAILED;
}

/* static */ int
DOM_MediaList::deleteMedium(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(medialist, DOM_TYPE_MEDIALIST, DOM_MediaList);
	DOM_CHECK_ARGUMENTS("s");

	if (!medialist->OriginCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	CSS_DOMException exception;
	OP_STATUS stat = medialist->m_medialist->DeleteMedium(argv[0].value.string, exception);
	if (stat == OpStatus::ERR)
		if (exception == CSS_DOMEXCEPTION_NOT_FOUND_ERR)
			return DOM_CALL_DOMEXCEPTION(NOT_FOUND_ERR);
	CALL_FAILED_IF_ERROR(stat);
	return ES_FAILED;
}

/* virtual */ ES_GetState
DOM_MediaList::GetIndex(int property_index, ES_Value* value, ES_Runtime *origining_runtime)
{
	CSS_DOMException exception;
	TempBuffer* buf = GetEmptyTempBuf();
	OP_STATUS stat = m_medialist->GetMedium(buf, property_index, exception);
	if (stat == OpStatus::ERR)
	{
		DOMSetNull(value);
	}
	else
	{
		GET_FAILED_IF_ERROR(stat);
		DOMSetString(value, buf);
	}
	return GET_SUCCESS;
}

/* virtual */ ES_GetState
DOM_MediaList::GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime)
{
	count = m_medialist->GetMediaCount();
	return GET_SUCCESS;
}

/* static */ int
DOM_MediaList::item(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(medialist, DOM_TYPE_MEDIALIST, DOM_MediaList);
	DOM_CHECK_ARGUMENTS("n");

	if (!medialist->OriginCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	DOMSetNull(return_value);

	int index = (int)argv[0].value.number;

	if (argv[0].value.number == (double) index)
		RETURN_GETNAME_AS_CALL(medialist->GetIndex(index, return_value, origining_runtime));
	else
		return ES_VALUE;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_MediaList)
	DOM_FUNCTIONS_FUNCTION(DOM_MediaList, DOM_MediaList::appendMedium, "appendMedium", "s-")
	DOM_FUNCTIONS_FUNCTION(DOM_MediaList, DOM_MediaList::deleteMedium, "deleteMedium", "s-")
	DOM_FUNCTIONS_FUNCTION(DOM_MediaList, DOM_MediaList::item, "item", "n-")
DOM_FUNCTIONS_END(DOM_MediaList)

