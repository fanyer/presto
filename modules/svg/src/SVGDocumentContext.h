/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef SVG_DOCUMENT_CONTEXT_H
#define SVG_DOCUMENT_CONTEXT_H

#ifdef SVG_SUPPORT

#include "modules/svg/src/SVGElementStateContext.h"
#include "modules/svg/src/SVGTextSelection.h"
#include "modules/svg/src/SVGPoint.h"
#include "modules/svg/src/SVGImageImpl.h"
#include "modules/svg/src/SVGAverager.h"

#include "modules/svg/src/animation/svganimationtime.h"

#include "modules/doc/frm_doc.h" // For GetHLDocProfile()

#include "modules/pi/OpSystemInfo.h" // for g_op_system_info

class SVGFontManagerImpl;
class OpFontDatabase;
class OpFontManager;
class SVGOpFontInfo;
class SVGDependencyGraph;
class SVGAnimationWorkplace;

#ifdef _DEBUG
#define ASSERT_REAL_DOCCTX() do { OP_ASSERT(m_elm == SVGUtils::GetTopmostSVGRoot(m_elm)); } while(0)
#else
#define ASSERT_REAL_DOCCTX()
#endif // _DEBUG

class SVGFrameTimeModel
{
public:
	SVGFrameTimeModel() :
		m_optimal_frame_interval(35), // 1000ms/35ms = ~29 fps
		m_last_invalid_call(op_nan(NULL)) {}

	OP_STATUS Initialize();

	void SamplePaintEnd();
	void SampleInvalidation() { m_last_invalid_call = g_op_time_info->GetRuntimeMS(); }

	unsigned int CalculateDelay();

	unsigned int m_optimal_frame_interval;

private:
	unsigned int GetEstimatedPaintLatency();

	SVGAverager m_invalid_to_paint_end_delay;

	// Invalidation timestamp
	double m_last_invalid_call;
};

class SVGDocumentContext : public SVGSVGElement
{
public:
	enum SVGVersion
	{
		SVG_VERSION_1_1,
		SVG_VERSION_1_2
	};

	struct SVGFrameElementContext {
		HTML_Element*		frame_element;
		FramesDocument*		frm_doc;
	};

	virtual						~SVGDocumentContext();

	static OP_STATUS			Make(SVGDocumentContext*& ctx, HTML_Element* e, LogicalDocument* logdoc);

	virtual SVGDocumentContext* GetAsDocumentContext() { return this; }

	/**
	 * Get animation workplace.
	 *
	 * Note: If the document has no animations, the animation
	 * workplace will be NULL.
	 */
	SVGAnimationWorkplace*		GetAnimationWorkplace() { ASSERT_REAL_DOCCTX(); return m_animation_workplace; }

	/**
	 * Get animation workplace, and if it didn't exist before, create
	 * the animation workplace.
	 *
	 * Returns NULL only on OOM.
	 */
	SVGAnimationWorkplace*		AssertAnimationWorkplace();

	SVGRenderer*				GetRenderingState();

	/*
	 * Note: The font may be added another document than this document
	 * context. The reason and occation for this is that external
	 * resource document can not have svg fonts themselves. Instead it
	 * uses the original document's fontmanager for storing the fonts
	 * elements.
	 */
	OP_STATUS					AddSVGFont(HTML_Element* font_elm, OpFontInfo* fi);

	BOOL						IsInvalidationPending() { ASSERT_REAL_DOCCTX(); return m_packed.invalidation_pending; }
	void						SetInvalidationPending(BOOL new_val) { ASSERT_REAL_DOCCTX(); m_packed.invalidation_pending = new_val ? 1 : 0; }

	BOOL						IsStandAlone();
	/**< Standalone means that we are not embedded within a
	 * compound-document or through an object/embed */

	BOOL						IsEmbeddedByIFrame();
	/**< 'Embedded by iframe' means that the SVG fragment is a
	 * separately stored resource that is referenced by an <iframe>
	 * element.
	 *
	 * It's used to differ between a SVG loaded as a document compared
	 * to a SVG loaded as <img> or <object>, where the behavior more
	 * closely resembles images. */

