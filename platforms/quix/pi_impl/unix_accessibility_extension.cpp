/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#include "core/pch.h"

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

# if 0
// This empty implementation seems to only be for FEATURE_UI_TEST
#include "platforms/quix/pi_impl/unix_accessibility_extension.h"

#ifdef FEATURE_UI_TEST
OP_STATUS DesktopAccessibilityExtension::Create(OpAccessibilityAdapter** adapter,
												OpAccessibleItem* accessible_item)
#else
OP_STATUS OpAccessibilityAdapter::Create(OpAccessibilityAdapter** adapter,
										 OpAccessibleItem* accessible_item)
#endif // FEATURE_UI_TEST
{
	*adapter = OP_NEW(UnixAccessibilityExtension, (accessible_item));

	if(*adapter == NULL)
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}


OP_STATUS OpAccessibilityAdapter::SendEvent(OpAccessibleItem* sender, Accessibility::Event evt)
{
	return OpStatus::OK;
}

# endif //0

#endif // ACCESSIBILITY_EXTENSION_SUPPORT
