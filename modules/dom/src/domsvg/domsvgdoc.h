/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef _DOM_SVG_DOCUMENT_
#define _DOM_SVG_DOCUMENT_

#include "modules/dom/src/domcore/domdoc.h"
#include "modules/svg/svg.h"

class DOM_SVGDocument
	: public DOM_Document
{
public:
	static OP_STATUS Make(DOM_SVGDocument *&document, DOM_DOMImplementation *implementation);

	virtual ES_GetState	GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState	PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_SVG_DOCUMENT || DOM_Document::IsA(type); }

private:

	HTML_Element* GetElement(Markup::Type type);

	DOM_SVGDocument(DOM_DOMImplementation *implementation);
};

#endif
