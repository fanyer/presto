/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_SELECTOR_H
#define DOM_SELECTOR_H

#include "modules/dom/src/domobj.h"

#ifdef DOM_SELECTORS_API

/** Implementation of query methods for DocumentSelector and ElementSelector interfaces. */
class DOM_Selector
{
public:
	enum querySelectorData
	{
		QUERY_DOCUMENT	= 0x00,
		QUERY_ELEMENT	= 0x01,
		QUERY_DOCFRAG	= 0x02,
		QUERY_ALL		= 0x04,
		/** Set if this is matchesSelector(). */
		QUERY_MATCHES	= 0x08
	};

	DOM_DECLARE_FUNCTION_WITH_DATA(querySelector);
};

#endif // DOM_SELECTORS_API
#endif // DOM_SELECTOR_H
