/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#ifndef SVGPAINTSERVER_H
#define SVGPAINTSERVER_H

#ifdef SVG_SUPPORT
#ifdef SVG_SUPPORT_PAINTSERVERS

class VEGAFill;
class VEGATransform;

class SVGPainter;
class SVGPaintNode;

/** An abstract representation of a "paint server" (a complex fill). */
class SVGPaintServer
{
public:
	static void IncRef(SVGPaintServer* p)
	{
		if (p)
			p->m_refcount++;
	}
	static void DecRef(SVGPaintServer* p)
	{
		if (p && --p->m_refcount == 0)
			OP_DELETE(p);
	}
	static void AssignRef(SVGPaintServer*& p, SVGPaintServer* q)
	{
		if (p == q)
			return;

		DecRef(p);
		IncRef(p = q);
	}

	/** Get a fill to use for painting <context_node> on
	 * <painter>. Any additional transform (like a
	 * 'gradientTransform') should be returned in <vfilltrans>.
	 * An implementation of this method should create a VEGAFill and
	 * set it up (except for fill-opacity and transform). */
	virtual OP_STATUS GetFill(VEGAFill** vfill, VEGATransform& vfilltrans,
							  SVGPainter* painter, SVGPaintNode* context_node) = 0;

	/** Return the fill acquired from GetFill - the fill should not be
	 * used again after this call.
	 * An implementation of this method is not required to do anything
	 * more involved than destroy the fill. It could also handle
	 * caching of the fills and associated resources. */
	virtual void PutFill(VEGAFill* vfill) = 0;

protected:
	SVGPaintServer() : m_refcount(1) {}
	virtual ~SVGPaintServer() { OP_ASSERT(m_refcount == 0); }

private:
	int m_refcount;
};

#endif // SVG_SUPPORT_PAINTSERVERS
#endif // SVG_SUPPORT
#endif // SVGPAINTSERVER_H
