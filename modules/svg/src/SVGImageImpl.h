/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef SVG_IMAGEIMPL_H
#define SVG_IMAGEIMPL_H

#ifdef SVG_SUPPORT

#include "modules/svg/svg_image.h"
#include "modules/display/vis_dev_transform.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/logdoc.h"
#include "modules/svg/src/SVGWorkplaceImpl.h"
#include "modules/inputmanager/inputcontext.h"
#include "modules/svg/src/SVGRenderer.h"
#include "modules/svg/src/SVGManagerImpl.h"
#include "modules/display/vis_dev_transform.h"
#include "modules/logdoc/elementref.h"

class CoreView;
class SVG_DOCUMENT_CLASS;
class SVGImage;
class SVGImageInvalidator;

struct SVGLayoutState
{
	SVGLayoutState() : invalid_state(NULL) {}

	void Set(SVGInvalidState* inv_state) { invalid_state = inv_state; }
	void AddInvalid(const OpRect& invalid_rect)
	{
		invalid_area.UnionWith(invalid_rect);
#ifdef SVG_OPTIMIZE_RENDER_MULTI_PASS
		invalid_state->Invalidate(invalid_rect);
#endif // SVG_OPTIMIZE_RENDER_MULTI_PASS
	}

	OpRect				invalid_area;
	SVGInvalidState*	invalid_state;
};

class SVGImageRefImpl : public SVGImageRef, private MessageObject, public Link, public ElementRef
{
private:
	SVG_DOCUMENT_CLASS* m_doc;
	int				m_id;
	BOOL			m_inserted_element;
	BOOL			m_doc_disconnected;			

	SVGImageRefImpl(SVG_DOCUMENT_CLASS* doc, int id, HTML_Element* element);

public:
	static OP_STATUS Make(SVGImageRefImpl*& newref, SVG_DOCUMENT_CLASS* doc, int id, HTML_Element* element);

	virtual				~SVGImageRefImpl();
	
	// SVGImageRef methods
	virtual BOOL		IsDocumentDisconnected() const { return m_doc_disconnected; }
	virtual SVGImage*	GetSVGImage() const;
	virtual OP_STATUS	Free();
	virtual URL*		GetUrl();

	// MessageObject
	virtual void		HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	// ElementRef
	virtual void OnDelete(FramesDocument *document) { Reset(); }

	HTML_Element*		GetContainerElement() const;
	FramesDocument*		GetDocument() const { return m_doc; }
	void				DisconnectFromDocument();
};

/**
 * This class represents an SVG as seen from the outside.
 */