	OP_STATUS					UpdateZoom(SVGNumber user_zoom_level);
	/** Zoom "around" the point specified (should be relative to the
	 * content box), i.e, keep the point at the same position after
	 * the zoom operation has been performed. Sends SCROLL event. */
	OP_STATUS					ZoomAroundPointBy(const OpPoint& local_point, SVGNumber zoom_factor);
	OP_STATUS					SetCurrentRotate(SVGNumber user_rotate_deg);

#ifdef SVG_SUPPORT_PANNING
	void						BeginPanning(const OpPoint& view_pos);
	void						EndPanning();
	BOOL						IsPanning() { return m_packed.is_panning;}
	void						ResetPan();
	/** Pan the SVG - unscaled/"document" coordinates. */
	void						Pan(int doc_pan_x, int doc_pan_y);

	OP_STATUS					UpdateZoomAndPan(const SVGManager::EventData& data);
	/** Fit the specified rect to the viewport of this
	 * document(context). Sends SCROLL event. */
	void						FitRectToViewport(OpRect doc_rect);

	static BOOL					CheckBounds(SVGNumber lb, SVGNumber t, SVGNumber s);
#endif // SVG_SUPPORT_PANNING

	/**
	 * Returns the Markup::SVGE_SVG element connected to this context.
	 */
	HTML_Element*  GetSVGRoot() { ASSERT_REAL_DOCCTX(); return m_elm; }
	HTML_Element*  GetElement() { return m_elm; }

	/**
	 * May be NULL in extreme cases.
	 */
	SVG_DOCUMENT_CLASS* GetDocument()
	{
		ASSERT_REAL_DOCCTX();
		return m_svg_image.GetDocument();
	}

	/**
	 * May be NULL in extreme cases.
	 */
	LogicalDocument* GetLogicalDocument() { return m_svg_image.GetLogicalDocument(); }

	/**
	 * May be NULL in extreme cases.
	 */
	HLDocProfile* GetHLDocProfile() 
	{
		ASSERT_REAL_DOCCTX();
		if (GetLogicalDocument())
		{
			return GetLogicalDocument()->GetHLDocProfile(); 
		}
		return NULL;
	}

	/**
	 * May be NULL if there is no scripting environment.
	 */
	DOM_Environment* GetDOMEnvironment() 
	{ 
		ASSERT_REAL_DOCCTX();
		SVG_DOCUMENT_CLASS *doc = GetDocument();
		if (doc)
		{
			return doc->GetDOMEnvironment();
		}
		return NULL;
	}
	OP_STATUS ConstructDOMEnvironment() 
	{ 
		ASSERT_REAL_DOCCTX();
		SVG_DOCUMENT_CLASS *doc = GetDocument();
		if (doc)
		{
			return doc->ConstructDOMEnvironment();
		}
		return OpStatus::ERR;
	}

	/**
	 * May be NULL in extreme cases.
	 */
	LayoutWorkplace* GetLayoutWorkplace()
	{
		ASSERT_REAL_DOCCTX();
		if (GetHLDocProfile())
		{
			return GetHLDocProfile()->GetLayoutWorkplace(); 
		}
		return NULL;
	}

	/**
	 * May be dummy (empty) URL in extreme cases.
	 */
	URL GetURL()
	{
		ASSERT_REAL_DOCCTX();
		SVG_DOCUMENT_CLASS *doc = GetDocument();
		if (doc)
		{
			return doc->GetURL();
		}
		return URL();
	}
	/**
	 * May be NULL in extreme cases.
	 */
	VisualDevice* GetVisualDevice()
	{
		ASSERT_REAL_DOCCTX();
		SVG_DOCUMENT_CLASS *doc = GetDocument();
		if (doc)
		{
			return doc->GetVisualDevice();
		}
		return NULL;
	}

	/**
	 * Register a dependency in the dependency graph.
	 *
	 * Note: The dependency is registered in the document-global
	 * dependency graph contained in the SVGWorkplace of the logical
	 * document this document context belongs to.  This means that two
	 * SVG fragments in the same document shares dependency graph.
	 */
	void RegisterDependency(HTML_Element* element, HTML_Element* target);

	/**
	 * Register that the given element has an unresolved dependency
	 * (e.g a missing paintserver, mask, clip-path etc).
	 *
	 * @param element The element that has an unresolved dependency
	 */
	void RegisterUnresolvedDependency(HTML_Element* element);

