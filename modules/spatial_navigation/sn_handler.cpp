/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/dochand/fdelm.h"
#include "modules/dochand/viewportcontroller.h"

#include "modules/doc/html_doc.h"
#include "modules/forms/piforms.h"
#include "modules/doc/frm_doc.h"
#include "modules/encodings/utility/charsetnames.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/layout/layout_workplace.h"
#include "modules/layout/box/box.h"
#include "modules/layout/content/content.h"
#include "modules/layout/traverse/traverse.h"

#include "modules/windowcommander/src/WindowCommander.h"

#include "modules/spatial_navigation/sn_handler.h"
#include "modules/spatial_navigation/sn_traversalobject.h"
#include "modules/spatial_navigation/sn_util.h"

#ifdef SVG_SUPPORT
#include "modules/svg/SVGManager.h"
#endif // SVG_SUPPORT

#include "modules/media/media.h"

#include "modules/prefs/prefsmanager/collections/pc_doc.h"

#ifdef _SPAT_NAV_SUPPORT_


OP_STATUS FrameElement::Init(FramesDocument* doc, FramesDocElm* docElm, RECT rect, HTML_Element* lastScrollable)
{
	frmDoc = doc;
	frmDocElm = docElm;
	iframeRect = rect;
	scrollable = lastScrollable;
	return OpStatus::OK;
}

OP_STATUS FramePath::AddFrameToPath(FramesDocument* doc, FramesDocElm* docElm, RECT rect, HTML_Element* lastScrollable, BOOL insert_last)
{
	FrameElement* newFrame = OP_NEW(FrameElement, ());
	if (newFrame == NULL)
		return OpStatus::ERR_NO_MEMORY;

	newFrame->Init(doc, docElm, rect, lastScrollable);
	if (insert_last)
		newFrame->Into(this);
	else
		newFrame->IntoStart(this);

	return OpStatus::OK;
}

void FramePath::RemoveFrameFromPath()
{
	FrameElement* frame = First();
	frame->Out();

	if (m_store_point == frame)
	{
		// store the frame we left so that we can restore it later
		m_store_point = First();
		frame->Into(&m_stored_path);
	}
	else
		OP_DELETE(frame);
}

void FramePath::RestorePreviousPath()
{
	if (!m_store_point)  // nothing to restore
		return;

	if (m_store_point == First() && m_stored_path.Empty())  // we're already in same frame
		return;

	OP_ASSERT(m_store_point->Pred() == NULL);  // with current usage this shouldn't happen, although it's not really an error
	// detach and delete all elements beyond store_point
	Link* elm = m_store_point->Pred();
	while (elm)
	{
		Link* next = elm->Pred();

		elm->Out();
		OP_DELETE(elm);

		elm = next;
	}

	OP_ASSERT(m_store_point == First());

	// re-insert stored path onto stored_point
	while ((elm = m_stored_path.Last()))
	{
		elm->Out();
		elm->IntoStart(this);
	}

	OP_ASSERT(m_stored_path.Empty());

	// reset store_point
	m_store_point = NULL;
}

void FramePath::DiscardPreviousPath()
{
	// unlink and delete elements in stored_path
	m_stored_path.Clear();

	// reset store_point
	m_store_point = NULL;
}

SnHandler::~SnHandler()
{
	OP_DELETE(default_nav_filter);
	OP_DELETE(heading_nav_filter);
	OP_DELETE(paragraph_nav_filter);
	OP_DELETE(nonlink_nav_filter);
	OP_DELETE(form_nav_filter);
	OP_DELETE(atag_nav_filter);
	OP_DELETE(image_nav_filter);
	OP_DELETE(configurable_nav_filter);
	if (currentFramePath)
	{
		OP_DELETE(currentFramePath);
		currentFramePath = NULL;
	}
}

void SnHandler::ElementDestroyed(HTML_Element* element)
{
	if (activeScrollable == element)
		activeScrollable = NULL;
	if (lastElement == element)
		lastElement = NULL;
}

void SnHandler::ElementNavigated(HTML_Document* doc, HTML_Element* element)
{
	if (lastElement && lastElement != element)
	{
#ifdef NO_MOUSEOVER_IN_HANDHELD_MODE
		if (!doc->GetWindow()->GetHandheld())
#endif // NO_MOUSEOVER_IN_HANDHELD_MODE
		{
			HTML_Element* dummy = NULL;
			FakeMouseOutOnPreviousElement(doc, lastElement, element, dummy); // Will also check whether lastElement == currently_hovered_element.
		}
		lastElement = NULL; // We can assume the lastElement is invalid now, since the highlight or focus changed.
	}
}

