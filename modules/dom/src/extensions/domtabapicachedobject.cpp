/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA. All rights reserved.
**
** This file is part of the Opera web browser. It may not be distributed
** under any circumstances.
*/

#include <core/pch.h>

#ifdef DOM_EXTENSIONS_TAB_API_SUPPORT
#include "modules/dom/src/extensions/domtabapicachedobject.h"
#include "modules/dom/src/extensions/domextension_background.h"
#include "modules/dom/src/extensions/domtabapicache.h"

/* virtual */
DOM_TabApiCachedObject::~DOM_TabApiCachedObject()
{
	if (DOM_ExtensionBackground* background = m_extension_support->GetBackground())
		background->GetTabApiCache()->OnCachedObjectDestroyed(this);
}

#endif // DOM_EXTENSIONS_TAB_API_SUPPORT
