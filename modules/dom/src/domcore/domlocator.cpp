/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domcore/domlocator.h"
#include "modules/dom/src/domcore/node.h"

/* static */ OP_STATUS
DOM_DOMLocator::Make(ES_Object *&location, DOM_EnvironmentImpl *environment, unsigned line, unsigned column, unsigned byteOffset, unsigned charOffset, DOM_Node *relatedNode, const uni_char *uri)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();
	ES_Value value;

	DOM_Object *dom_location;
	RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(dom_location = OP_NEW(DOM_Object, ()), runtime, runtime->GetObjectPrototype(), "DOMLocator"));
	location = *dom_location;

	DOM_Object::DOMSetNumber(&value, line);
	RETURN_IF_ERROR(runtime->PutName(location, UNI_L("lineNumber"), value, PROP_READ_ONLY));

	DOM_Object::DOMSetNumber(&value, column);
	RETURN_IF_ERROR(runtime->PutName(location, UNI_L("columnNumber"), value, PROP_READ_ONLY));

	DOM_Object::DOMSetNumber(&value, byteOffset);
	RETURN_IF_ERROR(runtime->PutName(location, UNI_L("byteOffset"), value, PROP_READ_ONLY));

	DOM_Object::DOMSetNumber(&value, charOffset);
	RETURN_IF_ERROR(runtime->PutName(location, UNI_L("utf16Offset"), value, PROP_READ_ONLY));

	DOM_Object::DOMSetObject(&value, relatedNode);
	RETURN_IF_ERROR(runtime->PutName(location, UNI_L("relatedNode"), value, PROP_READ_ONLY));

	DOM_Object::DOMSetString(&value, uri);
	RETURN_IF_ERROR(runtime->PutName(location, UNI_L("uri"), value, PROP_READ_ONLY));

	return OpStatus::OK;
}