OP_STATUS SnHandler::Init(Window* win)
{
	window = win;

	if (default_nav_filter == NULL)
	{
#ifdef SPATNAV_PICKER_MODE
		default_nav_filter = OP_NEW(SnPickerModeFilter, ());
#else
		default_nav_filter = OP_NEW(SnDefaultNavFilter, ());
#endif
		nav_filter = default_nav_filter;
	}
	if (default_nav_filter == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	if (currentFramePath == NULL)
	{
		currentFramePath = OP_NEW(FramePath, ());
	}
	if (currentFramePath == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

#ifdef SN_HIGHLIGHT_ON_LOAD
	highlightOnLoad = TRUE;
#else
	highlightOnLoad = FALSE;
#endif // SN_HIGHLIGHT_ON_LOAD

	SetScrollX(50);
	SetScrollY(50);
	allowDocScrolling = FALSE;

	return OpStatus::OK;
}

void SnHandler::SetActiveFrame(FramesDocument* frameDoc)
{
	if (frameDoc == activeFrameDoc)
		return;

	activeFrameDoc = frameDoc;

	if (activeFrameDoc)
		activeHTML_Doc = activeFrameDoc->GetHtmlDocument();
	else
		activeHTML_Doc = NULL;

	// Activate frame in document as well
	if (activeFrameDoc)
	{
		currentDoc = window->DocManager()->GetCurrentVisibleDoc();
		FramesDocElm* frame = currentDoc->GetFrmDocElm(frameDoc->GetSubWinId());

		if (frame)
			currentDoc->SetActiveFrame(frame, TRUE, FOCUS_REASON_KEYBOARD);
		else
			currentDoc->SetNoActiveFrame(FOCUS_REASON_KEYBOARD);  // will activate top frame
	}

	OP_ASSERT(activeFrameDoc == window->GetActiveFrameDoc());
}

SnElementType SnHandler::GetActiveLinkType()
{
	HTML_Element* activeLink = GetActiveLink();
	if (!activeFrameDoc || !activeLink)
		return ELEMENT_TYPE_NONE;

	SnElementType type = ELEMENT_TYPE_NONE;

	if (ElementTypeCheck::IsLink(activeLink, (FramesDocument*)activeFrameDoc))
		type = (SnElementType) (type | ELEMENT_TYPE_LINK);

	if (ElementTypeCheck::IsTabbable(activeLink))
		type = (SnElementType) (type | ELEMENT_TYPE_TABBABLE);

	if (ElementTypeCheck::IsFormObject(activeLink, (FramesDocument*)activeFrameDoc))
	{
		type = (SnElementType) (type | ELEMENT_TYPE_FORM);
		if (activeLink->IsMatchingType(HE_INPUT, NS_HTML) ||
			activeLink->IsMatchingType(HE_BUTTON, NS_HTML))
		{
			switch (activeLink->GetInputType())
			{
			case INPUT_TEXT:
				type = (SnElementType) (type | ELEMENT_TYPE_TEXT);
				break;
			case INPUT_CHECKBOX:
				type = (SnElementType) (type | ELEMENT_TYPE_CHECKBOX);
				break;
			case INPUT_RADIO:
				type = (SnElementType) (type | ELEMENT_TYPE_RADIO);
				break;
			case INPUT_SUBMIT:
				type = (SnElementType) (type | ELEMENT_TYPE_SUBMIT);
				break;
			case INPUT_RESET:
				type = (SnElementType) (type | ELEMENT_TYPE_RESET);
				break;
			case INPUT_IMAGE:
				type = (SnElementType) (type | ELEMENT_TYPE_IMAGE);
				break;
			case INPUT_PASSWORD:
				type = (SnElementType) (type | ELEMENT_TYPE_PASSWORD);
				break;
			case INPUT_BUTTON:
				type = (SnElementType) (type | ELEMENT_TYPE_BUTTON);
				break;
			case INPUT_FILE:
				type = (SnElementType) (type | ELEMENT_TYPE_FILE);
				break;
			case INPUT_TEXTAREA:
				type = (SnElementType) (type | ELEMENT_TYPE_TEXTAREA);
				break;
#ifdef WEBFORMS2_SUPPORT
			case INPUT_URI:
				type = (SnElementType) (type | ELEMENT_TYPE_URI | ELEMENT_TYPE_TEXT);
				break;
			case INPUT_SEARCH:
				type = (SnElementType) (type | ELEMENT_TYPE_SEARCH | ELEMENT_TYPE_TEXT);
				break;
			case INPUT_TEL:
				type = (SnElementType) (type | ELEMENT_TYPE_TEL | ELEMENT_TYPE_TEXT);
				break;
			case INPUT_DATE:
			case INPUT_WEEK:
			case INPUT_MONTH:
				type = (SnElementType) (type | ELEMENT_TYPE_DATE);
				break;
			case INPUT_TIME:
				type = (SnElementType) (type | ELEMENT_TYPE_TIME);
				break;
			case INPUT_DATETIME:
			case INPUT_DATETIME_LOCAL:
				type = (SnElementType) (type | ELEMENT_TYPE_DATE | ELEMENT_TYPE_TIME);
				break;
			case INPUT_EMAIL:
				type = (SnElementType) (type | ELEMENT_TYPE_EMAIL | ELEMENT_TYPE_TEXT);
				break;
			case INPUT_NUMBER:
				type = (SnElementType) (type | ELEMENT_TYPE_NUMBER);
				break;
			case INPUT_RANGE:
				type = (SnElementType) (type | ELEMENT_TYPE_RANGE);
				break;
			case INPUT_COLOR:
				type = (SnElementType) (type | ELEMENT_TYPE_COLOR);
				break;
#endif  // WEBFORMS2_SUPPORT
			default:
				break;
			}
		}

		if (activeLink->IsMatchingType(HE_SELECT, NS_HTML))
		{
			type = (SnElementType) (type | ELEMENT_TYPE_SELECT);
		}
		if (activeLink->IsMatchingType(HE_TEXTAREA, NS_HTML))
		{
			type = (SnElementType) (type | ELEMENT_TYPE_TEXTAREA);
		}
	}

	if (SnUtil::HasEventHandler((FramesDocument*)activeFrameDoc, activeLink))
		type = (SnElementType) (type | ELEMENT_TYPE_JS);

	if (ElementTypeCheck::IsImage(activeLink, activeFrameDoc))
		type = (SnElementType) (type | ELEMENT_TYPE_IMAGE);

	return type;
}

HTML_Element* SnHandler::GetActiveLink()
{
	HTML_Element* curr = NULL;

   	UpdateNavigationData();

	if (activeHTML_Doc)
	{
		curr = activeHTML_Doc->GetCurrentAreaElement();
		if (!curr)
			curr = activeHTML_Doc->GetNavigationElement();

		if (!curr)
			curr = activeHTML_Doc->GetFocusedElement();
	}

	return curr;
}

void SnHandler::SetActiveLink(HTML_Element* activeLink)
{
	OP_ASSERT(activeLink == NULL || activeHTML_Doc);

	if (activeHTML_Doc)
		activeHTML_Doc->SetNavigationElement(activeLink, TRUE);
}

void SnHandler::ScrollToIFrame(SnFrameIterator* frames, HTML_Document* doc, BOOL refocus)
{
	if (doc)
	{
		FramesDocument* frm_doc = doc->GetFramesDocument();
		VisualDevice* vis_dev = doc->GetVisualDevice();
		INT32 x = frames->GetX() + vis_dev->GetRenderingViewX();
		INT32 y = frames->GetY() + vis_dev->GetRenderingViewY();

		if (refocus)
		{
			// Scroll if iframe is not in view
			frm_doc->RequestScrollIntoView(OpRect(x, y, frames->GetWidth(), frames->GetHeight()),
										   SCROLL_ALIGN_TOP,
										   FALSE,
										   VIEWPORT_CHANGE_REASON_SPATIAL_NAVIGATION);
		}
		else
		{
			// Scroll if iframe doesn't fit inside the current view port
			OpRect visual_viewport = frm_doc->GetVisualViewport();
			if (!visual_viewport.Contains(OpRect(x, y, frames->GetWidth(), frames->GetHeight())))
			{
				frm_doc->RequestScrollIntoView(OpRect(x, y, frames->GetWidth(), frames->GetHeight()),
											   SCROLL_ALIGN_TOP,
											   FALSE,
											   VIEWPORT_CHANGE_REASON_SPATIAL_NAVIGATION);
				scrolledOnce = TRUE;
			}
		}
	}
}

void SnHandler::DocDestructed(HTML_Document *doc)
{
	// This method is probably not needed anymore as we reset all documents
	// before each navigation anyway.
	if (doc == activeHTML_Doc)
		activeHTML_Doc = NULL;
}
void SnHandler::DocDestructed(FramesDocument *doc)
{
	// This method is probably not needed anymore as we reset all documents
	// before each navigation anyway.
	if (doc == currentDoc)
		currentDoc = NULL;
	if (doc == activeFrameDoc)
		activeFrameDoc = NULL;
}

void SnHandler::FakeMouseOutOnPreviousElement(HTML_Document* doc, HTML_Element* last_elm, HTML_Element* navigated_elm, HTML_Element*& common_ancestor)
{
	OP_ASSERT(doc);

	HTML_Element* mouseout_elm = doc->GetHoverHTMLElement();
	if (mouseout_elm &&
		(!last_elm || last_elm->IsAncestorOf(mouseout_elm)))
	{
		int x = 0;
		long y = 0;
		FramesDocument* frm_doc = doc->GetFramesDocument();

		if (!last_elm)
		{
			x = allData.linkRect.left;
			y = allData.linkRect.top;
		}
		else if (navigated_elm)
		{
			RECT rect;
			Box* navigated_box = navigated_elm->GetLayoutBox();
			if (navigated_box)
			{
				navigated_box->GetBBox(frm_doc, BORDER_BOX, rect);
				x = rect.left;
				y = rect.top;
			}
		}

		RAISE_AND_RETURN_VOID_IF_ERROR(frm_doc->HandleMouseEvent(ONMOUSEOUT, navigated_elm, mouseout_elm, NULL, 0, 0, x, y, SHIFTKEY_NONE, 0));
		/* Mouseleave fires when the mouse leaves element and all its descendants.
		   Fire after mouse out. */
		common_ancestor = HTML_Element::GetCommonAncestorActual(navigated_elm, mouseout_elm);
		HTML_Element *target = mouseout_elm;
		while (target && target != common_ancestor)
		{
			RAISE_AND_RETURN_VOID_IF_ERROR(frm_doc->HandleMouseEvent(ONMOUSELEAVE, navigated_elm, target, NULL, 0, 0, x, y, SHIFTKEY_NONE, 0));
			target = target->ParentActual();
		}

		doc->SetHoverHTMLElement(NULL, TRUE);
	}
}

void SnHandler::ShowMouseOver(HTML_Document* doc, HTML_Element* helm)
{
	FramesDocument* frames_doc = doc ? doc->GetFramesDocument() : NULL;

	if (frames_doc)
	{
		HTML_Element* real_target = GetBetterTarget(helm);

		// Send mouseout to whatever element has hover (which is last element to get mousemove/mouseover)
		HTML_Element *common_ancestor = NULL;
		if (doc)
			FakeMouseOutOnPreviousElement(doc, NULL, real_target, common_ancestor);

		if (real_target)
		{
			int x = 0;
			long y = 0;

			if (helm)
			{
				x = allData.linkRect.left;
				y = allData.linkRect.top;
			}

			if (!common_ancestor)
				common_ancestor = HTML_Element::GetCommonAncestorActual(doc->GetHoverHTMLElement(), real_target);
			// Mouseenter fires when mouse enters an element or any of its descendants
			// for the first time, meaning the related target is not itself nor a descendant.
			// Fire before mouse over, and need to fire from parent to child.
			HE_AncestorToDescendantIterator iterator;
			RAISE_AND_RETURN_VOID_IF_ERROR(iterator.Init(common_ancestor, real_target, TRUE));
			for (HTML_Element *target; (target = iterator.GetNextActual()) != NULL;)
				RAISE_AND_RETURN_VOID_IF_ERROR(frames_doc->HandleMouseEvent(ONMOUSEENTER, doc->GetHoverHTMLElement(), target, NULL, 0, 0, x, y, SHIFTKEY_NONE, 0));

			RAISE_AND_RETURN_VOID_IF_ERROR(frames_doc->HandleMouseEvent(ONMOUSEOVER, GetActiveLink(), real_target, NULL, 0, 0, x, y, SHIFTKEY_NONE, 0));
			// Setting hover element makes sure it gets the mouseout event
			doc->SetHoverHTMLElement(real_target, TRUE);

			// mouse move should be last, see CORE-2194
			RAISE_AND_RETURN_VOID_IF_ERROR(frames_doc->HandleMouseEvent(ONMOUSEMOVE, GetActiveLink(), real_target, NULL, 0, 0, x, y, SHIFTKEY_NONE, 0));
		}
	}
}

void SnHandler::HighlightLink(HTML_Document* doc, HTML_Element* helm, BOOL showMouseOver)
{
	BOOL scroll = allowDocScrolling && !scrolledOnce;
	if (helm->Type() == HE_AREA)
		scroll = FALSE;
	Doc_HighlightElement(doc, helm, showMouseOver, TRUE, scroll);
}

void SnHandler::HighlightFormObject(HTML_Document* doc, HTML_Element* helm)
{
	doc->GetVisualDevice()->SetDrawFocusRect(TRUE);
#ifdef SELECT_TO_FOCUS_INPUT
	FocusElement(doc, helm);
#else
	doc->FocusElement(helm, HTML_Document::FOCUS_ORIGIN_SPATIAL_NAVIGATION, TRUE, allowDocScrolling);
#endif // SELECT_TO_FOCUS_INPUT
}

void SnHandler::Highlight(HTML_Document* doc, HTML_Element* helm, BOOL showMouseOver, BOOL focusDocument /* = TRUE */)
{
	if (doc && helm)
	{
		if (helm != doc->GetFocusedElement())
		{
			OP_ASSERT(focusDocument || activeFrameDoc == window->GetActiveFrameDoc());

			FramesDocument* document = doc->GetFramesDocument();

			if (focusDocument)
				document->SetFocus();

#ifdef NO_MOUSEOVER_IN_HANDHELD_MODE
			if (window->GetHandheld())
				showMouseOver = FALSE;
#endif

			// DIVs may have OnClick or Mouse Over handlers
			if (helm->Type() == HE_DIV)
			{
				HTML_Element* handling_elm;
				if (helm->HasEventHandler(document, ONCLICK, NULL, &handling_elm) ||
				    helm->HasEventHandler(document, ONMOUSEOVER, NULL, &handling_elm) ||
				    helm->HasEventHandler(document, ONMOUSEENTER, NULL, &handling_elm))
				{
					HighlightLink(doc, helm, showMouseOver);
					if (showMouseOver)
						ShowMouseOver(doc, helm);
					return;
				}
			}

			if (helm->GetFormObject() || helm->GetInputType() == INPUT_IMAGE ||
				FormManager::IsButton(helm) || (helm->Type() == HE_DIV))
			{
				HighlightFormObject(doc, helm);
  				if (helm->Type() == HE_DIV && allowDocScrolling)
					doc->ScrollToElement(helm, SCROLL_ALIGN_CENTER, FALSE, VIEWPORT_CHANGE_REASON_SPATIAL_NAVIGATION);
			}
  			else
  			{
				HighlightLink(doc, helm, showMouseOver);
  			}

			if (showMouseOver)
				ShowMouseOver(doc, helm);
		}
		else
		{
			// SnHandler may have been dirty, and we need to 'touch' the
			// current selection (in order to be able to unfocus it later)
			HighlightFormObject(doc, helm);
		}
	}
}

OP_STATUS SnHandler::HighlightCurrentElement(BOOL scroll, BOOL isNavigation)
{
	HTML_Element* activeLink = GetActiveLink();
	if (activeLink)
	{
		Highlight(activeHTML_Doc, activeLink, FALSE, TRUE);
		if (isNavigation)
			OnLinkNavigated();
	}

	return OpStatus::OK;
}

void SnHandler::OnLinkNavigated()
{
	HTML_Element* active_link = GetActiveLink();
	if (!active_link)
		return;

	const uni_char* title = active_link->GetStringAttr(ATTR_TITLE);
	URL url = active_link->GetAnchor_URL(activeFrameDoc);
	OpString link_str;

	const char* url_charset = activeFrameDoc->GetHLDocProfile()->GetCharacterSet();
	unsigned short charset_id = 0;
	if (!url_charset || OpStatus::IsSuccess(g_charsetManager->GetCharsetID(url_charset, &charset_id)))
		url.GetAttribute(URL::KUniName_With_Fragment_Username_Password_Hidden, charset_id, link_str);

	HTML_Element *child = active_link->FirstChild(), *image = NULL;

	while (child && active_link->IsAncestorOf(child))
	{
		Box* box = child->GetLayoutBox();
		if (box)
		{
			if (!image && box->IsContentReplaced() && (box->IsContentImage() || box->IsContentEmbed()))
				image = child;
			else if (child->Type() == HE_TEXT && !child->HasWhiteSpaceOnly())
			{
				image = NULL;
				break;
			}
		}

		child = (HTML_Element *) child->Next();
	}

	// Send the link's first rectangle to platform so it knows where the link is.
	if (!SN_IS_RECT_EMPTY(allData.linkRect))
	{
		OpRect linkRect(&allData.linkRect);
		linkRect = window->GetViewportController()->ConvertToToplevelRect(activeFrameDoc, linkRect);
		window->GetWindowCommander()->GetDocumentListener()->OnLinkNavigated(window->GetWindowCommander(), link_str.CStr(), title, image != NULL, &linkRect);
	}
	else
		window->GetWindowCommander()->GetDocumentListener()->OnLinkNavigated(window->GetWindowCommander(), link_str.CStr(), title, image != NULL);
}

BOOL SnHandler::Scroll(INT32 direction, FrameElement* frame, INT32 limit /*=-1*/)
{
	if (scrolledOnce)
		return TRUE;

	BOOL ret = FALSE;

	if (!frame)
		frame = currentFramePath->First();

	if (SnUtil::HasSpecialFrames(currentDoc))
	{
		if (frame->IsIFrame())
			ret = ScrollDocumentInternal(direction, frame->GetFramesDocument(), limit);
		else
			// with smart frames we have to scroll the top document as sub documents have been resized
			ret = ScrollDocumentInternal(direction, currentDoc, limit);
	}
	else // normal mode
	{
		FramesDocument* currentFrameDoc = frame->GetFramesDocument();
		if (currentFrameDoc && currentFrameDoc->GetHtmlDocument())
			ret = ScrollDocumentInternal(direction, currentFrameDoc, limit);
	}

	if (ret)
		scrolledOnce = TRUE;

	return ret;
}

HTML_Element* SnHandler::GetBetterTarget(HTML_Element* elm)
{
	if (elm)
	{
		// This is a simple hack to send the JS-event to the element that most likely would
		// have gotten the real event.  It could be more clever.
		while (elm->FirstChild() && elm->FirstChild()->Type() != HE_TEXT)
			elm = elm->FirstChild();
	}

	return elm;
}

BOOL SnHandler::ScrollDocumentInternal(INT32 direction, FramesDocument* doc, INT32 limit /*=-1*/)
{
	if (doc)
	{
		INT32 amount;
		INT32 max_amount; //based on top document's visual viewport

		switch (direction)
		{
		case DIR_RIGHT:
		case DIR_LEFT:
			max_amount = scrollX * currentDoc->GetVisualViewWidth() / 100;
			amount = scrollX * doc->GetVisualViewWidth() / 100;
			break;
		case DIR_UP:
		case DIR_DOWN:
			max_amount = scrollY * currentDoc->GetVisualViewHeight() / 100;
			amount = scrollY * doc->GetVisualViewHeight() / 100;
			break;
		default:	// Error
			return FALSE;
		}

		if (max_amount < amount)
			amount = max_amount;

		if (limit >= 0 && limit < amount)
			amount = limit;

		return ScrollDocumentInternalAmount(direction, doc, amount);
	}

	return FALSE;
}

BOOL SnHandler::ScrollDocumentInternalAmount(INT32 direction, FramesDocument* doc, INT32 amount)
{
	if (doc)
	{
		INT32 viewX = doc->GetVisualViewX();
		INT32 viewY = doc->GetVisualViewY();

		switch (direction)
		{
			case DIR_RIGHT:     // Right
				return doc->RequestSetVisualViewPos(viewX+amount, viewY, VIEWPORT_CHANGE_REASON_SPATIAL_NAVIGATION);

			case DIR_UP:    // Up
				return doc->RequestSetVisualViewPos(viewX, viewY-amount, VIEWPORT_CHANGE_REASON_SPATIAL_NAVIGATION);

			case DIR_LEFT:  // Left
				return doc->RequestSetVisualViewPos(viewX-amount, viewY, VIEWPORT_CHANGE_REASON_SPATIAL_NAVIGATION);

			case DIR_DOWN:  // Down
				return doc->RequestSetVisualViewPos(viewX, viewY+amount, VIEWPORT_CHANGE_REASON_SPATIAL_NAVIGATION);

			default:    // Error
				return FALSE;
		}
	}
	return FALSE;
}

BOOL SnHandler::PointerScrollAmountInternal(FramesDocument* doc, INT32 direction, INT32 amount)
{
	if (doc == NULL)
		return FALSE;

	BOOL ret = FALSE;
	// if active frame should be scrolled, scroll it
	ret = ScrollDocumentInternalAmount(direction, doc, amount);

	// if it couldn't scroll, or if it wasn't in view, scroll parent
	if (!ret)
		ret = PointerScrollAmountInternal(doc->GetParentDoc(), direction, amount);

	return ret;
}

BOOL SnHandler::ScrollAmountCurrentFrame(INT32 direction, INT32 amount)
{
	if (!activeFrameDoc)
		return FALSE;

	return PointerScrollAmountInternal(activeFrameDoc, direction, amount);
}

BOOL SnHandler::ScrollAmount(INT32 direction, INT32 amount)
{
	if (!currentDoc)
		return FALSE;

	FramesDocument* docToUse = activeHTML_Doc ? activeHTML_Doc->GetFramesDocument() : currentDoc;

	if (currentDoc->IsFrameDoc() &&
		(currentDoc->GetLayoutMode() != LAYOUT_NORMAL))
			docToUse = currentDoc;

	BOOL ret = FALSE;
	// if the current active frame is an iframe, try to scroll this one
	// if it fails, go ahead and scroll the document...
	FramesDocElm* felm = activeFrameDoc ? activeFrameDoc->GetDocManager()->GetFrame() : NULL;
	if (felm && felm->IsInlineFrame())
		ret = ScrollDocumentInternalAmount(direction, activeFrameDoc, amount);

	if (docToUse && SnUtil::HasSpecialFrames(currentDoc))
		docToUse = SnUtil::GetDocToScroll(currentDoc);

	if (!ret)
		ret = ScrollDocumentInternalAmount(direction, docToUse, amount);

	return ret;
}

void SnHandler::UpdateNavigationData()
{
	FramesDocument* new_currentDoc = window->DocManager()->GetCurrentVisibleDoc();

	// If we've changed document since last navigation
	if (currentDoc != new_currentDoc ||
		 activeFrameDoc != window->GetActiveFrameDoc() ||
		 activeHTML_Doc != (activeFrameDoc ? activeFrameDoc->GetHtmlDocument() : NULL))
	{
		currentDoc = new_currentDoc;
		activeFrameDoc = window->GetActiveFrameDoc();
		activeHTML_Doc = activeFrameDoc ? activeFrameDoc->GetHtmlDocument() : NULL;
		activeScrollable = NULL;

		if (!currentDoc)
			return;

		SN_EMPTY_RECT(lastRect);

		currentFramePath->Clear();
		SetInitialFrame();
		exclusions.Clear();
	}
	else
	{
		FrameElement* frameElement = currentFramePath->First();

		if (frameElement && frameElement->IsIFrame())  // check if iframe is still visible
		{
			FramesDocElm* frame = frameElement->GetFrmDocElm();
			Box* iframeBox = frame->GetHtmlElement()->GetLayoutBox();

			if (iframeBox)
			{
				RECT rect;
				if (iframeBox->GetRect(frame->GetParentFramesDoc(), CONTENT_BOX, rect))
					frameElement->SetIframeRect(rect);
			}
			else //the iframe element may have been undisplayed, exit iframe
				ExitIFrame();
		}
	}
}

BOOL SnHandler::InitNavigation(INT32 direction, BOOL scroll, INT32 scrollValue)
{
	FramesDocument* current_doc = window->DocManager()->GetCurrentVisibleDoc();

	if (!current_doc)
		return FALSE;

#ifdef _PRINT_SUPPORT_
    // Don't do stuff in print preview. It's a static display of
    // the document, not something to put highlight in.
    if (current_doc->IsPrintDocument())
    	return FALSE;
#endif // _PRINT_SUPPORT_

	// these can never be trusted between navigations, so always reset
	allData.linkID = NULL;
	allData.frameID = NULL;
	navigate_from = NULL;

	UpdateNavigationData();

	if(!activeFrameDoc || !activeFrameDoc->GetDocRoot())   // just bail if there is nothing to navigate
		return FALSE;

	if (scroll)
		scrolledOnce = FALSE;

	if (scrollValue)
	{
		SetScrollX(scrollValue);
		SetScrollY(scrollValue);
	}

#ifdef USE_4WAY_NAVIGATION
	// Always use fourway navigation
	fourway = TRUE;
#else
	// Use twoway navigation in handheld mode, otherwise fourway
	if (window->GetHandheld())
		fourway = FALSE;
	else
		fourway = TRUE;
#endif // USE_4WAY_NAVIGATION

	return TRUE;
}

#ifdef _PLUGIN_NAVIGATION_SUPPORT_
BOOL SnHandler::NavigateInsidePlugin(INT32 direction)
{
	HTML_Element* helm = GetActiveLink();

	if (helm == NULL)
		return FALSE;

	Box* box = helm->GetLayoutBox();
	if (box == NULL || box->IsContentEmbed() == FALSE)
		return FALSE;

	EmbedContent* content = (EmbedContent*) box->GetContent();
	if (content == NULL)
		return FALSE;

	OpNS4Plugin* plugin = content->GetOpNS4Plugin();
	if (plugin == NULL)
		return FALSE;

	return plugin->NavigateInsidePlugin(direction);
}
#endif

#ifdef MEDIA_SUPPORT
BOOL SnHandler::NavigateInsideMedia(INT32 direction, BOOL focus_gained)
{
	HTML_Element* helm = GetActiveLink();

	if (helm == NULL)
		return FALSE;

	Media* media_elm = helm->GetMedia();
	if (!media_elm)
		return FALSE;

	if (focus_gained)
	{
		media_elm->FocusInternalEdge(TRUE);
		return TRUE;
	}
	else
	{
		if (direction == DIR_UP || direction == DIR_DOWN)
			return FALSE;

		return media_elm->FocusNextInternalTabStop(direction == DIR_RIGHT);
	}
}
#endif // MEDIA_SUPPORT


BOOL SnHandler::GetNextLink(INT32 direction, POINT* startingPoint, BOOL scroll)
{
	OP_ASSERT(currentDoc->Type() == DOC_FRAMES);

	if (currentFramePath->Empty())
	{
		if (!SetInitialFrame())
			return FALSE;
	}

	HTML_Element* active_link = GetActiveLink();
	//Do not restore focus when navigating from element, that have been undisplayed
	if (active_link && active_link->GetLayoutBox())
		previous_link = active_link;
	else
		previous_link = NULL;

	previous_scrollable = activeScrollable;
	// don't restore highlight to text elements, that's text selection
	// TODO: probably should restore text selection though
	if (previous_link && previous_link->Type() == HE_TEXT)
		previous_link = NULL;
	currentFramePath->CreateStorePoint();

	FrameElement* currentFrame = currentFramePath->First();
	FrameElement* parent = currentFrame->Suc();
	FramesDocument* parentDoc = parent ? parent->GetFramesDocument() : NULL;

	// look for links until we've searched the entire frames stack
	NavigationResult result;
	BOOL changedFrame = FALSE;
	while (TRUE)
	{
		FramesDocument* frames_doc = currentFramePath->First()->GetFramesDocument();
		BOOL3 specialFrames = MAYBE;
		FramesDocElm* nextDocElm = NULL;
		if (!frames_doc->IsFrameDoc())
		{
			INT32 scroll_amount = -1;
			result = FindLinkInCurrentFrame(direction, startingPoint, scroll, &scroll_amount);
			if (result == FOUND_ELEMENT || result == SCROLLED || result == NEW_SCROLLABLE || changedFrame)  // we're done
				break;

			// With smart frames we only have the main frame to scroll, so
			// check if the next frame is in view before we scroll
			specialFrames = SnUtil::HasSpecialFrames(currentDoc) ? YES : NO;
			if (specialFrames == YES && parentDoc && parentDoc->IsFrameDoc())
			{
				nextDocElm = GetNextFrame(direction, parentDoc, currentFrame->GetFrmDocElm(), TRUE);
				if (nextDocElm)
				{
					// The next frame is in view, so we want to navigate to it instead of scrolling
					result = NEW_FRAME;
				}
			}

			// We found no link, so let's try scrolling this frame
			if (result != NEW_FRAME && scroll)
			{
				FrameElement* frame = NULL;
				if (result == SCROLL_OUTER)  // we want to scroll the frame outside the inner one, as the inner one has not yet scrolled entirely into view
				{
					frame = currentFramePath->First()->Suc();
					OP_ASSERT(frame);
				}

				if (Scroll(direction, frame, scroll_amount))
				{
					RestorePreviousLink();
					return TRUE;   // scrolling is result good enough
				}
			}
		}

		if (initial_highlight)
			//No sense of moving to another frame on initial marking, we can stop here
			break;

		currentFrame = currentFramePath->First();
		frames_doc = currentFrame->GetFramesDocument();
		// We have reached the end of the document and need to find the next frame
		if (currentFramePath->Last()->GetFramesDocument() == frames_doc)
		{
			RestorePreviousLink();
			return FALSE;	// Already in top frame, no more places to go
		}

		parent = currentFrame->Suc();
		parentDoc = parent->GetFramesDocument();

		/* We are about to change frame. navigate_from can point to some element
		   in the previous document, so it will become invalid. If we just finish
		   navigation without any action that just doesn't harm. */
		navigate_from = NULL;

		activeFrameDoc->ClearSelection(TRUE);

		if (parentDoc->IsFrameDoc())
		{
			activeScrollable = currentFrame->GetScrollable();
			currentFramePath->RemoveFrameFromPath();	// Return to parent

			// Frames document => Select next frame in direction if any
			if (specialFrames == MAYBE)
				//wasn't checked higher
				nextDocElm = GetNextFrame(direction, parentDoc, currentFrame->GetFrmDocElm(), SnUtil::HasSpecialFrames(currentDoc));
			else if (specialFrames == NO)
				nextDocElm = GetNextFrame(direction, parentDoc, currentFrame->GetFrmDocElm(), FALSE);
			//else specialFrames == YES, we have nextDocElm retrieved already (if any)
			if (nextDocElm)
			{
				RETURN_VALUE_IF_ERROR(EnterFrame(nextDocElm), FALSE);
				changedFrame = TRUE;
			}
			else
			{
				// We did not get a frame
				if (currentFramePath->First() == currentFramePath->Last())
				{
					// This is the top frame, so let's keep focus in the last frame
					RestorePreviousLink();
					return FALSE;
				}
				else // These frames are inside another (i)frame => so let's exit that one as well
				{
					currentFrame = currentFramePath->First();
					if (currentFrame->IsIFrame())  // inside an iframe
						ExitIFrame();
				}
			}
		}
		else  // iframe
			ExitIFrame();
	}

	currentFramePath->DiscardPreviousPath();

	return TRUE;
}

void SnHandler::SetActiveLinkAndHighlight()
{
	FramesDocument* doc = static_cast<FramesDocument*>(allData.frameID);
	HTML_Element* helm = static_cast<HTML_Element*>(allData.linkID);

	if (doc && helm)
	{
		HTML_Document* htmlDoc = NULL;
		OP_ASSERT(doc->Type() == DOC_FRAMES);
		OP_ASSERT(activeFrameDoc);
		htmlDoc = doc->GetHtmlDocument();

		BOOL doc_changed = activeFrameDoc != doc;

		if (doc_changed)
		{
			activeFrameDoc->ClearSelection(TRUE);
			SetActiveFrame(doc);
		}

		Highlight(htmlDoc, helm, TRUE, FALSE);  // Highlighting sets active link

		OnLinkNavigated();

		if (allData.isCSSNavLink)
		{
			// If allData.isCSSNavLink is TRUE, the rect should be valid.
			OP_ASSERT(!SN_IS_RECT_EMPTY(allData.linkRect));

			OpRect op_link_rect(&allData.linkRect);
			doc->ScrollToRect(op_link_rect, SCROLL_ALIGN_CENTER, FALSE, VIEWPORT_CHANGE_REASON_SPATIAL_NAVIGATION, helm);

			if (doc_changed)
				/* Since new CSS nav link can be in any document in the tree, the below
				   will force frame path data reset on next navigation. */
				currentDoc = NULL;
		}
	}
}

void SnHandler::RestorePreviousLink()
{
	currentFramePath->RestorePreviousPath();
	FrameElement* new_active = currentFramePath->First();

	SetActiveFrame(new_active->GetFramesDocument());

	OP_ASSERT(activeHTML_Doc == activeFrameDoc->GetHtmlDocument());

	if (previous_link && GetActiveLink() != previous_link)
	{
		SetActiveLink(previous_link);
		Highlight(activeHTML_Doc, previous_link, FALSE, FALSE);
	}

	activeScrollable = previous_scrollable;
}

BOOL SnHandler::EnterFirstSubFrame(FramesDocument* parent_frame_doc)
{
	// The current doc is a frames document so we need to get the first frame
	// with a FramesDocument and VisualDevice
	FramesDocElm* frmDocElm = parent_frame_doc->GetFrmDocRoot();
	while (frmDocElm && (!frmDocElm->GetVisualDevice() || !frmDocElm->GetCurrentDoc()))
		frmDocElm = (FramesDocElm*)frmDocElm->Next();

	if (frmDocElm)
	{
		OP_ASSERT(frmDocElm->GetCurrentDoc()->Type() == DOC_FRAMES);
		FramesDocument* frames_doc = (FramesDocument*)frmDocElm->GetCurrentDoc();
		OP_ASSERT(frames_doc);  // When do we not get a frames_doc?
		if(!frames_doc)
			return FALSE;

		SetActiveFrame(frames_doc);
		RECT rect = {0,0,0,0};
		currentFramePath->AddFrameToPath(frames_doc, frmDocElm, rect, activeScrollable);

		if (frames_doc->IsFrameDoc())
		{
			if (EnterFirstSubFrame(frames_doc))
				return TRUE;
			else
			{
				currentFramePath->RemoveFrameFromPath();
				return FALSE;
			}
		}
		else
			return TRUE;
	}
	else
	{
		// On initial highlight, sub frames may not have finished displaying yet.  But otherwise we should find something.
		OP_ASSERT(initial_highlight);
		return FALSE;
	}
}

BOOL SnHandler::SetInitialFrame()
{
	OP_ASSERT(currentDoc);

	if (currentDoc->IsFrameDoc())
	{
		if (activeFrameDoc && activeFrameDoc != currentDoc)
		{
			FramesDocElm* frmDocElm = activeFrameDoc->GetDocManager()->GetFrame();
			RECT rect = {0,0,0,0};
			currentFramePath->AddFrameToPath((FramesDocument*)currentDoc, NULL, rect, activeScrollable);
			currentFramePath->AddFrameToPath(activeFrameDoc, frmDocElm, rect, activeScrollable);
			if (activeFrameDoc->IsFrameDoc())
			{
				if (EnterFirstSubFrame(activeFrameDoc))
					return TRUE;
				else
				{
					currentFramePath->RemoveFrameFromPath();  // removes the parent we added first
					return FALSE;
				}
			}
		}
		else
		{
			// The current doc is a frames document so we need to get the first frame
			RECT rect = {0,0,0,0};
			currentFramePath->AddFrameToPath((FramesDocument*)currentDoc, NULL, rect, activeScrollable);
			if (EnterFirstSubFrame(currentDoc))
				return TRUE;
			else
			{
				currentFramePath->RemoveFrameFromPath();  // removes the parent we added first
				return FALSE;
			}
		}
	}
	else
	{
		// Build up iframe stack
		RECT rect = {0,0,0,0};
		FramesDocument *doc = NULL;

		do
		{
			BOOL firstIteration;

			if (doc)
				firstIteration = FALSE;
			else
			{
				firstIteration = TRUE;
				doc = activeFrameDoc;
			}

			if (doc->IsInlineFrame())
			{
				FramesDocElm* frame = doc->GetDocManager()->GetFrame();
				Box* iframeBox = frame->GetHtmlElement() ? frame->GetHtmlElement()->GetLayoutBox() : NULL;

				if ( !(iframeBox && iframeBox->GetRect(doc->GetParentDoc(), CONTENT_BOX, rect)) )
					//Safety increase, so that the rect has width > 0 and this frame will be considered iframe by spatnav
					rect.right++;

				currentFramePath->AddFrameToPath(doc, frame, rect, NULL, !firstIteration);
				rect.left = 0;
				rect.top = 0;
				rect.bottom = 0;
				rect.right = 0;
			}
			else
				currentFramePath->AddFrameToPath(doc, doc->GetDocManager()->GetFrame(), rect, NULL, !firstIteration);

		} while ((doc = doc->GetParentDoc()) != NULL);
	}
	return TRUE;
}

OP_STATUS SnHandler::EnterFrame(FramesDocElm* frame)
{
	FramesDocument* nextFrameDoc = (FramesDocument*) frame->GetCurrentDoc();
	RECT rect = {0,0,0,0};
	RETURN_IF_ERROR(currentFramePath->AddFrameToPath(nextFrameDoc, frame, rect, activeScrollable));

	if (nextFrameDoc->IsFrameDoc())
	{
		if (!EnterFirstSubFrame(nextFrameDoc))
			return OpStatus::ERR;
	}
	else
	{
		// We got a frame, let's give focus to it
		SetActiveFrame(nextFrameDoc);
		allData.frameID = nextFrameDoc;
		allData.linkID = NULL;
	}

	return OpStatus::OK;
}

OP_STATUS SnHandler::EnterIFrame(HTML_Element* iframe_element, FramesDocument* frames_doc, const RECT& iframe_rect)
{
	OP_ASSERT(ElementTypeCheck::IsIFrameElement(iframe_element, frames_doc->GetLogicalDocument()));

	// This is an iframe => find it and give focus to it
	FramesDocElm* docElm = FramesDocElm::GetFrmDocElmByHTML(iframe_element);
	if (!docElm)
		return OpStatus::ERR;

	FramesDocument* nextFrameDoc = (FramesDocument*)docElm->GetCurrentDoc();
	if (!nextFrameDoc)
		return OpStatus::ERR;

	RETURN_IF_ERROR(currentFramePath->AddFrameToPath(nextFrameDoc, docElm, iframe_rect, activeScrollable));
	activeFrameDoc->ClearSelection(TRUE);
	SetActiveFrame(nextFrameDoc);

	allData.frameID = nextFrameDoc;
	// Reset search area as we will use coordinates relative to iframe
	int x_scroll = nextFrameDoc->GetVisualViewX();
	long y_scroll = nextFrameDoc->GetVisualViewY();

	allData.linkRect.left = x_scroll;
	allData.linkRect.top = y_scroll;
	allData.linkRect.right = allData.linkRect.left + (iframe_rect.right - iframe_rect.left);
	allData.linkRect.bottom = allData.linkRect.top + (iframe_rect.bottom - iframe_rect.top);
	activeScrollable = NULL;

	if (allData.linkID)
		allData.linkID = NULL;

	return OpStatus::OK;
}

void SnHandler::ExitIFrame(BOOL exit_after_try /* = FALSE */)
{
	OP_ASSERT(currentFramePath->Last()->GetFramesDocument() != currentFramePath->First()->GetFramesDocument());  // don't call exit from top frame

	FrameElement* currentFrame = currentFramePath->First();
	FramesDocElm* oldElm = currentFrame->GetFrmDocElm();
	RECT oldRect = *(currentFrame->GetIframeRect());

	activeScrollable = currentFrame->GetScrollable(); //this frame has stored the information about activeScrollable in parent frame
	currentFramePath->RemoveFrameFromPath();	// Return to parent

	FrameElement* parent = currentFramePath->First();
	FramesDocument* parentDoc = parent->GetFramesDocument();

	if (!exit_after_try && parent->IsIFrame())
	{
		RECT rect;
		Box* parentBox = parent->GetFrmDocElm()->GetHtmlElement()->GetLayoutBox();

		if (parentBox)
		{
			if (parentBox->GetRect(parent->Suc()->GetFramesDocument(),CONTENT_BOX,rect))
				parent->SetIframeRect(rect);
		}
		else
		{
			// Parent iframe has been undisplayed, exit that one as well.
			ExitIFrame(exit_after_try);
			return;
		}
	}

	OP_ASSERT(!parentDoc->IsFrameDoc());

	SetActiveFrame(parentDoc);

	if (!exit_after_try)
	{
		HTML_Element* iframe_elm = oldElm->GetHtmlElement();
		navigate_from = iframe_elm;
		SetActiveLink(iframe_elm);

		allData.linkRect = oldRect;
		allData.frameID = parentDoc;
	}
}

SnHandler::NavigationResult SnHandler::FindLinkInFrameSet(INT32 direction, POINT* startingPoint, BOOL scroll)
{
	FramesDocument* frameset = currentFramePath->First()->GetFramesDocument();
	navigate_from = NULL;

	if (!frameset->IsFrameDoc())  // iframe
	{
		NavigationResult result;
		INT32 scroll_amount;
		result = FindLinkInCurrentFrame(direction, startingPoint, scroll, &scroll_amount);

		if (result == SCROLL_OUTER)
		{
			if (!scroll)
				return NO_ACTION;

			// couldn't find anything in iframe, but part of it is outside view
			// in search direction, so scroll outer frame so more of iframe is visible
			FrameElement* parent = currentFramePath->First()->Suc();
			OP_ASSERT(parent);
			if (parent && Scroll(direction, parent, scroll_amount))
				return SCROLLED;
			else
				return NO_ACTION;
		}
		if (result == NO_ACTION && scroll) // try scrolling
		{
			if (Scroll(direction))
				return SCROLLED;
		}

		return result;
	}

	if (!EnterFirstSubFrame(frameset))
		return NAVIGATION_ERROR;

	// search all subframes
	NavigationResult result;
	while ((result = FindLinkInCurrentFrame(direction, startingPoint, scroll)) == NO_ACTION)
	{
		FramesDocElm* previousFrame = currentFramePath->First()->GetFrmDocElm();
		// didn't find anything in this subframe
		currentFramePath->RemoveFrameFromPath();

		// find next subframe
		FramesDocElm* nextDocElm = GetNextFrame(direction, frameset, previousFrame);
		if (!nextDocElm)  // no more frames, and didn't find any links in any
			return NO_ACTION;

		RETURN_VALUE_IF_ERROR(EnterFrame(nextDocElm), NAVIGATION_ERROR);
	}

	return result;
}

SnHandler::NavigationResult SnHandler::FindLinkInCurrentFrame(INT32 direction,
															  POINT* startingPoint,
															  BOOL scroll,
															  INT32* scroll_amount /*= NULL*/,
															  const RECT* new_scrollable_rect_ptr /*= NULL*/)
{
	NavigationResult result = NO_ACTION;
	FrameElement* currentFrame = currentFramePath->First();
	BOOL reconsider_as_scrollable = TRUE;

	activeFrameDoc = currentFrame->GetFramesDocument();
	activeHTML_Doc = activeFrameDoc->GetHtmlDocument();
	OP_ASSERT(activeFrameDoc->Type() == DOC_FRAMES);

	if (activeFrameDoc->GetDocRoot() == NULL)
	{
		// Something is wrong with this document => Try changing to another one
		return NEW_FRAME;
	}

	RECT area = {0,0,0,0};  // Relative to rendering viewport. Initialise area to 0,0,0,0 to shut up compiler warning, it will always be set to its proper value furter down, even if the compiler can't figure that out

	FrameElement* parentFrame = currentFrame->Suc();
	FramesDocument* parentFrameDoc = NULL;
	VisualDevice* visual_device = activeFrameDoc->GetVisualDevice();
	OpRect visual_viewport(activeFrameDoc->GetVisualViewport());
	OpRect rendering_viewport(visual_device->GetRenderingViewport());
	BOOL activeLinkHasLineBreak = FALSE;
	const RECT* iframe_rect_ptr = NULL; //a pointer to iframe rectangle in parent document, remains NULL if current doc is not iframe

	if (currentFrame->IsIFrame())
	{
		parentFrameDoc = parentFrame->GetFramesDocument();
		iframe_rect_ptr = currentFrame->GetIframeRect();
	}

	HTML_Element* activeLink;
	OpRect scrollableRect;
	BOOL checkCurrentScrollable = FALSE;

	if (navigate_from)
	{
		// Normally we will enter a scrollable element even if it is activeLink
		// But not if it's the navigate_from element, that means we just exited
		// that scrollable and don't want to enter it again
		activeLink = navigate_from;
		reconsider_as_scrollable = FALSE;
		checkCurrentScrollable = TRUE;
	}
	else
	{
		activeLink = GetActiveLink();

		if (!new_scrollable_rect_ptr)
		{
			if (activeLink && (activeLink != lastElement))
				//Active link has been changed externally, we have to check whether it has scrollable parent
				activeScrollable = SnUtil::GetScrollableAncestor(activeLink, activeFrameDoc, &scrollableRect);
			else
				checkCurrentScrollable = TRUE;
		}
		else
			// We have just entered the scrollable, we can trust the cached rect.
			scrollableRect = OpRect(new_scrollable_rect_ptr);
	}

	if (checkCurrentScrollable)
	{
		if (activeScrollable)
		{
			Content* content = activeScrollable->GetLayoutBox() ? activeScrollable->GetLayoutBox()->GetContent() : NULL;

			if (content &&
				(content->GetScrollable()
#ifdef PAGED_MEDIA_SUPPORT
				 || content->GetPagedContainer()
#endif // PAGED_MEDIA_SUPPORT
					))
				SnUtil::GetScrollableElementRect(content, activeFrameDoc, scrollableRect);
			else
				activeScrollable = NULL;
		}

		if (activeLink && !activeScrollable)
			//Perhaps activeLink has other or new scrollable parent?
			activeScrollable = SnUtil::GetScrollableAncestor(activeLink, activeFrameDoc, &scrollableRect);
	}

	if (activeScrollable)
	{
		OP_ASSERT(activeScrollable->GetLayoutBox());
		if (SnUtil::IsPartiallyInView(scrollableRect, activeFrameDoc, iframe_rect_ptr))
		{
			area.left = scrollableRect.x - rendering_viewport.x;
			area.top = scrollableRect.y - rendering_viewport.y;
			area.right = scrollableRect.x + scrollableRect.width - rendering_viewport.x - 1;
			area.bottom = scrollableRect.y + scrollableRect.height- rendering_viewport.y - 1;
		}
		else  // If the current scrollable is completly out of view, then exit it
			activeScrollable = NULL;
	}

	if (!activeScrollable)
	{
		if (SnUtil::HasSpecialFrames(currentDoc) && parentFrame && !currentFrame->IsIFrame())
		{
			/* With frame stacking/smartframes part of the frame may
			 * be out of the visible part of the screen.  So adjust
			 * area to not include the parts of the area outside of the
			 * visible area
			 *
			 * area is relative to the frame
			 * top_vd is the visual device of the entire frame set (i.e. entire page)
			 * frame_abs_x/y is the position of the frame within the frame set
			 */

			int frame_abs_x = currentFrame->GetFrmDocElm()->GetAbsX();
			int frame_abs_y = currentFrame->GetFrmDocElm()->GetAbsY();
			OpRect top_visual_viewport = currentDoc->GetVisualViewport();

			area.left = MAX(0, top_visual_viewport.x - frame_abs_x);
			area.top = MAX(0, top_visual_viewport.y - frame_abs_y);
			area.right = MAX(1, top_visual_viewport.Right() - frame_abs_x);
			area.bottom = MAX(1, top_visual_viewport.Bottom() - frame_abs_y);
		}
		else
		{
			// Initialize area to visible area.  Coordinates are relative to frame.
			area.left = visual_viewport.x - rendering_viewport.x;
			area.top = visual_viewport.y - rendering_viewport.y;
			area.right = area.left + visual_viewport.width;
			area.bottom = area.top + visual_viewport.height;
		}

		if (currentFrame->IsIFrame())
		{
			// For iframes adjust for iframe position
			RECT iframeRect = *iframe_rect_ptr;

			OpRect parent_visual_viewport;

			FramesDocument* grandParentDoc = parentFrameDoc->GetParentDoc();
			if (!parentFrame->IsIFrame() && grandParentDoc && grandParentDoc->IsFrameDoc()
					&& SnUtil::HasSpecialFrames(grandParentDoc))
			{
				FramesDocElm* parentElm = parentFrame->GetFrmDocElm();
				//Get the viewport of a top document in frameset with special frames
				parent_visual_viewport = grandParentDoc->GetVisualViewport();

				//Adjust iframeRect according to frame position, so it matches with the grandparent document's viewport
				iframeRect.left += parentElm->GetAbsX();
				iframeRect.right += parentElm->GetAbsX();
				iframeRect.top += parentElm->GetAbsY();
				iframeRect.bottom += parentElm->GetAbsY();
			}
			else
			{
				parent_visual_viewport = parentFrameDoc->GetVisualViewport();
			}

			// Adjust iframe position for scrolling
			iframeRect.left -= parent_visual_viewport.x;
			iframeRect.right -= parent_visual_viewport.x;
			iframeRect.top -= parent_visual_viewport.y;
			iframeRect.bottom -= parent_visual_viewport.y;

			if (iframeRect.right < 0 || iframeRect.bottom < 0 ||
				iframeRect.left >= parent_visual_viewport.width ||
				iframeRect.top >= parent_visual_viewport.height)
				// if the iframe is completly out of view, so no need searching this one, get us a new!
				return NEW_FRAME;

			// limit search area to visible part of iframe, and tell caller that more
			// can be seen by scrolling iframe further into view
			if (iframeRect.left < 0)  // part of frame is outside view on the left
			{
				if (direction == DIR_LEFT)
				{
					result = SCROLL_OUTER;
					if (scroll_amount)
						*scroll_amount = -iframeRect.left;
				}
				area.left -= iframeRect.left;  // adjust area to start where frame enters visible area
				iframeRect.left = 0;
			}
			if (iframeRect.top < 0)  // part of frame is outside view above
			{
				if (direction == DIR_UP)
				{
					result = SCROLL_OUTER;
					if (scroll_amount)
						*scroll_amount = -iframeRect.top;
				}
				area.top -= iframeRect.top;  // adjust area to start where frame enters visible area
				iframeRect.top = 0;
			}
			if (iframeRect.right > parent_visual_viewport.width)  // part of frame is outside view to the right
			{
				if (direction == DIR_RIGHT)
				{
					result = SCROLL_OUTER;
					if (scroll_amount)
						*scroll_amount = iframeRect.right - parent_visual_viewport.width;
				}
				iframeRect.right = parent_visual_viewport.width;
			}
			if (iframeRect.bottom > parent_visual_viewport.height)  // part of frame is below view
			{
				if (direction == DIR_DOWN)
				{
					result = SCROLL_OUTER;
					if (scroll_amount)
						*scroll_amount = iframeRect.bottom - parent_visual_viewport.height;
				}
				iframeRect.bottom = parent_visual_viewport.height;
			}

			// get right and bottom boundaries by adding width to left and top.  But keep it inside visible area
			long width = MIN(iframeRect.right - iframeRect.left, area.right - area.left);
			long height = MIN(iframeRect.bottom - iframeRect.top, area.bottom - area.top);
			area.right = area.left + width;
			area.bottom = area.top + height;
		}
	}

	BOOL have_single_line_search_origin = FALSE;
	RECT search_origin = area, single_line_search_origin = { 0, 0, 0, 0 };
	// Search origin is relative to document root.
	search_origin.left += rendering_viewport.x;
	search_origin.top += rendering_viewport.y;
	search_origin.right += rendering_viewport.x;
	search_origin.bottom += rendering_viewport.y;

	if (startingPoint)
	{
		search_origin.top = startingPoint->y;
		search_origin.bottom = startingPoint->y+1;
		search_origin.left = startingPoint->x;
		search_origin.right = startingPoint->x+1;
		activeLink = NULL;
		switch (direction)
		{
		case DIR_RIGHT:
			area.left = startingPoint->x;
			break;

		case DIR_UP:
			area.bottom = startingPoint->y;
			break;

		case DIR_LEFT:
			area.right = startingPoint->x;
			break;

		case DIR_DOWN:
			area.top = startingPoint->y;
			break;

		default:
			OP_ASSERT(0);
			break;
		}
	}
	else if (activeLink)  	// if we have some element already we want to navigate from that, otherwise use entire area
	{
		HTML_Element* rect_element = activeLink;
		// When handling an image map, the areas don't have a layout box, so
		// we have to get the rect of the image and then adjust for the area's
		// position within the image
		if (activeLink->IsMatchingType(HE_AREA, NS_HTML))
		{
			HTML_Element* area = activeHTML_Doc->GetNavigationElement();
			if (area)
				rect_element = area;
		}

		BOOL has_rect = FALSE;
		BOOL firstVisibleFound = FALSE;
		BOOL can_use_last_rect = activeLink == lastElement && !SN_IS_RECT_EMPTY(lastRect);
		Box* rect_box = rect_element->GetLayoutBox();
		RectList rect_list;
		if (rect_box && rect_box->GetRectList(activeFrameDoc, BORDER_BOX, rect_list))
		{
			can_use_last_rect = FALSE;
			// We need to take the surrounding rectangle
			RectListItem* item = rect_list.First();
			for (; item != NULL; item = item->Suc())
			{
				RECT rect = item->rect;

				if (SN_IS_RECT_EMPTY(rect))
				{
				/* Try to use the left and/or top as the expected layout position
				   of the element. */

					if (rect.left >= rect.right)
						rect.right = rect.left + 1;

					if (rect.top >= rect.bottom)
						rect.bottom = rect.top + 1;
				}

				if (SnUtil::IsPartiallyInView(rect, activeFrameDoc, iframe_rect_ptr))
				{
					// For line-breaked links we use the the same line
					// that we found in the traverse as origin. But
					// only if it still covers the previous position,
					// if not we use the element's whole rect.
					if (lastElement == activeLink &&
						activeLink->Type() != HE_AREA &&  // image maps are not line-breaked links
						lastRect.left >= rect.left &&
						lastRect.right <= rect.right &&
						lastRect.top >= rect.top &&
						lastRect.bottom <= rect.bottom)
					{
						single_line_search_origin = rect;
						have_single_line_search_origin = TRUE;
					}

					if (firstVisibleFound)
					{
						activeLinkHasLineBreak = TRUE;
						if (search_origin.left > rect.left)
							search_origin.left = rect.left;
						if (search_origin.right < rect.right)
							search_origin.right = rect.right;
						if (search_origin.top > rect.top)
							search_origin.top = rect.top;
						if (search_origin.bottom < rect.bottom)
							search_origin.bottom = rect.bottom;
					}
					else
					{
						search_origin = rect;
						firstVisibleFound = TRUE;
					}
				}
			}

			has_rect = TRUE;
			rect_list.Clear();
		}
#ifdef SVG_SUPPORT
		else if (rect_element->GetNsType() == NS_SVG)
		{
			OpRect rect;
			if (OpStatus::IsSuccess(g_svg_manager->GetNavigationData(rect_element, rect)))
			{
				search_origin.left = rect.x;
				search_origin.top = rect.y;
				search_origin.right = rect.x + rect.width;
				search_origin.bottom = rect.y + rect.height;

				firstVisibleFound = SnUtil::IsPartiallyInView(rect, activeFrameDoc, iframe_rect_ptr);
				has_rect = TRUE;
				can_use_last_rect = FALSE;
			}
		}
#endif // SVG_SUPPORT

		if (can_use_last_rect)
		{
			/* If we end up here it means that:
			   1) We navigate from the same element we navigated to last time.
			   2) The last used rect for that element is not empty
			   3) For SVG element: we couldn't get the current rect from svg manager.
			      For HTML element: The element has no layout box or we couldn't get
				  its rect (rect list) or at least one rect from the list matches the
				  previous layout position assumed from lastRect and no rect from the
				  rect list is visible in the viewport now.

			   In such case it is comfortable to use the stored lastRect as a search
			   origin.
			   Two known cases when this can happen are:
			   - previously navigated element was undisplayed (setting display:none)
			   - previously navigated element was reached by CSS3 navigation
			   property and its BORDER_BOX was empty. In such case we turned it into
			   a single point to mark the further search origin. */

			has_rect = TRUE;
			firstVisibleFound = TRUE;
			search_origin = lastRect;
		}

		if (has_rect)
		{
			OP_ASSERT(search_origin.right >= search_origin.left);
			OP_ASSERT(search_origin.bottom >= search_origin.top);

			if (activeLink->IsMatchingType(HE_AREA, NS_HTML))
			{
				// Adjust for area's position within image
				RECT area_rect;
				if (SnUtil::GetImageMapAreaRect(activeLink, rect_element, search_origin, area_rect))
					search_origin = area_rect;
			}

			if (firstVisibleFound && SnUtil::IsPartiallyInView(search_origin, activeFrameDoc, iframe_rect_ptr))
			{
				allData.linkRect = search_origin;
				long tmp = 0;

				// restrict search area so as to only search from the previous object and in the direction we are searching
				switch (direction)
				{
				case DIR_RIGHT:
					tmp = search_origin.left - rendering_viewport.x;

					area.left = SnUtil::RestrictedTo(tmp, area.left, area.right);
					break;
				case DIR_UP:
					tmp = search_origin.bottom - rendering_viewport.y;

					area.bottom = SnUtil::RestrictedTo(tmp, area.top, area.bottom);
					break;

				case DIR_LEFT:
					tmp = search_origin.right - rendering_viewport.x;

					area.right = SnUtil::RestrictedTo(tmp, area.left, area.right);
					break;

				case DIR_DOWN:
					tmp = search_origin.top - rendering_viewport.y;

					area.top = 	SnUtil::RestrictedTo(tmp, area.top, area.bottom);
					break;

				default:
					OP_ASSERT(0);
					break;
				}

				// The single line rect may be restrict the area to much (so that
				// we don't find the active element during traversal), so not set
				// the search_origin until here, after we've restricted the area
				if (have_single_line_search_origin)
					search_origin = single_line_search_origin;

				if (activeLink == activeScrollable)
				{
					// If the search_origin is the scrollable we're entering, then widen
					// the search origin so that we get elements which goes out to the edges
					// of the scrollable which would otherwise be skipped
					// see http://t/core/bts/javascript/289649/001.xhtml
					search_origin.top -= 1;
					search_origin.bottom += 1;
					search_origin.left -= 1;
					search_origin.right += 1;
				}
			}
			else 	// deselect navigation element if it is outside view:
			{
				activeLink = NULL;
				activeHTML_Doc->ClearFocusAndHighlight();
				search_origin = area;
				search_origin.left += rendering_viewport.x;
				search_origin.top += rendering_viewport.y;
				search_origin.right += rendering_viewport.x + 1;
				search_origin.bottom += rendering_viewport.y + 1;
			}
		}
	}

	SnTraversalObject snTraversalObject(activeFrameDoc, currentDoc, visual_device, area, direction, nav_filter, activeLink, activeScrollable, search_origin, fourway, reconsider_as_scrollable, iframe_rect_ptr, exclusions.GetCount() ? &exclusions : NULL);
	if (OpStatus::IsError(snTraversalObject.Init(activeLinkHasLineBreak)))
		return result;

	snTraversalObject.FindNextLink(activeFrameDoc->GetDocRoot());

	if (snTraversalObject.GetNavDirElement())
	{
		FramesDocument* new_doc = snTraversalObject.GetNavDirDocument();
		allData.linkID = lastElement = snTraversalObject.GetNavDirElement();
		allData.frameID = new_doc;

		/* CSS3 nav dir element can be anywhere, update activeScrollable here.
		   Don't care for its rect now, it will be updated on next navigation anyway. */
		activeScrollable = SnUtil::GetScrollableAncestor(lastElement, new_doc, NULL);

		Box* box = lastElement->GetLayoutBox();
		if (box && box->GetBBox(new_doc, BORDER_BOX, lastRect))
		{
			if (SN_IS_RECT_EMPTY(lastRect))
			{
				/* Try to use the left and/or top as the expected layout position
				   of the element. */

				if (lastRect.left >= lastRect.right)
					lastRect.right = lastRect.left + 1;

				if (lastRect.top >= lastRect.bottom)
					lastRect.bottom = lastRect.top + 1;
			}

			allData.linkRect = lastRect;
			allData.isCSSNavLink = TRUE;
		}
		else
		{
			SN_EMPTY_RECT(allData.linkRect);
			SN_EMPTY_RECT(lastRect);
			// Doesn't matter in this case. We can pretend it is not CSS3 navigation found link.
			allData.isCSSNavLink = FALSE;
		}

		return FOUND_ELEMENT;
	}

	HTML_Element* newElm = snTraversalObject.GetCurrentElm();
	if (newElm)
	{
		RECT linkRect = snTraversalObject.GetCurrentRect();
		if (ElementTypeCheck::IsIFrameElement(newElm, activeFrameDoc->GetLogicalDocument()))
		{
			// Store the current iframe or scrollable that we are navigating from.
			HTML_Element* old_navigate_from = navigate_from;

			RETURN_VALUE_IF_ERROR(EnterIFrame(newElm, activeFrameDoc, linkRect), NAVIGATION_ERROR);
			if ((result = FindLinkInFrameSet(direction, NULL, scroll)) != NO_ACTION)
				return result;
			else
			{
				ExitIFrame(TRUE);

				if (old_navigate_from)
					navigate_from = old_navigate_from;
				else if (currentFramePath->IsOnStoredPoint())
					/* Back to the frame/doc from which we started navigation.
					   Use the previous link for the search origin again. */
					navigate_from = previous_link;

				exclusions.Add(newElm); // No point in navigating to the same iframe again.
				result = FindLinkInCurrentFrame(direction, NULL, scroll);
				exclusions.RemoveByItem(newElm);
				return result;
			}
		}
		else
		{
			activeScrollable = snTraversalObject.GetCurrentScrollable();
			if (activeScrollable == newElm)
				// Find first link in the scrollable (or scroll if none)
				return FindLinkInCurrentFrame(direction, startingPoint, scroll, NULL, &linkRect);
			else  // normal link selected, store it
			{
				if (!activeScrollable ||
					SnUtil::IsPartiallyInView(linkRect.left,
											  linkRect.top,
											  linkRect.right - linkRect.left,
											  linkRect.bottom - linkRect.top,
											  scrollableRect.Left(),
											  scrollableRect.Top(),
											  scrollableRect.width,
											  scrollableRect.height))
				{
					allData.linkID = newElm;
					allData.isCSSNavLink = FALSE;
					lastElement = newElm;

					allData.linkRect = linkRect;
					lastRect = linkRect;
					allData.frameID = activeFrameDoc;

					return FOUND_ELEMENT;
				}
				/* else if the element is out of view of the scrollable, should behave as nothing was found
				   It passes the check in SnTraversalObject, because it is done only against the whole visual viewport.
				   VisualTraversalObject can enter inline elements that are out of the area also. */
			}
		}
	}

	if (activeScrollable)
	{
		BOOL exit_scrollable = scrolledOnce;
		if (!exit_scrollable)
		{
			/* We are looking for links inside scrollable content (ScrollableContainer,
			   PagedContainer, etc.). Let's scroll it and look again. */

			Content* content = activeScrollable->GetLayoutBox()->GetContent();
			OpPoint old_scroll_pos = content->GetScrollOffset();

			if (ScrollableArea* scrollable = content->GetScrollable())
			{
				OpInputAction::Action scrolldirection = OpInputAction::ACTION_PAGE_DOWN;
				switch (direction)
				{
				case DIR_RIGHT:
					scrolldirection = OpInputAction::ACTION_PAGE_RIGHT;
					break;
				case DIR_LEFT:
					scrolldirection = OpInputAction::ACTION_PAGE_LEFT;
					break;
				case DIR_UP:
					scrolldirection = OpInputAction::ACTION_PAGE_UP;
					break;
				case DIR_DOWN:
					scrolldirection = OpInputAction::ACTION_PAGE_DOWN;
					break;
				}
				OpInputAction tmp(scrolldirection);
				scrollable->TriggerScrollInputAction(&tmp, FALSE);
			}
#ifdef PAGED_MEDIA_SUPPORT
			else
				if (PagedContainer* paged = content->GetPagedContainer())
				{
					unsigned int current_page = paged->GetCurrentPageNumber();
					int pagediff = 0;

					if (paged->IsPaginationVertical())
					{
						if (direction == DIR_UP || direction == DIR_DOWN)
							pagediff = direction == DIR_DOWN ? 1 : -1;
					}
					else
						if (direction == DIR_LEFT || direction == DIR_RIGHT)
# ifdef SUPPORT_TEXT_DIRECTION
							if (paged->IsRTL())
								pagediff = direction == DIR_LEFT ? 1 : -1;
							else
# endif // SUPPORT_TEXT_DIRECTION
								pagediff = direction == DIR_RIGHT ? 1 : -1;

					if (pagediff)
						if (pagediff > 0)
						{
							if (current_page < paged->GetTotalPageCount() - 1)
								paged->SetCurrentPageNumber(current_page + 1);
						}
						else
							if (current_page > 0)
								paged->SetCurrentPageNumber(current_page - 1);
				}
#endif // PAGED_MEDIA_SUPPORT

			OpPoint new_scroll_pos = content->GetScrollOffset();

			if (new_scroll_pos != old_scroll_pos)
			{
				if (direction == DIR_UP || direction == DIR_DOWN)
				{
					int diff = new_scroll_pos.y - old_scroll_pos.y;
					allData.linkRect.top -= diff;
					allData.linkRect.bottom -= diff;
				}
				else
				{
					int diff = new_scroll_pos.x - old_scroll_pos.x;
					allData.linkRect.left -= diff;
					allData.linkRect.right -= diff;
				}

				if (activeLink && !SnUtil::IsPartiallyInView(allData.linkRect.left,
															 allData.linkRect.top,
															 allData.linkRect.right-allData.linkRect.left,
															 allData.linkRect.bottom-allData.linkRect.top,
															 scrollableRect.x,
															 scrollableRect.y,
															 scrollableRect.width,
															 scrollableRect.height))
				{
					allData.linkID = NULL;
					activeHTML_Doc->ClearFocusAndHighlight();
				}

				scrolledOnce = TRUE;
				return SCROLLED;
			}
			else if (new_scrollable_rect_ptr)
			{
				if (activeLink)
					/* Previous link may have been outside the scrollable, we can't keep it highlighted,
					   otherwise we won't navigate from the new scrollable in next navigation. */
				{
					allData.linkID = NULL;
					activeHTML_Doc->ClearFocusAndHighlight();
				}
				return NEW_SCROLLABLE;
			}
			else // scrolling not possible, try continuing search outside
				exit_scrollable = TRUE;
		}

		if (exit_scrollable)
		{
			if (exclusions.Find(activeScrollable) == -1)
				exclusions.Add(activeScrollable); // do not bother entering the scrollable element once again. OOM handling?

			HTML_Element* lastScrollable = activeScrollable;

			/* Scroll in the requested direction not possible, or we're at the
			   first or last page and there's no page in the requested
			   direction. So let's get out of the scrollable content and find
			   links as usual. */

			navigate_from = activeScrollable;
			activeScrollable = NULL;

			if ((result = FindLinkInCurrentFrame(direction, startingPoint, scroll)) == NO_ACTION)
			{
				// if unable to find anything outside scrollable either,
				// then keep the last scrollable
				allData.linkID = NULL;
				activeScrollable = lastScrollable;
			}

			exclusions.RemoveByItem(lastScrollable);

			return result;
		}
	}

	OP_ASSERT(result != NEW_FRAME);
	return result;
}

FramesDocElm* SnHandler::GetNextFrame(INT32 direction, FramesDocument* parent, FramesDocElm* oldDocElm, BOOL checkVisibility /*= FALSE*/)
{
	FramesDocElm* frmDocElm = parent->GetFrmDocRoot();
	INT32 oldX = oldDocElm->GetAbsX();
	INT32 oldY = oldDocElm->GetAbsY();
	OpRect visual_viewport(parent->GetVisualViewport());
	INT32 best = (direction == DIR_UP || direction == DIR_LEFT) ? INT_MIN : INT_MAX;
	FramesDocElm* bestElm = NULL;
	INT32 refPoint = 0;
	if (direction == DIR_UP || direction == DIR_DOWN)
	{
		if (GetActiveLink())
			refPoint = (allData.linkRect.left+allData.linkRect.right)/2;
		else
			refPoint = visual_viewport.width/2;
		refPoint += oldX - visual_viewport.x;
	}
	else	// DIR_LEFT || DIR_RIGHT
	{
		if (GetActiveLink())
			refPoint = (allData.linkRect.top+allData.linkRect.bottom)/2;
		else
			refPoint = visual_viewport.height/2;
		refPoint += oldY - visual_viewport.y;
	}

	while (frmDocElm)
	{
		while (frmDocElm && (!frmDocElm->GetVisualDevice() || !frmDocElm->GetCurrentDoc()))
			frmDocElm = (FramesDocElm*)frmDocElm->Next();

		if (frmDocElm)
		{
			INT32 coord = 0;
			switch (direction)
			{
			case DIR_RIGHT:
				coord = frmDocElm->GetAbsX();
				if (coord > oldX)
				{
					if (coord < best)
					{
						best = coord;
						bestElm = frmDocElm;
					}
					else if (coord == best)
					{
						// Same as best => Choose the one closest to refpoint
						INT32 top = bestElm->GetAbsY();
						INT32 bottom = top + bestElm->GetHeight();
						if (!SnUtil::Intersect(top, bottom, refPoint, refPoint + 1))
						{
							INT32 newTop = frmDocElm->GetAbsY();
							INT32 newBottom = newTop + frmDocElm->GetHeight();
							if (SnUtil::Intersect(newTop, newBottom, refPoint, refPoint + 1))
							{
								bestElm = frmDocElm;
							}
							else
							{
								INT32 oldDist = MIN(op_abs(top-refPoint), op_abs(bottom-refPoint));
								INT32 newDist = MIN(op_abs(newTop-refPoint), op_abs(newBottom-refPoint));
								if (newDist < oldDist)
								{
									bestElm = frmDocElm;
								}
							}
						}
					}
				}
				break;

			case DIR_UP:
				coord = frmDocElm->GetAbsY();
				if (coord < oldY)
				{
					if (coord > best)
					{
						best = coord;
						bestElm = frmDocElm;
					}
					else if (coord == best)
					{
						// Same as best => Choose the one closest to refpoint
						INT32 left = bestElm->GetAbsX();
						INT32 right = left + bestElm->GetWidth();
						if (!SnUtil::Intersect(left, right, refPoint, refPoint + 1))
						{
							INT32 newLeft = frmDocElm->GetAbsX();
							INT32 newRight = newLeft + frmDocElm->GetWidth();
							if (SnUtil::Intersect(newLeft, newRight, refPoint, refPoint + 1))
							{
								bestElm = frmDocElm;
							}
							else
							{
								INT32 oldDist = MIN(op_abs(left-refPoint), op_abs(right-refPoint));
								INT32 newDist = MIN(op_abs(newLeft-refPoint), op_abs(newRight-refPoint));
								if (newDist < oldDist)
								{
									bestElm = frmDocElm;
								}
							}
						}
					}
				}
				break;

			case DIR_LEFT:
				coord = frmDocElm->GetAbsX();
				if (coord < oldX)
				{
					if (coord > best)
					{
						best = coord;
						bestElm = frmDocElm;
					}
					else if (coord == best)
					{
						// Same as best => Choose the one closest to refpoint
						INT32 top = bestElm->GetAbsY();
						INT32 bottom = top + bestElm->GetHeight();
						if (!SnUtil::Intersect(top, bottom, refPoint, refPoint + 1))
						{
							INT32 newTop = frmDocElm->GetAbsY();
							INT32 newBottom = newTop + frmDocElm->GetHeight();
							if (SnUtil::Intersect(newTop, newBottom, refPoint, refPoint + 1))
							{
								bestElm = frmDocElm;
							}
							else
							{
								INT32 oldDist = MIN(op_abs(top-refPoint), op_abs(bottom-refPoint));
								INT32 newDist = MIN(op_abs(newTop-refPoint), op_abs(newBottom-refPoint));
								if (newDist < oldDist)
								{
									bestElm = frmDocElm;
								}
							}
						}
					}
				}
				break;

			case DIR_DOWN:
				coord = frmDocElm->GetAbsY();
				if (coord > oldY)
				{
					if (coord < best)
					{
						best = coord;
						bestElm = frmDocElm;
					}
					else if (coord == best)
					{
						// Same as best => Choose the one closest to refpoint
						INT32 left = bestElm->GetAbsX();
						INT32 right = left + bestElm->GetWidth();
						if (!SnUtil::Intersect(left, right, refPoint, refPoint + 1))
						{
							INT32 newLeft = frmDocElm->GetAbsX();
							INT32 newRight = newLeft + frmDocElm->GetWidth();
							if (SnUtil::Intersect(newLeft, newRight, refPoint, refPoint + 1))
							{
								bestElm = frmDocElm;
							}
							else
							{
								INT32 oldDist = MIN(op_abs(left-refPoint), op_abs(right-refPoint));
								INT32 newDist = MIN(op_abs(newLeft-refPoint), op_abs(newRight-refPoint));
								if (newDist < oldDist)
								{
									bestElm = frmDocElm;
								}
							}
						}
					}
				}
				break;

			default:
				OP_ASSERT(0);
				break;
			}
			frmDocElm = (FramesDocElm*)frmDocElm->Next();
		}
	}

	if (checkVisibility && bestElm)
	{
		OpRect visible_area(currentDoc->GetVisualViewport());
		OpRect next_frame_area(bestElm->GetAbsX(), (INT32)bestElm->GetAbsY(), bestElm->GetWidth(), bestElm->GetHeight());

		// The next frame is not in view, clear it
		if (!visible_area.Intersecting(next_frame_area))
			bestElm	= NULL;
	}

	return bestElm;
}


BOOL SnHandler::MarkNext(INT32 direction, POINT* startingPoint, BOOL scroll, INT32 scrollValue)
{
	if (!InitNavigation(direction, scroll, scrollValue))
		return FALSE;

#ifdef _PLUGIN_NAVIGATION_SUPPORT_
	// If we've already navigated to a plugin earlier, see if it wants to keep
	// navigating inside the plugin
	if (nav_filter->AcceptsPlugins() && NavigateInsidePlugin(direction))
		return TRUE;
#endif // _PLUGIN_NAVIGATION_SUPPORT_

#ifdef MEDIA_SUPPORT
	if (NavigateInsideMedia(direction, FALSE))
		return TRUE;
#endif // MEDIA_SUPPORT

	if (!GetNextLink(direction, startingPoint, scroll))
		return FALSE;

	SetActiveLinkAndHighlight();

#ifdef MEDIA_SUPPORT
	if (NavigateInsideMedia(direction, TRUE))
		return TRUE;
#endif // MEDIA_SUPPORT

#ifdef _PLUGIN_NAVIGATION_SUPPORT_
	// If selected element is a plugin, then let it handle internal navigation
	if (nav_filter->AcceptsPlugins())
		NavigateInsidePlugin(direction);
#endif // _PLUGIN_NAVIGATION_SUPPORT_

	return TRUE;
}

URL SnHandler::GetImageURL()
{
	if (HTML_Element* activeLink = GetActiveLink())
	{
		URL tmpUrl = activeLink->GetImageURL();
		return tmpUrl;
	}
	return URL();
}

const uni_char* SnHandler::GetA_HRef()
{
	HTML_Element* activeLink = GetActiveLink();
	if (activeLink && activeFrameDoc)
	{
		return activeLink->GetA_HRef(activeFrameDoc);
	}
	return NULL;
}

void SnHandler::OnScroll()
{
}

void SnHandler::OnLoadingFinished()
{
	if (!g_pcdoc->GetIntegerPref(PrefsCollectionDoc::EnableSpatialNavigation, window->GetCurrentURL()))
		return;

	DocListElm* currElm = window->DocManager()->CurrentDocListElm();
	if (highlightOnLoad && currElm)
	{
		if (!currElm->Doc()->GetCurrentHTMLElement())  // No element is selected
		{
			initial_highlight = TRUE;
			MarkNextLinkInDirection(DIR_DOWN, NULL, FALSE);
			initial_highlight = FALSE;
		}
	}
}

void SnHandler::SetLowMemoryMode(BOOL lowMemory)
{
	if (lowMemory != lowMemoryMode)
		lowMemoryMode = lowMemory;
}

BOOL SnHandler::OnInputAction(OpInputAction* action)
{
	if (!g_pcdoc->GetIntegerPref(PrefsCollectionDoc::EnableSpatialNavigation, window->GetCurrentURL()))
		return FALSE;

#ifdef _PRINT_SUPPORT_
	// Don't do stuff in print preview. It's a static display of
	// the document, not something to put highlight in.
	if (FramesDocument* doc = window->GetActiveFrameDoc())
	{
		if (doc->IsPrintDocument())
			return FALSE;
	}
#endif // _PRINT_SUPPORT_
	INT32 direction = 0;

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_NAVIGATE_RIGHT:
			direction = DIR_RIGHT;
			break;

		case OpInputAction::ACTION_NAVIGATE_UP:
			direction = DIR_UP;
			break;

		case OpInputAction::ACTION_NAVIGATE_LEFT:
			direction = DIR_LEFT;
			break;

		case OpInputAction::ACTION_NAVIGATE_DOWN:
			direction = DIR_DOWN;
			break;

		default:
			return FALSE;
	}

	BOOL navigated = MarkNextItemInDirection(direction);

#ifdef SN_LEAVE_SUPPORT
	if (!navigated)
		window->LeaveInDirection(direction);
#endif // SN_LEAVE_SUPPORT

	return navigated;
}

BOOL SnHandler::MarkNextHeadingInDirection(INT32 direction, POINT* startingPoint, BOOL scroll)
{
	if (!g_pcdoc->GetIntegerPref(PrefsCollectionDoc::EnableSpatialNavigation, window->GetCurrentURL()))
		return FALSE;

	if (!heading_nav_filter)
		heading_nav_filter = OP_NEW(SnHeadingNavFilter, ());

	if (!heading_nav_filter)
		return FALSE;

	SetNavigationFilter(heading_nav_filter, FALSE, FALSE);
	return MarkNext(direction, startingPoint, scroll);
}

BOOL SnHandler::MarkNextParagraphInDirection(INT32 direction, POINT* startingPoint, BOOL scroll)
{
	if (!g_pcdoc->GetIntegerPref(PrefsCollectionDoc::EnableSpatialNavigation, window->GetCurrentURL()))
		return FALSE;

	if (!paragraph_nav_filter)
		paragraph_nav_filter = OP_NEW(SnParagraphNavFilter, ());

	if (!paragraph_nav_filter)
		return FALSE;

	SetNavigationFilter(paragraph_nav_filter, FALSE, FALSE);
	return MarkNext(direction, startingPoint, scroll);
}

BOOL SnHandler::MarkNextNonLinkInDirection(INT32 direction, POINT* startingPoint, BOOL scroll)
{
	if (!g_pcdoc->GetIntegerPref(PrefsCollectionDoc::EnableSpatialNavigation, window->GetCurrentURL()))
		return FALSE;

	if (!nonlink_nav_filter)
		nonlink_nav_filter = OP_NEW(SnNonLinkNavFilter, ());

	if (!nonlink_nav_filter)
		return FALSE;

	SetNavigationFilter(nonlink_nav_filter, FALSE, FALSE);
	return MarkNext(direction, startingPoint, scroll);
}

BOOL SnHandler::MarkNextFormInDirection(INT32 direction, POINT* startingPoint, BOOL scroll)
{
	if (!g_pcdoc->GetIntegerPref(PrefsCollectionDoc::EnableSpatialNavigation, window->GetCurrentURL()))
		return FALSE;

	if (!form_nav_filter)
		form_nav_filter = OP_NEW(SnFormNavFilter, ());

	if (!form_nav_filter)
		return FALSE;

	SetNavigationFilter(form_nav_filter, FALSE, FALSE);
	return MarkNext(direction, startingPoint, scroll);
}

BOOL SnHandler::MarkNextATagInDirection(INT32 direction, POINT* startingPoint, BOOL scroll)
{
	if (!g_pcdoc->GetIntegerPref(PrefsCollectionDoc::EnableSpatialNavigation, window->GetCurrentURL()))
		return FALSE;

	if (!atag_nav_filter)
		atag_nav_filter = OP_NEW(SnATagNavFilter, ());

	if (!atag_nav_filter)
		return FALSE;

	SetNavigationFilter(atag_nav_filter, FALSE, FALSE);
	return MarkNext(direction, startingPoint, scroll);
}

BOOL SnHandler::MarkNextLinkInDirection(INT32 direction, POINT* startingPoint, BOOL scroll)
{
	if (!g_pcdoc->GetIntegerPref(PrefsCollectionDoc::EnableSpatialNavigation, window->GetCurrentURL()))
		return FALSE;

	SetNavigationFilter(default_nav_filter, FALSE, FALSE);
	BOOL hadActiveLink = GetActiveLink() ? TRUE : FALSE;
	BOOL ret = MarkNext(direction, startingPoint, scroll);
	if (hadActiveLink && !GetActiveLink())
		window->GetWindowCommander()->GetDocumentListener()->OnNoNavigation(window->GetWindowCommander());
	return ret;
}

BOOL SnHandler::MarkNextImageInDirection(INT32 direction, POINT* startingPoint, BOOL scroll)
{
	if (!g_pcdoc->GetIntegerPref(PrefsCollectionDoc::EnableSpatialNavigation, window->GetCurrentURL()))
		return FALSE;

	if (!image_nav_filter)
		image_nav_filter = OP_NEW(SnImageNavFilter, ());

	if (!image_nav_filter)
		return FALSE;

	SetNavigationFilter(image_nav_filter, FALSE, FALSE);
	return MarkNext(direction, startingPoint, scroll);
}

BOOL SnHandler::MarkNextItemInDirection(INT32 direction, POINT* startingPoint, BOOL scroll, INT32 scrollValue)
{
	if (!g_pcdoc->GetIntegerPref(PrefsCollectionDoc::EnableSpatialNavigation, window->GetCurrentURL()))
		return FALSE;

	BOOL hadActiveLink = GetActiveLink() ? TRUE : FALSE;
	BOOL ret = MarkNext(direction, startingPoint, scroll, scrollValue);
	if (hadActiveLink && !GetActiveLink())
		window->GetWindowCommander()->GetDocumentListener()->OnNoNavigation(window->GetWindowCommander());

	return ret;
}

OpSnNavFilter*
SnHandler::GetFilter(NavigationMode mode)
{
	switch (mode)
	{
	case NavigateDefault:
		return default_nav_filter;

	case NavigateHeaders:
		if (!heading_nav_filter)
			heading_nav_filter = OP_NEW(SnHeadingNavFilter, ());
		return heading_nav_filter;

	case NavigateParagraphs:
		if (!paragraph_nav_filter)
			paragraph_nav_filter = OP_NEW(SnParagraphNavFilter, ());
		return paragraph_nav_filter;

	case NavigateForms:
		if (!form_nav_filter)
			form_nav_filter = OP_NEW(SnFormNavFilter, ());
		return form_nav_filter;

	case NavigateLinks:
		if (!atag_nav_filter)
			atag_nav_filter = OP_NEW(SnATagNavFilter, ());
		return atag_nav_filter;

	case NavigateImages:
		if (!image_nav_filter)
			image_nav_filter = OP_NEW(SnImageNavFilter, ());
		return image_nav_filter;

	case NavigateNonLinks:
		if (!nonlink_nav_filter)
			nonlink_nav_filter = OP_NEW(SnNonLinkNavFilter, ());
		return nonlink_nav_filter;

	default:
		OP_ASSERT(FALSE);
		return NULL;
	}
}

void
SnHandler::SetNavigationMode(int mode)
{
	switch (mode)
	{
	case NavigateDefault:
		SetNavigationFilter(NULL, FALSE, FALSE);
		break;

	case NavigateHeaders:
	case NavigateParagraphs:
	case NavigateForms:
	case NavigateLinks:
	case NavigateImages:
	case NavigateNonLinks:
		{
			OpSnNavFilter* filter = GetFilter(static_cast<NavigationMode>(mode));
			if (!filter)
				return;
			SetNavigationFilter(filter, FALSE, FALSE);
			break;
		}

	default:
		{
			if (!configurable_nav_filter)
				configurable_nav_filter = OP_NEW(SnConfigurableNavFilter, ());

			if (!configurable_nav_filter)
				return;

			OpSnNavFilter* filters[OpSnNavFilter::MAX_NUMBERS_OF_FILTERS+1];
			int i = 0;
			for (int j = 0; j<OpSnNavFilter::MAX_NUMBERS_OF_FILTERS; j++)
			{
				NavigationMode m = static_cast<NavigationMode>((1<<j) & mode);
				if (m)
				{
					filters[i] = GetFilter(m);
					if (!filters[i])
						return;
					i++;
				}
			}
			filters[i] = NULL;
			static_cast<SnConfigurableNavFilter*>(configurable_nav_filter)->SetFilterArray(filters);

			SetNavigationFilter(configurable_nav_filter, FALSE, FALSE);
			break;
		}
	}

	nav_mode = mode;
}

void
SnHandler::SetNavigationFilter(OpSnNavFilter *nav_filter, BOOL change_focus, BOOL scroll)
{
	if (nav_filter)
		this->nav_filter = nav_filter;
	else
		this->nav_filter = default_nav_filter;
}

#ifdef SELECT_TO_FOCUS_INPUT
/**
 * Only some stuff (see below) should be focused, the rest should be
 * only highlighted by us.
 * All elements should get a frame around them (InvertFormBorderRect).
 */
BOOL SnHandler::FocusElement(HTML_Document* doc, HTML_Element* helm)
{
	BOOL normal = FALSE;

	if (!helm)
		return FALSE;

	// Determine if normal focusing is to be done (through doc->FocusElement).
	switch (helm->Type())
	{
		case HE_INPUT:
#ifdef _SSL_SUPPORT_
		case HE_KEYGEN:
#endif
		case HE_SELECT:
		case HE_TEXTAREA:
		{
			currentForm = helm;
			this->currentFormDoc = doc;

			FormObject* form_obj = helm->GetFormObject();
			if (form_obj)
			{
				switch (form_obj->GetInputType())
				{
					case INPUT_CHECKBOX:
					case INPUT_SUBMIT:
					case INPUT_RESET:
					case INPUT_RADIO:
					case INPUT_FILE:
					case INPUT_BUTTON:
					case INPUT_DATE:
					case INPUT_WEEK:
					case INPUT_MONTH:
					case INPUT_DATETIME:
					case INPUT_DATETIME_LOCAL:
						/* normal for the above */
						normal = TRUE;
						break;
					default:
						break;
				}
			}
		}
		break;
		case HE_BUTTON:
			normal = TRUE;
			break;
		default:
			break;
	}

	if (helm->GetInputType() == INPUT_IMAGE)
	{
		doc->GetVisualDevice()->SetDrawFocusRect(TRUE);
		normal = TRUE;
	}

	if (normal)
	{
		Doc_FocusElement(doc, helm);
	}
	else
	{
		/* focus by our own, but do not activate element */
		doc->ClearFocusAndHighlight();
		doc->SetNavigationElement(helm, FALSE);
	}

	/* draw highlight rect. Needed also after focusing. */
	doc->InvertFormBorderRect(helm);

    // Apply pre-focus
    if (!normal)
    {
        helm->SetIsPreFocused(TRUE);
        if (FramesDocument* frm_doc = doc->GetFramesDocument())
        {
    	    if (LayoutWorkplace* workplace = frm_doc->GetLogicalDocument() ? frm_doc->GetLogicalDocument()->GetLayoutWorkplace() : NULL)
	    	    RAISE_AND_RETURN_VALUE_IF_ERROR(workplace->ApplyPropertyChanges(helm, CSS_PSEUDO_CLASS__O_PREFOCUS, TRUE), FALSE);
		}
    }

	return TRUE;
}

#endif // SELECT_TO_FOCUS_INPUT


// Call doc->FocusElement(helm)
BOOL SnHandler::Doc_FocusElement(HTML_Document* doc, HTML_Element* helm)
{
	if (doc && helm)
		return doc->FocusElement(helm, HTML_Document::FOCUS_ORIGIN_SPATIAL_NAVIGATION, TRUE, allowDocScrolling);

	return FALSE;
}

// Call doc->HighlightElement(helm)
BOOL SnHandler::Doc_HighlightElement(HTML_Document* doc, HTML_Element* helm, BOOL send_event, BOOL focusFormElm, BOOL scroll)
{
	if (doc && helm)
		return doc->HighlightElement(helm, HTML_Document::FOCUS_ORIGIN_SPATIAL_NAVIGATION, focusFormElm, scroll);
	return FALSE;
}

// Return TRUE if something got focus.
BOOL SnHandler::FocusCurrentForm()
{
#ifdef SELECT_TO_FOCUS_INPUT
	FramesDocument* current_doc = window->DocManager()->GetCurrentVisibleDoc();

	if (!current_doc)
		return FALSE;

	if (currentForm != GetActiveLink())
	{
	    currentForm = NULL;
	    return FALSE;
	}
	if (currentForm)
	{
		FormObject* form_obj = currentForm->GetFormObject();
		if (form_obj && form_obj != currentFormDoc->GetFramesDocument()->current_focused_formobject)
		{
			switch (form_obj->GetInputType())
			{
			case INPUT_CHECKBOX:
			case INPUT_BUTTON:
			case INPUT_SUBMIT:
			case INPUT_RESET:
			case INPUT_RADIO:
			case INPUT_FILE:
				return FALSE;
			default:
				currentFormDoc->GetVisualDevice()->SetDrawFocusRect(TRUE);
				Doc_FocusElement(currentFormDoc, currentForm);
				currentFormDoc->SetNavigationElement(currentForm, FALSE);
				g_input_manager->InvokeAction(OpInputAction::ACTION_SHOW_DROPDOWN);
		        currentForm->SetIsPreFocused(FALSE);
	            if (LayoutWorkplace* workplace = activeFrameDoc->GetLogicalDocument() ? activeFrameDoc->GetLogicalDocument()->GetLayoutWorkplace() : NULL)
		    	    RAISE_AND_RETURN_VALUE_IF_ERROR(workplace->ApplyPropertyChanges(currentForm, CSS_PSEUDO_CLASS__O_PREFOCUS, TRUE), FALSE);

				return TRUE;
			}
		}
	}
#endif
	return FALSE;

}

BOOL SnHandler::UnfocusCurrentForm()
{
#ifdef SELECT_TO_FOCUS_INPUT
	FramesDocument* current_doc = window->DocManager()->GetCurrentVisibleDoc();

	if (!current_doc)
		return FALSE;

	UpdateNavigationData();

	HTML_Element* helm = GetActiveLink();
	if (helm)
	{
		FormObject* form_obj = helm->GetFormObject();
		if (form_obj)
		{
			switch (form_obj->GetInputType())
			{
			case INPUT_CHECKBOX:
			case INPUT_BUTTON:
			case INPUT_SUBMIT:
			case INPUT_RESET:
			case INPUT_RADIO:
			case INPUT_FILE:
				break;
			default:
				HighlightCurrentElement(TRUE, FALSE);

		        helm->SetIsPreFocused(TRUE);
	    	    if (LayoutWorkplace* workplace = activeFrameDoc->GetLogicalDocument() ? activeFrameDoc->GetLogicalDocument()->GetLayoutWorkplace() : NULL)
		    	    RAISE_AND_RETURN_VALUE_IF_ERROR(workplace->ApplyPropertyChanges(helm, CSS_PSEUDO_CLASS__O_PREFOCUS, TRUE), FALSE);

				return TRUE;
			}
		}
	}
#endif
	return FALSE;
}

BOOL SnHandler::MarkNextItemInActiveFrame(INT32 direction, POINT* startingPoint)
{
	BOOL ret = MarkNextItemInDirection(direction, startingPoint, FALSE);
	return ret;
}

#endif // _SPAT_NAV_SUPPORT_
