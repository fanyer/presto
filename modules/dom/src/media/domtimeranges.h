/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_TIMERANGES
#define DOM_TIMERANGES

#ifdef MEDIA_HTML_SUPPORT

#include "modules/dom/src/domobj.h"
#include "modules/media/mediaplayer.h"

class DOM_TimeRanges
	: public DOM_Object
{
private:
	DOM_TimeRanges() : m_length(0), m_data(NULL) {}
	virtual ~DOM_TimeRanges() { OP_DELETEA(m_data); }

	int m_length;
	double *m_data;

public:
	static OP_STATUS Make(DOM_TimeRanges*& domranges, DOM_Environment* environment,
						  const OpMediaTimeRanges* ranges);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_TIMERANGES || DOM_Object::IsA(type); }

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	static DOM_FunctionWithDataImpl start_or_end;
	enum { FUNCTIONS_WITH_DATA_ARRAY_SIZE = 3 };

};

#endif // MEDIA_HTML_SUPPORT
#endif // DOM_TIMERANGES
