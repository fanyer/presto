/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGWorkplaceImpl.h"
#include "modules/svg/src/SVGImageImpl.h"
#include "modules/svg/src/SVGDependencyGraph.h"
#include "modules/svg/src/SVGDynamicChangeHandler.h"
#include "modules/svg/src/SVGDocumentContext.h"
#include "modules/svg/src/AttrValueStore.h"
#include "modules/svg/src/SVGElementStateContext.h"
#include "modules/svg/src/SVGManagerImpl.h"
#include "modules/svg/src/AttrValueStore.h"
#include "modules/doc/frm_doc.h" // For IsCurrentDoc debug check
#include "modules/dochand/fdelm.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/svg/src/SVGFontImpl.h"
#include "modules/svg/src/SVGManagerImpl.h"
#include "modules/style/css_webfont.h"
#ifdef SVG_THROTTLING
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#endif // SVG_THROTTLING

OP_STATUS SVGWorkplace::Create(SVGWorkplace** newwp, SVG_DOCUMENT_CLASS* document)
{
	if(!newwp || !document)
		return OpStatus::ERR_NULL_POINTER;

	*newwp = OP_NEW(SVGWorkplaceImpl, (document));
	if(!*newwp)
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

void SVGWorkplaceImpl::RegisterVisibleArea(const OpRect& visible_area)
{
	// Walk through all svg:s, check their locations and if they're
	// outside the visible_area, set the visible flag to FALSE.

	SVGImageImpl* image = static_cast<SVGImageImpl*>(m_svg_images.First());
	while (image)
	{
		if (!visible_area.Intersecting(image->GetDocumentRect()))
		{
			image->SetVisibleInView(FALSE);
		}

		// Don't set to TRUE otherwise since if the svg image was
		// hidden then the document pos isn't updated correctly. :It
		// will be set to true when it receives a paint call.
		image = static_cast<SVGImageImpl*>(image->Suc());
	}
}

OP_STATUS SVGWorkplaceImpl::SetAsCurrentDoc(BOOL enable_document)
{
	if (!enable_document)
		StopAllTimers();

	SVGImage* svg_image = GetFirstSVG();
	while (svg_image)
	{
		SVGDocumentContext* doc_ctx = static_cast<SVGImageImpl*>(svg_image)->GetSVGDocumentContext();

		if (SVGAnimationWorkplace *animation_workplace = doc_ctx->GetAnimationWorkplace())
		{
			SVGAnimationWorkplace::AnimationCommand cmd =
				enable_document ?
				SVGAnimationWorkplace::ANIMATION_UNPAUSE :
				SVGAnimationWorkplace::ANIMATION_PAUSE;

			animation_workplace->ProcessAnimationCommand(cmd);
		}

		if (enable_document)
		{
			static_cast<SVGImageImpl*>(svg_image)->ForceUpdate();
		}

		svg_image = GetNextSVG(svg_image);
	}
	return OpStatus::OK;
}

BOOL SVGWorkplaceImpl::HasSelectedText()
{
#ifdef SVG_SUPPORT_TEXTSELECTION
	SVGImage* svg_image = GetFirstSVG();
	while (svg_image)
	{
		if (svg_image->HasSelectedText())
			return TRUE;

		svg_image = GetNextSVG(svg_image);
	}
#endif // SVG_SUPPORT_TEXTSELECTION
	return FALSE;
}

BOOL SVGWorkplaceImpl::GetSelection(SelectionBoundaryPoint &anchor, SelectionBoundaryPoint &focus)
{
#ifdef SVG_SUPPORT_TEXTSELECTION
	SVGImage* svg_image = GetFirstSVG();
	while (svg_image)
	{
		if (svg_image->HasSelectedText())
		{
			SVGTextSelection& ts = static_cast<SVGImageImpl*>(svg_image)->GetSVGDocumentContext()->GetTextSelection();
			anchor = ts.GetLogicalStartPoint();
			focus = ts.GetLogicalEndPoint();
			return TRUE;
		}

		svg_image = GetNextSVG(svg_image);
	}
#endif // SVG_SUPPORT_TEXTSELECTION
	return FALSE;
}

OP_STATUS SVGWorkplaceImpl::GetSVGImageRef(URL url, SVGImageRef** out_ref)
{
	OP_ASSERT(out_ref);

	if (url.IsEmpty() || url.ContentType() != URL_SVG_CONTENT)
		return OpStatus::ERR;

	*out_ref = NULL;

	HTML_Element* element = NEW_HTML_Element();
	if (!element)
		return OpStatus::ERR_NO_MEMORY;

	SVGImageRefImpl* image_reference;

	if (OpStatus::IsMemoryError(SVGImageRefImpl::Make(image_reference, m_doc, g_svg_manager_impl->GetImageRefId(), element)))
	{
		DELETE_HTML_Element(element);
		return OpStatus::ERR_NO_MEMORY;
	}

	// using <iframe> element
	const uni_char* urlstr = url.GetAttribute(URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI).CStr();
	const uni_char* stylestr = UNI_L("display: none;");
	const uni_char* widthstr = UNI_L("0");
	const uni_char* heightstr = UNI_L("0");

	HtmlAttrEntry attrs[5];
	attrs[0].ns_idx = NS_IDX_DEFAULT;
	attrs[0].attr = ATTR_SRC;
	attrs[0].is_specified = FALSE;
	attrs[0].value = urlstr;
	attrs[0].value_len = uni_strlen(urlstr);

	attrs[1].ns_idx = NS_IDX_DEFAULT;
	attrs[1].attr = ATTR_STYLE;
	attrs[1].is_specified = FALSE;
	attrs[1].value = stylestr;
	attrs[1].value_len = uni_strlen(stylestr);

	attrs[2].ns_idx = NS_IDX_DEFAULT;
	attrs[2].attr = ATTR_WIDTH;
	attrs[2].is_specified = FALSE;
	attrs[2].value = widthstr;
	attrs[2].value_len = uni_strlen(widthstr);

	attrs[3].ns_idx = NS_IDX_DEFAULT;
	attrs[3].attr = ATTR_HEIGHT;
	attrs[3].is_specified = FALSE;
	attrs[3].value = heightstr;
	attrs[3].value_len = uni_strlen(heightstr);

	attrs[4].attr = ATTR_NULL;

	if (OpStatus::IsMemoryError(element->Construct(m_doc->GetHLDocProfile(), NS_IDX_HTML, HE_IFRAME, attrs, HE_INSERTED_BY_SVG)))
	{
		OP_DELETE(image_reference);
		DELETE_HTML_Element(element);
		element = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}

	image_reference->Into(&m_svg_imagerefs);
	*out_ref = image_reference;
	return OpStatus::OK;
}

void SVGWorkplaceImpl::Clean()
{
	// We can have images if the svg node lives longer than the LogicalDocument. Make sure they
	// don't try to access the document.
	OP_DELETE(m_depgraph);
	m_depgraph = NULL;

	SVGImageRefImpl* ref = static_cast<SVGImageRefImpl*>(m_svg_imagerefs.First());
	while (ref)
	{
		SVGImageRefImpl* nextref = static_cast<SVGImageRefImpl*>(ref->Suc());
		OP_ASSERT(ref->GetDocument() == m_doc);
		ref->Out();
		ref->DisconnectFromDocument();
		m_doc->GetWindow()->RemoveSVGWindowIcon(ref);
		ref = nextref;
	}

	SVGImageImpl* image = static_cast<SVGImageImpl*>(m_svg_images.First());
	while (image)
	{
		SVGImageImpl* next_image = static_cast<SVGImageImpl*>(image->Suc());
		image->Out();
		image->DisconnectFromDocument();
		image = next_image;
	}

	m_pending_discards.RemoveAll();
	m_pending_mark_extra_dirty.RemoveAll();
	m_proxy_links.Clear();
	m_queued_inlines_changed.Clear();

	m_layout_pass_timer.Stop();

	g_main_message_handler->UnsetCallBacks(this);
}

SVGWorkplaceImpl::~SVGWorkplaceImpl()
{
	Clean();
}

void SVGWorkplaceImpl::HandleInvalidationMessage()
{
	SVGImage* svg_image = GetFirstSVG();
	while (svg_image)
	{
		SVGImageImpl *svg_image_impl = static_cast<SVGImageImpl*>(svg_image);
		if (svg_image_impl->IsInTree())
		{
			svg_image_impl->ForceUpdate();
		}

		svg_image = GetNextSVG(svg_image);
	}
}

void SVGWorkplaceImpl::OnTimeOut(OpTimer* timer)
{
	double curr_time = g_op_time_info->GetRuntimeMS();
	m_lag = curr_time - m_current_timer_fire_time;
	m_listening = FALSE;
#ifdef SVG_THROTTLING
	UpdateTimestamps(curr_time);
#endif // SVG_THROTTLING
	HandleInvalidationMessage();
}

#ifdef SVG_THROTTLING
BOOL SVGWorkplaceImpl::IsThrottlingNeeded(double curr_time)
{
	int switch_throttling_interval = g_pccore->GetIntegerPref(PrefsCollectionCore::SwitchAnimationThrottlingInterval);
	if (op_isnan(m_last_load_check_timestamp) || (curr_time > m_last_load_check_timestamp + switch_throttling_interval))
	{
		int threshold_for_throttling = g_pccore->GetIntegerPref(PrefsCollectionCore::LagThresholdForAnimationThrottling);
		BOOL adapted = AdaptFps(switch_throttling_interval);
		m_throttling_needed = adapted || (g_message_dispatcher->GetAverageLag() >= threshold_for_throttling);
		m_last_load_check_timestamp = curr_time;
	}

	return m_throttling_needed;
}

void SVGWorkplaceImpl::UpdateTimestamps(double curr_time)
{
	m_last_allowed_invalidation_timestamp = curr_time;
	m_next_allowed_invalidation_timestamp = m_last_allowed_invalidation_timestamp + 1000 / m_current_throttling_fps;
}

BOOL SVGWorkplaceImpl::AdaptFps(int adapt_interval)
{
	if (m_throttling_needed)
	{
		//decrease FPS if possible
		if (m_current_throttling_fps > SVG_THROTTLING_FPS_LOW)
		{
			/* count a decrease step. The step is counted dynamically
			 * depending on the load check interval to reach proper FPS in reasonable time
			 * because AdaptFps is only performed at times when the load is checked
			 */
			m_last_adapt_step = (adapt_interval + 1000 - 1)/ 1000;
			m_current_throttling_fps -= m_last_adapt_step;
			m_current_throttling_fps = MAX(m_current_throttling_fps, SVG_THROTTLING_FPS_LOW);
		}
	}
	else
	{
		if (m_last_adapt_step != 0)
		{
			//if last time throttling was needed and FPS was decreased, try to increase it to find an equilibrium
			m_current_throttling_fps += m_last_adapt_step;
			m_last_adapt_step = 0;
			return TRUE;
		}
		else
		{
			//throttling may be turned off
			m_current_throttling_fps = SVG_THROTTLING_FPS_HIGH;
		}
	}

	return FALSE;
}
#endif // SVG_THROTTLING

void SVGWorkplaceImpl::AddPendingDiscard(HTML_Element* discard_target)
{
	RETURN_VOID_IF_ERROR(QueueDelayedAction());

	m_pending_discards.Add(discard_target);
}

class SVGDiscardProcessor : public OpHashTableForEachListener
{
public:
	SVGDiscardProcessor(FramesDocument* doc) : frames_doc(doc) {}

	void HandleKeyData(const void* key, void* data)
	{
		HTML_Element* elm_to_discard = reinterpret_cast<HTML_Element*>(const_cast<void*>(key));
		ProcessDiscard(elm_to_discard);
	}

private:
	void ProcessDiscard(HTML_Element* discard_target);

	FramesDocument* frames_doc;
};

void SVGDiscardProcessor::ProcessDiscard(HTML_Element* discard_target)
{
	if(!discard_target)
		return;

	// The code below has been borrowed from HTML_Element::DOMRemoveFromParent
	// FIXME: Try and separate this from dom/logdoc so that we can get mutation events.
	LogicalDocument *logdoc = frames_doc ? frames_doc->GetLogicalDocument() : NULL;
	HTML_Element *parent = discard_target->Parent();

	// This was added because the document is left in some flux state if the root element is removed.
	if(!parent || parent->Type() == HE_DOC_ROOT)
	{
		return;
	}

	BOOL in_document = FALSE;
	if (logdoc && logdoc->GetRoot())
		in_document = logdoc->GetRoot()->IsAncestorOf(discard_target);

	BOOL had_body = in_document && logdoc->GetBodyElm() != NULL;

#ifdef SVG_SUPPORT
	// SVG size isn't affected by their children and the
	// internal invalidation is done in HandleDocumentTreeChange
	if (!parent->IsMatchingType(Markup::SVGE_SVG, NS_SVG))
#endif // SVG_SUPPORT
		parent->MarkDirty(frames_doc, TRUE, TRUE);

	discard_target->Remove(frames_doc);
	BOOL free = discard_target->Clean(frames_doc);

	if (in_document)
	{
		if (logdoc->GetDocRoot() == discard_target)
		{
			logdoc->SetDocRoot(NULL);
		}

		if (had_body && !logdoc->GetBodyElm())
			if (HTML_Element *body = logdoc->GetRoot()->GetFirstElm(HE_BODY))
				logdoc->SetBodyElm(body);

#if 0 //def DOCUMENT_EDIT_SUPPORT
		if (frames_doc->GetDocumentEdit())
			frames_doc->GetDocumentEdit()->OnElementRemoved(discard_target);
#endif

#if 0 //def XML_EVENTS_SUPPORT
		for (XML_Events_Registration *registration = frames_doc->GetFirstXMLEventsRegistration();
		     registration;
		     registration = (XML_Events_Registration *) registration->Suc())
			RETURN_IF_ERROR(registration->HandleElementRemovedFromDocument(frames_doc, discard_target));
#endif // XML_EVENTS_SUPPORT

		ES_Thread *current_thread = NULL; //environment->GetCurrentScriptThread();
		discard_target->HandleDocumentTreeChange(frames_doc, parent, discard_target, current_thread, FALSE);
	}

	if(free)
	{
		discard_target->Free(frames_doc);
	}
}

void SVGWorkplaceImpl::HandleRemovedSubtree(HTML_Element* subroot)
{
	if(m_pending_discards.GetCount() > 0)
	{
		HTML_Element* stop_it = subroot->NextSiblingActual();
		HTML_Element* it = subroot;
		while (it != stop_it)
		{
			m_pending_discards.Remove(it);
			it = it->NextActual();
		}
	}
	if(m_pending_mark_extra_dirty.GetCount() > 0)
	{
		HTML_Element* stop_it = subroot->NextSiblingActual();
		HTML_Element* it = subroot;
		while (it != stop_it)
		{
			m_pending_mark_extra_dirty.Remove(it);
			it = it->NextActual();
		}
	}
}

void SVGWorkplaceImpl::ScheduleLayoutPass(unsigned int max_delay)
{
	// Make sure we don't do svg layouts of documents in the history.
	// If this triggers, a document currently in the history still has
	// timers/callbacks/messages that makes it consume resources.
	OP_ASSERT(m_doc->IsCurrentDoc());

	double curr_time = g_op_time_info->GetRuntimeMS();

#ifdef SVG_THROTTLING
	if (IsThrottlingNeeded(curr_time) && !IsInvalidationAllowed(curr_time + max_delay))
	{
		OP_ASSERT(curr_time <= m_next_allowed_invalidation_timestamp);
		max_delay = static_cast<unsigned int>(m_next_allowed_invalidation_timestamp - curr_time);
	}
#endif // SVG_THROTTLING

	double new_timer_fire_time = curr_time + max_delay;
	if (IsListening())
	{
		if (new_timer_fire_time >= m_current_timer_fire_time)
		{
			// The current timer is good enough
			return;
		}
		m_layout_pass_timer.Stop();
		SetListening(FALSE);
	}

	// printf("LAYOUT SCHED: %u\n", max_delay);

	m_layout_pass_timer.SetTimerListener(this);
	m_layout_pass_timer.Start(max_delay);
	m_current_timer_fire_time = new_timer_fire_time;
	SetListening(TRUE);
}

void SVGWorkplaceImpl::StopAllTimers()
{
	// Note that the discard timer isn't stopped since the elements it should discard have already been
	// targetted for removal, if the timer was started it was only so that we do the removal at a safe
	// time.
	// Similarly for the mark extra dirty timer, which is used for css background image elements that must
	// be marked as dirty since they lack layoutboxes. It shouldn't harm performance to allow them to run.
	// This method (StopAllTimers) might be renamed to something more descriptive in the future.
	if (IsListening())
	{
		m_layout_pass_timer.Stop();
		SetListening(FALSE);
	}
}

void SVGWorkplaceImpl::InsertIntoSVGList(SVGImage* image)
{
	SVGImageImpl* impl = static_cast<SVGImageImpl*>(image);
	OP_ASSERT(!m_svg_images.HasLink(impl));
	impl->Into(&m_svg_images);
}

void SVGWorkplaceImpl::RemoveFromSVGList(SVGImage* image)
{
	SVGImageImpl* impl = static_cast<SVGImageImpl*>(image);
	OP_ASSERT(m_svg_images.HasLink(impl));
	impl->Out();
}

SVGImage* SVGWorkplaceImpl::GetFirstSVG()
{
	SVGImageImpl* svg_impl = static_cast<SVGImageImpl*>(m_svg_images.First());
	while (svg_impl && svg_impl->IsSubSVG())
	{
		svg_impl = static_cast<SVGImageImpl*>(svg_impl->Suc());
	}

	return svg_impl;
}

SVGImage* SVGWorkplaceImpl::GetNextSVG(SVGImage* prev_svg)
{
	OP_ASSERT(prev_svg);
	SVGImageImpl* svg_impl = static_cast<SVGImageImpl*>(prev_svg);
	do
	{
		svg_impl = static_cast<SVGImageImpl*>(svg_impl->Suc());
	}
	while (svg_impl && svg_impl->IsSubSVG());

	return svg_impl;
}

OP_STATUS
SVGWorkplaceImpl::CreateDependencyGraph()
{
	OP_ASSERT(!m_depgraph);
	OP_ASSERT(m_doc && !m_doc->IsUndisplaying());

	m_depgraph = OP_NEW(SVGDependencyGraph, ());
	if (!m_depgraph)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

OP_STATUS SVGWorkplaceImpl::AddPendingMarkExtraDirty(HTML_Element* elm)
{
	RETURN_IF_ERROR(QueueDelayedAction());

	return m_pending_mark_extra_dirty.Add(elm);
}

class SVGMarkExtraDirtyProcessor : public OpHashTableForEachListener
{
public:
	SVGMarkExtraDirtyProcessor(FramesDocument* doc) : frames_doc(doc) {}

	void HandleKeyData(const void* key, void* data)
	{
		HTML_Element* element = reinterpret_cast<HTML_Element*>(const_cast<void*>(key));
		element->MarkExtraDirty(frames_doc);
	}

private:
	FramesDocument* frames_doc;
};

void SVGWorkplaceImpl::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (par1 != (MH_PARAM_1)this)
		return;

	if (msg != MSG_SVG_PERFORM_DELAYED_ACTION)
		return;

	m_delayed_actions_pending = FALSE;

	if (m_pending_discards.GetCount())
	{
		SVGDiscardProcessor discard_proc(m_doc);
		m_pending_discards.ForEach(&discard_proc);
		m_pending_discards.RemoveAll();
	}

	if (m_pending_mark_extra_dirty.GetCount())
	{
		SVGMarkExtraDirtyProcessor dirty_proc(m_doc);
		m_pending_mark_extra_dirty.ForEach(&dirty_proc);
		m_pending_mark_extra_dirty.RemoveAll();
	}

	while (m_queued_inlines_changed.First())
	{
		URLLink* loaded_url = (URLLink*)m_queued_inlines_changed.First();
		SignalInlineChanged(loaded_url->GetURL());
		loaded_url->Out();
		OP_DELETE(loaded_url);
	}

	if (m_forced_reflow)
	{
		m_forced_reflow = FALSE;

		m_doc->GetDocRoot()->MarkDirty(m_doc);

		if (m_doc->IsTopDocument() ||
			(m_doc->GetDocManager()->GetFrame() &&
			 m_doc->GetDocManager()->GetFrame()->IsSVGResourceDocument() &&
			 m_doc->GetDocManager()->GetFrame()->GetParentFramesDoc()->GetLogicalDocument()))
		{
			OpStatus::Ignore(m_doc->Reflow(FALSE, FALSE, FALSE, FALSE));
		}
	}
}

OP_STATUS SVGWorkplaceImpl::QueueDelayedAction()
{
	if (m_delayed_actions_pending)
		return OpStatus::OK;

	// Assure callback
	if (!g_main_message_handler->HasCallBack(this, MSG_SVG_PERFORM_DELAYED_ACTION, (MH_PARAM_1)this))
		RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_SVG_PERFORM_DELAYED_ACTION, (MH_PARAM_1)this));
	g_main_message_handler->PostMessage(MSG_SVG_PERFORM_DELAYED_ACTION, (MH_PARAM_1)this, 0);

	m_delayed_actions_pending = TRUE;
	return OpStatus::OK;
}