	/**
	 * SVG documents can be external in an number of ways. The common
	 * method currently to implement external references is to load
	 * the external document through the use of
	 * SVGUtils::FindURLRelReferredNode on a proxy element acting as
	 * common gateway to the external document. The proxy element can
	 * be extracted by the use of 'GetExternalFrameElement'
	 *
	 * 'Parent document' means the document that pulls in other
	 * documents. 'External document' means documents that are pulled
	 * in. If an external document pulls in a document of its own, it
	 * may be both a parent document and external document, but it
	 * depends on the type of "pulls".
	 *
	 * Different types of "pulls" are treated differently:
	 *
	 * 1. External use and external paint servers
	 *
	 *    This case describes when a document is created to supply
	 *    nodes for shadow trees the external document.
	 *
	 *    The external document does not have any animation, which
	 *    means no animation workplace. All animation is done in the
	 *    parent document.
	 *
	 *    The external document does not have a dependency graph. The
	 *    parent document has dependencies to nodes in the external
	 *    document directly.
	 *
	 *    EXTERNAL USE CAN BE RECURSIVE.
	 *
	 *    If an external resource document references an external
	 *    document, this additional external resource is loaded at the
	 *    original document. The reason being that we in this way keep
	 *    control over what resources that are loaded and can avoid
	 *    loading the same resources over and over in loops.
	 *
	 *    This introduces some import fact to have in mind:
	 *
	 *    - GetExternalFrameElement can be called on a resource
	 *      document context (IsExternalResource() == TRUE) but the
	 *      call is dispatched to the original document. This means
	 *      that the element returned GetExternalFrameElement() may
	 *      NOT be in this document. So if the corresponding document
	 *      context is needed, use
	 *      AttrValueStore::GetSVGDocumentContext() on the returned
	 *      element.
	 *
	 * 3. External animation
	 *
	 *    External animations is whole document referenced by a
	 *    <animation>-element.
	 *
	 *    The external document has full animation support on its own,
	 *    and does its own scheduling through the SVGImageImpl of the
	 *    external document.
	 *
	 * 4. Foreign objects
	 *
	 *    Very much like animation but without the controlled
	 *    animations. Each foreign object gets its own iframe.
	 *
	 */
	void SetIsExternalResource(BOOL value) { ASSERT_REAL_DOCCTX(); m_packed.is_external_resource = (value) ? 1 : 0; }
	void SetIsExternalAnimation(BOOL value) { ASSERT_REAL_DOCCTX(); m_packed.is_external_animation = (value) ? 1 : 0; }

	BOOL IsExternalResource() { return m_packed.is_external_resource; }
	BOOL IsExternalAnimation() { return m_packed.is_external_animation; }
	BOOL IsExternal() { return m_packed.is_external_animation || m_packed.is_external_resource; }

	/**
	 * Get the element carrying external references for this
	 * element. May be the 'parent' element it self, or another
	 * proxy-element (a special foreign-object element).
	 *
	 * Note: The returned element may be in another document than this
	 * document context. The reason and occation for this is that
	 * external resource document can not have external references
	 * themselves. Instead they use the original document document
	 * context for storing external frame elements.
	 */
	HTML_Element*				GetExternalFrameElement(URL url, HTML_Element* parent);

	/**
	 * Get the element carrying external references for this
	 * element. May be the 'parent' element it self, or another
	 * proxy-element (a special foreign-object element).
	 *
	 * Note: The returned element may be in another document than this
	 * document context. The reason and occation for this is that
	 * external resource document can not have external references
	 * themselves. Instead they use the original document document
	 * context for storing external frame elements. This method
	 * should be used when you need to know which document that
	 * the frame element is physically in.
	 */
	OP_STATUS					GetExternalFrameElement(URL fullurl, HTML_Element* parent, SVGFrameElementContext* ctx);

	/**
	 * Get element by reference as specified in a URL. External
	 * resources are supported. If the URL has no relative part, the
	 * root element of that document is returned.
	 *
	 * If the external resource hasn't been loaded yet, a dependency
	 * is created between the element 'element' and the element that
	 * is responsible for loading. This means that when the external
	 * resource is loaded, the element sent to this function is
	 * invalidated.
	 */
	HTML_Element*				GetElementByReference(SVGElementResolver* resolver, HTML_Element *element, URL url);

