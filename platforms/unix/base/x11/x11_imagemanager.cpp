/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Morten Stenshorne
 */
#include "core/pch.h"

#ifndef VEGA_OPPAINTER_SUPPORT

#include "platforms/unix/base/x11/x11_imagemanager.h"
#include "platforms/unix/base/x11/x11_opbitmap.h"
#include "platforms/unix/base/x11/x11_opbitmap_internal.h"

#include "platforms/unix/base/common/imagecache.h"

X11ImageManager::X11ImageManager()
{
}

X11ImageManager::~X11ImageManager()
{
}

void X11ImageManager::AddImage(X11OpBitmap *bmp)
{
	bmp->Into(&core_bitmaps);
}

void X11ImageManager::ColorcellsChanged()
{
	g_imagecache->Empty();
	for (X11OpBitmap *b=(X11OpBitmap *)core_bitmaps.First(); b; b=b->Suc())
	{
		b->ColorcellsChanged();
	}
}

#endif // !VEGA_OPPAINTER_SUPPORT
