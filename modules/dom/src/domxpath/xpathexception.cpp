/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef DOM3_XPATH

#include "modules/dom/src/domxpath/xpathexception.h"
#include "modules/dom/src/domobj.h"

/* static */ void
DOM_XPathException::ConstructXPathExceptionObjectL(ES_Object *object, DOM_Runtime *runtime)
{
	DOM_Object::PutNumericConstantL(object, "INVALID_EXPRESSION_ERR", DOM_Object::INVALID_EXPRESSION_ERR, runtime);
	DOM_Object::PutNumericConstantL(object, "TYPE_ERR", DOM_Object::TYPE_ERR, runtime);
}

#endif // DOM3_XPATH
