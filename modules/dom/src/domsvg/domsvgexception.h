/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_SVGEXCEPTION
#define DOM_SVGEXCEPTION

class DOM_Object;

class DOM_SVGException
{
public:
	static void ConstructSVGExceptionObjectL(ES_Object *object, DOM_Runtime *runtime);
};

#endif // DOM_DOMEXCEPTION
