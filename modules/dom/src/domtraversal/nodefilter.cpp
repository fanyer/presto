/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef DOM2_TRAVERSAL

#include "modules/dom/src/domtraversal/nodefilter.h"

#include "modules/dom/src/domobj.h"

/* static */ void
DOM_NodeFilter::ConstructNodeFilterObjectL(ES_Object *object, DOM_Runtime *runtime)
{
	DOM_Object::PutNumericConstantL(object, "FILTER_ACCEPT", FILTER_ACCEPT, runtime);
	DOM_Object::PutNumericConstantL(object, "FILTER_REJECT", FILTER_REJECT, runtime);
	DOM_Object::PutNumericConstantL(object, "FILTER_SKIP", FILTER_SKIP, runtime);

	DOM_Object::PutNumericConstantL(object, "SHOW_ALL", static_cast<UINT32>(SHOW_ALL), runtime);
	DOM_Object::PutNumericConstantL(object, "SHOW_ELEMENT", SHOW_ELEMENT, runtime);
	DOM_Object::PutNumericConstantL(object, "SHOW_ATTRIBUTE", SHOW_ATTRIBUTE, runtime);
	DOM_Object::PutNumericConstantL(object, "SHOW_TEXT", SHOW_TEXT, runtime);
	DOM_Object::PutNumericConstantL(object, "SHOW_CDATA_SECTION", SHOW_CDATA_SECTION, runtime);
	DOM_Object::PutNumericConstantL(object, "SHOW_ENTITY_REFERENCE", SHOW_ENTITY_REFERENCE, runtime);
	DOM_Object::PutNumericConstantL(object, "SHOW_ENTITY", SHOW_ENTITY, runtime);
	DOM_Object::PutNumericConstantL(object, "SHOW_PROCESSING_INSTRUCTION", SHOW_PROCESSING_INSTRUCTION, runtime);
	DOM_Object::PutNumericConstantL(object, "SHOW_COMMENT", SHOW_COMMENT, runtime);
	DOM_Object::PutNumericConstantL(object, "SHOW_DOCUMENT", SHOW_DOCUMENT, runtime);
	DOM_Object::PutNumericConstantL(object, "SHOW_DOCUMENT_TYPE", SHOW_DOCUMENT_TYPE, runtime);
	DOM_Object::PutNumericConstantL(object, "SHOW_DOCUMENT_FRAGMENT", SHOW_DOCUMENT_FRAGMENT, runtime);
	DOM_Object::PutNumericConstantL(object, "SHOW_NOTATION", SHOW_NOTATION, runtime);
}

#endif // DOM2_TRAVERSAL
