/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef MEDIA_HTML_SUPPORT

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domglobaldata.h"
#include "modules/dom/src/media/domtimeranges.h"

/* static */ OP_STATUS
DOM_TimeRanges::Make(DOM_TimeRanges*& domranges, DOM_Environment* environment,
					 const OpMediaTimeRanges* ranges)
{
	DOM_Runtime *runtime = ((DOM_EnvironmentImpl*)environment)->GetDOMRuntime();

	domranges = OP_NEW(DOM_TimeRanges, ());
	if (!domranges)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(DOMSetObjectRuntime(domranges, runtime, runtime->GetPrototype(DOM_Runtime::TIMERANGES_PROTOTYPE), "TimeRanges"));

	// make a static array from MediaTimeRanges

	if (!ranges || ranges->Length() == 0)
		return OpStatus::OK;

	domranges->m_data = OP_NEWA(double, 2*ranges->Length());
	if (!domranges->m_data)
		return OpStatus::ERR_NO_MEMORY;

	domranges->m_length = ranges->Length();

	for (UINT i = 0; i < ranges->Length(); i++)
	{
		domranges->m_data[2*i] = ranges->Start(i);
		domranges->m_data[2*i+1] = ranges->End(i);
	}

	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_TimeRanges::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_length)
	{
		DOMSetNumber(value, m_length);
		return GET_SUCCESS;
	}
	return DOM_Object::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_TimeRanges::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_length)
		return PUT_READ_ONLY;
	return DOM_Object::PutName(property_name, value, origining_runtime);
}

/* static */ int
DOM_TimeRanges::start_or_end(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(domranges, DOM_TYPE_TIMERANGES, DOM_TimeRanges);
	DOM_CHECK_ARGUMENTS("n");

	int index = (int)argv[0].value.number;
	if (index < 0 || index >= domranges->m_length)
		return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

	// data is 0 for start, 1 for end...
	DOMSetNumber(return_value, domranges->m_data[2*index+data]);
	return ES_VALUE;
}

DOM_FUNCTIONS_WITH_DATA_START(DOM_TimeRanges)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_TimeRanges, DOM_TimeRanges::start_or_end, 0, "start", "n-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_TimeRanges, DOM_TimeRanges::start_or_end, 1, "end", "n-")
DOM_FUNCTIONS_WITH_DATA_END(DOM_TimeRanges)

#endif // MEDIA_HTML_SUPPORT
