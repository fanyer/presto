/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGAGRADIENT_H
#define VEGAGRADIENT_H

#ifdef VEGA_SUPPORT

#include "modules/libvega/vegafill.h"
#include "modules/libvega/vegafixpoint.h"
#include "modules/libvega/vegatransform.h"

class VEGATransform;
class VEGAImage;

class VEGAGradient : public VEGAFill
{
public:
	enum
	{
		MAX_STOPS = (1 << 15) - 1
	};

	VEGAGradient();
	~VEGAGradient();

#ifdef VEGA_3DDEVICE
	virtual const VEGATransform& getTexTransform();
	virtual VEGA3dTexture* getTexture();
	virtual VEGA3dShaderProgram* getCustomShader();
	virtual bool usePremulRenderState() { return !is_premultiplied; }
	virtual VEGA3dVertexLayout* getCustomVertexLayout();
#endif // VEGA_3DDEVICE

	virtual OP_STATUS prepare();
	virtual void apply(VEGAPixelAccessor color, struct VEGASpanInfo& span);

#ifdef VEGA_USE_GRADIENT_CACHE
	virtual VEGAFill* getCache(VEGARendererBackend* backend, VEGA_FIX gx, VEGA_FIX gy, VEGA_FIX gw, VEGA_FIX gh);
#endif // VEGA_USE_GRADIENT_CACHE

	/** Initiate a linear gradient. The start and stop positions should be
	  * transformed when calling this.
	  * @param numStops the number of stops used by this gradient.
	  * @param spread the spread method used.
	  * @param x1, y1, x2, y2 the start and stop position.
	  * @param premultiply_stops true if stops should be premultiplied
	  * when added - interpolation will be performed on premultiplied
	  * data.
	  * @returns OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM. */
	OP_STATUS initLinear(unsigned int numStops, VEGA_FIX x1, VEGA_FIX y1, VEGA_FIX x2, VEGA_FIX y2,
						 bool premultiply_stops = false);

	/** Initiate a radial gradient. The circle positions (and radii)
	  * should not be transformed before calling this.
	  * @param numStops the number of stops used by this gradient.
	  * @param x1, y1, r1 the outer circle used to calculate distance.
	  * @param x0, y0, r0 the inner circle used when calculating distance.
	  * @param premultiply_stops true if stops should be premultiplied
	  * when added - interpolation will be performed on premultiplied
	  * data.
	  * @returns OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM. */
	OP_STATUS initRadial(unsigned int numStops,
						 VEGA_FIX x1, VEGA_FIX y1, VEGA_FIX r1,
						 VEGA_FIX x0, VEGA_FIX y0, VEGA_FIX r0 = 0,
						 bool premultiply_stops = false);

	/** Add a stop. You should add exactly the same number of stops 
	  * specified in init. They must be specified starting with the 
	  * smallest stop and ending with the greatest.
	  * @param stopNum the number of this stop.
	  * @param offset the offset of this stop.
	  * @param color the color in this stop. */
	void setStop(unsigned int stopNum, VEGA_FIX offset, unsigned long color);

private:
	VEGA_FIX *stopOffsets;
	unsigned int *stopColors;
	UINT32* stopInvDists;
	unsigned int numStops;

	// Invariants that are setup in prepare() and then valid until
	// complete().
	struct
	{
		VEGA_FIX cdx;
		VEGA_FIX cdy;
		VEGA_FIX cdr;

		VEGA_FIX norm;

		VEGA_FIX delta_x;
		VEGA_FIX delta_y;

		VEGA_FIX aq;

		VEGA_FIX dbq;
		VEGA_FIX dpq;

		bool circular;
		bool focus_at_border;
	} invariant;

	union
	{
		struct
		{
			VEGA_FIX x, y, dist;
		} linear;
		struct
		{
			VEGA_FIX x1, y1, r1;
			VEGA_FIX x0, y0, r0;
		} radial;
	};
	unsigned has_calculated_invdists:1;
	unsigned is_simple_radial:1;
	unsigned is_premultiplied:1;
	unsigned is_linear:1;

	struct CachedGradient* generateCacheImage(VEGARendererBackend* backend, VEGA_FIX gx, VEGA_FIX gy, VEGA_FIX gw, VEGA_FIX gh);

#ifdef VEGA_3DDEVICE
	VEGA3dTexture* m_texture;
	unsigned m_texture_line;
	VEGATransform m_texTransform;

	VEGA3dShaderProgram* m_shader;
	VEGA3dRenderState* m_blend_mode;
	VEGA3dVertexLayout* m_vlayout;
#endif // VEGA_3DDEVICE
};

#ifdef VEGA_USE_GRADIENT_CACHE
#include "modules/util/simset.h"


struct CachedGradient
{
	CachedGradient() : fill(NULL) {}
	~CachedGradient();

	unsigned GetMemUsage() const;

	VEGAImage* fill;
	VEGA_FIX sx;
	VEGA_FIX sy;
	VEGA_FIX w;
	VEGA_FIX h;
};


class VEGAGradientCache
{
public:
	friend class VEGAGradient;

	VEGAGradientCache() : m_memUsed(0) {}
	~VEGAGradientCache() { m_lru.Clear(); }

	CachedGradient* lookup(UINT32* cols, VEGA_FIX* ofs, VEGA_FIX lx, VEGA_FIX ly, UINT32 nstops, VEGAFill::Spread spread, VEGA_FIX sx, VEGA_FIX sy, VEGA_FIX w, VEGA_FIX h);
	OP_STATUS insert(UINT32* cols, VEGA_FIX* ofs, VEGA_FIX lx, VEGA_FIX ly, UINT32 nstops, VEGAFill::Spread spread, CachedGradient* cached);

private:
	struct Entry : public ListElement<Entry>
	{
		Entry() : cachedGradient(NULL) {}
		~Entry();

		bool match(UINT32* ccols, VEGA_FIX* cofs, VEGA_FIX clx, VEGA_FIX cly, UINT32 cnstops, VEGAFill::Spread cspread, VEGA_FIX sx, VEGA_FIX sy, VEGA_FIX w, VEGA_FIX h) const;
		void reset(UINT32* pcols, VEGA_FIX* pofs, VEGA_FIX plx, VEGA_FIX ply, UINT32 pnstops, VEGAFill::Spread pspread, CachedGradient* cached);

		enum { MAX_STOPS = 8 };

		VEGA_FIX lx;
		VEGA_FIX ly;
		VEGA_FIX ofs[MAX_STOPS];
		UINT32 cols[MAX_STOPS];
		UINT32 nstops;
		VEGAFill::Spread spread;

		CachedGradient* cachedGradient;
	};

	enum { MAX_SIZE = 32 };
	enum { MAX_MEM = VEGA_GRADIENT_CACHE_MEM_LIMIT*1024 };

	List<Entry> m_lru;

	unsigned m_memUsed;
};
#endif // VEGA_USE_GRADIENT_CACHE

#endif // VEGA_SUPPORT
#endif // VEGAGRADIENT_H
