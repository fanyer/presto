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
#include "modules/dom/src/domcore/domexception.h"

/* static */ void
DOM_DOMException::ConstructDOMExceptionObjectL(ES_Object *object, DOM_Runtime *runtime)
{
	DOM_Object::PutNumericConstantL(object, "INDEX_SIZE_ERR", DOM_Object::INDEX_SIZE_ERR, runtime);
	DOM_Object::PutNumericConstantL(object, "DOMSTRING_SIZE_ERR", DOM_Object::DOMSTRING_SIZE_ERR, runtime);
	DOM_Object::PutNumericConstantL(object, "HIERARCHY_REQUEST_ERR", DOM_Object::HIERARCHY_REQUEST_ERR, runtime);
	DOM_Object::PutNumericConstantL(object, "WRONG_DOCUMENT_ERR", DOM_Object::WRONG_DOCUMENT_ERR, runtime);
	DOM_Object::PutNumericConstantL(object, "INVALID_CHARACTER_ERR", DOM_Object::INVALID_CHARACTER_ERR, runtime);
	DOM_Object::PutNumericConstantL(object, "NO_DATA_ALLOWED_ERR", DOM_Object::NO_DATA_ALLOWED_ERR, runtime);
	DOM_Object::PutNumericConstantL(object, "NO_MODIFICATION_ALLOWED_ERR", DOM_Object::NO_MODIFICATION_ALLOWED_ERR, runtime);
	DOM_Object::PutNumericConstantL(object, "NOT_FOUND_ERR", DOM_Object::NOT_FOUND_ERR, runtime);
	DOM_Object::PutNumericConstantL(object, "NOT_SUPPORTED_ERR",  DOM_Object::NOT_SUPPORTED_ERR, runtime);
	DOM_Object::PutNumericConstantL(object, "INUSE_ATTRIBUTE_ERR", DOM_Object::INUSE_ATTRIBUTE_ERR, runtime);

	// DOM Level 2
	DOM_Object::PutNumericConstantL(object, "INVALID_STATE_ERR", DOM_Object::INVALID_STATE_ERR, runtime);
	DOM_Object::PutNumericConstantL(object, "SYNTAX_ERR", DOM_Object::SYNTAX_ERR, runtime);
	DOM_Object::PutNumericConstantL(object, "INVALID_MODIFICATION_ERR", DOM_Object::INVALID_MODIFICATION_ERR, runtime);
	DOM_Object::PutNumericConstantL(object, "NAMESPACE_ERR", DOM_Object::NAMESPACE_ERR, runtime);
	DOM_Object::PutNumericConstantL(object, "INVALID_ACCESS_ERR", DOM_Object::INVALID_ACCESS_ERR, runtime);

	// DOM Level 3
	DOM_Object::PutNumericConstantL(object, "VALIDATION_ERR", DOM_Object::VALIDATION_ERR, runtime);
	DOM_Object::PutNumericConstantL(object, "TYPE_MISMATCH_ERR", DOM_Object::TYPE_MISMATCH_ERR, runtime);
	DOM_Object::PutNumericConstantL(object, "SECURITY_ERR", DOM_Object::SECURITY_ERR, runtime);
	DOM_Object::PutNumericConstantL(object, "NETWORK_ERR", DOM_Object::NETWORK_ERR, runtime);
	DOM_Object::PutNumericConstantL(object, "ABORT_ERR", DOM_Object::ABORT_ERR, runtime);
	DOM_Object::PutNumericConstantL(object, "URL_MISMATCH_ERR", DOM_Object::URL_MISMATCH_ERR, runtime);
	DOM_Object::PutNumericConstantL(object, "QUOTA_EXCEEDED_ERR", DOM_Object::QUOTA_EXCEEDED_ERR, runtime);
	DOM_Object::PutNumericConstantL(object, "TIMEOUT_ERR", DOM_Object::TIMEOUT_ERR, runtime);
	DOM_Object::PutNumericConstantL(object, "INVALID_NODE_TYPE_ERR", DOM_Object::INVALID_NODE_TYPE_ERR, runtime);
	DOM_Object::PutNumericConstantL(object, "DATA_CLONE_ERR", DOM_Object::DATA_CLONE_ERR, runtime);
}

