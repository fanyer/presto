/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_LSEXCEPTION
#define DOM_LSEXCEPTION

#ifdef DOM3_LOAD

class DOM_Object;

class DOM_LSException
{
public:
	enum Code
	{
		PARSE_ERR = 81,
		SERIALIZE_ERR
	};

	static void ConstructLSExceptionObjectL(ES_Object *object, DOM_Runtime *runtime);

	static int CallException(DOM_Object *object, Code code, ES_Value *value);
};

#endif // DOM3_LOAD
#endif // DOM_LSEXCEPTION
