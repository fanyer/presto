/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_DOMERROR
#define DOM_DOMERROR

#undef SEVERITY_WARNING
#undef SEVERITY_ERROR
#undef SEVERITY_FATAL_ERROR

class ES_Object;
class ES_Value;
class DOM_Environment;

class DOM_DOMError
{
public:
	enum ErrorSeverity
	{
		SEVERITY_WARNING     = 1,
		SEVERITY_ERROR       = 2,
		SEVERITY_FATAL_ERROR = 3
	};

	static OP_STATUS Make(ES_Object *&input, DOM_EnvironmentImpl *environment, ErrorSeverity severity, const uni_char *message, const uni_char *type, ES_Value *relatedException, ES_Object *relatedData, ES_Object *location);

	static void ConstructDOMErrorObjectL(ES_Object *object, DOM_Runtime *runtime);
};

#endif // DOM_DOMERROR
