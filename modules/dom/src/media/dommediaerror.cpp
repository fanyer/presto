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

#include "modules/dom/src/media/dommediaerror.h"

/* static */ OP_STATUS
DOM_MediaError::Make(DOM_MediaError*& error, DOM_Environment* environment, MediaError code)
{
	DOM_Runtime *runtime = ((DOM_EnvironmentImpl*)environment)->GetDOMRuntime();

	error = OP_NEW(DOM_MediaError, ());
	if (!error)
		return OpStatus::ERR_NO_MEMORY;

	error->m_code = code;

	RETURN_IF_ERROR(DOMSetObjectRuntime(error, runtime, runtime->GetPrototype(DOM_Runtime::MEDIAERROR_PROTOTYPE), "MediaError"));
	return OpStatus::OK;
}

/* static */ void
DOM_MediaError::ConstructMediaErrorObjectL(ES_Object *object, DOM_Runtime *runtime)
{
	DOM_Object::PutNumericConstantL(object, "MEDIA_ERR_ABORTED", MEDIA_ERR_ABORTED, runtime);
	DOM_Object::PutNumericConstantL(object, "MEDIA_ERR_NETWORK", MEDIA_ERR_NETWORK, runtime);
	DOM_Object::PutNumericConstantL(object, "MEDIA_ERR_DECODE", MEDIA_ERR_DECODE, runtime);
	DOM_Object::PutNumericConstantL(object, "MEDIA_ERR_SRC_NOT_SUPPORTED", MEDIA_ERR_SRC_NOT_SUPPORTED, runtime);
}

/*virtual */ ES_GetState
DOM_MediaError::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_code)
	{
		DOMSetNumber(value, m_code);
		return GET_SUCCESS;
	}
	return DOM_Object::GetName(property_name, value, origining_runtime);
}

/*virtual */ ES_PutState
DOM_MediaError::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_code)
		return PUT_READ_ONLY;
	return DOM_Object::PutName(property_name, value, origining_runtime);
}

#endif // MEDIA_HTML_SUPPORT