	/**
	 * If we are a external document, this method returns the
	 * document context of our mother document
	 */
	SVGDocumentContext *		GetParentDocumentContext();

	/**
	 * If we are an external document, this method returns the
	 * HTML_Element that has the FramesDocElm.
	 */
	HTML_Element*				GetReferencingElement();

#if defined(SVG_DOM) || defined(SVG_SUPPORT_PANNING)
	void						GetCurrentMatrix(SVGMatrix& matrix);
	SVGPoint*					GetCurrentTranslate() { return m_translate; }
	SVGNumber					GetCurrentScale() { return m_scale; }
	SVGNumber					GetCurrentRotate() { return m_rotate; }
	void						SetCurrentScale(const SVGNumber& scale) { m_scale = scale; }
	void						SetCurrentTranslate(const SVGNumber& x, const SVGNumber& y) { m_translate->x = x; m_translate->y = y; }
#endif // SVG_DOM || SVG_SUPPORT_PANNING

	SVGImageImpl* 				GetSVGImage() { return &m_svg_image; }
	OP_STATUS					SendDOMEvent(DOM_EventType type, HTML_Element* target);

#ifdef SVG_SUPPORT_TEXTSELECTION
	SVGTextSelection& GetTextSelection() { return m_textselection; }
#endif // SVG_SUPPORT_TEXTSELECTION

	void						SubtreeRemoved(HTML_Element* elm);

	void						RecordInvalidCall() { m_frametime_model.SampleInvalidation(); }
	void						RecordPaintEnd() { m_frametime_model.SamplePaintEnd(); }
	unsigned int				CalculateDelay() { return m_frametime_model.CalculateDelay(); }

#ifdef SVG_INTERRUPTABLE_RENDERING
	double GetTimeForLastScreenBlit() { return m_time_for_last_screen_blit; }
	void RecordTimeForScreenBlit() { m_time_for_last_screen_blit = g_op_time_info->GetRuntimeMS(); }
#endif // SVG_INTERRUPTABLE_RENDERING

	SVGDependencyGraph*			GetDependencyGraph();

	BOOL						AllowReportError() { return m_packed.reported_errors <= 100; }
	BOOL						IsAtFirstUnreportedError() { return m_packed.reported_errors == 100; }
	void						IncReportErrorCount() { m_packed.reported_errors++; }

	/**
	 * Returns the svg quality for the svg. 1 - .... 100 is low, 25 is high. 1 is ridicously high.
	 */
	int							GetRenderingQuality() { return m_quality; }
	void						SetRenderingQuality(int quality) { m_quality = quality; }

	BOOL						ExistsTRefInSVG() { ASSERT_REAL_DOCCTX(); return m_packed.seen_tref_in_document; }
	void						SetTRefSeenInSVG() { ASSERT_REAL_DOCCTX(); m_packed.seen_tref_in_document = TRUE; }

	/**
	 * Get/set flag for optimization - don't bother keeping background
	 * if no referenced filter in the SVG is using one.
	 */
	BOOL						NeedsBackgroundLayers() { ASSERT_REAL_DOCCTX(); return m_packed.seen_background_image; }
	void						SetNeedsBackgroundLayers() { ASSERT_REAL_DOCCTX(); m_packed.seen_background_image = TRUE; }

	/**
	 * Ignore the size of layout box when drawing. Useful if
	 * rendering outside the window. This is set for the lifetime of
	 * this svg.  */
	void						ForceSize(short width, long height);
	void						UnforceSize() { m_packed.force_size = 0; }
	BOOL						HasForcedSize() { return m_packed.force_size == 1; }
	short						ForcedWidth() { OP_ASSERT(m_packed.force_size); return m_forced_w; }
	long						ForcedHeight() { OP_ASSERT(m_packed.force_size); return m_forced_h; }
	void						SetForceViewbox(SVGRect* vb) { m_forced_viewbox = vb; }
	SVGRect*					GetForcedViewbox() { return m_forced_viewbox; }

	void						DestroyAnimationWorkplace();

#ifdef SVG_DOM
	/**
	 * Set the viewport matching the viewport property in SVG DOM.
	 */
	void						UpdateViewport(const SVGRect &viewport);

