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
#include "modules/dom/src/domcore/domerror.h"

/* static */ OP_STATUS
DOM_DOMError::Make(ES_Object *&error, DOM_EnvironmentImpl *environment, ErrorSeverity severity, const uni_char *message, const uni_char *type, ES_Value *relatedException, ES_Object *relatedData, ES_Object *location)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();
	ES_Value value;

	DOM_Object *dom_error;
	RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(dom_error = OP_NEW(DOM_Object, ()), runtime, runtime->GetObjectPrototype(), "DOMError"));
	error = *dom_error;

	DOM_Object::DOMSetNumber(&value, severity);
	RETURN_IF_ERROR(runtime->PutName(error, UNI_L("severity"), value));

	DOM_Object::DOMSetString(&value, message);
	RETURN_IF_ERROR(runtime->PutName(error, UNI_L("message"), value));

	DOM_Object::DOMSetString(&value, type);
	RETURN_IF_ERROR(runtime->PutName(error, UNI_L("type"), value));

	if (relatedException)
		RETURN_IF_ERROR(runtime->PutName(error, UNI_L("relatedException"), *relatedException));

	DOM_Object::DOMSetObject(&value, relatedData);
	RETURN_IF_ERROR(runtime->PutName(error, UNI_L("relatedData"), value));

	DOM_Object::DOMSetObject(&value, location);
	RETURN_IF_ERROR(runtime->PutName(error, UNI_L("location"), value));

	return OpStatus::OK;
}

/* static */ void
DOM_DOMError::ConstructDOMErrorObjectL(ES_Object *object, DOM_Runtime *runtime)
{
	DOM_Object::PutNumericConstantL(object, "SEVERITY_WARNING", SEVERITY_WARNING, runtime);
	DOM_Object::PutNumericConstantL(object, "SEVERITY_ERROR", SEVERITY_ERROR, runtime);
	DOM_Object::PutNumericConstantL(object, "SEVERITY_FATAL_ERROR", SEVERITY_FATAL_ERROR, runtime);
}