OP_STATUS SVGWorkplaceImpl::QueueReflow()
{
	RETURN_IF_ERROR(QueueDelayedAction());

	m_forced_reflow = TRUE;
	return OpStatus::OK;
}

HTML_Element* SVGWorkplaceImpl::GetProxyElement(URL fullurl)
{
	// Find proxy element
	HTML_Element *proxy_element = NULL;
	ExternalProxyFrameElementItem *iter =
		static_cast<ExternalProxyFrameElementItem *>(m_proxy_links.First());
	while(iter != NULL)
	{
		if (iter->proxy_element_url == fullurl)
		{
			proxy_element = iter->GetElm();
			break;
		}

		iter = static_cast<ExternalProxyFrameElementItem *>(iter->Suc());
	}

	return proxy_element;
}

void SVGWorkplaceImpl::StopLoadingCSSResource(URL url, SVGWorkplace::LoadListener* listener)
{
	// Find proxy element
	ExternalProxyFrameElementItem *iter =
		static_cast<ExternalProxyFrameElementItem *>(m_proxy_links.First());
	while(iter != NULL)
	{
		if (iter->proxy_listener == listener)
		{
			iter->Out();
			OP_DELETE(iter);
			break;
		}

		iter = static_cast<ExternalProxyFrameElementItem *>(iter->Suc());
	}
}

