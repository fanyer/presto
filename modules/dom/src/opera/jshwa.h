/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_JS_HWA
#define DOM_JS_HWA

#if defined(VEGA_3DDEVICE) && defined(VEGA_BACKENDS_USE_BLOCKLIST)

#include "modules/dom/src/domobj.h"
#include "modules/dom/src/opera/jsopera.h"

/**
 Adds properties to the opera DOM object for getting information about
 hardware acceleration settings and details about the graphics card used.

 Access to these properties will be restricted to Opera's bugwizard and
 related pages.
 */

class JS_HWA
	: public DOM_Object
{
public:
	virtual ~JS_HWA() {}

	static OP_STATUS Make(JS_HWA *&hwa, JS_Opera *opera);

	virtual ES_GetState GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime);
};

#endif // VEGA_3DDEVICE && VEGA_BACKENDS_USE_BLOCKLIST

#endif /* DOM_JS_HWA */
