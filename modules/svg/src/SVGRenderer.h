/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef _SVG_RENDERER_
#define _SVG_RENDERER_

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGCache.h"
#include "modules/svg/src/svgpainter.h"

#include "modules/libvega/vegarenderer.h"

#include "modules/hardcore/mh/messobj.h"

#include "modules/util/OpRegion.h"

class SVGRenderer;
class SVGDocumentContext;

class SVGInvalidState
{
public:
	void			AddExtraInvalidation(const OpRect& rect);
	void			GetExtraInvalidation(OpRect& rect);

	OP_STATUS		Invalidate(const OpRect& rect);
	void			InvalidateAll() { m_areas.IncludeRect(m_dimension); }

	void			Validate(const OpRect& rect) { m_areas.RemoveRect(rect); }

#ifdef SVG_OPTIMIZE_RENDER_MULTI_PASS
	OP_STATUS		GetInvalidArea(const OpRect& area, OpVector<OpRect>& area_list);
#else
	OP_STATUS		GetInvalidArea(const OpRect& area, OpRect& subarea);
#endif // SVG_OPTIMIZE_RENDER_MULTI_PASS

	void			Translate(int pan_x, int pan_y);

	OP_STATUS		Reset();

	void			SetDimensions(const OpRect& dim) { m_dimension = dim; }
	const OpRect&	GetDimensions() { return m_dimension; }

	OpRegion*		GetRegion() { return &m_areas; }

private:
	OpRect			m_dimension;		///< The 'size' of the renderers area
	OpRegion		m_areas;			///< Areas left to render
	OpRect			m_extra;			///< Area that should be invalidated that an INVALIDATION pass won't see (from removed nodes for instance)
};

class SVGRenderBuffer
	: public SVGCacheData
{
public:
	SVGRenderBuffer() : rendertarget(NULL) {}
	virtual ~SVGRenderBuffer() { VEGARenderTarget::Destroy(rendertarget); }

	virtual unsigned int GetMemUsed()
	{
		if (!rendertarget || area.IsEmpty())
			return 0; // Well, close to zero anyway

		// FIXME: Just an approximation (hopefully decent enough)
		return area.height * area.width * 4;
	}
	virtual BOOL IsEvictable() { return TRUE; }

	SVGRect extents;
	OpRect area;
	VEGARenderTarget* rendertarget;
};

class SVGRendererListener
{
public:
	virtual ~SVGRendererListener() {}

	virtual OP_STATUS OnAreaDone(SVGRenderer* renderer, const OpRect& area) = 0;
#ifdef SVG_INTERRUPTABLE_RENDERING
	virtual OP_STATUS OnAreaPartial(SVGRenderer* renderer, const OpRect& area) = 0;
	virtual OP_STATUS OnStopped() = 0;
	virtual OP_STATUS OnError() = 0;
#endif // SVG_INTERRUPTABLE_RENDERING
};

class SVGRenderer :
#ifdef SVG_INTERRUPTABLE_RENDERING
	private MessageObject,
