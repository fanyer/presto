/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOMXMLDOCUMENT_H
#define DOMXMLDOCUMENT_H

#include "modules/dom/src/domcore/domdoc.h"

class DOM_XMLDocument
	: public DOM_Document
{
protected:
	DOM_XMLDocument(DOM_DOMImplementation *implementation);

#ifdef DOM3_LOAD
	BOOL async;
#endif // DOM3_LOAD

public:
	static OP_STATUS Make(DOM_Document *&document, DOM_DOMImplementation *implementation, BOOL create_placeholder = FALSE);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState	PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_XML_DOCUMENT || DOM_Document::IsA(type); }

#ifdef DOM3_LOAD
	DOM_DECLARE_FUNCTION(load);
#endif // DOM3_LOAD

	enum {
		FUNCTIONS_BASIC = 0,
#ifdef DOM3_LOAD
		FUNCTIONS_load,
#endif // DOM3_LOAD
		FUNCTIONS_ARRAY_SIZE
	};
};

#endif /* DOMXMLDOCUMENT_H */
