/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Morten Stenshorne
*/

#ifndef UNIX__IMAGECACHE_H__
#define UNIX__IMAGECACHE_H__

#include "modules/pi/OpBitmap.h"
#include "modules/util/simset.h"

class ImageCache
{
private:
	Head cache;
	int pixelcount;
	int cachesize;

public:
	class Fragment : public Link
	{
	private:
		OpBitmap *original;
		OpBitmap *fragment;
		int scaled_width, scaled_height; // The scaled size of the whole image
		int x_off, y_off; // Offset (after scaling) into image

	public:
		Fragment(OpBitmap *orig, OpBitmap *fragment,
				 int scaled_width, int scaled_height, int x_off, int y_off)
			: original(orig), fragment(fragment),
			  scaled_width(scaled_width), scaled_height(scaled_height),
			  x_off(x_off), y_off(y_off)
			{ }

		~Fragment();

		bool Matches(OpBitmap *orig, int sw, int sh, int x_off, int y_off,
					 int fragwidth, int fragheight) {
			return orig == original && sw == scaled_width &&
				sh == scaled_height && this->x_off == x_off &&
				this->y_off == y_off && fragwidth == (int)fragment->Width() &&
				fragheight == (int)fragment->Height();
		}

		OpBitmap *GetOriginal() { return original; }
		OpBitmap *GetFragment() { return fragment; }
	};


	ImageCache();
	~ImageCache();

	/**
	 * Set the total cache size in pixels. If the new number of pixels is
	 * smaller than the old one, a cleanup may be performed immediately to
	 * reflect the new settings.
	 */
	void SetCacheSize(int pixels);

	/**
	 * Empty the image cache.
	 */
	void Empty();

	/**
	 * Try to make room for 'min_size_free' pixels in the cache.
	 */
	void CleanUp(int min_size_free);

	/**
	 * Removes the oldest bitmap in the cache that uses a shared memory pixmap
	 * Should only be used when we have exceeded the number of allowed pixmaps
	 * of this kind
	 *
	 * @return TRUE if a image was removed, otherwise FALSE
	 */
	BOOL RemoveSharedMemoryBitmap();

	/**
	 * Add scaled OpBitmap object to cache. From now on, ImageCache owns
	 * this object. Don't ever use the pointer again, unless it is returned
	 * from FindScaling().
	 */
	void Add(OpBitmap *original, OpBitmap *fragment,
			 int scaled_width, int scaled_height, int x_off, int y_off);

	/**
	 * Search the cache for a specific bitmap of a specific size.
	 * @return pointer to OpBitmap if found, NULL otherwise. The
	 * pointer is invalid after next call to any method in this class,
	 * except for the Find*() methods.
	 */
	OpBitmap *Find(OpBitmap *original,
				   int scaled_width, int scaled_height,
				   int x_off, int y_off, int fragwidth, int fragheight);

	/**
	 * Search the cache for a specific bitmap of a specific size.
	 * @return pointer to OpBitmap if found, NULL otherwise. The
	 * pointer is invalid after next call to any method in this class,
	 * except for the Find*() methods.
	 */
	OpBitmap *Find(OpBitmap* original, int scaled_width, int scaled_height, const OpRect& rect);

	/**
	 * Delete everything related to a specific bitmap from the cache. Must be
	 * called before any OpBitmap (that may be involved with this cache) is
	 * deleted. It should also be called when the content of the OpBitmap has
	 * been changed.
	 */
	void Remove(OpBitmap *bitmap);
};

#endif // UNIX__IMAGECACHE_H__
