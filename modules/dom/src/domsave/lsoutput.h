/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_LSOUTPUT
#define DOM_LSOUTPUT

#ifdef DOM3_SAVE

class ES_Object;
class DOM_Environment;

class DOM_LSOutput
{
public:
	static OP_STATUS Make(ES_Object *&output, DOM_EnvironmentImpl *environment, const uni_char *systemId = NULL);

	static OP_STATUS GetCharacterStream(ES_Object *&characterStream, ES_Object *output, DOM_EnvironmentImpl *environment);
	static OP_STATUS GetByteStream(ES_Object *&byteStream, ES_Object *output, DOM_EnvironmentImpl *environment);
	static OP_STATUS GetSystemId(const uni_char *&systemId, ES_Object *output, DOM_EnvironmentImpl *environment);
	static OP_STATUS GetEncoding(const uni_char *&encoding, ES_Object *output, DOM_EnvironmentImpl *environment);
};

#endif // DOM3_SAVE
#endif // DOM_LSOUTPUT