OP_STATUS SVGWorkplaceImpl::CreateProxyElement(URL fullurl, HTML_Element** out_proxy_element, SVGWorkplaceImpl::LoadListener* listener)
{
	OP_STATUS err = OpStatus::ERR;
	if(m_doc->GetLogicalDocument() && m_doc->GetLogicalDocument()->GetDocRoot())
	{
		OpString str;
		if(OpStatus::IsSuccess(fullurl.GetAttribute(URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI, str)))
		{
			HTML_Element* proxy_element = NEW_HTML_Element();
			if (!proxy_element)
				return OpStatus::ERR_NO_MEMORY;

			ExternalProxyFrameElementItem * proxy_link = OP_NEW(ExternalProxyFrameElementItem, ());
			if (!proxy_link)
			{
				DELETE_HTML_Element(proxy_element);
				return OpStatus::ERR_NO_MEMORY;
			}

			proxy_link->SetElm(proxy_element);
			proxy_link->proxy_element_url = fullurl;
			proxy_link->proxy_listener = listener;

			HtmlAttrEntry attrs[2];
			attrs[0].ns_idx = NS_IDX_XLINK;
			attrs[0].attr = Markup::XLINKA_HREF;
			attrs[0].is_specified = FALSE;
			attrs[0].value = str.CStr();
			attrs[0].value_len = str.Length();
			attrs[1].attr = ATTR_NULL;

			err = proxy_element->Construct(m_doc->GetHLDocProfile(),
										   NS_IDX_SVG, Markup::SVGE_FOREIGNOBJECT,
										   attrs, HE_INSERTED_BY_SVG);
			if (OpStatus::IsSuccess(err))
			{
				proxy_element->SetEndTagFound();
				proxy_element->Under(m_doc->GetLogicalDocument()->GetDocRoot());
				proxy_link->Into(&m_proxy_links);
				if(out_proxy_element)
				{
					*out_proxy_element = proxy_element;
				}
			}
			else
			{
				DELETE_HTML_Element(proxy_element);
				proxy_element = NULL;
				OP_DELETE(proxy_link);
			}
		}
	}

	return err;
}

