/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_XPATHEXCEPTION
#define DOM_XPATHEXCEPTION

#ifdef DOM3_XPATH

class ES_Object;
class DOM_Runtime;

class DOM_XPathException
{
public:
	static void ConstructXPathExceptionObjectL(ES_Object *object, DOM_Runtime *runtime);
};

#endif // DOM3_XPATH
#endif // DOM_XPATHEXCEPTION
