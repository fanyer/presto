/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_DOMEXCEPTION
#define DOM_DOMEXCEPTION

class DOM_Object;

class DOM_DOMException
{
public:
	static void ConstructDOMExceptionObjectL(ES_Object *object, DOM_Runtime *runtime);
};

#endif // DOM_DOMEXCEPTION
