/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Morten Stenshorne
*/

#ifndef X11_IMAGEMANAGER_H
#define X11_IMAGEMANAGER_H

#include "modules/util/simset.h"


class X11OpBitmap;

class X11ImageManager
{
private:
	Head core_bitmaps;

public:
	X11ImageManager();
	~X11ImageManager();

	void AddImage(X11OpBitmap *bmp);
	void ColorcellsChanged();
};

#endif // X11_IMAGEMANAGER_H
