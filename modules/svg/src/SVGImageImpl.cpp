/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2005-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGImageImpl.h"

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
#include "modules/display/pixelscaler.h"
#endif // PIXEL_SCALE_RENDERING_SUPPORT
#include "modules/display/vis_dev.h"

#include "modules/doc/frm_doc.h"

#include "modules/dochand/fdelm.h"
#include "modules/dochand/win.h"

#include "modules/logdoc/logdoc.h"
#include "modules/svg/src/SVGManagerImpl.h"
#include "modules/svg/src/SVGCache.h"
#include "modules/svg/src/SVGInternalEnum.h"
#include "modules/svg/src/AttrValueStore.h"
#include "modules/svg/src/SVGDocumentContext.h"
#include "modules/svg/src/SVGDebugSettings.h"
#include "modules/svg/src/SVGTraverse.h"
#include "modules/svg/src/SVGChildIterator.h"
#include "modules/svg/src/SVGDynamicChangeHandler.h"
#include "modules/svg/src/SVGRenderer.h"
#include "modules/svg/src/SVGCanvas.h"
#include "modules/svg/src/svgpaintnode.h"

#include "modules/svg/src/animation/svganimationworkplace.h"

#include "modules/layout/cascade.h"
#include "modules/layout/layoutprops.h"
#include "modules/layout/layout_workplace.h"
#include "modules/layout/box/box.h"

#include "modules/prefs/prefsmanager/collections/pc_doc.h"

//#define SVG_DEBUG_CSS_BACKGROUNDS

static FramesDocElm* GetInlineFrame(LogicalDocument* logdoc)
{
	if(logdoc && logdoc->GetFramesDocument())
	{
		DocumentManager* docman = logdoc->GetFramesDocument()->GetDocManager();
		if(docman)
		{
			FramesDocElm* fdelm = docman->GetFrame();
			if(fdelm)
			{
				if(fdelm->IsInlineFrame())
				{
					return fdelm;
				}
			}
		}
	}

	return NULL;
}

SVGImageRefImpl::SVGImageRefImpl(SVG_DOCUMENT_CLASS* doc, int id, HTML_Element* element)
	: m_doc(doc), m_id(id), m_inserted_element(FALSE), m_doc_disconnected(FALSE)
{
}