BOOL SVGWorkplaceImpl::IsProxyElement(HTML_Element* elm)
{
	if(elm->GetInserted() != HE_INSERTED_BY_SVG)
		return FALSE;

	ExternalProxyFrameElementItem *iter =
		static_cast<ExternalProxyFrameElementItem *>(m_proxy_links.First());
	while(iter != NULL)
	{
		if(iter->GetElm() == elm)
			return TRUE;

		iter = static_cast<ExternalProxyFrameElementItem *>(iter->Suc());
	}

	return FALSE;
}

void SVGWorkplaceImpl::SignalInlineIgnored(URL fullurl)
{
	ExternalProxyFrameElementItem *iter =
		static_cast<ExternalProxyFrameElementItem *>(m_proxy_links.First());

	while(iter != NULL)
	{
		if (iter->proxy_element_url == fullurl)
		{
			if (iter->proxy_listener && iter->GetElm())
			{
				iter->proxy_listener->OnLoadingFailed(m_doc, iter->proxy_element_url);
			}

			iter->proxy_listener = NULL; // stop listening
			break;
		}

		iter = static_cast<ExternalProxyFrameElementItem *>(iter->Suc());
	}
}

void SVGWorkplaceImpl::SignalInlineChanged(URL fullurl)
{
	ExternalProxyFrameElementItem *iter =
		static_cast<ExternalProxyFrameElementItem *>(m_proxy_links.First());

	if(m_doc->IsReflowing())
	{
		URLLink *loaded_url = OP_NEW(URLLink, (fullurl));
		if(loaded_url)
		{
			loaded_url->Into(&m_queued_inlines_changed);

			if(OpStatus::IsError(QueueDelayedAction()))
			{
				// Try to clean up, since otherwise we'd sit on the resource
				// until Opera exits, or there was another queued action.
				loaded_url->Out();
				OP_DELETE(loaded_url);
			}
		}
		else
			OP_DELETE(loaded_url);

		return;
	}

	while(iter != NULL)
	{
		if (iter->proxy_element_url == fullurl)
		{
			for (SVGImageImpl* img = static_cast<SVGImageImpl*>(GetFirstSVG()); img != NULL; img = static_cast<SVGImageImpl*>(GetNextSVG(img)))
			{
				SVGDynamicChangeHandler::HandleInlineChanged(img->GetSVGDocumentContext(), iter->GetElm(), FALSE);
			}

			if (iter->proxy_listener && iter->GetElm())
			{
				HTML_Element* target = SVGUtils::FindURLReferredNode(
					NULL,
					m_doc,
					iter->GetElm(),
					iter->proxy_element_url.UniRelName());

				iter->proxy_listener->OnLoad(m_doc, iter->proxy_element_url, target);
			}

			iter->proxy_listener = NULL; // stop listening
			break;
		}

		iter = static_cast<ExternalProxyFrameElementItem *>(iter->Suc());
	}
}

