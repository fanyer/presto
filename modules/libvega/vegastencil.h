/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGASTENCIL_H
#define VEGASTENCIL_H

#ifdef VEGA_SUPPORT

#include "modules/libvega/vegarendertarget.h"
class VEGAImage;

/** The stencil buffer is a special render target which can be used as a
 * clipping mask for rendering. Clipping can be per component or for all
 * components. */
class VEGAStencil : public VEGARenderTarget
{
public:
	VEGAStencil() :
		offsetX(0), offsetY(0),
		/* When a stencil buffer is created, it is immediately cleared
		 * using a function that is not affected by and does not
		 * modify the dirty rect.  So the initial dirty rect should be
		 * empty. */
		dirtyLeft(INT_MAX), dirtyRight(0), dirtyTop(INT_MAX), dirtyBottom(0) {}
	virtual ~VEGAStencil(){}

	/** @returns true if the clipping is to be done per component, false
	 * otherwise. */
	bool isComponentBased(){return getColorFormat()!=RT_ALPHA8;/*perComponent;*/}

	/** Set the offset of the start point of this stencil. Offset is only
	 * used when applying the stencil as a mask, not when rendering to it.
	 * The offset should always be negative, since the stencil can only
	 * start before the render target, never after it. */
	void setOffset(int x, int y){offsetX = x; offsetY = y;}

	/** @returns the x offset of the start point of the stencil as set by
	 * setOffset. */
	int getOffsetX(){return offsetX;}

	/** @returns the y offset of the start point of the stencil as set by
	 * setOffset. */
	int getOffsetY(){return offsetY;}

	virtual void markDirty(int left, int right, int top, int bottom)
	{
		if (left < dirtyLeft)
			dirtyLeft = left;
		if (right > dirtyRight)
			dirtyRight = right;
		if (top < dirtyTop)
			dirtyTop = top;
		if (bottom > dirtyBottom)
			dirtyBottom = bottom;
	}

	virtual void unmarkDirty(int left, int right, int top, int bottom)
	{
		/* The dirty rectangle includes the right and bottom edges.
		 * The common convention is not to include the right and
		 * bottom edges.
		 *
		 * So this code does things like "dirtyTop = bottom + 1" where
		 * equivalent code elsewhere would be "dirtyTop = bottom".
		 */
		if (left <= dirtyLeft && right >= dirtyRight)
		{
			if (top <= dirtyTop && bottom >= dirtyBottom)
			{
				resetDirtyRect();
				return;
			}

			if (top <= dirtyTop && bottom >= dirtyTop)
				dirtyTop = bottom + 1;
			else if (bottom >= dirtyBottom && top <= dirtyBottom)
				dirtyBottom = top - 1;
			return;
		}
		if (top <= dirtyTop && bottom >= dirtyBottom)
		{
			if (left <= dirtyLeft && right >= dirtyLeft)
				dirtyLeft = right + 1;
			if (right >= dirtyRight && left <= dirtyRight)
				dirtyRight = left - 1;
		}
	}

	/** Get the dirty rectangle. The area which has ever been rendered to.
	 * Everything not in this area has to be clipped. */
	virtual bool getDirtyRect(int& left, int& right, int& top, int& bottom)
	{
		left = dirtyLeft;
		right = dirtyRight;
		top = dirtyTop;
		bottom = dirtyBottom;
		return true;
	}

	/** Reset the dirty rectangle to its initial values. */
	virtual void resetDirtyRect()
	{
		dirtyLeft = INT_MAX;
		dirtyRight = 0;
		dirtyTop = INT_MAX;
		dirtyBottom = 0;
	}
private:
	int offsetX;
	int offsetY;

	int dirtyLeft;
	int dirtyRight;
	int dirtyTop;
	int dirtyBottom;
};

#endif // VEGA_SUPPORT
#endif // VEGASTENCIL_H