/* static */ OP_STATUS
SVGImageRefImpl::Make(SVGImageRefImpl*& newref, SVG_DOCUMENT_CLASS* doc, int id, HTML_Element* element)
{
	OP_ASSERT(doc != NULL && element != NULL);

	newref = OP_NEW(SVGImageRefImpl, (doc, id, element));
	if (!newref)
		return OpStatus::ERR_NO_MEMORY;

	newref->SetElm(element);

	OP_STATUS status = g_main_message_handler->SetCallBack(newref, MSG_SVG_INSERT_IMAGEREF, id);
	if(OpStatus::IsError(status))
	{
		OP_DELETE(newref);
		return status;
	}
	
	if (!g_main_message_handler->PostMessage(MSG_SVG_INSERT_IMAGEREF, id, 0))
	{
		OP_DELETE(newref);
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

SVGImageRefImpl::~SVGImageRefImpl()
{
	g_main_message_handler->UnsetCallBacks(this);

	HTML_Element *elm = GetElm();
	if (elm)
	{
		Reset();
		if (m_inserted_element)
		{
			if (m_doc && !m_doc->IsBeingDeleted())
			{
				elm->MarkExtraDirty(LOGDOC_DOC(m_doc));
				elm->OutSafe(LOGDOC_DOC(m_doc));

				if (elm->Clean(LOGDOC_DOC(m_doc)))
					elm->Free(LOGDOC_DOC(m_doc));
			}
		}
		else if (!m_doc)
		{
			// If the element hadn't been inserted yet, we are owners
			// and should delete it, otherwise the HTML_Element
			// deletion happens as the rest of the tree is deleted.
			DELETE_HTML_Element(elm);
		}
	}
	
	m_doc = NULL;
}

OP_STATUS SVGImageRefImpl::Free()
{
	if (!m_doc || m_doc->IsBeingDeleted())
	{
		Out();
		OP_DELETE(this);
	}
	else
	{
		RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_SVG_FREE_IMAGEREF, m_id));

		if (!g_main_message_handler->PostMessage(MSG_SVG_FREE_IMAGEREF, m_id, 0))
			return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

URL* SVGImageRefImpl::GetUrl()
{
	return GetElm() ? GetElm()->GetUrlAttr(ATTR_SRC) : NULL;
}

void SVGImageRefImpl::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	int id = (int)par1;
	if (m_id != id)
		return;

	if (msg == MSG_SVG_FREE_IMAGEREF)
	{
		Out();
		OP_DELETE(this);
	}
	else if (msg == MSG_SVG_INSERT_IMAGEREF)
	{
		if (m_doc && !m_doc->IsBeingDeleted() && m_doc->GetLogicalDocument() && 
			m_doc->GetLogicalDocument()->GetDocRoot() && GetElm())
		{
			HTML_Element *root = m_doc->GetLogicalDocument()->GetDocRoot();
			OP_ASSERT(root);
			GetElm()->UnderSafe(m_doc, root, TRUE);
			m_inserted_element = TRUE;
		}
	}
}

HTML_Element* SVGImageRefImpl::GetContainerElement() const
{
	return GetElm();
}

void SVGImageRefImpl::DisconnectFromDocument() 
{ 
	m_doc = NULL; 
	m_doc_disconnected = TRUE;
}

SVGImage* SVGImageRefImpl::GetSVGImage() const
{
	HTML_Element* cont_elm = GetContainerElement();
	if(cont_elm && m_doc)
	{
		FramesDocElm* frame = FramesDocElm::GetFrmDocElmByHTML(cont_elm);
		if(frame)
		{
			SVG_DOCUMENT_CLASS *frame_doc = frame->GetCurrentDoc();
			if(frame_doc && frame_doc->IsLoaded())
			{
				LogicalDocument *logdoc = frame_doc->GetLogicalDocument();
				if (logdoc != NULL)
				{
					HTML_Element* root = logdoc->GetDocRoot();
					if(root)
					{
						return g_svg_manager_impl->GetSVGImage(logdoc, root);
					}
				}
			}
		}
	}
	return NULL;
}

SVGImageImpl::SVGImageImpl(SVGDocumentContext* doc_ctx, LogicalDocument* logdoc) :
OpInputContext(),
m_doc_ctx(doc_ctx),
m_logdoc(logdoc),
m_current_vd(NULL),
m_content_width(0),
m_content_height(0),
m_last_known_fps(0),
m_last_time(0),
m_active_es_listener(NULL),
activity_svgpaint(ACTIVITY_SVGPAINT),
m_state(IDLE),
m_info_init(0)
{
	OP_ASSERT(doc_ctx);
	// Note that doc_ctx is only a pointer here. We're called from the SVGDocumentContext constructor and can't rely on anything in doc_ctx to work (or not crash)
	OP_ASSERT(logdoc);
	IntoWorkplace(logdoc);
	//		fprintf(stderr, "%p created with is_visible = %d\n", this, m_is_visible);
	
#ifdef SVG_TRUST_SECURE_FLAG
	SetSecure(TRUE); // always start as being secure
#endif // SVG_TRUST_SECURE_FLAG
}

void SVGImageImpl::IntoWorkplace(LogicalDocument* logdoc)
{
	SVGWorkplaceImpl* workplace = static_cast<SVGWorkplaceImpl*>(logdoc->GetSVGWorkplace());
	workplace->InsertIntoSVGList(this);
	SetParentInputContext(logdoc->GetFramesDocument() ? logdoc->GetFramesDocument()->GetVisualDevice() : NULL);
}

SVGImageImpl::~SVGImageImpl()
{
#ifdef SVG_INTERRUPTABLE_RENDERING
	// Only check for renderingstate on topmost stuff
	if (!IsSubSVG())
	{
		SVGRenderer* r = m_doc_ctx->GetRenderingState();
		if (r)
			r->Abort();
	}
#endif // SVG_INTERRUPTABLE_RENDERING
	CancelThreadListener();
	SetParentInputContext(NULL);
}

class SVGImageInvalidator : public ES_ThreadListener
{
public:
	SVGImageInvalidator(SVGImageImpl* image) : m_image(image) {}

	virtual OP_STATUS Signal(ES_Thread *thread, ES_ThreadSignal signal)
	{
		switch (signal)
		{
		case ES_SIGNAL_CANCELLED:
		case ES_SIGNAL_FINISHED:
		case ES_SIGNAL_FAILED:
			{
				m_image->OnThreadCompleted();
			}
		}
		return OpStatus::OK;
	}

private:
	SVGImageImpl* m_image;
};

void SVGImageImpl::OnThreadCompleted()
{
	m_active_es_listener = NULL;

	ScheduleInvalidation();
}

BOOL SVGImageImpl::WaitForThreadCompletion()
{
	if (m_active_es_listener)
		return TRUE;

	if (DOM_Environment* domenvironment = GetDocument() ? GetDocument()->GetDOMEnvironment() : NULL)
		if (ES_Thread* current_thread = domenvironment->GetCurrentScriptThread())
		{
			current_thread = current_thread->GetRunningRootThread();
			m_active_es_listener = OP_NEW(SVGImageInvalidator, (this));
			if (m_active_es_listener)
			{
				current_thread->AddListener(m_active_es_listener);
				return TRUE;
			}
			// Note to self: also add possibility to get out before the
			// thread has finished running
		}

	return FALSE;
}

void SVGImageImpl::CancelThreadListener()
{
	if (m_active_es_listener)
	{
		m_active_es_listener->Remove();
		OP_DELETE(m_active_es_listener);
		m_active_es_listener = NULL;
	}
}

void SVGImageImpl::ScheduleUpdate()
{
	if (WaitForThreadCompletion())
		return;

	if (m_logdoc)
	{
		SVGWorkplaceImpl* workplace = static_cast<SVGWorkplaceImpl*>(m_logdoc->GetSVGWorkplace());
		workplace->ScheduleLayoutPass(m_doc_ctx->CalculateDelay());
	}
}

void SVGImageImpl::ScheduleAnimationFrameAt(unsigned int delay_ms)
{
	if (m_logdoc)
	{
		SVGWorkplaceImpl* workplace = static_cast<SVGWorkplaceImpl*>(m_logdoc->GetSVGWorkplace());
		delay_ms = MAX(m_doc_ctx->CalculateDelay(), delay_ms);
		workplace->ScheduleLayoutPass(delay_ms);
	}
}

void SVGImageImpl::DisconnectFromDocument() 
{
	// Called when the LogicalDocument is destroyed. Nothing can really happen after this point, but we
	// have to clear the references so that we don't access the LogicalDocument in our own destructor.

	SVGDocumentContext* doc_ctx = GetSVGDocumentContext();
	if(doc_ctx)
	{
		SVGRenderer* r = doc_ctx->GetRenderingState();
		if (r)
			r->Abort();

		if(g_svg_manager_impl)
			g_svg_manager_impl->GetCache()->Remove(SVGCache::RENDERER, doc_ctx);

		m_doc_ctx->DestroyAnimationWorkplace();
	}

	CancelThreadListener();

	m_logdoc = NULL;
	SetParentInputContext(NULL);
}

void SVGImageImpl::OnDisable()
{
	SetParentInputContext(NULL);
}

void SVGImageImpl::InvalidateAll()
{
	m_info.interactivity_state = 0; // re-check
	SVGDynamicChangeHandler::MarkWholeSVGForRepaint(m_doc_ctx);
}

/*static */
BOOL SVGImageImpl::IsViewVisible(CoreView* view)
{
	// Calculate the visible rect.
	if (!view->GetVisibility())
		return FALSE;

	OpRect visible_rect = view->GetExtents();

	CoreView* parent = view->GetParent();
	while (parent)
	{
		if (!parent->GetVisibility())
			return FALSE;

		OpRect parent_extents = parent->GetExtents();
		OpRect parent_rect(0, 0, parent_extents.width, parent_extents.height);
		visible_rect.IntersectWith(parent_rect);

		visible_rect.x += parent_extents.x;
		visible_rect.y += parent_extents.y;

		parent = parent->GetParent();
	}

	return !visible_rect.IsEmpty();
}

BOOL SVGImageImpl::IsVisible()
{
	if (m_logdoc && IsVisibleInView())
	{
		SVG_DOCUMENT_CLASS* doc = GetDocument();
		if (doc)
		{
			Window* win = doc->GetWindow();
			if (win && win->IsVisibleOnScreen())
			{
				VisualDevice* vis_dev = doc->GetVisualDevice();
				if (vis_dev)
				{
					CoreView* view = vis_dev->GetView();
					if (view && IsViewVisible(view))
					{
						// The window is visible,
						// and the view is visible in the window
						// and the svg is visible in the view
						return TRUE;
					}
				}
			}
		}
	}
	return FALSE;
}

BOOL
SVGImageImpl::IsInTree()
{
	HTML_Element *element = m_doc_ctx->GetElement()->Parent();
	while(element)
	{
		if (element->Type() == HE_DOC_ROOT)
			return TRUE;
		element = element->Parent();
	}

	return FALSE;
}

/* static */ OP_STATUS
SVGImageImpl::GetDesiredSize(HTML_Element* root, float &width, short &widthUnit,
							 float &height, short &heightUnit)
{
	if (!root || (int)root->Type() != Markup::SVGE_SVG && (int)root->Type() != Markup::SVGE_FOREIGNOBJECT)
		return OpStatus::OK;

	// These are the defaults
	width = 100.f;
	height = 100.f;
	widthUnit = CSS_PERCENTAGE;
	heightUnit = CSS_PERCENTAGE;

	OP_STATUS status = OpStatus::OK;

	if (AttrValueStore::HasObject(root, Markup::SVGA_WIDTH, NS_IDX_SVG))
	{
		SVGLengthObject* dw = NULL;
		status = AttrValueStore::GetLength(root, Markup::SVGA_WIDTH, &dw);

		// invalid length == use default value, NULL is a valid returnvalue
		if (OpStatus::IsSuccess(status) &&
			dw && dw->GetScalar() >= 0)
		{
			width = dw->GetScalar().GetFloatValue();
			widthUnit = dw->GetUnit();
			if (widthUnit == CSS_NUMBER)
				widthUnit = CSS_PX;
		}
	}

	if (OpStatus::IsSuccess(status) && AttrValueStore::HasObject(root, Markup::SVGA_HEIGHT, NS_IDX_SVG))
	{
		SVGLengthObject* dh = NULL;
		status = AttrValueStore::GetLength(root, Markup::SVGA_HEIGHT, &dh);

		// invalid length == use default value, NULL is a valid returnvalue
		if (OpStatus::IsSuccess(status) &&
			dh && dh->GetScalar() >= 0)
		{
			height = dh->GetScalar().GetFloatValue();
			heightUnit = dh->GetUnit();
			if (heightUnit == CSS_NUMBER)
				heightUnit = CSS_PX;
		}
	}

	return status;
}

/* virtual */ OP_STATUS
SVGImageImpl::GetIntrinsicSize(LayoutProperties *cascade, LayoutCoord &width, LayoutCoord &height, int &intrinsic_ratio)
{
	float real_width, real_height;
	short wunit, hunit;

	HTML_Element *root = m_doc_ctx->GetSVGRoot();

	GetDesiredSize(root, real_width, wunit, real_height, hunit);

	VisualDevice* vis_dev = m_doc_ctx->GetVisualDevice();
	const HTMLayoutProperties& props = *(cascade->GetProps());
	CSSLengthResolver length_resolver(vis_dev, FALSE, 0.0f, props.decimal_font_size_constrained, props.font_number, m_doc_ctx->GetDocument() ? m_doc_ctx->GetDocument()->RootFontSize() : LayoutFixed(0));

	if (wunit != CSS_PERCENTAGE)
		width = length_resolver.GetLengthInPixels(real_width, wunit);
	else
		width = LayoutCoord(-1);

	if (hunit != CSS_PERCENTAGE)
		height = length_resolver.GetLengthInPixels(real_height, hunit);
	else
		height = LayoutCoord(-1);

	SVGNumber ratio = 0;

	if (wunit != CSS_PERCENTAGE && hunit != CSS_PERCENTAGE)
		ratio = (float)width / height;
	else
	{
		SVGRectObject *viewbox = NULL;
		if (OpStatus::IsSuccess(AttrValueStore::GetViewBox(root, &viewbox)) && viewbox != NULL)
			ratio = (viewbox->rect.height == 0) ? 0 : viewbox->rect.width / viewbox->rect.height;
	}

	intrinsic_ratio = ratio.GetFixedPointValue(16);

	return OpStatus::OK;
}

OP_STATUS SVGImageImpl::GetViewBox(SVGRect& out_viewBox)
{
	SVGRectObject *viewbox = NULL;
	if (OpStatus::IsSuccess(AttrValueStore::GetViewBox(m_doc_ctx->GetSVGRoot(), &viewbox)) && viewbox != NULL)
	{
		out_viewBox = viewbox->rect;
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

OP_STATUS
SVGImageImpl::GetResolvedSize(VisualDevice* vis_dev, LayoutProperties* props,
								int enclosing_width, int enclosing_height,
								int& width, int& height)
{
	if(!vis_dev)
		return OpStatus::ERR_NULL_POINTER;

	float w, h;
	short wunit, hunit;
	RETURN_IF_ERROR(GetDesiredSize(m_doc_ctx->GetSVGRoot(), w, wunit, h, hunit));

	LayoutFixed font_size;
	int font_number;

	if (props)
	{
		const HTMLayoutProperties& htm_props = *props->GetProps();
		font_size = htm_props.decimal_font_size_constrained;
		font_number = htm_props.font_number;
	}
	else
	{
		font_size = IntToLayoutFixed(vis_dev->GetFontSize());
		font_number = static_cast<int>(vis_dev->GetFontAtt().GetFontNumber());
	}

	CSSLengthResolver length_resolver(vis_dev, FALSE, float(enclosing_width), font_size, font_number,
		m_doc_ctx->GetDocument() ? m_doc_ctx->GetDocument()->RootFontSize() : LayoutFixed(0));
	width = length_resolver.GetLengthInPixels(w, wunit);
	height = length_resolver.ChangePercentageBase(float(enclosing_height)).GetLengthInPixels(h, hunit);

	return OpStatus::OK;
}

OP_STATUS SVGImageImpl::BlitCanvas(VisualDevice* visual_device, const OpRect& area)
{
	SVGRenderer* renderer = m_doc_ctx->GetRenderingState();
	if (!renderer)
		return OpStatus::ERR;

#ifdef _DEBUG
       //canvas->Dump(UNI_L("canvas"));
#endif // _DEBUG

#if defined(SVG_SUPPORT_MEDIA) && defined(PI_VIDEO_LAYER)
	SVGMediaManager* mm = g_svg_manager_impl->GetMediaManager();
	RETURN_IF_ERROR(mm->ClearOverlays(visual_device));
#endif // defined SVG_SUPPORT_MEDIA && defined PI_VIDEO_LAYER

#ifdef SVG_INTERRUPTABLE_RENDERING
	m_doc_ctx->RecordTimeForScreenBlit();
#endif // SVG_INTERRUPTABLE_RENDERING

	VDStateNoScale paint_state = visual_device->BeginNoScalePainting(GetContentRect());

	OpPoint screen_pos_of_svg = paint_state.dst_rect.TopLeft();
	OpBitmap* bitmap = NULL;

#ifndef VEGA_OPPAINTER_SUPPORT
	OP_STATUS status = OpBitmap::Create(&bitmap, area.width, area.height);
	OpBitmap* allocated_bitmap = bitmap;
	if (OpStatus::IsError(status))
	{
		g_memory_manager->RaiseCondition(status);
	}
	else
#endif // !VEGA_OPPAINTER_SUPPORT
	{
		OpRect bitmap_rect = area;
		OpStatus::Ignore(renderer->GetResult(bitmap_rect, &bitmap));

		if (bitmap)
		{
			OpRect rect(area.x, area.y, bitmap_rect.width, bitmap_rect.height);

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
			const PixelScaler& scaler = visual_device->GetVPScale();
			rect = FROM_DEVICE_PIXEL(scaler, rect);
#endif // PIXEL_SCALE_RENDERING_SUPPORT

			rect.OffsetBy(OpPoint(screen_pos_of_svg.x, screen_pos_of_svg.y));
			visual_device->BlitImage(bitmap, bitmap_rect, rect);

#ifndef VEGA_OPPAINTER_SUPPORT
			OP_DELETE(allocated_bitmap);
#endif // !VEGA_OPPAINTER_SUPPORT
		}
	}

	visual_device->EndNoScalePainting(paint_state);
	return OpStatus::OK;
}

void SVGImageImpl::UpdateTime()
{
	double last_time = GetLastTime();
	double stop_time = g_op_time_info->GetRuntimeMS();
	if (last_time != 0)
	{
		int diff = (int)(stop_time - last_time);
		if(diff > 0)
			g_svg_manager_impl->AddFrameTimeSample(diff);
	}
	SetLastTime(stop_time);
}

void SVGImageImpl::CheckPending()
{
#ifdef SVG_INTERRUPTABLE_RENDERING
	if (!m_pending_area.IsEmpty())
	{
		// Invalidate, and let it queue a new paint job
		Invalidate(m_pending_area);
		m_pending_area.Empty();
	}
#endif // SVG_INTERRUPTABLE_RENDERING

	ExecutePendingActions();
	UpdateTime();
#ifdef SVG_INTERRUPTABLE_RENDERING
	ResumeScriptExecution();
#endif // SVG_INTERRUPTABLE_RENDERING
}

#ifndef SVG_INTERRUPTABLE_RENDERING
OP_STATUS SVGImageImpl::OnAreaDone(SVGRenderer* renderer, const OpRect& area)
{
	OP_ASSERT(m_state == RENDER || m_state == WAITING_FOR_BLIT);
	OP_ASSERT(!m_active_paint_area.IsEmpty());

	OP_ASSERT(m_current_vd);

	OpStatus::Ignore(BlitCanvas(m_current_vd, m_active_paint_area));

	m_state = IDLE;
	CheckPending();

	OP_ASSERT(m_state == IDLE);
	activity_svgpaint.End();

	return OpStatus::OK;
}
#else // !SVG_INTERRUPTABLE_RENDERING
OP_STATUS SVGImageImpl::OnAreaDone(SVGRenderer* renderer, const OpRect& area)
{
	OP_ASSERT(m_state == RENDER || m_state == WAITING_FOR_BLIT);

	VisualDevice* vd = m_current_vd ? m_current_vd : m_doc_ctx->GetVisualDevice();
	BOOL is_done = TRUE;

	do
	{
		if (!vd)
			break;

		if (m_current_vd && !m_active_paint_area.IsEmpty() &&
			(m_active_paint_area.Contains(area) ||
			 (m_active_paint_area.Intersecting(area) && m_state == WAITING_FOR_BLIT)))
		{
			// Paint possible
			OpStatus::Ignore(BlitCanvas(m_current_vd, m_active_paint_area));
			break;
		}

		CoreView* view = vd->GetView();
		if (!view)
			break;

		// Note: GetContentRect might be expensive
		RECT offsetRect = {0,0,0,0};
		if (!GetContentRect(offsetRect))
			break;

		OpRect view_rect = view->GetExtents();
		OpPoint view_offset(vd->GetRenderingViewX(), vd->GetRenderingViewY());
		OpPoint content_offset(offsetRect.left, offsetRect.top);
		content_offset = vd->ScaleToScreen(content_offset - view_offset);

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
		const PixelScaler& scaler = vd->GetVPScale();
		content_offset = TO_DEVICE_PIXEL(scaler, content_offset);
		view_rect = TO_DEVICE_PIXEL(scaler, view_rect);
#endif //PIXEL_SCALE_RENDERING_SUPPORT

		OpRect ofs_rect(area);
		ofs_rect.OffsetBy(content_offset);
		if (ofs_rect.Intersecting(view_rect))
		{
			// Request a paint for the area
			Invalidate(area);
			m_state = WAITING_FOR_BLIT;
			is_done = FALSE;
		}
	} while (FALSE);

	if (is_done)
	{
		m_state = IDLE;

		SVGRenderer* r = m_doc_ctx->GetRenderingState();
		r->Abort();

		CheckPending();

		OP_ASSERT(m_state == IDLE);
		activity_svgpaint.End();
	}
	return OpStatus::OK;
}

OP_STATUS SVGImageImpl::OnAreaPartial(SVGRenderer* renderer, const OpRect& area)
{
	OP_ASSERT(m_state == RENDER);
	// FIXME: Check active_area
	// Request a paint for the area
	Invalidate(area);
	return OpStatus::OK;
}

OP_STATUS SVGImageImpl::OnStopped()
{
	m_state = IDLE;

	// Needs to paint the current area again
	SVGRenderer* r = m_doc_ctx->GetRenderingState();
	m_pending_area.UnionWith(r->GetArea());

	CheckPending();

	OP_ASSERT(m_state == IDLE);
	activity_svgpaint.End();

	return OpStatus::OK;
}

OP_STATUS SVGImageImpl::OnError()
{
	m_state = IDLE;

	SVGRenderer* r = m_doc_ctx->GetRenderingState();
	r->Abort();

	OP_ASSERT(m_state == IDLE);
	activity_svgpaint.End();

	return OpStatus::OK;
}
#endif // !SVG_INTERRUPTABLE_RENDERING

void SVGImageImpl::ExecutePendingActions()
{
	if (m_info.update_animations_pending)
		UpdateAnimations();

	if (m_info.layout_pass_pending)
	{
		m_info.layout_pass_pending = 0;
		Layout(NULL, OpRect(), NULL);
	}
	if (m_info.invalidate_pending)
	{
		m_info.invalidate_pending = 0;

		if (SVGRenderer* renderer = m_doc_ctx->GetRenderingState())
		{
			OpRect invalid;
			SVGInvalidState* invalid_state = renderer->GetInvalidState();
			invalid_state->GetExtraInvalidation(invalid);
			OP_ASSERT(!invalid.IsEmpty());
			Invalidate(invalid);
		}
	}
}

void SVGImageImpl::UpdateAnimations()
{
	m_info.update_animations_pending = 0;

	// Update animations
	if (SVGAnimationWorkplace *animation_workplace = m_doc_ctx->GetAnimationWorkplace())
		OpStatus::Ignore(animation_workplace->UpdateAnimations());
}

void SVGImageImpl::UpdateState(int state)
{
	// Don't do anything if we're not even attached to the tree
	if (!m_logdoc || !m_logdoc->GetRoot()->IsAncestorOf(m_doc_ctx->GetElement()))
		return;

	if (state & UPDATE_ANIMATIONS)
		m_info.update_animations_pending = 1;
	if (state & UPDATE_LAYOUT)
		m_info.layout_pass_pending = 1;
	if (m_active_es_listener)
		return;
#ifdef SVG_INTERRUPTABLE_RENDERING
	SVGRenderer* renderer = m_doc_ctx->GetRenderingState();
	if (renderer && renderer->IsActive())
		// We're rendering previous frame, state will be updated
		// when done
		return;
#endif // SVG_INTERRUPTABLE_RENDERING
	ExecutePendingActions();
}

void SVGImageImpl::OnRendererChanged(SVGRenderer* renderer)
{
#ifdef SVG_INTERRUPTABLE_RENDERING
	// Stop the renderer if it was running
	if (renderer->IsActive())
		renderer->Stop();
#endif // SVG_INTERRUPTABLE_RENDERING

	m_state = IDLE;
	m_pending_area.Empty();

	OP_ASSERT(m_state == IDLE);
	activity_svgpaint.Cancel();
}

OP_STATUS SVGImageImpl::UpdateRenderer(VisualDevice* vis_dev, SVGRenderer*& renderer)
{
	SVGNumber vdscale = SVGNumber(vis_dev->GetScale()) / 100;
	int scaled_content_width = vis_dev->ScaleToScreen(m_content_width);
	int scaled_content_height = vis_dev->ScaleToScreen(m_content_height);

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	const PixelScaler& scaler = vis_dev->GetVPScale();
	vdscale = vdscale * scaler.m_multiplier / scaler.m_divider;
	scaled_content_width = TO_DEVICE_PIXEL(scaler, scaled_content_width);
	scaled_content_height = TO_DEVICE_PIXEL(scaler, scaled_content_height);
#endif //PIXEL_SCALE_RENDERING_SUPPORT

	float scale  = vdscale.GetFloatValue();

	OP_ASSERT(scaled_content_width >= 0); // will be casted to unsigned
	OP_ASSERT(scaled_content_height >= 0); // will be casted to unsigned

	renderer = m_doc_ctx->GetRenderingState();

	if (renderer != NULL)
	{
		const OpRect& content_dim = renderer->GetContentSize();

		BOOL content_size_changed = (content_dim.width != m_content_width ||
									 content_dim.height != m_content_height);
		BOOL scale_changed = renderer->GetScale() != scale;

		if (scale_changed || content_size_changed)
		{
			// Did some aspect of the renderer change? If so, stop any
			// activity.
			OnRendererChanged(renderer);

			if (content_size_changed)
			{
				// The viewport size changed - need to relayout
				SVGDynamicChangeHandler::MarkWholeSVGForRepaint(m_doc_ctx);

				g_svg_manager_impl->GetCache()->Remove(SVGCache::RENDERER, m_doc_ctx);
				renderer = NULL;
			}
			else if (scale_changed)
			{
				// The (VD) scale changed - resize the renderer
				RETURN_IF_ERROR(renderer->ScaleChanged(m_content_width, m_content_height,
													   scaled_content_width, scaled_content_height,
													   scale));
			}
		}
	}

	if (renderer == NULL)
	{
		OpAutoPtr<SVGRenderer> rend(OP_NEW(SVGRenderer, ()));
		if (!rend.get())
			return OpStatus::ERR_NO_MEMORY;

		RETURN_IF_ERROR(rend->Create(m_doc_ctx, m_content_width, m_content_height,
									 scaled_content_width, scaled_content_height, scale));
		RETURN_IF_ERROR(g_svg_manager_impl->GetCache()->Add(SVGCache::RENDERER, m_doc_ctx, rend.get()));

		renderer = rend.release();
	}
	return OpStatus::OK;
}

#if defined _DEBUG && defined EXPENSIVE_DEBUG_CHECKS
static void ConsistancyCheckTreeAfterLayoutPass(HTML_Element* svg_root)
{
	OP_ASSERT(svg_root);
	OP_ASSERT(svg_root->IsMatchingType(Markup::SVGE_SVG, NS_SVG));
	HTML_Element* stop = static_cast<HTML_Element*>(svg_root->NextSibling());

	HTML_Element* it = svg_root;
	while (it != stop)
	{
		HTML_Element* real_element = SVGUtils::GetElementToLayout(it);
		if (real_element->GetNsType() == NS_SVG)
		{
			Markup::Type type = real_element->Type();
			BOOL is_animation_thing =
				type == Markup::SVGE_ANIMATEMOTION ||
				type == Markup::SVGE_ANIMATE ||
				type == Markup::SVGE_ANIMATECOLOR ||
				type == Markup::SVGE_ANIMATETRANSFORM ||
				type == Markup::SVGE_SET;

			BOOL skip_check = is_animation_thing/* ||
				real_element->IsMatchingType(Markup::SVGE_TITLE, NS_SVG) ||
				real_element->IsMatchingType(Markup::SVGE_DESC, NS_SVG) ||
				real_element->IsMatchingType(Markup::SVGE_SCRIPT, NS_SVG)*/;
			if (!skip_check)
			{
				SVGElementContext* elm_ctx = AttrValueStore::GetSVGElementContext(it);
				OP_ASSERT(elm_ctx); // If they are created automatically later, then it will have wrong validation flags
				if (elm_ctx)
				{
					OP_ASSERT(elm_ctx->HasTrustworthyScreenBox());
					OP_ASSERT(!elm_ctx->HasSubTreeChanged());
					OP_ASSERT(!elm_ctx->IsCSSReloadNeeded());
				}
			}
		}
		it = it->Next();
	}
}
#endif // _DEBUG

void SVGImageImpl::OnLayoutFinished(SVGLayoutState& layout_state,
									const OpRect& known_invalid)
{
	OP_NEW_DBG("SVGImageImpl::OnLayoutFinished", "svg_invalid");

#if defined _DEBUG && defined EXPENSIVE_DEBUG_CHECKS
	ConsistancyCheckTreeAfterLayoutPass(m_doc_ctx->GetSVGRoot());
#endif // _DEBUG

	OpRect& invalid = layout_state.invalid_area;
	SVGInvalidState* invalid_state = layout_state.invalid_state;

	OP_DBG(("resulted in (%d,%d,%d,%d)=",
			invalid.x, invalid.y, invalid.width, invalid.height));

	invalid_state->GetExtraInvalidation(invalid);

	OP_DBG(("(%d,%d,%d,%d)", invalid.x, invalid.y, invalid.width, invalid.height));
	OP_DBG(("Already invalid (%d,%d,%d,%d)", known_invalid.x, known_invalid.y,
			known_invalid.width, known_invalid.height));
#ifdef _DEBUG
	if (known_invalid.Contains(invalid))
	{
		OP_DBG(("Skipping update since we're already painting that area.\n"));
	}
#endif // _DEBUG

	if (!invalid.IsEmpty())
	{
#ifndef SVG_OPTIMIZE_RENDER_MULTI_PASS
		OpStatus::Ignore(invalid_state->Invalidate(invalid));
#endif // SVG_OPTIMIZE_RENDER_MULTI_PASS

		if (!known_invalid.Contains(invalid))
		{
			if (!known_invalid.IsEmpty())
			{
				// Subtract "already known invalid" from the invalidation rect
				OpRegion extra_painting;
				if (extra_painting.IncludeRect(invalid) &&
					extra_painting.RemoveRect(known_invalid))
				{
					invalid = extra_painting.GetUnionOfRects();
				}
			}
			Invalidate(invalid);
		}

		layout_state.invalid_area.Empty();
	}
}

#ifdef _DEBUG
class DebugLayoutPassReentrance
{
public:
	DebugLayoutPassReentrance()
	{
		OP_ASSERT(!g_svg_debug_is_in_layout_pass);
		g_svg_debug_is_in_layout_pass = TRUE;
	}
	~DebugLayoutPassReentrance()
	{
		OP_ASSERT(g_svg_debug_is_in_layout_pass);
		g_svg_debug_is_in_layout_pass = FALSE;
	}
};
#define DEBUG_LAYOUTPASS_REENTRANCE DebugLayoutPassReentrance __dlpreent
#else
#define DEBUG_LAYOUTPASS_REENTRANCE ((void)0)
#endif // _DEBUG

OP_STATUS SVGImageImpl::LayoutNoInvalidate(LayoutProperties* layout_props, const OpRect& known_invalid,
										   SVGRenderer* renderer)
{
	m_info.invalidation_not_allowed = TRUE;

	OP_STATUS status = Layout(layout_props, known_invalid, renderer);

	m_info.invalidation_not_allowed = FALSE;

	return status;
}

OP_STATUS SVGImageImpl::Layout(LayoutProperties* layout_props, const OpRect& known_invalid,
							   SVGRenderer* renderer)
{
	if (!m_doc_ctx->IsInvalidationPending())
		return OpStatus::OK;

	if (m_doc_ctx->IsExternal())
	{
		// No layoutpass in external SVGs because these has to be done
		// by the mother-document. Otherwise we may overwrite correct
		// data with incorrect data.
		m_doc_ctx->SetInvalidationPending(FALSE);
		return OpStatus::OK;
	}

	m_info.invalidate_pending = 0; // Layout will trigger invalidation

	OP_NEW_DBG("SVGImageImpl::Layout", "svg_invalid");

	// Calculate rect to invalidate
	if (renderer == NULL)
		renderer = m_doc_ctx->GetRenderingState();

	OP_STATUS status = OpStatus::OK;

	// Hmm, without a renderer we can't invalidate. Why? [because we
	// need the region] And what if they have been pushed out from the
	// cache?
	if (renderer)
	{
		DEBUG_LAYOUTPASS_REENTRANCE;

		SVGInvalidationCanvas canvas;
		canvas.SetDefaults(m_doc_ctx->GetRenderingQuality());
		canvas.SetBaseScale(renderer->GetScale());

		canvas.ResetTransform();

		SVGLayoutState layout_state;
		SVGInvalidState* invalid_state = renderer->GetInvalidState();
		layout_state.Set(invalid_state);

#ifdef LAZY_LOADPROPS
		// Avoid calls to MarkForRepaint while layouting
		m_doc_ctx->GetHLDocProfile()->GetLayoutWorkplace()->ReloadCssProperties();
#endif // LAZY_LOADPROPS

		SVGRenderingTreeChildIterator rtci;
		SVGLayoutObject layout_object(&rtci);

		RETURN_IF_ERROR(layout_object.SetupResolver());

		layout_object.SetDocumentContext(m_doc_ctx);
		layout_object.SetLayoutState(&layout_state);
		layout_object.SetCanvas(&canvas);

		m_doc_ctx->SetInvalidationPending(FALSE);

 		OP_DBG(("Invalidation... "));

		status = SVGTraverser::Traverse(&layout_object, m_doc_ctx->GetSVGRoot(), layout_props);

		OP_ASSERT(canvas.GetStackDepth() == 0);

		OnLayoutFinished(layout_state, known_invalid);

		OP_ASSERT(!m_doc_ctx->IsInvalidationPending());
	}
	else
	{
		// This shouldn't happen if we are in a Paint
		OP_ASSERT(m_current_vd == NULL);
		Invalidate(OpRect());
	}
	return status;
}

BOOL SVGImageImpl::GetContentRect(RECT& rect)
{
	if (IsPainting())
	{
		// We are in a paint, so the current document position and
		// content-size should be correct.
		OpRect doc_rect = GetDocumentRect();
		rect.left = doc_rect.x;
		rect.top = doc_rect.y;
		rect.right = doc_rect.x + doc_rect.width;
		rect.bottom = doc_rect.y + doc_rect.height;
		return TRUE;
	}

	HTML_Element* svg_root = m_doc_ctx->GetSVGRoot();
	LogicalDocument* logdoc = m_doc_ctx->GetLogicalDocument();
	if (logdoc)
		return logdoc->GetBoxRect(svg_root, CONTENT_BOX, rect);
	else
		return FALSE;
}

void SVGImageImpl::Invalidate(const OpRect& invalid)
{
	if(m_info.invalidation_not_allowed)
		return;

	if (VisualDevice* vis_dev = m_doc_ctx->GetVisualDevice())
	{
		RECT offsetRect = {0,0,0,0};

		if (GetContentRect(offsetRect))
		{
			if (CoreView* view = vis_dev->GetView())
			{
				OpRect invalidate_screenrect(&offsetRect);

				invalidate_screenrect.x -= vis_dev->GetRenderingViewX();
				invalidate_screenrect.y -= vis_dev->GetRenderingViewY();

				invalidate_screenrect = vis_dev->ScaleToScreen(invalidate_screenrect);

				// Empty 'invalid' means 'invalidate everything'
				if (!invalid.IsEmpty())
				{
					OpRect clip(invalidate_screenrect);
					OpRect invalid_rect = invalid;

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
					const PixelScaler& scaler = vis_dev->GetVPScale();
					invalid_rect = FROM_DEVICE_PIXEL(scaler, invalid_rect);
#endif //PIXEL_SCALE_RENDERING_SUPPORT

					// Add offset if any
					invalidate_screenrect.x += invalid_rect.x;
					invalidate_screenrect.y += invalid_rect.y;

					invalidate_screenrect.width = invalid_rect.width;
					invalidate_screenrect.height = invalid_rect.height;

					// Clip against the content rect
					invalidate_screenrect.IntersectWith(clip);
				}

				OP_NEW_DBG("SVGImageImpl::Invalidate", "svg_invalid");

				OP_DBG(("view->Invalidate(%d,%d,%d,%d)",
					invalidate_screenrect.x,
					invalidate_screenrect.y,
					invalidate_screenrect.width,
					invalidate_screenrect.height));

				m_doc_ctx->RecordInvalidCall();

				view->Invalidate(invalidate_screenrect);
			}
		}
		return;
	}
	OP_ASSERT(FALSE);
}

void SVGImageImpl::QueueInvalidate(const OpRect& invalid_rect)
{
	SVGRenderer* renderer = m_doc_ctx->GetRenderingState();
	OP_ASSERT(renderer);

	if (WaitForThreadCompletion())
	{
		m_info.invalidate_pending = 1;

		// The invalidated area should also be added to the renderer,
		// note that SVGPaintNode::AddPixelExtents also does this.
		renderer->GetInvalidState()->AddExtraInvalidation(invalid_rect);
	}
	else if (renderer->IsActive())
	{
		OP_ASSERT(!invalid_rect.IsEmpty());
		m_pending_area.UnionWith(invalid_rect);
	}
	else
	{
		Invalidate(invalid_rect);
	}
}

#ifdef SVG_SUPPORT_PANNING
void SVGImageImpl::Pan(int doc_pan_x, int doc_pan_y)
{
	SVGRenderer* renderer = m_doc_ctx->GetRenderingState();
	if (renderer)
	{
		renderer->Stop();

		if (VisualDevice* vis_dev = m_doc_ctx->GetVisualDevice())
		{
			SVGViewportCompositePaintNode* vpnode =
				static_cast<SVGViewportCompositePaintNode*>(m_doc_ctx->GetPaintNode());
			if (vpnode)
			{
				SVGMatrix user_transform;
				m_doc_ctx->GetCurrentMatrix(user_transform);
				vpnode->SetUserTransform(user_transform);

				int pixel_pan_x = vis_dev->ScaleToScreen(doc_pan_x);
				int pixel_pan_y = vis_dev->ScaleToScreen(doc_pan_y);

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
				const PixelScaler& scaler = vis_dev->GetVPScale();
				int dp_pan_x = TO_DEVICE_PIXEL(scaler, pixel_pan_x);
				int dp_pan_y = TO_DEVICE_PIXEL(scaler, pixel_pan_y);
				if (OpStatus::IsSuccess(renderer->Move(-dp_pan_x, dp_pan_y)))
#else
				if (OpStatus::IsSuccess(renderer->Move(-pixel_pan_x, -pixel_pan_y)))
#endif //PIXEL_SCALE_RENDERING_SUPPORT
				{
					RECT offsetRect = {0,0,0,0};

					// Note: GetContentRect might be expensive
					if (GetContentRect(offsetRect))
					{
						OP_NEW_DBG("SVGImageImpl::Pan", "svg_invalid");

						OpRect view_r(&offsetRect);
						view_r.x -= vis_dev->GetRenderingViewX();
						view_r.y -= vis_dev->GetRenderingViewY();
						view_r = vis_dev->ScaleToScreen(view_r);

						OpPoint view_offset(vis_dev->GetRenderingViewX(), vis_dev->GetRenderingViewY());
						view_offset = vis_dev->ScaleToScreen(view_offset);

						int box_scr_width = view_r.width;
						int box_scr_height = view_r.height;

						CoreView* view = vis_dev->GetView();
						if (view)
							view_r.IntersectWith(view->GetExtents());

						OP_DBG(("Pan (%d, %d)", pixel_pan_x, pixel_pan_y));
						if (pixel_pan_x != 0)
						{
							OpRect pan_rect(view_offset.x, 0, pixel_pan_x, box_scr_height);
							if (pixel_pan_x > 0)
							{
								pan_rect.x += view_r.width - pixel_pan_x;
							}
							else
							{
								pan_rect.width = -pan_rect.width;
							}

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
							pan_rect = TO_DEVICE_PIXEL(scaler, pan_rect);
#endif //PIXEL_SCALE_RENDERING_SUPPORT

							renderer->GetInvalidState()->Invalidate(pan_rect);
						}
						if (pixel_pan_y != 0)
						{
							OpRect pan_rect(0, view_offset.y, box_scr_width, pixel_pan_y);
							if (pixel_pan_y > 0)
							{
								pan_rect.y += view_r.height - pixel_pan_y;
							}
							else
							{
								pan_rect.height = -pan_rect.height;
							}

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
							pan_rect = TO_DEVICE_PIXEL(scaler, pan_rect);
#endif //PIXEL_SCALE_RENDERING_SUPPORT

							renderer->GetInvalidState()->Invalidate(pan_rect);
						}

						if (view)
						{
							OP_DBG(("view->Invalidate(%d,%d,%d,%d)",
									view_r.x, view_r.y, view_r.width, view_r.height));

							m_doc_ctx->RecordInvalidCall();
							view->Invalidate(view_r);
						}
					}
					else
					{
						OP_ASSERT(!"Couldn't get boxrect, will give bad performance.");
						m_doc_ctx->RecordInvalidCall();
						vis_dev->UpdateAll();
					}
					return;
				}
			}
		}
	}

	// Hammer fallback
	SVGDynamicChangeHandler::MarkWholeSVGForRepaint(m_doc_ctx);
}
#endif // SVG_SUPPORT_PANNING

#ifdef SVG_INTERRUPTABLE_RENDERING
#define DEFAULT_POLICY SVGRenderer::POLICY_ASYNC
#else
#define DEFAULT_POLICY SVGRenderer::POLICY_SYNC
#endif // SVG_INTERRUPTABLE_RENDERING

OP_STATUS SVGImageImpl::PaintOnScreenInternal(VisualDevice* visual_device,
											  LayoutProperties* layout_props,
											  const OpRect& screen_area_to_paint)
{
	OP_ASSERT(m_doc_ctx->GetSVGRoot());
	OP_ASSERT(m_doc_ctx->GetSVGRoot()->IsMatchingType(Markup::SVGE_SVG, NS_SVG));

	SVGRenderer* renderer = NULL;
	RETURN_IF_ERROR(UpdateRenderer(visual_device, renderer));

#ifdef SVG_SUPPORT_LOCKING
	if (IsLocked())
	{
		// Put back the rect so that we can use it later when
		// invalidating.
		renderer->GetInvalidState()->AddExtraInvalidation(screen_area_to_paint);
		return OpStatus::OK;
	}
#endif // SVG_SUPPORT_LOCKING

#ifdef SVG_INTERRUPTABLE_RENDERING
	// Queue area
	if (renderer->IsActive() && !renderer->Contains(screen_area_to_paint))
		m_pending_area.UnionWith(screen_area_to_paint);

	switch (m_state)
	{
	case RENDER:
		// Do partial blit (or maybe not)
		return BlitCanvas(visual_device, screen_area_to_paint);
	case WAITING_FOR_BLIT:
		return renderer->Update();
	}
#endif // SVG_INTERRUPTABLE_RENDERING

	OP_ASSERT(m_state == IDLE);

	// Since we got a paint message we know that we're visible
	SetVisibleInView(TRUE);

	// Synchronize all internal state if things have changed.
	if (m_doc_ctx->IsInvalidationPending())
	{
		// Normally we don't want to update the animation time and such here since
		// that would make the current repaint area wrong and probably cut
		// off parts of the animated elements, but there is one exception,
		// when the animation processing has been put on hold since we
		// haven't been visible. Then it has to be restarted and updated
		// to the current time and we do that here, when we get a paint.
		m_info.layout_pass_pending = 0; // To avoid layouting twice
		UpdateState(UPDATE_ANIMATIONS);
		Layout(layout_props, screen_area_to_paint, NULL);
	}

	// FIXME: Union with pending area?
	OP_ASSERT(m_pending_area.IsEmpty());

	renderer->SetPolicy(DEFAULT_POLICY);
	renderer->SetAllowTimeout(IsTimeoutAllowed() &&
							  (GetDocument() && !GetDocument()->IsPrintDocument()));
	renderer->SetAllowPartial(IsPartialAllowed());
	RETURN_IF_ERROR(renderer->Setup(screen_area_to_paint));
	renderer->SetListener(this);

	activity_svgpaint.Begin();

	OP_STATUS status = OpStatus::OK;
	m_state = RENDER;

	{
		SVG_PROBE_START("SVGImageImpl::PaintOnScreenInternal");

		status = renderer->Update();

#ifdef SVG_INTERRUPTABLE_RENDERING
		if (renderer->HasBufferedResult())
			OpStatus::Ignore(BlitCanvas(visual_device, screen_area_to_paint));
#endif // SVG_INTERRUPTABLE_RENDERING

		SVG_PROBE_END();
	}

#ifdef SVG_INTERRUPTABLE_RENDERING
	if(status != OpSVGStatus::TIMED_OUT)
#endif // SVG_INTERRUPTABLE_RENDERING
	{
		unsigned int average = g_svg_manager_impl->GetAverageFrameTime();

		if (average != 0)
		{
			SVGNumber fps = SVGNumber(1000) / average;
			SetLastKnownFPS(fps);
		}

		m_doc_ctx->RecordPaintEnd();
	}

	// FIXME: Handle errors ?

	return status;
}

/* virtual */
OP_STATUS SVGImageImpl::PaintToBuffer(OpBitmap*& bitmap, int time_to_draw, int width, int height, SVGRect* override_viewbox)
{
	if (!IsInTree())
		return OpStatus::ERR;

	SVGRenderer* r = m_doc_ctx->GetRenderingState();
	if (r && (r->IsActive() || m_state != IDLE))
	{
		m_pending_area.UnionWith(r->GetArea());
		r->Stop();
	}

	SVGDynamicChangeHandler::MarkWholeSVGForRepaint(m_doc_ctx);

	BOOL height_was_specified = (height >= 0);
	BOOL width_was_specified = (width >= 0);

	HLDocProfile* hld_profile = m_doc_ctx->GetHLDocProfile();

	if(!hld_profile)
		return OpStatus::ERR;

	AutoDeleteHead prop_list;

	LayoutProperties *props = LayoutProperties::CreateCascade(m_doc_ctx->GetSVGRoot(), prop_list, LAYOUT_WORKPLACE(hld_profile));
	if(!props)
		return OpStatus::ERR_NO_MEMORY;

	VisualDevice* vd = m_doc_ctx->GetVisualDevice();
	if(!vd)
		return OpStatus::ERR;

	if(width < 0 || height < 0)
	{
		if (r)
		{
			OpRect image_rect = vd->ScaleToScreen(GetDocumentRect());

			if (width < 0)
			{
				width = (m_doc_ctx->GetCurrentScale() * image_rect.width).GetIntegerValue();
			}
			if (height < 0)
			{
				height = (m_doc_ctx->GetCurrentScale() * image_rect.height).GetIntegerValue();
			}
		}
		else
		{
			OpStatus::Ignore(GetResolvedSize(vd, props, 300, 150, width, height));
		}

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
		width = TO_DEVICE_PIXEL(vd->GetVPScale(), width);
		height = TO_DEVICE_PIXEL(vd->GetVPScale(), height);
#endif //PIXEL_SCALE_RENDERING_SUPPORT
	}

	// Only adjust aspect if one or both of width/height was not given
	if(!height_was_specified || !width_was_specified)
	{
		SVGNumber s_aspect(1);
		SVGRectObject *viewbox;
		OP_STATUS err = AttrValueStore::GetViewBox(m_doc_ctx->GetSVGRoot(), &viewbox);
		if (OpStatus::IsSuccess(err) && viewbox)
			s_aspect = (viewbox->rect.height == 0) ? 0 : viewbox->rect.width / viewbox->rect.height;

		if (!s_aspect.Close(SVGNumber(width) / SVGNumber(height)))
		{
			double aspect = s_aspect.GetFloatValue();

			if(width_was_specified || (!height_was_specified && !width_was_specified))
			{
				height = (int)(height / aspect);
			}
			else if(height_was_specified)
			{
				width = (int)(width * aspect);
			}
		}
	}

	m_doc_ctx->ForceSize(width, height);
	m_doc_ctx->SetForceViewbox(override_viewbox);

	SVG_ANIMATION_TIME previous_animation_time = -1;
	SVGAnimationWorkplace::AnimationStatus previous_animation_status = SVGAnimationWorkplace::STATUS_PAUSED;

	SVGAnimationWorkplace *animation_workplace = m_doc_ctx->GetAnimationWorkplace();
	if (animation_workplace && time_to_draw >= 0)
	{
		previous_animation_status = animation_workplace->GetAnimationStatus();
		RETURN_IF_ERROR(animation_workplace->ProcessAnimationCommand(SVGAnimationWorkplace::ANIMATION_PAUSE));

		previous_animation_time = animation_workplace->DocumentTime();
		animation_workplace->SetDocumentTime(time_to_draw);
		RETURN_IF_ERROR(animation_workplace->UpdateAnimations());
	}

	OpRect paint_area(0, 0, width, height);

	SVGRenderer renderer;
	RETURN_IF_ERROR(renderer.Create(m_doc_ctx, width, height, width, height, 1.0));

	if (m_doc_ctx->IsInvalidationPending())
		RETURN_IF_ERROR(LayoutNoInvalidate(props, paint_area, &renderer));

	renderer.SetAllowTimeout(FALSE);
	renderer.SetPolicy(SVGRenderer::POLICY_SYNC);

	UINT32 old_visdev_scale = vd->SetTemporaryScale(100);

	OP_STATUS result = renderer.Setup(paint_area);
	if (OpStatus::IsSuccess(result))
	{
		result = renderer.Update();
	}

	vd->SetTemporaryScale(old_visdev_scale);
	m_doc_ctx->SetForceViewbox(NULL);

	RETURN_IF_ERROR(result);

	// Allocate new bitmap
	OP_STATUS status =
		OpBitmap::Create(&bitmap, paint_area.width, paint_area.height,
						 FALSE, TRUE, 0, 0, 
#ifdef VEGA_OPPAINTER_SUPPORT
						 TRUE
#else
						 FALSE
#endif // VEGA_OPPAINTER_SUPPORT
						 );

	RETURN_IF_ERROR(status);
	RETURN_IF_ERROR(renderer.CopyToBitmap(paint_area, &bitmap));

#if defined SVG_DEBUG_CSS_BACKGROUNDS && defined IMG_DUMP_TO_BMP
	DumpOpBitmap(bitmap, UNI_L("css_background"), TRUE);
#endif // SVG_DEBUG_CSS_BACKGROUNDS && IMG_DUMP_TO_BMP

	if (previous_animation_time >= 0)
	{
		animation_workplace->SetDocumentTime(previous_animation_time);
		RETURN_IF_ERROR(animation_workplace->UpdateAnimations());
	}

	if (previous_animation_status == SVGAnimationWorkplace::STATUS_RUNNING)
		RETURN_IF_ERROR(animation_workplace->ProcessAnimationCommand(SVGAnimationWorkplace::ANIMATION_UNPAUSE));

	SVGDynamicChangeHandler::MarkWholeSVGForRepaint(m_doc_ctx);

	return OpStatus::OK;
}

BOOL SVGImageImpl::IsZoomAndPanAllowed()
{
	SVGZoomAndPan zoomandpan =
		(SVGZoomAndPan)AttrValueStore::GetEnumValue(m_doc_ctx->GetSVGRoot(), Markup::SVGA_ZOOMANDPAN,
													SVGENUM_ZOOM_AND_PAN,
													SVGZOOMANDPAN_MAGNIFY);
	return (zoomandpan == SVGZOOMANDPAN_MAGNIFY);
}

BOOL SVGImageImpl::HasAnimation()
{
	SVGAnimationWorkplace *animation_workplace = m_doc_ctx->GetAnimationWorkplace();
	if(!animation_workplace)
		return FALSE;

	return animation_workplace->HasAnimations();
}

BOOL SVGImageImpl::IsAnimationRunning()
{
	SVGAnimationWorkplace *animation_workplace = m_doc_ctx->GetAnimationWorkplace();
	if(!animation_workplace)
		return FALSE;

	return animation_workplace->IsValidCommand(SVGAnimationWorkplace::ANIMATION_PAUSE);
}

OP_STATUS SVGImageImpl::SetURLRelativePart(const uni_char* rel_part)
{
	// Old implementation. FIXME Improve this
	InvalidateAll();
	return OpStatus::OK;
}

BOOL SVGImageImpl::HasSelectedText()
{
#ifdef SVG_SUPPORT_TEXTSELECTION
	return !GetSVGDocumentContext()->GetTextSelection().IsEmpty();
#else
	return  FALSE;
#endif // SVG_SUPPORT_TEXTSELECTION
}

/* virtual */ BOOL 	
SVGImageImpl::OnInputAction(OpInputAction* action) 	
{ 
	return g_svg_manager->OnInputAction(action, m_doc_ctx->GetSVGRoot(), GetDocument());
}
 	
/* virtual */ void 	
SVGImageImpl::OnKeyboardInputGained(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason) 	
{ 	
    OpInputContext::OnKeyboardInputGained(new_input_context, old_input_context, reason); 	
 	
    if (this != new_input_context || !m_logdoc)
        return; 	
 	
	FramesDocument* doc = m_logdoc->GetFramesDocument();
	if(!doc)
		return;

    // If svg gains focus then clear textselection in surrounding document
	FramesDocument* parent_doc = doc->GetParentDoc();
    while (parent_doc)
	{
		doc = parent_doc;
		parent_doc = parent_doc->GetParentDoc();
	}
	doc->ClearSelection(TRUE);
} 	
 	
/* virtual */ void  	
SVGImageImpl::OnKeyboardInputLost(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason) 	
{ 	
	OpInputContext::OnKeyboardInputLost(new_input_context, old_input_context, reason); 	
 	
    if (this != old_input_context) 	
        return; 	
     	
    OpInputAction action(OpInputAction::ACTION_DESELECT_ALL); 	
	g_svg_manager->OnInputAction(&action, m_doc_ctx->GetSVGRoot(), m_logdoc ? m_logdoc->GetFramesDocument() : NULL);
} 

/* virtual */ UINT32
SVGImageImpl::GetLastKnownFPS()
{
	return m_last_known_fps.GetFixedPointValue(16);
}

/* virtual */ void 
SVGImageImpl::SetTargetFPS(UINT32 fps)
{ 
	m_doc_ctx->SetTargetFPS(fps); 
}

/* virtual */ UINT32 
SVGImageImpl::GetTargetFPS() 
{ 
	return m_doc_ctx->GetTargetFPS(); 
}

/* virtual */
void SVGImageImpl::SwitchDocument(LogicalDocument* new_document)
{
	// Remove from the previous document's svg workplace
	if(m_logdoc)
	{
		SVGWorkplaceImpl* workplace = static_cast<SVGWorkplaceImpl*>(m_logdoc->GetSVGWorkplace());
		workplace->RemoveFromSVGList(this);
	}

	m_logdoc = new_document;

	// Insert into new document's svg workplace
	if(new_document)
	{
		IntoWorkplace(new_document);
	}
	else
	{
		CancelThreadListener();
		SetParentInputContext(NULL);
	}
}

BOOL SVGImageImpl::IsInImgElement()
{
	if (FramesDocElm* fdelm = GetInlineFrame(m_logdoc))
		if (HTML_Element* frame_elm = fdelm->GetHtmlElement())
			return frame_elm->IsMatchingType(HE_IMG, NS_HTML) || frame_elm->IsMatchingType(HE_INPUT, NS_HTML);

	return FALSE;
}

BOOL SVGImageImpl::IsParamSet(const char* name, const char* value)
{
	BOOL match = FALSE;
	if(FramesDocElm* fdelm = GetInlineFrame(m_logdoc))
	{
		// Linear search through the parent document
		if(HTML_Element* frame_elm = fdelm->GetHtmlElement())
		{
			if(frame_elm->IsMatchingType(HE_OBJECT, NS_HTML))
			{
				int param_count = frame_elm->CountParams();
				if (param_count)
				{
					int next_idx = 0;
					const uni_char** param_keys = OP_NEWA(const uni_char*, param_count);
					const uni_char** param_values = OP_NEWA(const uni_char*, param_count);

					if (param_keys && param_values)
					{
						frame_elm->GetParams(param_keys, param_values, next_idx);

						if (next_idx < param_count)
							param_count = next_idx;
						
						for(int i = 0; i < param_count; i++)
						{
							if(uni_str_eq(param_keys[i], name))
							{
								match = uni_str_eq(param_values[i], value);
							}
						}
					}

					OP_DELETEA(param_keys);
					OP_DELETEA(param_values);
				}
			}
		}
	}

	return match;
}

BOOL SVGImageImpl::IsEcmaScriptEnabled(FramesDocument* frm_doc) 
{
	if(GetDocument() && !g_svg_manager_impl->IsEcmaScriptEnabled(GetDocument())) 
		return FALSE;

	if(IsParamSet("render", "frozen"))
		return FALSE;

	return g_svg_manager_impl->IsEcmaScriptEnabled(frm_doc);
}

BOOL SVGImageImpl::IsInteractive()
{
	if(!m_info.interactivity_state)
	{
		m_info.interactivity_state = IS_INTERACTIVE;
		FramesDocElm* fdelm = GetInlineFrame(m_logdoc);
		while(fdelm)
		{
			// Linear search through the parent document
			HTML_Element* frame_elm = fdelm->GetHtmlElement();
			if(frame_elm)
			{
				int type = frame_elm->Type();
				NS_Type ns = frame_elm->GetNsType();
				if(ns == NS_HTML)
				{
					if(type == HE_IMG || type == HE_INPUT ||
						(frame_elm->GetInserted() == HE_INSERTED_BY_SVG &&  
						 type == HE_IFRAME)
#ifdef ON_DEMAND_PLUGIN
						 || ((type == HE_OBJECT || type == HE_EMBED || type == HE_APPLET) && // testing type first, because it is less expensive than the placeholder check
							  frame_elm->IsPluginPlaceholder())
#endif // ON_DEMAND_PLUGIN
						 )
					{
						m_info.interactivity_state = IS_NONINTERACTIVE;
						break;
					}
					else if(type == HE_OBJECT)
					{					
						if(IsParamSet("render", "frozen"))
						{
							m_info.interactivity_state = IS_NONINTERACTIVE;
							break;
						}
					}
				}
			}

			fdelm = fdelm->Parent();
		}
	}

	return(m_info.interactivity_state == IS_INTERACTIVE);
}

BOOL SVGImageImpl::IsSubSVG()
{
	HTML_Element* our_svg_elm = GetSVGDocumentContext()->GetElement();
	return our_svg_elm != SVGUtils::GetTopmostSVGRoot(our_svg_elm);
}

/* virtual */ SVGNumber
SVGImageImpl::GetUserZoomLevel()
{
	// Check for doc manager. If we are being disconnected from
	// document, this may be NULL.
	SVGDocumentContext *doc_ctx = GetSVGDocumentContext();
	OP_ASSERT(doc_ctx);

	// We have user zoom-level 1.0 if we are embedded.
	return (doc_ctx->IsStandAlone()) ? doc_ctx->GetCurrentScale() : 1.0;
}

/* virtual */ OP_STATUS
SVGImageImpl::SetAnimationTime(int ms)
{
	SVGAnimationWorkplace *animation_workplace = m_doc_ctx->GetAnimationWorkplace();
	if (animation_workplace && ms >= 0)
	{
		RETURN_IF_ERROR(animation_workplace->ProcessAnimationCommand(SVGAnimationWorkplace::ANIMATION_PAUSE));
		RETURN_IF_ERROR(animation_workplace->SetDocumentTime(ms));
		RETURN_IF_ERROR(animation_workplace->UpdateAnimations());
	}
	return OpStatus::OK;
}

/* virtual */ void
SVGImageImpl::SetVirtualAnimationTime(int ms)
{
	SVGAnimationWorkplace *animation_workplace = m_doc_ctx->GetAnimationWorkplace();
	if (animation_workplace)
	{
		if (ms >= 0)
			animation_workplace->ForceTime((SVG_ANIMATION_TIME)ms);
		else
			animation_workplace->UnforceTime();
	}
}

/* virtual */ OP_STATUS
SVGImageImpl::OnReflow(LayoutProperties* cascade)
{
	OP_NEW_DBG("SVGImageImpl::OnReflow", "svg_reflow");
	SVGRenderer* renderer = NULL;
	OP_STATUS status;

	if (m_content_width > 0 && m_content_height > 0)
		if (VisualDevice* visual_device = m_doc_ctx->GetVisualDevice())
			status = UpdateRenderer(visual_device, renderer);

	OpRect known_invalid;

	status = LayoutNoInvalidate(cascade, known_invalid, renderer);

	OP_DBG((UNI_L("Status: %d Known_invalid (%d,%d,%d,%d) %s %d id: %s"),
			status,
			known_invalid.x, known_invalid.y, known_invalid.width, known_invalid.height,
			GetDocumentRect().IsEmpty() ? UNI_L("Unresolved docpos") : UNI_L(""),
			cascade->html_element->Type(),
			cascade && cascade->html_element && cascade->html_element->GetId() ? cascade->html_element->GetId() : UNI_L("N/A")));

	return status;
}

/* virtual */ OP_STATUS
SVGImageImpl::OnPaint(VisualDevice* visual_device, LayoutProperties* cascade, const RECT &area)
{
	OP_NEW_DBG("SVGImageImpl::OnPaint", "svg_paint");
	const OpRect rendering_view = visual_device->GetRenderingViewport();
	OpRect doc_paint_area; // Repaint area in doc coordinates
	doc_paint_area.x = area.left + rendering_view.x;
	doc_paint_area.y = area.top  + rendering_view.y;
	doc_paint_area.width = area.right - area.left;
	doc_paint_area.height = area.bottom - area.top;

	// Repaint area in (content) local coordinates
	OpRect local_paint_area = doc_paint_area;
	m_document_ctm.ApplyInverse(local_paint_area);

	OpRect local_area = GetContentRect(); // svg content area

	// Clip paint area to content (in local space)
	local_paint_area.IntersectWith(local_area);

	OP_DBG((UNI_L("Paint outside: %s Known_invalid (%d,%d,%d,%d) id: %s"),
			local_paint_area.IsEmpty() ? UNI_L("yes") : UNI_L("no"),
			local_area.x, local_area.y, local_area.width, local_area.height,
			cascade && cascade->html_element && cascade->html_element->GetId() ? cascade->html_element->GetId() : UNI_L("N/A") ));

	if (local_paint_area.IsEmpty())
		// We were asked to paint outside the actual SVG. Why call us at all?
		return OpStatus::OK;

	m_current_vd = visual_device;

	// The area within the svg content box that we want to paint
	// NOTE: invalidate receives an invalid rect in screen coords,
	// which is later offset by (negated) rendering viewport coords
	// scaled to screen. this may lead to differences due to rounding
	// in some cases (see CORE-8244) unless the scaling back is done
	// in the same manner. thus, local_paint_area is converted to be
	// relative to the rendering viewport before scaling to screen,
	// and the scaled viewport offset is added afterwards.
	local_paint_area.OffsetBy(-rendering_view.x, -rendering_view.y);

	m_active_paint_area = visual_device->ScaleToScreen(local_paint_area);
	m_active_paint_area.x += visual_device->ScaleToScreen(rendering_view.x);
	m_active_paint_area.y += visual_device->ScaleToScreen(rendering_view.y);

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	const PixelScaler& scaler = visual_device->GetVPScale();
	m_active_paint_area = TO_DEVICE_PIXEL(scaler, m_active_paint_area);
#endif //PIXEL_SCALE_RENDERING_SUPPORT

	OP_STATUS status = PaintOnScreenInternal(visual_device, cascade, m_active_paint_area);

	m_active_paint_area.Empty();

	m_current_vd = NULL;

	return status;
}

/* virtual */ void
SVGImageImpl::OnContentSize(LayoutProperties* cascade, int width, int height)
{
	m_content_width = width;
	m_content_height = height;

	OnSignalChange();
}

/* virtual */ void
SVGImageImpl::OnSignalChange()
{
	SVGDynamicChangeHandler::MarkWholeSVGForRepaint(m_doc_ctx);
}

#ifdef SVG_INTERRUPTABLE_RENDERING
OP_STATUS
SVGImageImpl::SuspendScriptExecution()
{
	// FIXME: Implement
#if 0
	SVGDocumentContext* docctx = GetSVGDocumentContext();
	FramesDocument* frm_doc = docctx->GetDocument();

	ES_ThreadScheduler*	essched = frm_doc->GetESScheduler();
	if(essched)
	{
		essched->SuspendScheduler();
	}
#else
	// Current implementation until there's a way to suspend scripts:
	// 1. Stop ongoing rendering
	// 2. Force invalidation of the entire tree (AKA sledgehammer #1)
	// 3. Set flag to make next render pass non-interruptible
	SVGRenderer* r = m_doc_ctx->GetRenderingState();
	if(r && r->IsActive())
	{
		r->Stop();
		InvalidateAll();
		m_info.timeout_not_allowed = 1;
	}
#endif

	return OpStatus::OK;
}

OP_STATUS
SVGImageImpl::ResumeScriptExecution()
{
	// FIXME: Implement
#if 0
	SVGDocumentContext* docctx = GetSVGDocumentContext();
	FramesDocument* frm_doc = docctx->GetDocument();

	ES_ThreadScheduler*	essched = frm_doc->GetESScheduler();
	if(essched)
	{
		essched->UnsuspendScheduler();
	}
#endif
	return OpStatus::OK;
}
#endif // SVG_INTERRUPTABLE_RENDERING

BOOL
SVGImageImpl::IsAnimationAllowed()
{
	BOOL result = !!g_pcdoc->GetIntegerPref(PrefsCollectionDoc::ShowAnimation);
	if(result)
	{
		FramesDocElm* fdelm = GetInlineFrame(m_logdoc);
		if(fdelm)
		{
			if(HTML_Element* frame_elm = fdelm->GetHtmlElement())
			{
				if(frame_elm->IsMatchingType(HE_OBJECT, NS_HTML))
				{
					int param_count = frame_elm->CountParams();
					BOOL timeline_enabled = TRUE;
					BOOL render_static = FALSE;
					if (param_count)
					{
						int next_idx = 0;
						const uni_char** param_keys = OP_NEWA(const uni_char*, param_count);
						const uni_char** param_values = OP_NEWA(const uni_char*, param_count);

						if (param_keys && param_values)
						{
							frame_elm->GetParams(param_keys, param_values, next_idx);

							if (next_idx < param_count)
								param_count = next_idx;
							
							for(int i = 0; i < param_count; i++)
							{
								if(uni_str_eq(param_keys[i], "render"))
								{
									if(uni_str_eq(param_values[i], "frozen") || uni_str_eq(param_values[i], "static"))
										render_static = TRUE;
								}
								else if(uni_str_eq(param_keys[i], "timeline"))
								{
									if(uni_str_eq(param_values[i], "disable"))
										timeline_enabled = FALSE;
								}
							}
						}

						OP_DELETEA(param_keys);
						OP_DELETEA(param_values);

						result = !render_static;
						if(result)
							result = timeline_enabled;
					}
				}
			}
		}
	}

	return result;
}

void SVGImageImpl::ForceUpdate()
{
	SVG_PROBE_START("SVGImageImpl::ForceUpdate");

	UpdateState(UPDATE_ANIMATIONS | UPDATE_LAYOUT);

	SVG_PROBE_END();
}

void SVGImageImpl::ScheduleInvalidation()
{
	SVGWorkplaceImpl* workplace = static_cast<SVGWorkplaceImpl*>(m_logdoc->GetSVGWorkplace());
	workplace->ScheduleInvalidation(this);
}

#endif // SVG_SUPPORT