OP_STATUS SVGWorkplaceImpl::LoadCSSResource(URL url, SVGWorkplaceImpl::LoadListener* listener)
{
	if(!listener)
		return OpStatus::ERR_NULL_POINTER;

	OP_STATUS status = OpStatus::OK;

	// need to get the url moved to for the below test to work
	URL moved_to = url.GetAttribute(URL::KMovedToURL, TRUE);
	if (!moved_to.IsEmpty())
	{
		url = moved_to;
	}

	if(url == m_doc->GetURL() || url.Type() == URL_OPERA)
	{
		HTML_Element* target = SVGUtils::FindElementById(m_doc->GetLogicalDocument(), url.UniRelName());
		listener->OnLoad(m_doc, url, target);
	}
	else
	{
		// 1. map url -> proxy element
		// 2. fetch font element from proxy element contents
		// 3. check element type
		// 4. set load status in webfont

		// Find proxy element
		HTML_Element *proxy_element = GetProxyElement(url);
		if(proxy_element)
		{
			HTML_Element* target = SVGUtils::FindURLReferredNode(NULL, m_doc, proxy_element, url.UniRelName());
			listener->OnLoad(m_doc, url, target);
		}
		else
		{
			status = CreateProxyElement(url, &proxy_element, listener);
			if(OpStatus::IsSuccess(status))
			{
				SVGUtils::LoadExternalDocument(url, m_doc, proxy_element);
			}
		}
	}

	return status;
}