	/**
	 * Get the viewport rectangle
	 */
	SVGRectObject *				GetViewPort() { return m_viewport; }

	BOOL						HasViewPort() { return (m_packed.has_set_viewport == 1); }
#endif // SVG_DOM

	/**
	 * Fetch the default sync. behavior for this document fragment
	 *
	 * Will search upwards in the document structure if required.
	 */
	SVGSyncBehavior				GetDefaultSyncBehavior();

	/**
	 * Fetch the default sync. tolerance value for this document fragment
	 *
	 * Will search upwards in the document structure if required.
	 */
	SVG_ANIMATION_TIME			GetDefaultSyncTolerance();

	/**
	 * Helper method for extracting a document context for a frame
	 * element.
	 */
	SVGDocumentContext *		GetSubDocumentContext(HTML_Element *frame_element);

	void SetTransform(const SVGMatrix& m) { m_transform = m; }
	SVGMatrix& GetTransform() { return m_transform; }

	void SetTargetFPS(UINT32 fps)
	{
		if (fps > 0 && fps <= 1000)
			m_frametime_model.m_optimal_frame_interval = 1000 / fps;
	}

	UINT32 GetTargetFPS() { return 1000 / m_frametime_model.m_optimal_frame_interval;  }

private:
	SVGMatrix m_transform;

#if defined(SVG_DOM) || defined(SVG_SUPPORT_PANNING)
	/**
	 * Current scale. Used in SVG DOM for the currentScale property in
	 * the SVGSVGElement interface.
	 */
	SVGNumber m_scale;

	/**
	 * Current translation. Used in SVG DOM for the currentTranslate
	 * property in the SVGSVGElement interface.
	 */
	SVGPoint* m_translate;

	/**
	 * Current rotate. Used in SVG DOM for the currentRotate property in
	 * the SVGSVGElement interface.
	 */
	SVGNumber m_rotate;
#endif // SVG_DOM || SVG_SUPPORT_PANNING

#ifdef SVG_DOM
	SVGRectObject* m_viewport;
#endif // SVG_DOM

#ifdef SVG_SUPPORT_TEXTSELECTION
	SVGTextSelection m_textselection;
#endif // SVG_SUPPORT_TEXTSELECTION

	SVGFrameTimeModel m_frametime_model;

#ifdef SVG_INTERRUPTABLE_RENDERING
	double m_time_for_last_screen_blit;
#endif // SVG_INTERRUPTABLE_RENDERING

	short m_forced_w;
	long m_forced_h;
	SVGRect* m_forced_viewbox;

	/**
	 * Represents this SVG:s external interface.
	 */
	SVGImageImpl	m_svg_image;
	union
	{
		struct
		{
			unsigned int invalidation_pending : 1;
// 			unsigned int has_built_fonts : 1;
			unsigned int reported_errors : 8; // We print at most 100 errors per svg fragment
			unsigned int seen_tref_in_document : 1; // Trefs slow down text invalidations
			unsigned int seen_background_image : 1; // Background images makes us paint things twice
#ifdef SVG_SUPPORT_PANNING
			unsigned int is_panning : 1;
#endif // SVG_SUPPORT_PANNING
			unsigned int force_size : 1;
			unsigned int is_external_resource : 1;
			unsigned int is_external_animation : 1;
			unsigned int has_set_viewport : 1;
		} m_packed;
		unsigned int m_packed_init;
	};

#ifdef SVG_SUPPORT_PANNING
	OpPoint last_view_panning_position;
#endif // SVG_SUPPORT_PANNING

	int m_quality;
	HTML_Element* m_elm;

	BOOL IsZoomAndPanAllowed() { return m_svg_image.IsZoomAndPanAllowed(); }
	void UpdateUserTransform();

	/**
	 * Animation workplace.
	 *
	 * Allocated when a document has one or more animation elements.
	 *
	 * Only present for top-most SVG elements.
	 */
	SVGAnimationWorkplace *m_animation_workplace;

protected:
	SVGDocumentContext(HTML_Element* e, LogicalDocument* logdoc);
};

#endif // SVG_SUPPORT
#endif // SVG_DOCUMENT_CONTEXT_H
