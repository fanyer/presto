/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_LSINPUT
#define DOM_LSINPUT

#ifdef DOM3_LOAD

class ES_Object;
class DOM_Environment;

class DOM_LSInput
{
public:
	static OP_STATUS Make(ES_Object *&input, DOM_EnvironmentImpl *environment, const uni_char *stringData = NULL, const uni_char *systemId = NULL);

	static OP_STATUS GetByteStream(ES_Object *&byteStream, ES_Object *input, DOM_EnvironmentImpl *environment);
	static OP_STATUS GetStringData(const uni_char *&stringData, ES_Object *input, DOM_EnvironmentImpl *environment);
	static OP_STATUS GetSystemId(const uni_char *&systemId, ES_Object *input, DOM_EnvironmentImpl *environment);
};

#endif // DOM3_LOAD
#endif // DOM_LSINPUT