/* virtual */ void
SVGWorkplaceImpl::InvalidateFonts()
{
	SVGImage* svg_image = GetFirstSVG();
	while (svg_image)
	{
		SVGImageImpl *svg_image_impl = static_cast<SVGImageImpl*>(svg_image);
		if (svg_image_impl->IsInTree())
		{
			SVGDocumentContext *doc_ctx = svg_image_impl->GetSVGDocumentContext();

			// Invalidate use of fonts and font metrics and repaint
			// the root element

			doc_ctx->AddInvalidState(INVALID_FONT_METRICS);
			doc_ctx->SetInvalidationPending(TRUE);

			SVGDynamicChangeHandler::RepaintElement(doc_ctx, doc_ctx->GetElement());
		}

		svg_image = GetNextSVG(svg_image);
	}
}

/* virtual */ OP_STATUS
SVGWorkplaceImpl::GetDefaultStyleProperties(HTML_Element* element, CSS_Properties* css_properties)
{
	StyleModule::DefaultStyle* default_style = &g_opera->style_module.default_style;

	const BOOL is_svg_fragment_root = (element->Type() == Markup::SVGE_SVG &&
									   SVGUtils::GetTopmostSVGRoot(element) == element);

	if (is_svg_fragment_root)
	{
		SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(element);

		if (!doc_ctx)
			/* A document context has for some reason not been created
			   for this fragment root (yet). Do nothing, since we
			   don't know how to load properties. */

			return OpStatus::OK;

		/* For inline SVG, specified width/height attributes are inserted into the style system at
		   author style level with zero-specificity, similarly to how it's done for <img>, <object>,
		   etc.

		   For "standalone" SVG, when the SVG is the root element beneath the document root and is
		   part of the top level document, the implicit width and height are 100% of the initial
		   containing block in the respective dimension. Inline SVGs don't have implicit width or
		   height (which may seem contrary to current specification). */

		const BOOL standalone_or_embedded_by_iframe =
			doc_ctx->IsStandAlone() ||
			doc_ctx->IsEmbeddedByIFrame();
		const BOOL fragment_root_controls_size =
			standalone_or_embedded_by_iframe ||
			element->Parent() != m_doc->GetDocRoot();

		const BOOL set_width = css_properties->GetDecl(CSS_PROPERTY_width) == NULL;
		const BOOL set_height = css_properties->GetDecl(CSS_PROPERTY_height) == NULL;

		CSS_number_decl* width_decl = default_style->content_width;
		CSS_number_decl* height_decl = default_style->content_height;

		if (fragment_root_controls_size)
		{
			SVGLengthObject* specified;

			if (set_width)
			{
				if (OpStatus::IsSuccess(AttrValueStore::GetLength(element, Markup::SVGA_WIDTH, &specified, NULL)) &&
					specified != NULL)
				{
					CSSValueType unit = CSSValueType(specified->GetUnit());

					if (unit == CSS_NUMBER)
						unit = CSS_PX;

					width_decl->SetNumberValue(0, specified->GetScalar().GetFloatValue(), unit);
					css_properties->SetDecl(CSS_PROPERTY_width, width_decl);
				}
				else if (standalone_or_embedded_by_iframe)
				{
					width_decl->SetNumberValue(0, 100, CSS_PERCENTAGE);
					css_properties->SetDecl(CSS_PROPERTY_width, width_decl);
				}
			}

			if (set_height)
			{
				if (OpStatus::IsSuccess(AttrValueStore::GetLength(element, Markup::SVGA_HEIGHT, &specified, NULL)) &&
					specified != NULL)
				{
					CSSValueType unit = CSSValueType(specified->GetUnit());

					if (unit == CSS_NUMBER)
						unit = CSS_PX;

					height_decl->SetNumberValue(0, specified->GetScalar().GetFloatValue(), unit);
					css_properties->SetDecl(CSS_PROPERTY_height, height_decl);
				}
				else if (standalone_or_embedded_by_iframe)
				{
					height_decl->SetNumberValue(0, 100, CSS_PERCENTAGE);
					css_properties->SetDecl(CSS_PROPERTY_height, height_decl);
				}
			}
		}
		else
		{
			/* We don't control the size, the "placeholder" does, which may be an <img> or
			   <object>. */

			if (set_width)
			{
				width_decl->SetNumberValue(0, 100, CSS_PERCENTAGE);
				css_properties->SetDecl(CSS_PROPERTY_width, width_decl);
			}

			if (set_height)
			{
				height_decl->SetNumberValue(0, 100, CSS_PERCENTAGE);
				css_properties->SetDecl(CSS_PROPERTY_height, height_decl);
			}
		}
	}
	else if (element->Type() == Markup::SVGE_FOREIGNOBJECT)
	{
		const BOOL set_width = css_properties->GetDecl(CSS_PROPERTY_width) == NULL;
		const BOOL set_height = css_properties->GetDecl(CSS_PROPERTY_height) == NULL;

		CSS_number_decl* width_decl = default_style->content_width;
		CSS_number_decl* height_decl = default_style->content_height;

		SVGLengthObject* specified;

		if (set_width)
		{
			if (OpStatus::IsSuccess(AttrValueStore::GetLength(element, Markup::SVGA_WIDTH, &specified, NULL)) &&
				specified != NULL)
			{
				CSSValueType unit = CSSValueType(specified->GetUnit());

				if (unit == CSS_NUMBER)
					unit = CSS_PX;

				width_decl->SetNumberValue(0, specified->GetScalar().GetFloatValue(), unit);
				css_properties->SetDecl(CSS_PROPERTY_width, width_decl);
			}
		}

		if (set_height)
		{
			if (OpStatus::IsSuccess(AttrValueStore::GetLength(element, Markup::SVGA_HEIGHT, &specified, NULL)) &&
				specified != NULL)
			{
				CSSValueType unit = CSSValueType(specified->GetUnit());

				if (unit == CSS_NUMBER)
					unit = CSS_PX;

				height_decl->SetNumberValue(0, specified->GetScalar().GetFloatValue(), unit);
				css_properties->SetDecl(CSS_PROPERTY_height, height_decl);
			}
		}
	}

	return OpStatus::OK;
}

void SVGWorkplaceImpl::ScheduleInvalidation(SVGImageImpl *img_impl)
{
#ifdef SVG_THROTTLING
	double curr_time = g_op_time_info->GetRuntimeMS();
	if (IsThrottlingNeeded(curr_time) && !IsInvalidationAllowed(curr_time))
	{
		ScheduleLayoutPass(0); //make sure timer will be set up
		return;
	}

	UpdateTimestamps(curr_time);
#endif // SVG_THROTTLING

	img_impl->ForceUpdate();
}

#endif // SVG_SUPPORT
