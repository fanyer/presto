/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef DOM_BASE64_H
#define DOM_BASE64_H
#ifdef URL_UPLOAD_BASE64_SUPPORT

#include "modules/dom/src/domobj.h"

/**
 * Base64 encoding and decoding entrypoints for convenient DOM API uses.
 */
class DOM_Base64
{
public:
	static int btoa(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime);
	static int atob(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime);
};

#endif // URL_UPLOAD_BASE64_SUPPORT
#endif // DOM_BASE64_H
