/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef NEARBY_ELEMENT_DETECTION

#include "modules/widgets/finger_touch/element_expander.h"

#include "modules/widgets/finger_touch/element_expander_impl.h"
#include "modules/pi/OpScreenInfo.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"

ElementExpander* ElementExpander::Create(FramesDocument* document, OpPoint origin, unsigned int radius)
{
	return OP_NEW(ElementExpanderImpl, (document, origin, radius));
}

/* static */
int ElementExpander::GetFingertipPixelRadius(OpWindow* opWindow)
{
	static const int POINTS_PER_INCH = 72;

	UINT32 horizontal_dpi;
	UINT32 vertical_dpi;
	g_op_screen_info->GetDPI(&horizontal_dpi, &vertical_dpi, opWindow);
	
	return g_pccore->GetIntegerPref(PrefsCollectionCore::NearbyElementsRadius) * horizontal_dpi / POINTS_PER_INCH;
}

/* static */
BOOL ElementExpander::IsEnabled()
{
	return g_pccore->GetIntegerPref(PrefsCollectionCore::DetectNearbyElements) != 0;
}

#endif // NEARBY_ELEMENT_DETECTION