#endif // SVG_INTERRUPTABLE_RENDERING
	public SVGCacheData
{
public:
	SVGRenderer();
  	virtual ~SVGRenderer();

	enum RendererPolicy
	{
		POLICY_SYNC,
		POLICY_ASYNC
	};

	/**
     * Allocate a SVGRenderer and initialize it to render the given document with the given scale.
     *
     * @param doc_ctx The SVG document, must not be NULL.
     * @param scale The scalefactor
     * @return OpStatus::OK if successful
     */
    OP_STATUS				Create(SVGDocumentContext* doc_ctx,
								   int content_width, int content_height,
								   int scaled_width, int scaled_height,
								   float scale);

	/**
	 * The scale of the represented image changed
	 */
	OP_STATUS				ScaleChanged(int content_width, int content_height,
										 int scaled_width, int scaled_height,
										 float scale);

	/**
	 * Setup the area to render - initialises internal structure for Update()
	 */
	OP_STATUS				Setup(const OpRect& area);

	OP_STATUS				Update();

	/**
	 * Stops rendering, throws away the current traverse if any.
	 */
	void 					Stop();
#ifdef SVG_INTERRUPTABLE_RENDERING
	BOOL					HasPendingStop() const
	{
		return m_config.deferred_stop ? TRUE : FALSE;
	}
#endif // SVG_INTERRUPTABLE_RENDERING
	void					Abort()
	{
		SetListener(NULL);
		Stop();
	}

	void					SetPolicy(RendererPolicy policy)
	{
#ifdef SVG_INTERRUPTABLE_RENDERING
		OP_ASSERT(!IsActive() || m_config.policy == (unsigned int)policy);
		if ((unsigned int)policy != m_config.policy)
			Stop();
		m_config.policy = (unsigned int)policy;
#else
		m_config.policy = POLICY_SYNC;
#endif // SVG_INTERRUPTABLE_RENDERING
	}
	void					SetAllowTimeout(BOOL allow_timeout)
	{
		m_config.timeout = allow_timeout ? 1 : 0;
	}
	void					SetAllowPartial(BOOL allow_partial)
	{
		m_config.partial = allow_partial ? 1 : 0;
	}
	void					SetListener(SVGRendererListener* listener) { m_listener = listener; }

#ifdef SVG_INTERRUPTABLE_RENDERING
	BOOL					HasBufferedResult() const { return IsActive() || HasPendingStop(); }
	BOOL					IsActive() const { return HaveSubAreas() || !m_config.stopped; }
#else
	BOOL					IsActive() const { return HaveSubAreas(); }
#endif // SVG_INTERRUPTABLE_RENDERING
	BOOL					Contains(const OpRect& area) const { return m_area.Contains(area); }

	SVGInvalidState*		GetInvalidState() { return &m_invalid; }

	OP_STATUS				Move(int pan_x, int pan_y);

	float					GetScale() { return m_scale; }
	const OpRect&			GetContentSize() const { return m_content_size; }
	const OpRect&			GetArea() { return m_area; }

	/**
	 * Get the rendered image. The bitmap in bm will contain the 
	 * resulting bitmap. bm might be changed to a different bitmap
	 * if interruptable rendering is used. The area parameter will 
	 * be changed to the area of the bitmap in *bm which contains 
	 * the area requested through the area parameter. */
	OP_STATUS				GetResult(OpRect& area, OpBitmap** bm);
	OP_STATUS				CopyToBitmap(const OpRect& rect, OpBitmap **bitmap);

	// SVGCacheData methods
	virtual unsigned int	GetMemUsed();
	virtual BOOL 			IsEvictable() { return (m_config.is_traversing == 0); }

#ifdef SVG_INTERRUPTABLE_RENDERING
	virtual void			HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
#endif // SVG_INTERRUPTABLE_RENDERING

#ifdef SVG_DEBUG_SCREEN_BOXES
	static void				PaintScreenBoxes(SVGPainter* painter, HTML_Element* root);
#endif // SVG_DEBUG_SCREEN_BOXES
#ifdef SVG_DEBUG_RENDER_RECTS
	static void				PaintDebugRect(SVGPainter* painter, const OpRect& rect, UINT32 color = 0xFFC800AA /*purple*/);
#endif // SVG_DEBUG_RENDER_RECTS

protected:
	void					Validate();
	void					OnFinished();
#ifdef SVG_INTERRUPTABLE_RENDERING
	void					OnPartial();
	OP_STATUS				OnTimeout();
	OP_STATUS				OnError();
	OP_STATUS				CreateBackupBitmap();
#endif // SVG_INTERRUPTABLE_RENDERING
	OP_STATUS				SetTarget(const OpRect& area);

	void					BeforeTraverse(const OpRect& rect);
	OP_STATUS				AfterTraverse(const OpRect& rect, OP_STATUS status);

	SVGPainter				m_painter;

	struct RenderTargetInfo
	{
		RenderTargetInfo() :
#ifdef VEGA_OPPAINTER_SUPPORT
			bitmap(NULL),
#endif // VEGA_OPPAINTER_SUPPORT
			rendertarget(NULL) {}

		void Free();

#ifdef VEGA_OPPAINTER_SUPPORT
		OpBitmap* bitmap;
#endif // VEGA_OPPAINTER_SUPPORT
		VEGARenderTarget* rendertarget;
	};
	OP_STATUS				CreateRenderTarget(RenderTargetInfo& rtinfo, unsigned rt_width, unsigned rt_height);

	VEGARenderer			m_renderer;
	RenderTargetInfo		m_target;
	OpRect					m_target_area;

	OpBitmap*				m_prev_result;	///< Temporary bitmap to store previous result of the currently painted area

	SVGInvalidState			m_invalid;

	SVGDocumentContext*		m_doc_ctx;		///< The SVG document
	float					m_scale;		///< The external scalefactor
	union
	{
		struct
		{
			unsigned int	policy : 2;
			unsigned int	timeout : 1;
			unsigned int	partial : 1;
			unsigned int	is_traversing : 1;
			unsigned int	deferred_stop : 1;
			unsigned int	stopped : 1;
		} m_config;
		unsigned char m_config_init;
	};

#ifdef SVG_OPTIMIZE_RENDER_MULTI_PASS
	OpAutoVector<OpRect>	m_subareas;

	OP_STATUS				SetupSubAreas(const OpRect& area)
	{
		return m_invalid.GetInvalidArea(area, m_subareas);
	}
	const OpRect& GetSubArea() { return *m_subareas.Get(m_subareas.GetCount() - 1); }
	BOOL HaveSubAreas() const { return m_subareas.GetCount() != 0; }
	void EndSubArea() { m_subareas.Delete(m_subareas.GetCount() - 1); }
	void ResetSubAreas() { m_subareas.DeleteAll(); }
#else
	OpRect					m_subarea;

	OP_STATUS				SetupSubAreas(const OpRect& area)
	{
		return m_invalid.GetInvalidArea(area, m_subarea);
	}
	const OpRect& GetSubArea() { return m_subarea; }
	BOOL HaveSubAreas() const { return !m_subarea.IsEmpty(); }
	void EndSubArea() { m_subarea.Empty(); }
	void ResetSubAreas() { EndSubArea(); }
#endif // SVG_OPTIMIZE_RENDER_MULTI_PASS

	OpRect					m_content_size;
	OpRect					m_area;

	struct SyncPolicyHandler
	{
		OP_STATUS			Update(SVGRenderer* renderer);
	};
	SyncPolicyHandler		m_sync_backend;

	friend struct SyncPolicyHandler;

#ifdef SVG_INTERRUPTABLE_RENDERING
	struct AsyncPolicyHandler
	{
		AsyncPolicyHandler() : m_state(AREA_SETUP) {}

		OP_STATUS			Update(SVGRenderer* renderer);
		void				Reset();

		enum {
			AREA_SETUP,
			AREA_RENDER,
			DONE
		} m_state;
	};
	AsyncPolicyHandler		m_async_backend;

	friend struct AsyncPolicyHandler;
#endif // SVG_INTERRUPTABLE_RENDERING

	SVGRendererListener*	m_listener;
};

#endif // SVG_SUPPORT
#endif // _SVG_RENDERER_
