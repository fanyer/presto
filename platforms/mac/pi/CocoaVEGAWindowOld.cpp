/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/pi/CocoaVEGAWindow.h"

#ifndef NS4P_COMPONENT_PLUGINS
#include "platforms/mac/util/MachOCompatibility.h"

void CocoaVEGAWindow::DestroyGWorld()
{
#ifndef NP_NO_QUICKDRAW
	if(m_qd_world)
	{
		DisposeGWorld(m_qd_world);
	}
#endif
}

#ifndef NP_NO_QUICKDRAW
GWorldPtr CocoaVEGAWindow::getGWorld()
{
	if(!m_qd_world)
	{
		Rect window_bounds = {0, 0, m_store.height, m_store.width};
		SInt32 rowbytes = static_cast<SInt32>(CGBitmapContextGetBytesPerRow(m_ctx));
		Ptr dataptr = static_cast<Ptr>(CGBitmapContextGetData(m_ctx));
		NewGWorldFromPtr(&m_qd_world, k32BGRAPixelFormat, &window_bounds, NULL, NULL, 0, dataptr, rowbytes);
	}
	return m_qd_world;
}
#endif

#endif // NS4P_COMPONENT_PLUGINS
