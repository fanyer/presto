/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_DOMLOCATOR
#define DOM_DOMLOCATOR

class ES_Object;
class DOM_Environment;
class DOM_Node;

class DOM_DOMLocator
{
public:
	static OP_STATUS Make(ES_Object *&location, DOM_EnvironmentImpl *environment, unsigned line, unsigned column, unsigned byteOffset, unsigned charOffset, DOM_Node *relatedNode, const uni_char *uri);
};

#endif // DOM_DOMLOCATOR