class SVGImageImpl :
	public SVGImage, public Link, public OpInputContext, private SVGRendererListener
{
public:
	SVGImageImpl(SVGDocumentContext* doc_ctx, LogicalDocument* logdoc);
	virtual void SetVisibleInView(BOOL is_visible) {
		//		if (m_is_visible != is_visible)
			//			fprintf(stderr, "%p now has is_visible = %d\n", this, is_visible);
		m_info.is_visible = is_visible ? 1 : 0;
	}

	virtual ~SVGImageImpl();

	void InvalidateAll();

	/**
	 * Returns if this image is believed to be visible in the current view (not scrolled away). It
	 * will err on the safe side, i.e. say that an image is visible when it's potentially not. It
	 * is not guaranteed that the view itself is visible though. Use |IsVisible()| instead to
	 * check that.
	 */
	virtual BOOL IsVisibleInView() { return m_info.is_visible; }

	/**
	 * Returns if this image is believed to be visible on the screen (need to change things)
	 */
	BOOL IsVisible();

	/**
	 * Returns if this image is reachable from the document root
	 */
	BOOL IsInTree();

	virtual void SetDocumentPos(const AffinePos& ctm) { m_document_ctm = ctm; }
	const AffinePos& GetDocumentPos() const { return m_document_ctm; }

	OpRect GetContentRect() const { return OpRect(0, 0, m_content_width, m_content_height); }
	OpRect GetDocumentRect() const { OpRect r = GetContentRect(); m_document_ctm.Apply(r); return r; }

	void DisconnectFromDocument();
	virtual void OnDisable();

	SVGWorkplaceImpl* GetSVGWorkplace() { return m_logdoc ? static_cast<SVGWorkplaceImpl*>(m_logdoc->GetSVGWorkplace()) : NULL; }

	virtual BOOL IsZoomAndPanAllowed();
	virtual BOOL HasAnimation();
	virtual BOOL IsAnimationRunning();
	virtual OP_STATUS SetURLRelativePart(const uni_char* rel_part);

	static OP_STATUS GetDesiredSize(HTML_Element* root, float &width, short &widthUnit,
									float &height, short &heightUnit);
	virtual OP_STATUS GetResolvedSize(VisualDevice* visual_device, LayoutProperties* props,
							 		  int enclosing_width, int enclosing_height,
									  int& width, int& height);
	virtual OP_STATUS GetIntrinsicSize(LayoutProperties *props, LayoutCoord &width, LayoutCoord &height, int &intrinsic_ratio);

	virtual OP_STATUS PaintToBuffer(OpBitmap*& bitmap, int ms, int width, int height, SVGRect* override_viewbox = NULL);
	virtual OP_STATUS SetAnimationTime(int ms);
	virtual void SetVirtualAnimationTime(int ms);

	virtual UINT32 GetLastKnownFPS();
	virtual void SetTargetFPS(UINT32 fps);
	virtual UINT32 GetTargetFPS();
	virtual BOOL HasSelectedText();

	/** OpInputContext methods */
	virtual BOOL            OnInputAction(OpInputAction* action);
	virtual void            OnKeyboardInputGained(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason);
	virtual void            OnKeyboardInputLost(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason);
	virtual const char*		GetInputContextName() {return "SVG Image";}
	/** End OpInputContext methods */

	HLDocProfile* 	GetHLDocProfile() { return m_logdoc ? m_logdoc->GetHLDocProfile() : NULL; }
	SVG_DOCUMENT_CLASS* GetDocument() {
		return m_logdoc ? m_logdoc->GetFramesDocument() : NULL;
	}
	LogicalDocument* GetLogicalDocument() { return m_logdoc; }

	void SetLastKnownFPS(SVGNumber fps) { m_last_known_fps = fps; }
	double GetLastTime() { return m_last_time; }
	void SetLastTime(double v) { m_last_time = v; }

	virtual SVGNumber GetUserZoomLevel();

	// Needed even if SVGDOM is disabled since DOM-Core can move svg nodes.
	virtual void SwitchDocument(LogicalDocument* new_document);

	void OnThreadCompleted();

	virtual BOOL IsEcmaScriptEnabled(FramesDocument* frm_doc);

	virtual BOOL IsInImgElement();
	virtual BOOL IsInteractive();

	SVGDocumentContext* GetSVGDocumentContext() { return m_doc_ctx; }

	BOOL IsTimeoutAllowed() { return m_info.timeout_not_allowed == 0; }
	BOOL IsAnimationAllowed();

	enum
	{
		UPDATE_ANIMATIONS	= 0x1,
		UPDATE_LAYOUT		= 0x2
	};

	void ScheduleUpdate();
	void ScheduleAnimationFrameAt(unsigned int delay_ms);
	void ScheduleInvalidation();

	void OnRendererChanged(SVGRenderer* renderer);
	OP_STATUS UpdateRenderer(VisualDevice* vis_dev, SVGRenderer*& renderer);
	void UpdateState(int state);
	/**
	 * Synchronously executes animations.
	 * To be used only if there is a real need to do this synchronously,
	 * e.g. when svg.setCurrentTime() is called from scripts that
	 * query animation state immediately after.
	 */
	void UpdateAnimations();
	void QueueInvalidate(const OpRect& invalid_rect);
	void Invalidate(const OpRect& invalid);
	BOOL GetContentRect(RECT& rect);
	void ForceUpdate();
#ifdef SVG_SUPPORT_PANNING
	void Pan(int doc_pan_x, int doc_pan_y);
#endif // SVG_SUPPORT_PANNING

	// SVGRendererListener interface
	virtual OP_STATUS OnAreaDone(SVGRenderer* renderer, const OpRect& area);
#ifdef SVG_INTERRUPTABLE_RENDERING
	virtual OP_STATUS OnAreaPartial(SVGRenderer* renderer, const OpRect& area);
	virtual OP_STATUS OnStopped();
	virtual OP_STATUS OnError();

	OP_STATUS SuspendScriptExecution();
	OP_STATUS ResumeScriptExecution();
#endif // SVG_INTERRUPTABLE_RENDERING

	/**
	 * Returns TRUE if this SVGImage/SVGDocumentContext is only part of a bigger SVG.
	 */
	BOOL IsSubSVG();

	// Helper method for checking if a particular param is set
	BOOL IsParamSet(const char* name, const char* value);

	virtual BOOL IsSecure() { return m_info.is_secure; }
	void SetSecure(BOOL val) { m_info.is_secure = val?1:0; }

	virtual OP_STATUS OnReflow(LayoutProperties* cascade);

	virtual void OnContentSize(LayoutProperties* cascade, int width, int height);
	virtual void OnSignalChange();
	virtual OP_STATUS OnPaint(VisualDevice* visual_device,
							  LayoutProperties* casacde,
							  const RECT &rect);

	virtual OP_STATUS GetViewBox(SVGRect& viewBox);

private:
	static BOOL IsViewVisible(CoreView* view);

	BOOL IsPartialAllowed() { return TRUE; /* needs tweaking */ }
	BOOL IsPainting() { return m_current_vd != NULL; }

	OP_STATUS BlitCanvas(VisualDevice* visual_device, const OpRect& area);
	OP_STATUS PaintOnScreenInternal(VisualDevice* visual_device,
				   LayoutProperties* layout_props,
				   const OpRect& screen_area_to_paint);

	/**
	 * This layouts the SVG and invalidates areas needed to be
	 * changed. Must be called before doing the actual paint.
	 *
	 * @param layout_props The LayoutProperties to cascade further
	 *			(NULL makes us call CreateCascade)
	 * @param known_invalid A rect with the area that is already going
	 *			to be painted. Normally this is empty, but if this
	 *			layout pass is done in response to a paint event, the
	 *			rect from the paint event should be used here. If not,
	 *			the same area might be invalidated again and we get an
	 *			unnecessary repaint.
	 * @return Returns TRUE if the layout pass finds something to
	 *		invalidate on the visual device. Returns FALSE otherwise.
	 */
	OP_STATUS Layout(LayoutProperties* layout_props, const OpRect& known_invalid,
					 SVGRenderer* renderer);
	/**
	 * Same as above, but does not invalidate the VisualDevice based
	 * on the results of the layout calculations.
	 */
	OP_STATUS LayoutNoInvalidate(LayoutProperties* layout_props, const OpRect& known_invalid,
								 SVGRenderer* renderer);
	void OnLayoutFinished(SVGLayoutState& layout_state, const OpRect& known_invalid);

	void CheckPending();
	void UpdateTime();
	void ExecutePendingActions();

	void IntoWorkplace(LogicalDocument* logdoc);

	BOOL WaitForThreadCompletion();
	void CancelThreadListener();

	SVGDocumentContext* m_doc_ctx;
	LogicalDocument* m_logdoc;
	VisualDevice* m_current_vd;

	int m_content_width, m_content_height;
	AffinePos m_document_ctm;

	SVGNumber m_last_known_fps;
	double m_last_time;

	/* These are in device pixel coordinates. */
	OpRect m_pending_area;
	OpRect m_active_paint_area;

	SVGImageInvalidator* m_active_es_listener;

	/** Keeps a local 'is-active' state for svg painting.*/
	OpActivity activity_svgpaint;

	enum
	{
		IDLE,
		RENDER,
		WAITING_FOR_BLIT
	} m_state;

	enum SVGInteractivityState
	{
		UNKNOWN = 0,
		IS_INTERACTIVE = 1,
		IS_NONINTERACTIVE = 2
	};

	union {
		struct {
			unsigned int		is_visible:1;
			unsigned int		timeout_not_allowed:1;
			unsigned int		layout_pass_pending:1;
			unsigned int		update_animations_pending:1;
			unsigned int		invalidate_pending:1;
			unsigned int		is_secure:1;
			unsigned int		invalidation_not_allowed:1;
			unsigned int		interactivity_state:2; // SVGInteractivityState
			/* 9 bits */
		} m_info;
		unsigned short m_info_init;
	};
};

#endif // SVG_SUPPORT
#endif // _SVG_IMAGEIMPL_H
