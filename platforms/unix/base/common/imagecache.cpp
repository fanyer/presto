/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Morten Stenshorne
*/

#include "core/pch.h"

#ifndef VEGA_OPPAINTER_SUPPORT

#include "imagecache.h"

#include "modules/pi/OpBitmap.h"

#include "platforms/unix/base/x11/x11_opbitmap.h"

ImageCache::Fragment::~Fragment()
{
	OP_DELETE(fragment);
}


ImageCache::ImageCache()
	: pixelcount(0), cachesize(4000000)
{
}

ImageCache::~ImageCache()
{
	Empty();
}

void ImageCache::SetCacheSize(int pixels)
{
	cachesize = pixels;
}

void ImageCache::Empty()
{
	cache.Clear();
}

void ImageCache::CleanUp(int min_size_free)
{
	Fragment *f = (Fragment *)cache.Last();
	while (f && pixelcount + min_size_free > cachesize)
	{
		OpBitmap *bm = f->GetFragment();
		pixelcount -= bm->Width() * bm->Height();
		Fragment *prev = (Fragment *)f->Pred();
		f->Out();
		OP_DELETE(f);
		f = prev;
	}
}


BOOL ImageCache::RemoveSharedMemoryBitmap()
{
	Fragment *f = (Fragment *)cache.Last();
	while (f)
	{
		X11OpBitmap* bm = (X11OpBitmap*)f->GetFragment();
		if( bm->HasSharedMemory() )
		{
			f->Out();
			OP_DELETE(f);
			return TRUE;
		}
		f = (Fragment *)f->Pred();
	}

	return FALSE;
}




void ImageCache::Add(OpBitmap *original, OpBitmap *fragment,
					 int scaled_width, int scaled_height,
					 int x_off, int y_off)
{
	//printf("ImageCache::Add: Disabled\n");
	//OP_DELETE(fragment);
	//return;

	if (Find(original, scaled_width, scaled_height, x_off, y_off,
			 fragment->Width(), fragment->Height()))
	{
		// Already in cache.
		OP_ASSERT(FALSE);
		return;
	}

	int this_size = fragment->Width() * fragment->Height();
	if (this_size * 3 / 2 > cachesize)
	{
		// Very big image. Don't cache it.
		OP_DELETE(fragment);
		return;
	}
	if (pixelcount + this_size > cachesize)
	{
		// Cache too full. Try to clean up.
		CleanUp(this_size);
	}

	if (pixelcount + this_size > cachesize)
	{
		// No room in cache... not even after cleanup.
		OP_DELETE(fragment);
		return;
	}

	// Since we got this far, it means that the scaling should be cached.
	Fragment *elm = OP_NEW(Fragment, (original, fragment,
								 scaled_width, scaled_height,
								 x_off, y_off));
	if (!elm)
	{
		// Non-critical OOM
		OP_DELETE(fragment);
		return;
	}
	elm->IntoStart(&cache);

	pixelcount += this_size;
}

OpBitmap *ImageCache::Find(OpBitmap* original, int scaled_width, int scaled_height, const OpRect& rect)
{
	return Find(original, scaled_width, scaled_height, rect.x, rect.y, rect.width, rect.height);
}

OpBitmap *ImageCache::Find(OpBitmap *original,
						   int scaled_width, int scaled_height,
						   int x_off, int y_off, int fragwidth, int fragheight)
{
	for (Fragment *f=(Fragment *)cache.First(); f; f=(Fragment *)f->Suc())
	{
		if (f->Matches(original, scaled_width, scaled_height, x_off, y_off,
					   fragwidth, fragheight))
			return f->GetFragment();
	}

	return 0;
}

void ImageCache::Remove(OpBitmap *bitmap)
{
	Fragment *f = (Fragment *)cache.First();
	while (f)
	{
		Fragment *next = (Fragment *)f->Suc();
		if (f->GetOriginal() == bitmap)
		{
			OpBitmap *bm = f->GetFragment();
			pixelcount -= bm->Width() * bm->Height();
			f->Out();
			OP_DELETE(f);
		}
		f = next;
	}
}

#endif // !VEGA_OPPAINTER_SUPPORT

