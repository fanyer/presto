/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2001-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/
#include "core/pch.h"

#include "modules/display/VisDevListeners.h"
#include "modules/display/coreview/coreview.h"
#include "modules/layout/content/content.h"
#include "modules/layout/layout_workplace.h"

#ifdef DOCUMENT_EDIT_SUPPORT
#include "modules/documentedit/OpDocumentEdit.h"
#endif // DOCUMENT_EDIT_SUPPORT

#include "modules/logdoc/src/textdata.h"

#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/cpuusagetracker/cpuusagetrackeractivator.h"

#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"

# include "modules/util/winutils.h"
# include "modules/util/tempbuf.h"
# include "modules/dochand/docman.h"
# include "modules/doc/html_doc.h"
# include "modules/doc/frm_doc.h"
# include "modules/logdoc/htm_elm.h"

#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"

#include "modules/dochand/viewportcontroller.h"

#include "modules/forms/piforms.h"
#include "modules/widgets/OpScrollbar.h"
#include "modules/widgets/OpDropDown.h"

#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/windowcommander/src/WindowCommander.h"
#if defined (WIC_USE_DOWNLOAD_CALLBACK)
# include "modules/viewers/viewers.h"
# include "modules/windowcommander/src/TransferManagerDownload.h"
#endif // WIC_USE_DOWNLOAD_CALLBACK

#ifdef MSWIN
# include "platforms/windows/windows_ui/docwin_mousewheel.h"
#elif defined(_X11_)
# include "platforms/unix/product/x11quick/panningmanager.h"
#else // MSWIN
	BOOL IsPanning() { return FALSE; }
#endif // MSWIN

#ifdef _X11_SELECTION_POLICY_
# define NO_PANNING_ON_FORMS
# include "modules/pi/OpClipboard.h"
#endif // _X11_SELECTION_POLICY_

#include "modules/inputmanager/inputmanager.h"
#include "modules/dochand/fdelm.h"

#if defined(QUICK)
BOOL g_dont_show_popup = FALSE;
#include "modules/inputmanager/inputaction.h"
#include "adjunct/quick/Application.h" // Hotclick menu action
#include "adjunct/quick_toolkit/widgets/OpBrowserView.h" // Hotclick menu action
#include "adjunct/quick/managers/KioskManager.h"
#endif //QUICK

#ifdef _WML_SUPPORT_
# include "modules/logdoc/wml.h"
#endif // _WML_SUPPORT_

#ifdef SVG_SUPPORT
# include "modules/svg/SVGManager.h"
# include "modules/svg/svg_image.h"
#endif // SVG_SUPPORT

#include "modules/security_manager/include/security_manager.h"

#ifdef DRAG_SUPPORT
# include "modules/dragdrop/dragdrop_manager.h"
class OpDragObject;
#endif // DRAG_SUPPORT

BOOL ScrollDocument(FramesDocument* doc, OpInputAction::Action action, int times = 1, OpInputAction::ActionMethod method = OpInputAction::METHOD_OTHER);

#ifdef DISPLAY_CLICKINFO_SUPPORT
static void GetTitleFromHTMLElement( OpString& title, HTML_Element& element )
{
	HTML_Element* a_tag = element.GetA_Tag();
	if( a_tag )
	{
		if (a_tag->GetStringAttr(ATTR_TITLE))
		{
			title.Set(a_tag->GetStringAttr(ATTR_TITLE));//FIXME: OOM
		}
		else if (element.Type() == HE_TEXT)
		{
			TempBuffer tmp_buf;
			TRAPD(err,a_tag->AppendEntireContentAsStringL(&tmp_buf, TRUE, FALSE));
			title.Set( tmp_buf.GetStorage() );
			if( title.IsEmpty() )
				title.Set(element.GetTextData()->GetText());//FIXME: OOM
		}

		// Strip newlines and spaces (bug #150805)

		// In front and at end
		title.Strip(TRUE,TRUE);

		// Remove multiple spaces and newlines
		int length = title.Length();
		for (int i=0; i<length; i++)
		{
			if (uni_isspace(title.CStr()[i]))
			{
				if( title.CStr()[i] == '\n' || title.CStr()[i] == '\r' )
				{
					// The text we collect here can end up in a single-line edit field
					// so we must convert to a space (bug #152469)
					title.CStr()[i] = ' ';
				}

				int j;
				for (j=i+1; j<length && uni_isspace(title.CStr()[j]); j++) {}
				if (j > i+1)
				{
					OpString tmp;
					tmp.Set(title.CStr(),i+1);//FIXME: OOM
					tmp.Append(title.SubString(j));//FIXME: OOM
					title.Set(tmp);//FIXME: OOM
					length = title.Length();
				}

			}
		}
	}
}
#endif // DISPLAY_CLICKINFO_SUPPORT

#if !defined(MOUSELESS) && defined(PAN_SUPPORT)
/** Get current mouse position, relative to the visual viewport of the top-level VisualDevice. */
static OpPoint GetTopLevelMousePos(Window* window)
{
	VisualDevice* vis_dev = window->VisualDev();
	OpPoint point;

	OP_ASSERT(window->VisualDev() && window->VisualDev()->GetView());

	if (vis_dev && vis_dev->GetView())
	{
		vis_dev->GetView()->GetMousePos(&point.x, &point.y);
		point -= vis_dev->ScaleToScreen(
			window->GetViewportController()->GetVisualViewport().TopLeft() -
			window->GetViewportController()->GetRenderingViewport().TopLeft());
	}

	return point;
}
#endif // !MOUSELESS && PAN_SUPPORT

static OP_STATUS ReflowOnDemand(FramesDocument* doc, BOOL allow_yield)
{
	LayoutWorkplace* wp = doc && doc->GetLogicalDocument() ? doc->GetLogicalDocument()->GetLayoutWorkplace() : NULL;

	if (!wp || doc->IsPrintDocument())
		return OpStatus::OK;

	OP_STATUS status = OpStatus::OK;
#ifdef LAYOUT_YIELD_REFLOW
	if (allow_yield)
		wp->SetCanYield(TRUE);
#endif // LAYOUT_YIELD_REFLOW

	if (HTML_Element* root = doc->GetDocRoot())
		if (root->IsDirty() || wp->HasDirtyContentSizedIFrameChildren() || root->HasDirtyChildProps() || root->IsPropsDirty())
		{
			OpStatus::Ignore(status);
			status = doc->Reflow(FALSE);
		}

#ifdef LAYOUT_YIELD_REFLOW
	if (allow_yield)
		wp->SetCanYield(FALSE);
#endif // LAYOUT_YIELD_REFLOW

	return status;
}

#ifdef DISPLAY_CLICKINFO_SUPPORT

MouseListenerClickInfo::MouseListenerClickInfo()
{
	Reset();
}

void MouseListenerClickInfo::Reset()
{
	m_document = 0;
	m_image_element = 0;
	m_link_element = 0;
}

void MouseListenerClickInfo::SetLinkElement( FramesDocument* document, HTML_Element* link_element )
{
	if( !m_link_element )
	{
		m_document = document;
		m_link_element = link_element;
	}
}

void MouseListenerClickInfo::SetTitleElement( FramesDocument* document, HTML_Element* element )
{
	OpString title;
	do
	{
		Reset();
		SetLinkElement(document,element);
		GetLinkTitle(title);
		element = element ? element->FirstChild() : 0;
	}
	while( element && title.IsEmpty() );
}

void MouseListenerClickInfo::SetImageElement( HTML_Element* image_element )
{
	if( !m_image_element )
	{
		m_image_element = image_element;
	}
}

void MouseListenerClickInfo::GetLinkTitle( OpString &title )
{
	if ( m_link_element && m_document )
	{
		GetTitleFromHTMLElement( title, *m_link_element );
	}
}

#endif // DISPLAY_CLICKINFO_SUPPORT


static Box* GetInnerBox(VisualDevice* vis_dev, Window* window, FramesDocument *doc, const OpPoint& point)
{
	if ( !window || !doc )
	{
		return NULL;
	}

	LogicalDocument* logdoc = doc->GetLogicalDocument();
	if (logdoc && logdoc->GetRoot() && vis_dev)
	{
		INT32 x = vis_dev->ScaleToDoc(point.x);
		INT32 y = vis_dev->ScaleToDoc(point.y);
		x += doc->GetVisualDevice()->GetRenderingViewX();
		y += doc->GetVisualDevice()->GetRenderingViewY();
		return logdoc->GetRoot()->GetInnerBox(x, y, doc);
	}
	return NULL;
}

FormObject* GetFormElement( VisualDevice* vis_dev, Window* window, FramesDocument *doc, const OpPoint& point )
{
	Box* inner_box = GetInnerBox(vis_dev, window, doc, point);

	if (inner_box)
	{
		HTML_Element* h_elm = inner_box->GetHtmlElement();
		if (h_elm)
			return h_elm->GetFormObject();
	}
	return NULL;
}

#if defined(_X11_SELECTION_POLICY_) && defined(DOCUMENT_EDIT_SUPPORT)
static OpDocumentEdit* GetDocumentEditElement( VisualDevice* vis_dev, Window* window, FramesDocument *doc, const OpPoint& point )
{
	OpDocumentEdit* edit = 0;
	if (doc)
	{
		if (doc->GetDesignMode())
			return doc->GetDocumentEdit();

		Box* inner_box = NULL;
		LogicalDocument* logdoc = doc->GetLogicalDocument();
		if (logdoc && logdoc->GetRoot() && logdoc->GetRoot()->GetLayoutBox())
		{
			INT32 x = vis_dev->ScaleToDoc(point.x) + doc->GetVisualDevice()->GetRenderingViewX();
			INT32 y = vis_dev->ScaleToDoc(point.y) + doc->GetVisualDevice()->GetRenderingViewY();
			IntersectionObject intersection(doc, LayoutCoord(x), LayoutCoord(y), TRUE);
			intersection.Traverse(logdoc->GetRoot());
			inner_box = intersection.GetInnerBox();

			if (doc->GetDocumentEdit() && inner_box &&
				doc->GetDocumentEdit()->GetEditableContainer(inner_box->GetHtmlElement()))
			{
				edit = doc->GetDocumentEdit();
			}
		}
	}

	return edit;
}
#endif


#ifdef DRAG_SUPPORT

OpWidget* GetWidget( VisualDevice* vis_dev, const OpPoint& point )
{
	DocumentManager* doc_man = vis_dev->GetDocumentManager();

	if (doc_man)
	{
		Window* window = doc_man->GetWindow();
		FramesDocument* doc = doc_man->GetCurrentVisibleDoc();
		FormObject* form_object = GetFormElement(vis_dev, window, doc, point);

		if (form_object)
		{
			return form_object->GetWidget();
		}
	}
	return NULL;
}

#endif // DRAG_SUPPORT

// ======================================================
// PaintListener
// ======================================================


PaintListener::PaintListener(VisualDevice* vis_dev) :
	vis_dev(vis_dev)
{
}

BOOL PaintListener::BeforePaint()
{
	BOOL ready_for_paint = TRUE;

	DocumentManager* doc_man = vis_dev->GetDocumentManager();
	TRACK_CPU_PER_TAB(doc_man ? doc_man->GetWindow()->GetCPUUsageTracker() : NULL);
	if (doc_man)
	{
	FramesDocument* doc = NULL;
#ifdef _PRINT_SUPPORT_
		doc = doc_man->GetPrintDoc();
		if (!doc)
#endif // _PRINT_SUPPORT_
			doc = doc_man->GetCurrentVisibleDoc();

		OP_STATUS result = ReflowOnDemand(doc, TRUE);

		if (OpStatus::IsMemoryError(result))
		{
			doc_man->GetWindow()->RaiseCondition(result);
			return FALSE;
		}
#ifdef LAYOUT_YIELD_REFLOW
		ready_for_paint = result != OpStatus::ERR_YIELD;
#endif // LAYOUT_YIELD_REFLOW
	}

	// We must cache pending_paint_rect because the system might call several OnPaint's for this paint, and we must also empty pending_paint_rect in OnPaint.
	vis_dev->pending_paint_rect_onbeforepaint = vis_dev->pending_paint_rect;

	if (doc_man)
	{
		vis_dev->SyncDelayedUpdates();
	}

	return ready_for_paint;
}

void PaintListener::OnPaint(const OpRect &rect, OpPainter* painter, CoreView* view, int translate_x, int translate_y)
{
	OP_NEW_DBG("PaintListener::OnPaint()", "printing");
	TRACK_CPU_PER_TAB(vis_dev->GetDocumentManager() ? vis_dev->GetDocumentManager()->GetWindow()->GetCPUUsageTracker() : NULL);
	vis_dev->OnPaint(rect, painter);
}

void PaintListener::OnMoved()
{
	vis_dev->OnMoved();
}
// ======================================================
// Methods shared between TouchListener and MouseListener
// ======================================================

#ifndef MOUSELESS
static void UpdateOverlappedStatus(CoreView* view, VisualDevice* vis_dev)
{
	DocumentManager* doc_man = vis_dev->GetDocumentManager();
	FramesDocument* doc = doc_man ? doc_man->GetCurrentVisibleDoc() : NULL;
	if (doc)
		doc->GetVisualDevice()->CheckOverlapped();
}

static CoreView* GetPointHitView(const OpPoint &point, CoreView* view, VisualDevice* vis_dev)
{
	//OP_ASSERT(OpRect(0, 0, view->GetRect().width, view->GetRect().height).Contains(point));

	DocumentManager* doc_man = vis_dev->GetDocumentManager();
	Window* window = doc_man->GetWindow();
	FramesDocument* doc = doc_man->GetCurrentVisibleDoc();
	if (!doc)
		return NULL;

	if (doc->GetIFrmRoot())
	{
		/* There are IFRAME children. Need to reflow this document now while
		   it's safe then, since reflowing may delete the IFRAMEs (if they were
		   inserted by layout and are no longer valid, for example). */

		OP_STATUS result = ReflowOnDemand(doc, FALSE);

		if (OpStatus::IsMemoryError(result))
		{
			doc_man->RaiseCondition(result);
			return NULL;
		}
	}

	CoreView* candidate = view->GetIntersectingChild(point.x, point.y);
	if (!candidate)
		return NULL;

	if (doc->GetFrmDocRoot())
		return candidate;

	candidate->UpdateOverlappedStatusRecursive();
	if (!candidate->GetIsOverlapped())
		return candidate;

	Box* box = GetInnerBox(doc->GetVisualDevice(), window, doc, point);

#ifdef DOCUMENT_EDIT_SUPPORT
	if (doc->GetDocumentEdit() && box && doc->GetDocumentEdit()->GetEditableContainer(box->GetHtmlElement()))
		return NULL;
#endif

	if (box && box->GetContent())
	{
		VisualDevice* vd = NULL;

		if (box->GetContent()->IsIFrame())
		{
			IFrameContent* iframe = (IFrameContent*) box->GetContent();
			if (FramesDocElm* fdelm = iframe->GetFramesDocElm())
				vd = fdelm->GetVisualDevice();
		}
#ifdef SVG_SUPPORT
		else if (box->GetContent()->GetSVGContent())
		{
			HTML_Element* event_target = NULL;
			int offset_x, offset_y;

			g_svg_manager->FindSVGElement(box->GetHtmlElement(), doc, point.x, point.y,
										  &event_target, offset_x, offset_y);
			if (event_target && g_svg_manager->AllowInteractionWithElement(event_target))
			{
				// Find the visual device for this pseudo-iframe
				if (FramesDocElm* ifrmroot = doc->GetIFrmRoot())
					if (FramesDocElm* root = ifrmroot->GetFrmDocElmByHTML(event_target))
						vd = root->GetVisualDevice();
			}
		}
#endif // SVG_SUPPORT

		if (vd)
			return vd->GetContainerView();
	}

	return NULL;
}

// ======================================================
// MouseListener
// ======================================================

static void DocMouseActivate(DocumentManager* doc_man, BOOL setfocus)
{
	if(doc_man)
	{
		FramesDocument* currentDoc = doc_man->GetCurrentVisibleDoc();

		if (setfocus && doc_man->GetWindow()->GetState() != BUSY && currentDoc)
		{
			currentDoc->SetFocus(FOCUS_REASON_MOUSE);
		}
	}
}
//------------------------------------------------------------

void MouseListener::UpdateOverlappedStatus(CoreView* view)
{
	::UpdateOverlappedStatus(view, vis_dev);
}

CoreView* MouseListener::GetMouseHitView(const OpPoint &point, CoreView* view)
{
	return GetPointHitView(point, view, vis_dev);
}

//------------------------------------------------------------
#ifdef DRAG_SUPPORT
void MouseListener::StopDragging()
{
#ifdef GADGET_SUPPORT
	if(is_dragging == YES && vis_dev->GetWindow()->GetType() == WIN_TYPE_GADGET)
	{
		WindowCommander* commander = vis_dev->GetWindow()->GetWindowCommander();
		OpDocumentListener* listener = commander->GetDocumentListener();

		listener->OnGadgetDragStop(commander);
	}
#endif // GADGET_SUPPORT

	is_dragging = NO;
}
#endif // DRAG_SUPPORT


//------------------------------------------------------------
void MouseListener::OnMouseLeftDown(CoreView* view, Window* window, DocumentManager* doc_man, FramesDocument* doc, UINT8 nclicks, ShiftKeyState keystate)
{
	left_key_is_down = TRUE;
	if (right_key_is_down)
		left_while_right_down = TRUE;

	last_click_was_double = nclicks != 2;
    if (window->GetState() != BUSY)
    {
#ifdef PAN_SUPPORT
		// don't pan on double click, make sure we've got pan state NO instead,
		// since some code (notably widgets) checks this state from time to time.
		// if mouse down started panning, the mouse click is consumed.
#ifdef DO_NOT_PAN_PLUGINS
		HTML_Document* htmldoc = doc ? doc->GetHtmlDocument() : NULL;
		HTML_Element* helm = htmldoc ? htmldoc->GetHoverHTMLElement() : NULL;
		if (!helm || !helm->GetLayoutBox() || !helm->GetLayoutBox()->IsContentEmbed())
#endif // DO_NOT_PAN_PLUGINS
			if (nclicks == 1)
				vis_dev->PanMouseDown(GetTopLevelMousePos(window), keystate);
#endif // PAN_SUPPORT

		if (doc)
		{

#ifdef DRAG_SUPPORT
			is_dragging = MAYBE;

			if (in_editable_element)
				is_dragging = NO;

#endif // DRAG_SUPPORT

			PropagateMouseEvent(view, window, doc, MOUSE_BUTTON_1, nclicks, keystate);
        }
    }

#ifdef DRAG_SUPPORT
	dragTimer->Start(1000);
#endif // DRAG_SUPPORT
}

/** This is used to store the nclick (clicknumber) for the last mousedown, used in mouseup. */

static void SetLastNumberOfClicksMouseDown(MouseButton button, UINT8 count)
{
	switch (button)
	{
	case MOUSE_BUTTON_1:
		g_opera->display_module.last_mouse_down_count[0] = count;
		break;
	case MOUSE_BUTTON_2:
		g_opera->display_module.last_mouse_down_count[1] = count;
		break;
	case MOUSE_BUTTON_3:
		g_opera->display_module.last_mouse_down_count[2] = count;
		break;
	}
}

/** This is used to store the nclick (clicknumber) for the last mousedown, used in mouseup. */

static UINT8 GetLastNumberOfClicksMouseDown(MouseButton button)
{
	switch (button)
	{
	case MOUSE_BUTTON_1:
		return g_opera->display_module.last_mouse_down_count[0];
	case MOUSE_BUTTON_2:
		return g_opera->display_module.last_mouse_down_count[1];
	case MOUSE_BUTTON_3:
		return g_opera->display_module.last_mouse_down_count[2];
	}
	return 1;
}

//------------------------------------------------------------
void MouseListener::OnMouseLeftUp (CoreView* view, Window* window, DocumentManager* doc_man, FramesDocument* document, UINT8 nclicks, ShiftKeyState keystate)
{
	left_key_is_down = FALSE;

#ifdef PAN_SUPPORT
# ifdef ACTION_COMPOSITE_PAN_STOP_ENABLED
	if (vis_dev->GetPanState() == YES)
	{
		FramesDocument* doc = vis_dev->GetDocumentManager() ? vis_dev->GetDocumentManager()->GetCurrentDoc() : 0;

		if (doc && doc->GetHtmlDocument())
		{
			HTML_Element* h_elm = doc->GetHtmlDocument()->GetHoverHTMLElement();
			OpInputContext* input_context = vis_dev;
			OpInputContext* inner_input_context = h_elm ? LayoutWorkplace::FindInputContext(h_elm) : NULL;

			if (inner_input_context)
				input_context = inner_input_context;

			g_input_manager->InvokeAction(OpInputAction::ACTION_COMPOSITE_PAN_STOP, 0, 0,
										  input_context, NULL, TRUE, OpInputAction::METHOD_MOUSE);
		}
	}
# endif // ACTION_COMPOSITE_PAN_ENABLED

	if (vis_dev->PanMouseUp(window->GetWindowCommander() ? window->GetWindowCommander()->GetOpWindow() : NULL))
		return;
#endif // PAN_SUPPORT

#ifdef DRAG_SUPPORT
	BOOL3 was_dragging = is_dragging;

	StopDragging();
	dragTimer->Stop();

#ifdef GADGET_SUPPORT
	if (was_dragging == YES && vis_dev->GetWindow()->GetType() == WIN_TYPE_GADGET)
		return;  //  Ending a gadget-drag should not result in a left-click.
#endif
#endif // DRAG_SUPPORT

    if (!g_windowManager)
        return;

    if (window->GetState() != BUSY)
    {
        FramesDocument* doc = doc_man->GetCurrentVisibleDoc();

        if (doc)
        {
            int mouse_x = vis_dev->ScaleToDoc(mousepos.x);
            int mouse_y = vis_dev->ScaleToDoc(mousepos.y);

			// Check that it's not a link (don't textselect links with doubleclick)

			haveActivatedALink = FALSE;

			if (!in_editable_element)
				for (HTML_Element* active_element = doc->GetActiveHTMLElement(); active_element; active_element = active_element->ParentActual())
					if (active_element->Type() == HE_A)
					{
						haveActivatedALink = TRUE;
						break;
					}

			int sequence_count = GetLastNumberOfClicksMouseDown(MOUSE_BUTTON_1);
			doc->MouseUp(mouse_x, mouse_y,
						 (keystate & SHIFTKEY_SHIFT) != 0,
						 (keystate & SHIFTKEY_CTRL) != 0,
						 (keystate & SHIFTKEY_ALT) != 0,
						 MOUSE_BUTTON_1,
						 sequence_count);
		}
    }
}

#ifdef DISPLAY_HOTCLICK

//------------------------------------------------------------
// quick helper for now.
// this is only called for 2/3/4 click menu
// activation.. the menu might also show up
// as normal popup when right clicking over a selection
// (handlet in OnMouseRightUp())

void ShowHotclickMenu(VisualDevice* vis_dev, Window* window, CoreView* view, DocumentManager* doc_man, FramesDocument* doc, BOOL reset_pos /*= FALSE*/)
{
#ifndef HAS_NOTEXTSELECTION
	if (window && doc_man && doc && g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::AutomaticSelectMenu))
	{
		int x;
		int y;
		int width;
		int height;

		doc->GetSelectionBoundingRect(x, y, width, height);

		if (doc->GetTopDocument()->GetSelectedTextLen() > 0)
		{
#ifndef _NO_GLOBALS_
			static POINT g_ptWhere;
			static RECT g_avoid_rect;
#endif

			if (reset_pos)
			{
				int scale = vis_dev->GetScale();
				g_avoid_rect.left = (x * scale) / 100;
				g_avoid_rect.top = (y * scale) / 100;
				g_avoid_rect.right = ((x + width) * scale) / 100;
				g_avoid_rect.bottom = ((y + height) * scale) / 100;

				g_ptWhere.x = g_avoid_rect.left;
				g_ptWhere.y = g_avoid_rect.bottom;
			}

#if defined(QUICK)
			// Temporary fix until window-commander supports the avoid rectangle [espen 2003-03-21]
			OpPoint p1 = view->ConvertToScreen(OpPoint(g_avoid_rect.left, g_avoid_rect.top));
			OpPoint p2 = view->ConvertToScreen(OpPoint(g_avoid_rect.right, g_avoid_rect.bottom));
			OpRect avoid_rect( p1.x, p1.y, p2.x-p1.x, p2.y-p1.y );

			const uni_char* menu_name = 0;
#ifdef GADGET_SUPPORT
			if( window->GetType() == WIN_TYPE_JS_CONSOLE || window->GetType() == WIN_TYPE_GADGET )
#else
			if( window->GetType() == WIN_TYPE_JS_CONSOLE )
#endif // GADGET_SUPPORT
				menu_name = UNI_L("Console Hotclick Popup Menu");
			else
				menu_name = UNI_L("Hotclick Popup Menu");

			// using delayed action, fix for DSK-265082 Crash after navigating to page by invoking link context menu on double click (Crash in >OpTimer::Start)
			// when double clicking a link, ShowHotclickMenu is invoked on previous CoreView. if we displayed a menu synchrounously here
			// the menu will start a message loop(only on Windows?) and the CoreView could be deleted(as the new page is loading) before the menu return.
			OpInputAction action(OpInputAction::ACTION_DELAY);
			OpInputAction* async_action = OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_POPUP_MENU));
			if (async_action)
			{
				async_action->SetActionDataString(menu_name);
				async_action->SetActionPosition(avoid_rect);
				action.SetActionData(10);
				action.SetNextInputAction(async_action);
				g_input_manager->InvokeAction(&action);

#elif defined(_POPUP_MENU_SUPPORT_)
				NotifyMenuListener(OpPoint(g_ptWhere.x, g_ptWhere.y), window->GetWindowCommander(), view, doc, FALSE, FALSE, TRUE, FALSE, NULL, NULL, NULL, FALSE, 0);
#endif
			}
			else
			{
				window->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			}
		}
	}
#endif

}
#endif // DISPLAY_HOTCLICK

//------------------------------------------------------------

#define SEL_NORMAL 0
#define SEL_WORDSTART 1
#define SEL_WORDEND 2
#define SEL_SENTENCESTART 3
#define SEL_SENTENCEEND 4
#define SEL_PSTART 5
#define SEL_PEND 6

void MouseListener::OnMouseLeftDblClk(CoreView* view, Window* window, DocumentManager* doc_man, FramesDocument* doc, ShiftKeyState keystate)
{
	last_click_was_double = TRUE;
	if (doc)
	{
		int mouse_x = vis_dev->ScaleToDoc(mousepos.x);
		int mouse_y = vis_dev->ScaleToDoc(mousepos.y);

		int view_x = vis_dev->GetRenderingViewX();
		int view_y = vis_dev->GetRenderingViewY();

		doc->MouseAction(ONDBLCLICK, mouse_x + view_x,
										mouse_y + view_y,
										doc->GetVisualViewX(), doc->GetVisualViewY(),
										MOUSE_BUTTON_1,
										(keystate & SHIFTKEY_SHIFT) != 0,
										(keystate & SHIFTKEY_CTRL) != 0,
										(keystate & SHIFTKEY_ALT) != 0);

#ifndef HAS_NOTEXTSELECTION
		//doubleclick current word
		if (!haveActivatedALink)
		{
			selection_scroll_active = FALSE; // don't scroll after doubleclick!

# if defined _X11_SELECTION_POLICY_
			doc->CopySelectedTextToMouseSelection();
# endif
		}
#endif // HAS_NOTEXTSELECTION
	}
}


void MouseListener::OnMouseLeft3Clk(CoreView* view, Window* window, DocumentManager* doc_man, FramesDocument* doc, ShiftKeyState keystate)
{
#ifndef HAS_NOTEXTSELECTION
	if (window->HasFeature(WIN_FEATURE_FOCUSABLE))
	{
		if (!view->GetMouseButton(MOUSE_BUTTON_2))
		{
			//triple click current word

			if (doc)
			{
				doc->ExpandSelection(TEXT_SELECTION_SENTENCE);

			    doc->SetSelectingText(FALSE);

#ifdef DISPLAY_HOTCLICK
				if (!in_editable_element)
				{
#ifdef _X11_SELECTION_POLICY_
					doc->CopySelectedTextToMouseSelection();
#endif // _X11_SELECTION_POLICY_
					ShowHotclickMenu(vis_dev, window, view, doc_man, doc);
				}
#endif // DISPLAY_HOTCLICK
			}
		}
	}
#endif // HAS_NOTEXTSELECTION
}

void MouseListener::OnMouseLeft4Clk(CoreView* view, Window* window, DocumentManager* doc_man, FramesDocument* doc, ShiftKeyState keystate)
{
#ifndef HAS_NOTEXTSELECTION
	if (window->HasFeature(WIN_FEATURE_FOCUSABLE))
	{
		if (!view->GetMouseButton(MOUSE_BUTTON_2))
		{
			//quad click current word

			if (doc)
			{
				doc->ExpandSelection(TEXT_SELECTION_PARAGRAPH);

			    doc->SetSelectingText(FALSE);

#ifdef DISPLAY_HOTCLICK
				if (!in_editable_element)
				{
#ifdef _X11_SELECTION_POLICY_
					doc->CopySelectedTextToMouseSelection();
#endif // _X11_SELECTION_POLICY_
					ShowHotclickMenu(vis_dev, window, view, doc_man, doc);
				}
#endif
			}
		}
	}
#endif
}

//------------------------------------------------------------------------------------------------------

void MouseListener::OnMouseRightDown  (CoreView* view, Window* window, DocumentManager* doc_man, FramesDocument* doc, UINT8 nclicks, ShiftKeyState keystate)
{
	right_key_is_down = TRUE;
	left_while_right_down = FALSE;

	if (doc)
	{
		//Pass the mouse right button down event to document

		PropagateMouseEvent(view, window, doc, MOUSE_BUTTON_2, nclicks, keystate);
	}

#ifndef HAS_NOTEXTSELECTION
	if (doc) doc->SetSelectingText(FALSE); // when right mouse is pressed any selectioning should stop
#endif // HAS_NOTEXTSELECTION
}

//------------------------------------------------------------------------------------------------------

void MouseListener::OnMouseRightUp(CoreView* view, Window* window, DocumentManager* doc_man, FramesDocument* doc, UINT8 nclicks, ShiftKeyState keystate)
{
	right_key_is_down = FALSE;
	left_while_right_down = FALSE;

	DocMouseActivate(doc_man, FALSE);

	if (doc)
	{
		int sequence_count = GetLastNumberOfClicksMouseDown(MOUSE_BUTTON_2);
		doc->MouseUp(vis_dev->ScaleToDoc(mousepos.x),
		             vis_dev->ScaleToDoc(mousepos.y),
		             (keystate & SHIFTKEY_SHIFT) != 0,
		             (keystate & SHIFTKEY_CTRL) != 0,
		             (keystate & SHIFTKEY_ALT) != 0,
		             MOUSE_BUTTON_2,
		             sequence_count);
	}
}

//------------------------------------------------------------------------------------------------------

void MouseListener::OnMouseMiddleDown (CoreView* view, Window* window, DocumentManager* doc_man, FramesDocument* doc, UINT8 nclicks, ShiftKeyState keystate)
{
#if defined(_X11_SELECTION_POLICY_) && defined(DOCUMENT_EDIT_SUPPORT)
	// Workaround as long as logdoc is not ready for this functionality.
	// Not ideal as we loose ecmascript handling for the center mouse button
	// (but only when we are inside an OpDocumentEdit area) Bug #CORE-17868
	OpDocumentEdit* edit = GetDocumentEditElement(vis_dev, window, doc, mousepos);
	if (edit)
	{
		// We are inside the edit area

		INT32 x = vis_dev->ScaleToDoc(mousepos.x) + doc->GetVisualDevice()->GetRenderingViewX();
		INT32 y = vis_dev->ScaleToDoc(mousepos.y) + doc->GetVisualDevice()->GetRenderingViewY();

		edit->RestoreFocus(FOCUS_REASON_MOUSE);
		edit->m_caret.Place(x, y);

		// Paste from selection buffer
		g_clipboard_manager->SetMouseSelectionMode(TRUE);
		if (g_clipboard_manager->HasText())
		{
			edit->Paste();
		}
		g_clipboard_manager->SetMouseSelectionMode(FALSE);

		return;
	}

#endif

	if(doc)
		PropagateMouseEvent(view, window, doc, MOUSE_BUTTON_3, nclicks, keystate);

	// Scroll stuff
	if(keystate & SHIFTKEY_CTRL)
	{
		window->GetMessageHandler()->PostMessage
			(WM_OPERA_SCALEDOC, 0, MAKELONG(100 /* scale */, TRUE /* is absolute */));
	}
	else
	{
#if defined(NO_PANNING_ON_FORMS)
		// In X-windows it is standard behavior to paste text by pressing
		// the middle mouse button on an edit field.
		if( GetFormElement( vis_dev, window, doc, mousepos ) )
		{
			// Since PropagateMouseEvent is now called for all platforms
			// above we just return after sending the OnMouseLeftUp()
			// which is required to show a normal mouse cursor over
			// a scrollbar in a multiline edit field after a middleclick
			// [espen 2005-09-26]

			DocumentManager* doc_man = vis_dev->GetDocumentManager();
			OnMouseLeftUp(view, window, doc_man, doc, 1, keystate);
			return;
		}
#endif
    }

}

//------------------------------------------------------------------------------------------------------

void MouseListener::OnMouseMiddleUp (CoreView* view, Window* window, DocumentManager* doc_man, FramesDocument* doc, UINT8 nclicks, ShiftKeyState keystate)
{
	if (doc)
	{
		int mouse_x = vis_dev->ScaleToDoc(mousepos.x);
		int mouse_y = vis_dev->ScaleToDoc(mousepos.y);

		int sequence_count = GetLastNumberOfClicksMouseDown(MOUSE_BUTTON_3);
		doc->MouseUp(mouse_x, mouse_y,
						(keystate & SHIFTKEY_SHIFT) != 0,
						(keystate & SHIFTKEY_CTRL) != 0,
						(keystate & SHIFTKEY_ALT) != 0,
						MOUSE_BUTTON_3,
						sequence_count);
	}
}

//------------------------------------------------------------------------------------------------------

/*
// temporary here
void MouseListener::OpenSelectMenu(OpView* view, Window* window, DocumentManager* doc_man, Document* doc)
{
	unsigned int idr = IDR_SELECTMENU; //markeringsmeny

	POINT g_ptWhere = {mousepos.x, mousepos.y};
	window->TrackContextMenu(doc_man->GetCurrentDoc(), idr, g_ptWhere);
}
*/

//------------------------------------------------------------------------------------------------------
void MouseListener::OnMouseRightDblClk(CoreView* view, Window* window, DocumentManager* doc_man, FramesDocument* doc, ShiftKeyState keystate)
{
}


//------------------------------------------------------------------------------------------------------
MouseListener::MouseListener(VisualDevice* vis_dev) :
	  vis_dev(vis_dev)
	, in_editable_element(FALSE)
#ifdef DRAG_SUPPORT
	, dragTimer(NULL)
#endif // DRAG_SUPPORT
	, right_key_is_down(FALSE)
	, left_key_is_down(FALSE)
	, left_while_right_down(FALSE)
{
#ifdef DRAG_SUPPORT
	is_dragging = NO;
#endif // DRAG_SUPPORT
}

OP_STATUS MouseListener::Init()
{
#ifdef DRAG_SUPPORT
	dragTimer = OP_NEW(OpTimer, ());
	if (dragTimer == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	dragTimer->SetTimerListener(this);
#endif // DRAG_SUPPORT

	return OpStatus::OK;
}

MouseListener::~MouseListener()
{
#ifdef DRAG_SUPPORT
	OP_DELETE(dragTimer);
#endif // DRAG_SUPPORT
}

//------------------------------------------------------------------------------------------------------
void MouseListener::OnMouseMove(const OpPoint &point, ShiftKeyState keystate, CoreView* view)
{
	DocumentManager* doc_man = vis_dev->GetDocumentManager();
	TRACK_CPU_PER_TAB(doc_man ? doc_man->GetWindow()->GetCPUUsageTracker() : NULL);
#ifdef PAN_SUPPORT
	if (vis_dev->GetPanState() != NO)
	{
		OpPoint sp = GetTopLevelMousePos(vis_dev->GetWindow());
		OpWindow* ow = vis_dev->GetWindow() ?
			(vis_dev->GetWindow()->GetWindowCommander() ?
				vis_dev->GetWindow()->GetWindowCommander()->GetOpWindow() : 0) :
			0;

#if defined DRAG_SUPPORT
		if (g_drag_manager->IsDragging())
			vis_dev->PanMouseUp(ow);
		else
#endif // DRAG_SUPPORT
		{
			// find target
			OpInputContext* input_context = vis_dev;
			FramesDocument* doc = doc_man ? doc_man->GetCurrentVisibleDoc() : 0;
			if (doc && doc->GetHtmlDocument())
			{
				HTML_Element* h_elm = doc->GetHtmlDocument()->GetHoverHTMLElement();
				OpInputContext* inner_input_context = h_elm ? LayoutWorkplace::FindInputContext(h_elm) : NULL;

				if (inner_input_context)
					input_context = inner_input_context;
			}
			OpWidget* widget = OpWidget::hooked_widget;
			if (widget)
				input_context = widget;

			BOOL being_panned = vis_dev->IsPanning();
			if (vis_dev->PanMouseMove(sp, input_context, ow))
			{
				if (doc && !being_panned && left_key_is_down)
				{
					// Panning state went from MAYBE drag state to YES, so
					// dispatch a mouseup to compensate the previous mousedown.
					// Subsequent calls to OnMouseMove and OnMouseLeftUp will
					// gobble up the events because panning is enabled,
					// so it's safe to call this here because we won't have
					// duplicate events.
					doc->PreventNextClick();
					doc->MouseUp(vis_dev->ScaleToDoc(mousepos.x),
						vis_dev->ScaleToDoc(mousepos.y),
						(keystate & SHIFTKEY_SHIFT) != 0,
						(keystate & SHIFTKEY_CTRL) != 0,
						(keystate & SHIFTKEY_ALT) != 0,
						MOUSE_BUTTON_1,
						GetLastNumberOfClicksMouseDown(MOUSE_BUTTON_1));
				}
				return;
			}
		}
	}
#endif // PAN_SUPPORT

	g_input_manager->SetMouseInputContext(vis_dev);

	OP_NEW_DBG("WindowsOpView::FramesWndProc()", "printing");
	OP_DBG(("pos: %d, %d", point.x, point.y));

#ifdef DRAG_SUPPORT
	if (is_dragging == MAYBE)
		is_dragging = g_drag_manager->IsDragging() && !g_drag_manager->IsBlocked() ? NO : MAYBE;
#ifdef GADGET_SUPPORT
	if(vis_dev->GetWindow()->GetType() == WIN_TYPE_GADGET)
	{

		WindowCommander* commander = vis_dev->GetWindow()->GetWindowCommander();
		OpDocumentListener* listener = commander->GetDocumentListener();

		listener->OnMouseGadgetEnter(commander);

		if (is_dragging == MAYBE)
		{
			if (listener->HasGadgetReceivedDragRequest(NULL))
			{
				OpPoint dp = view->ConvertToScreen(g_opera->display_module.m_drag_point);
				CoreViewContainer* viewCont = view->GetContainer();
				while (viewCont->GetParent())
				{
					viewCont = viewCont->GetParent()->GetContainer();
				}
				OpPoint origo = viewCont->ConvertToScreen(OpPoint(0,0));
				dp.x -= origo.x;
				dp.y -= origo.y;
				listener->OnGadgetDragStart(commander, dp.x, dp.y);
				is_dragging = YES;
				return;
			}
			else
				StopDragging();
		}
		else if (is_dragging != YES)
		{
			listener->CancelGadgetDragRequest(commander);
		}
	}
#endif // GADGET_SUPPORT
#endif // DRAG_SUPPORT

	if(IsPanning()) // Do nothing in here if we are panning
		return;

#ifdef DRAG_SUPPORT
	if (is_dragging == YES || (g_drag_manager->IsDragging() && !g_drag_manager->IsBlocked()))
	{
#ifdef GADGET_SUPPORT
		if (is_dragging == YES && vis_dev->GetWindow()->GetType() == WIN_TYPE_GADGET)
		{
			WindowCommander* commander = vis_dev->GetWindow()->GetWindowCommander();

			OpPoint dp = view->ConvertToScreen(point);
			CoreViewContainer* viewCont = view->GetContainer();
			while (viewCont->GetParent())
			{
				viewCont = viewCont->GetParent()->GetContainer();
			}
			OpPoint origo = viewCont->ConvertToScreen(OpPoint(0,0));
			dp.x -= origo.x;
			dp.y -= origo.y;
			commander->GetDocumentListener()->OnGadgetDragMove(commander, dp.x, dp.y);
		}
#endif // GADGET_SUPPORT
#ifndef HAS_NOTEXTSELECTION
		if (FramesDocument* doc = doc_man->GetCurrentVisibleDoc())
		{
			doc->SetSelectingText(FALSE);
		}
#endif // HAS_NOTEXTSELECTION
	}
#endif // DRAG_SUPPORT

	mousepos = point;
	Window* window = doc_man->GetWindow();

	if (window->GetState() != BUSY)
	{
		FramesDocument* doc = doc_man->GetCurrentVisibleDoc();
		if (doc)
		{
#ifdef _PRINT_SUPPORT_
			if (window->GetPreviewMode())
				window->SetCursor(CURSOR_DEFAULT_ARROW);
#else
			if (FALSE)
				;
#endif // _PRINT_SUPPORT_
			else
			{
				int mouse_x = vis_dev->ScaleToDoc(point.x);
				int mouse_y = vis_dev->ScaleToDoc(point.y);

				current_mouse_event = ONMOUSEMOVE;
				doc->MouseMove(mouse_x, mouse_y,
							   (keystate & SHIFTKEY_SHIFT) != 0,
							   (keystate & SHIFTKEY_CTRL) != 0,
							   (keystate & SHIFTKEY_ALT) != 0,
							   view->GetMouseButton(MOUSE_BUTTON_1),
							   view->GetMouseButton(MOUSE_BUTTON_3),
							   view->GetMouseButton(MOUSE_BUTTON_2));
				current_mouse_event = DOM_EVENT_NONE;
			}
		}
	}
}

//------------------------------------------------------------------------------------------------------
void MouseListener::OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks, ShiftKeyState keystate, CoreView* view)
{
	// We want a sequence of 4 here. Ideally, we would send the original value to script. But something somewhere is bugging then, with popups, hotclicks etc.
#ifdef DISPLAY_NO_MULTICLICK_LOOP
	nclicks = nclicks <= 4 ? nclicks : 4;
#else
	nclicks = (nclicks - 1) % 4 + 1;
#endif

	DocumentManager* doc_man = vis_dev->GetDocumentManager();
	Window* window = doc_man->GetWindow();
	TRACK_CPU_PER_TAB(window->GetCPUUsageTracker());

#ifdef GADGET_SUPPORT
    if(window->GetType() == WIN_TYPE_GADGET)
    {
		WindowCommander* commander = window->GetWindowCommander();
        OpDocumentListener* listener = commander->GetDocumentListener();

		if (listener->OnGadgetClick(commander, point, button, nclicks))
			return;
    }
#endif // GADGET_SUPPORT


    if (IsPanning() && button != MOUSE_BUTTON_3) // Do nothing inhere if we are panning
		return;

#if defined(QUICK)
	// quick kludge until we can clean up event eating later
	g_dont_show_popup = FALSE;
#endif

	g_input_manager->SetMouseInputContext(vis_dev);
//	vis_dev->SetFocus(FOCUS_REASON_MOUSE); fix for bug 115876 (rfz)

	current_mouse_button = button;

	//Activate window
	window->Activate(TRUE);

	FramesDocument* doc = doc_man->GetCurrentVisibleDoc();

	mousepos = point;
	if (doc)
	{
		OpRect visual_viewport = doc->GetVisualViewport();
		docpos.x = visual_viewport.x;
		docpos.y = visual_viewport.y;
	}
#ifdef DRAG_SUPPORT
	g_opera->display_module.m_drag_point = mousepos;
#endif
	SetLastNumberOfClicksMouseDown(button, nclicks);

	g_windowManager->SetCurrentClickedWindow(window);

#ifdef DOCUMENT_EDIT_SUPPORT
	in_editable_element = FALSE;
	if (doc && doc->GetHtmlDocument())
	{
		if (doc->GetDocumentEdit())
		{
			HTML_Element* hover_elm = doc->GetHtmlDocument()->GetHoverHTMLElement();
			if (hover_elm && doc->GetDocumentEdit()->GetEditableContainer(hover_elm))
				in_editable_element = TRUE;
		}
	}
#endif

	current_mouse_event = ONMOUSEDOWN;
	if (button == MOUSE_BUTTON_1)
	{
		if (nclicks == 4)
			OnMouseLeft4Clk(view, window, doc_man, doc, keystate);
		else if (nclicks == 3)
			OnMouseLeft3Clk(view, window, doc_man, doc, keystate);
		else if (nclicks == 2)
			OnMouseLeftDblClk(view, window, doc_man, doc, keystate);

		OnMouseLeftDown(view, window, doc_man, doc, nclicks, keystate);
	}
	else if (button == MOUSE_BUTTON_2)
	{
		if (nclicks == 2)
			OnMouseRightDblClk(view, window, doc_man, doc, keystate);

		OnMouseRightDown(view, window, doc_man, doc, nclicks, keystate);
	}
	else if (button == MOUSE_BUTTON_3)
	{
		OnMouseMiddleDown(view, window, doc_man, doc, nclicks, keystate);
    }
	current_mouse_event = DOM_EVENT_NONE;
}


//------------------------------------------------------------------------------------------------------
void MouseListener::OnMouseUp(const OpPoint &point, MouseButton button, ShiftKeyState keystate, CoreView* view)
{
	if (IsPanning()) // Do nothing inhere if we are panning
		return;

	DocumentManager* doc_man = vis_dev->GetDocumentManager();
	Window* window = doc_man->GetWindow();
	TRACK_CPU_PER_TAB(window->GetCPUUsageTracker());

	current_mouse_button = button;

	FramesDocument* doc = doc_man->GetCurrentVisibleDoc();
	UINT8 nclicks = GetLastNumberOfClicksMouseDown(button);
	mousepos = point;
	current_mouse_event = ONMOUSEUP;
	switch (button)
	{
	case MOUSE_BUTTON_1:
		OnMouseLeftUp(view, window, doc_man, doc, nclicks, keystate);
		break;

	case MOUSE_BUTTON_2:
		OnMouseRightUp(view, window, doc_man, doc, nclicks, keystate);
		break;

	case MOUSE_BUTTON_3:
		OnMouseMiddleUp(view, window, doc_man, doc, nclicks, keystate);

		// So that hover events will arrive before we click with left button

		if (doc && doc->GetHtmlDocument())
			doc->GetHtmlDocument()->SetCapturedWidgetElement(NULL);

		break;
    }
	current_mouse_event = DOM_EVENT_NONE;
}

//------------------------------------------------------------------------------------------------------
void MouseListener::OnMouseLeave()
{
#ifdef DRAG_SUPPORT
	StopDragging();
#endif

#if defined(QUICK)
	// quick kludge until we can clean up event eating later
	g_dont_show_popup = FALSE;
#endif

#ifdef DRAG_SUPPORT
# ifdef GADGET_SUPPORT
	if(vis_dev->GetWindow()->GetType() == WIN_TYPE_GADGET)
	{
		WindowCommander* commander = vis_dev->GetWindow()->GetWindowCommander();
		OpDocumentListener* listener = commander->GetDocumentListener();

		listener->OnMouseGadgetLeave(commander);
	}
# endif //GADGET_SUPPORT
#endif //DRAG_SUPPORT

	DocumentManager* doc_man = vis_dev->GetDocumentManager();
	if (!doc_man)
		return;

	TRACK_CPU_PER_TAB(doc_man->GetWindow()->GetCPUUsageTracker());
	FramesDocument* doc = doc_man->GetCurrentVisibleDoc();

	//MAKE A PORTABLE OpTimer for theese kind of stuff !!!!!!
	//KillTimer(window->Hwin(), ID_TIMER_MARKING);
	if (doc)
		doc->MouseOut();
}

//------------------------------------------------------------------------------------------------------
void MouseListener::OnSetCursor()
{
	DocumentManager* doc_man = vis_dev->GetDocumentManager();
	doc_man->GetWindow()->SetCurrentCursor();
}

//------------------------------------------------------------------------------------------------------
BOOL MouseListener::OnMouseWheel(INT32 delta,BOOL vertical, ShiftKeyState keystate, CoreView* view)
{
	DocumentManager* doc_man = vis_dev->GetDocumentManager();
	Window* window = doc_man->GetWindow();
	TRACK_CPU_PER_TAB(window->GetCPUUsageTracker());
	FramesDocument* doc = doc_man->GetCurrentVisibleDoc();

	if (doc && doc->IsFrameDoc())
		doc = doc->GetActiveSubDoc();

	if (!doc)
	{
		return FALSE;
	}

	if(keystate & SHIFTKEY_SHIFT) // Prev/Next
	{
		if( vertical )
		{
			g_input_manager->InvokeAction(delta < 0 ? OpInputAction::ACTION_FORWARD : OpInputAction::ACTION_BACK, 0, NULL, vis_dev, NULL, TRUE, OpInputAction::METHOD_MOUSE);
		}
	}
	else if (keystate & DISPLAY_SCALE_MODIFIER) // Scale
	{
		if( vertical )
		{
			if(delta < 0)
				delta = 10;
			else
				delta = -10;
			window->GetMessageHandler()->PostMessage(WM_OPERA_SCALEDOC, 0, MAKELONG(delta, 0));
		}
	}
	else // Scroll
	{
		if (doc->current_focused_formobject && doc->current_focused_formobject->GetWidget()->GetType() == OpTypedObject::WIDGET_TYPE_DROPDOWN
			&& ((OpDropDown*)doc->current_focused_formobject->GetWidget())->m_dropdown_window)
		{
			// A dropdown with open menu should always get the wheel
			FormObject *form_object = doc->current_focused_formobject;

			// Scrollable forms should stop bubbling the wheel up in the hierarchy, even if it didn't scroll now (might have hit the bottom/top).
			form_object->UpdatePosition();
			form_object->GetWidget()->GenerateOnMouseWheel(delta,vertical);

			return TRUE;
		}

		view->GetMousePos(&mousepos.x, &mousepos.y);

		doc->MouseWheel(vis_dev->ScaleToDoc(mousepos.x), vis_dev->ScaleToDoc(mousepos.y),
		                (keystate & SHIFTKEY_SHIFT) != 0,
		                (keystate & SHIFTKEY_CTRL) != 0,
		                (keystate & SHIFTKEY_ALT) != 0,
		                delta, vertical);
	}

	return TRUE;
}

void MouseListener::OnTimeOut(OpTimer* timer)
{
#ifdef DRAG_SUPPORT
	if (timer == dragTimer && is_dragging != YES)
	{
		StopDragging(); // if we time out, then drag is off.
		OP_NEW_DBG("MouseListener::OnTimeOut", "drag");
		OP_DBG(("there's no drag like no drag"));
	}
#endif // DRAG_SUPPORT
}

/**
 * This method passes on the mousedown event to document, in particular, passing
 * information about the button that was clicked. This method is called by the
 * OnMouse{Left,Right,Middle}Down methods.
 */
void MouseListener::PropagateMouseEvent(CoreView* view, Window* window, FramesDocument* doc, MouseButton button, UINT8 nclicks, ShiftKeyState keystate)
{
	int mouse_x = vis_dev->ScaleToDoc(mousepos.x);
	int mouse_y = vis_dev->ScaleToDoc(mousepos.y);

	int sequence_count = nclicks; // 2, 3... for doubleclicks and trippleclicks
	doc->MouseDown(mouse_x, mouse_y,
				   (keystate & SHIFTKEY_SHIFT) != 0,
				   (keystate & SHIFTKEY_CTRL) != 0,
				   (keystate & SHIFTKEY_ALT) != 0,
				   button,
 				   sequence_count);
}

#endif // !MOUSELESS

//------------------------------------------------------------------------------------------------------

// Check if we can scroll in a specified direction based on the action
BOOL CanScrollInDirection(FramesDocument* doc, OpInputAction::Action action)
{
	if(doc == NULL || doc->IsUndisplaying())
		return FALSE;

	BOOL isOverflowX = FALSE;
	LayoutWorkplace* wp = doc->GetLogicalDocument() ? doc->GetLogicalDocument()->GetLayoutWorkplace() : NULL;
	if (wp)
		// don't scroll if we're not allowed to
		isOverflowX = (wp->GetViewportOverflowX() == CSS_VALUE_hidden);

	VisualDevice* vd = doc->GetVisualDevice();
	OpRect visual_viewport = doc->GetVisualViewport();
	long left = -doc->NegativeOverflow();
	long right = (doc->Width() > visual_viewport.width) ? doc->Width() - visual_viewport.width : 0;

	// Make it possible to scroll a litle bit outside if scaling doesn't round up exactly. (So we can show all content).
	right = vd->ApplyScaleRoundingNearestUp(right);

	switch (action)
	{
		case OpInputAction::ACTION_SCROLL_LEFT:
			return (visual_viewport.x > left) && !isOverflowX;

		case OpInputAction::ACTION_SCROLL_RIGHT:
			return (visual_viewport.x < right) && !isOverflowX;
	}

	return TRUE;
}

BOOL ScrollDocument(FramesDocument* doc, OpInputAction::Action action, int times, OpInputAction::ActionMethod method)
{
	if(doc == NULL || doc->IsUndisplaying())
		return FALSE;

#ifdef DEBUG_GFX
	// Trig test of scrollspeed.
	if (action == OpInputAction::ACTION_GO_TO_END)
	{
		VisualDevice* vd = doc->GetVisualDevice();
		vd->TestSpeedScroll();
		return TRUE;
	}
#endif

	FramesDocElm* doc_frame = doc->GetDocManager()->GetFrame();
	if (doc_frame && doc_frame->GetFrameScrolling() == SCROLLING_NO)
		return FALSE; // Allow the OpInputAction to bubble to the parent document.

	VisualDevice* vd = doc->GetVisualDevice();
	ScrollListener *scroll_listener = ((ScrollListener*)vd->scroll_listener);

	long line_height = method == OpInputAction::METHOD_MOUSE ? DISPLAY_SCROLLWHEEL_STEPSIZE : DISPLAY_SCROLL_STEPSIZE;
	line_height *= times;

	long view_width, view_height, scroll_valx, scroll_valy;

	OpRect visual_viewport = doc->GetVisualViewport();

	view_width = visual_viewport.width;
	view_height = visual_viewport.height;
	scroll_valx = visual_viewport.x;
	scroll_valy = visual_viewport.y;
	if (scroll_listener && scroll_listener->IsSmoothScrollRunning())
		scroll_valy = scroll_listener->GetSmoothScrollTargetValue();

	long top = 0;
	long left = -doc->NegativeOverflow();
	long bottom = 0;
	long right = (doc->Width() > view_width) ? doc->Width() - view_width : 0;

	long new_scroll_valx = scroll_valx;
	long new_scroll_valy = scroll_valy;

#ifdef PAGED_MEDIA_SUPPORT
	PageDescription* current_page = NULL;

	if (CSS_IsPagedMedium(doc->GetMediaType()) && doc->GetMediaType() != CSS_MEDIA_TYPE_PRINT && doc->CountPages() != 1)
    {
		current_page = doc->GetPageDescription(vd->GetRenderingViewY() + vd->step);

		OP_ASSERT(current_page); // FIXME:OOM

		if (current_page)
		{
			top = current_page->GetPageTop();
			bottom = current_page->GetPageTop();

			if (view_height <= current_page->GetSheetHeight())
				bottom += current_page->GetSheetHeight() - view_height;
		}
	}
	else
#endif // PAGED_MEDIA_SUPPORT
	{
		bottom = (doc->Height() > view_height) ? doc->Height() - view_height : 0;
	}

	// Make it possible to scroll a litle bit outside if scaling doesn't round up exactly. (So we can show all content).
	right = vd->ApplyScaleRoundingNearestUp(right);
	bottom = vd->ApplyScaleRoundingNearestUp(bottom);

	BOOL do_smooth_scroll =
#ifdef PAGED_MEDIA_SUPPORT
		!current_page &&
#endif // PAGED_MEDIA_SUPPORT
		g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::SmoothScrolling);

#ifdef CONTENT_MAGIC
	if (action == OpInputAction::ACTION_GO_TO_TOP_CM_BOTTOM)
	{
		INT32 ypos = visual_viewport.y;
		if (ypos < doc->Height() - view_height - 10 /* use 10px slack -- coordinates are miffed as always (rg) */)
		{
				action = OpInputAction::ACTION_GO_TO_END;// go to bottom
		}
		else
		{
			// go to top
			action = OpInputAction::ACTION_GO_TO_START;
		}
	}
#endif // CONTENT_MAGIC

	switch (action)
	{
		case OpInputAction::ACTION_SCROLL:
			new_scroll_valy += times;

	        if (new_scroll_valy < top)
				new_scroll_valy = top;
			else
				if (new_scroll_valy > bottom)
					new_scroll_valy = bottom;

			break;

		case OpInputAction::ACTION_PAGE_UP:
#ifdef PAGED_MEDIA_SUPPORT
			if (current_page)
			{
	            new_scroll_valy = scroll_valy - VisualDevice::GetPageScrollAmount(view_height);

				if (new_scroll_valy >= top)
					// Still on current page

					break;
				else
					if (scroll_valy <= top)
						// Was already scrolled to top of page; scroll to previous

						if (current_page->Pred())
						{
							current_page = (PageDescription*) current_page->Pred();
							top = current_page->GetPageTop();
						}

				new_scroll_valy = top;
			}
			else
#endif // PAGED_MEDIA_SUPPORT
			{
				new_scroll_valy = scroll_valy - VisualDevice::GetPageScrollAmount(view_height);

				if (new_scroll_valy < top)
					new_scroll_valy = top;
			}

            break;

		case OpInputAction::ACTION_PAGE_LEFT:
	        new_scroll_valx = scroll_valx - VisualDevice::GetPageScrollAmount(view_width);

			if (new_scroll_valx < left)
				new_scroll_valx = left;

			break;

		case OpInputAction::ACTION_PAGE_DOWN:
#ifdef PAGED_MEDIA_SUPPORT
			if (current_page)
			{
	            new_scroll_valy = scroll_valy + VisualDevice::GetPageScrollAmount(view_height);

				if (new_scroll_valy >= bottom)
				{
					if (scroll_valy + vd->step >= bottom)
					{
						// Was already scrolled to bottom of page; scroll to next

						if (current_page->Suc())
						{
							current_page = (PageDescription*) current_page->Suc();
							new_scroll_valy = current_page->GetPageTop();
						}
					}
					else
						// Go to bottom of page

						new_scroll_valy = bottom;
				}
			}
			else
#endif // PAGED_MEDIA_SUPPORT
			{
				new_scroll_valy = scroll_valy + VisualDevice::GetPageScrollAmount(view_height);

				if (new_scroll_valy > bottom)
					new_scroll_valy = bottom;
			}

            break;

		case OpInputAction::ACTION_PAGE_RIGHT:
            new_scroll_valx = scroll_valx + VisualDevice::GetPageScrollAmount(view_width);

			if (new_scroll_valx > right)
				new_scroll_valx = right;

			break;

		case OpInputAction::ACTION_SCROLL_UP:
            new_scroll_valy = (scroll_valy - line_height >= top) ? scroll_valy - line_height : top;
            break;

		case OpInputAction::ACTION_SCROLL_DOWN:
            new_scroll_valy = (line_height + scroll_valy <= bottom) ? scroll_valy + line_height : bottom;
            break;

		case OpInputAction::ACTION_SCROLL_LEFT:
            new_scroll_valx = (scroll_valx - line_height >= left) ? scroll_valx - line_height : left;
            break;

		case OpInputAction::ACTION_SCROLL_RIGHT:
            new_scroll_valx = (line_height + scroll_valx <= right) ? scroll_valx + line_height : right;
            break;

		case OpInputAction::ACTION_GO_TO_START:
			do_smooth_scroll = FALSE;
			new_scroll_valy = 0;
            break;

		case OpInputAction::ACTION_GO_TO_END:
			do_smooth_scroll = FALSE;
#ifdef PAGED_MEDIA_SUPPORT
			if (current_page)
				new_scroll_valy = doc->Height() - view_height;
			else
#endif // PAGED_MEDIA_SUPPORT
				new_scroll_valy = bottom;

            break;

#ifdef CONTENT_MAGIC
		case OpInputAction::ACTION_GO_TO_CONTENT_MAGIC:
		{
			do_smooth_scroll = FALSE;
			FramesDocument* frm_doc = doc->GetDocManager()->GetCurrentVisibleDoc();
			if (long pos = frm_doc->GetContentMagicPosition())
			{
				new_scroll_valy = pos;
			}
			else
			{
				return FALSE;
			}
			break;
		}
#endif // CONTENT_MAGIC

		default:
			return FALSE;
	}

	LayoutWorkplace* wp = doc->GetLogicalDocument() ? doc->GetLogicalDocument()->GetLayoutWorkplace() : NULL;

	if (wp)
	{
		// don't scroll if we're not allowed to
		short overflow_x = wp->GetViewportOverflowX();
		short overflow_y = wp->GetViewportOverflowY();

		if (overflow_x == CSS_VALUE_hidden)
			new_scroll_valx = scroll_valx;
		if (overflow_y == CSS_VALUE_hidden)
			new_scroll_valy = scroll_valy;
	}

	if (scroll_valx != new_scroll_valx || scroll_valy != new_scroll_valy)
	{
		doc->GetDocManager()->CancelPendingViewportRestoration();

		if (do_smooth_scroll && scroll_valx == new_scroll_valx)
		{
			if (scroll_listener)
				scroll_listener->SetValueSmoothScroll(scroll_valy, new_scroll_valy);
		}
		else
		{
			if (scroll_listener)
				scroll_listener->StopSmoothScroll();
			return doc->RequestSetVisualViewPos(new_scroll_valx, new_scroll_valy, VIEWPORT_CHANGE_REASON_INPUT_ACTION);
		}
	}
	else
	{
		return FALSE;
	}

	return TRUE;
}

// ======================================================
// ScrollListener
// ======================================================


ScrollListener::ScrollListener(VisualDevice* vis_dev) :
	vis_dev(vis_dev)
{
}

BOOL ScrollListener::OnSmoothScroll(INT32 value)
{
	if (DocumentManager* doc_man = vis_dev->GetDocumentManager())
		if (FramesDocument* doc = doc_man->GetCurrentVisibleDoc())
		{
			doc->RequestSetVisualViewPos(doc->GetVisualViewX(), value, VIEWPORT_CHANGE_REASON_INPUT_ACTION);

			return value == doc->GetVisualViewY();
		}

	return TRUE;
}

void ScrollListener::OnScroll(OpWidget *widget, INT32 old_val, INT32 new_val, BOOL caused_by_input)
{
	DocumentManager* doc_man = vis_dev->GetDocumentManager();
	OpScrollbar* scrollbar = (OpScrollbar*) widget;
	FramesDocument* frames_doc = doc_man->GetCurrentVisibleDoc();

	if (caused_by_input)
	{
		doc_man->CancelPendingViewportRestoration();

		INT32 newx = vis_dev->GetRenderingViewX();
		INT32 newy = vis_dev->GetRenderingViewY();

		if (scrollbar->IsHorizontal())
			newx = new_val;
		else
			newy = new_val;

		if (frames_doc)
			frames_doc->RequestSetVisualViewPos(newx, newy, VIEWPORT_CHANGE_REASON_INPUT_ACTION);
	}

	if (frames_doc && !frames_doc->IsTopDocument())
		frames_doc->HandleDocumentEvent(ONSCROLL);
}

#ifdef DRAG_SUPPORT

// ======================================================
// DragListener
// ======================================================
void DragListener::OnDragStart(CoreView* view, const OpPoint& start_point, ShiftKeyState modifiers, const OpPoint& current_point)
{
	if (FramesDocument* doc = m_vis_dev->GetDocumentManager()->GetCurrentDoc())
		g_drag_manager->OnDragStart(this, doc->GetHtmlDocument(), m_vis_dev->ScaleToDoc(start_point.x), m_vis_dev->ScaleToDoc(start_point.y),
		modifiers, m_vis_dev->ScaleToDoc(current_point.x), m_vis_dev->ScaleToDoc(current_point.y));
}

void DragListener::OnDragEnter(CoreView* view, OpDragObject* drag_object, const OpPoint& point, ShiftKeyState modifiers)
{
	g_drag_manager->OnDragEnter();
	if (FramesDocument* doc = m_vis_dev->GetDocumentManager()->GetCurrentDoc())
		g_drag_manager->OnDragMove(doc->GetHtmlDocument(), m_vis_dev->ScaleToDoc(point.x), m_vis_dev->ScaleToDoc(point.y), modifiers);
}

void DragListener::OnDragCancel(CoreView* view, OpDragObject* drag_object, ShiftKeyState modifiers)
{
	INT32 x = -1;
	INT32 y = -1;
	view->GetMousePos(&x, &y);

	g_drag_manager->OnDragCancel(m_vis_dev->ScaleToDoc(x), m_vis_dev->ScaleToDoc(y), modifiers);
}

void DragListener::OnDragLeave(CoreView* view, OpDragObject* drag_object, ShiftKeyState modifiers)
{
	g_drag_manager->OnDragLeave(modifiers);
}

void DragListener::OnDragMove(CoreView* view, OpDragObject* drag_object, const OpPoint& point, ShiftKeyState modifiers)
{
	if(FramesDocument* doc = m_vis_dev->GetDocumentManager()->GetCurrentDoc())
		g_drag_manager->OnDragMove(doc->GetHtmlDocument(), m_vis_dev->ScaleToDoc(point.x), m_vis_dev->ScaleToDoc(point.y), modifiers);
}

void DragListener::OnDragDrop(CoreView* view, OpDragObject* drag_object, const OpPoint& point, ShiftKeyState modifiers)
{
	g_drag_manager->OnDragDrop(m_vis_dev->ScaleToDoc(point.x), m_vis_dev->ScaleToDoc(point.y), modifiers);
}

void DragListener::OnDragEnd(CoreView* view, OpDragObject* drag_object, const OpPoint& point, ShiftKeyState modifiers)
{
	g_drag_manager->OnDragEnd(m_vis_dev->ScaleToDoc(point.x), m_vis_dev->ScaleToDoc(point.y), modifiers);
}

#endif // DRAG_SUPPORT

// ======================================================
// TouchListener
// ======================================================

#ifdef TOUCH_EVENTS_SUPPORT
TouchListener::TouchListener(VisualDevice* vis_dev) :
	vis_dev(vis_dev)
	, in_editable_element(FALSE)
{
}

void TouchListener::OnTouchDown(int id, const OpPoint &point, int radius, ShiftKeyState modifiers, CoreView* view, void* user_data)
{
	OnTouch(TOUCHSTART, id, point, radius, modifiers, user_data);
}

void TouchListener::OnTouchUp(int id, const OpPoint &point, int radius, ShiftKeyState modifiers, CoreView* view, void* user_data)
{
	OnTouch(TOUCHEND, id, point, radius, modifiers, user_data);
}

void TouchListener::OnTouchMove(int id, const OpPoint &point, int radius, ShiftKeyState modifiers, CoreView* view, void* user_data)
{
	OnTouch(TOUCHMOVE, id, point, radius, modifiers, user_data);
}

void TouchListener::OnTouch(DOM_EventType type, int id, const OpPoint &point, int radius, ShiftKeyState modifiers, void* user_data)
{
	DocumentManager* doc_man = vis_dev->GetDocumentManager();
	Window* window = doc_man->GetWindow();
	FramesDocument* doc = doc_man->GetCurrentVisibleDoc();

	if (type == TOUCHSTART)
		window->Activate(TRUE);

	if (doc && window->GetState() != BUSY)
	{
		int doc_x = vis_dev->ScaleToDoc(point.x);
		int doc_y = vis_dev->ScaleToDoc(point.y);
		int doc_radius = vis_dev->ScaleToDoc(radius);

		BOOL shift_pressed = (modifiers & SHIFTKEY_SHIFT) ? TRUE : FALSE;
		BOOL ctrl_pressed = (modifiers & SHIFTKEY_CTRL) ? TRUE : FALSE;
		BOOL alt_pressed = (modifiers & SHIFTKEY_ALT) ? TRUE : FALSE;

#ifdef PAN_SUPPORT
#ifdef DO_NOT_PAN_PLUGINS
		HTML_Document* htmldoc = doc ? doc->GetHtmlDocument() : NULL;
		HTML_Element* helm = htmldoc ? htmldoc->GetHoverHTMLElement() : NULL;
		if (!IsPanning() && (!helm || !helm->GetLayoutBox() || !helm->GetLayoutBox()->IsContentEmbed()))
#endif // DO_NOT_PAN_PLUGINS
		{
			OpWindow *op_window = window->GetWindowCommander() ? window->GetWindowCommander()->GetOpWindow() : NULL;

			if (type == TOUCHSTART)
				vis_dev->TouchPanMouseDown(OpPoint(doc_x, doc_y));
			else if(type == TOUCHMOVE)
				vis_dev->PanMouseMove(point, vis_dev, op_window);
			else if(type == TOUCHEND)
				vis_dev->PanMouseUp(op_window);
		}
#endif // PAN_SUPPORT

		doc->TouchAction(type, id, doc_x, doc_y, doc->GetVisualViewX(), doc->GetVisualViewY(), doc_radius, shift_pressed, ctrl_pressed, alt_pressed, user_data);
	}
}

void TouchListener::UpdateOverlappedStatus(CoreView* view)
{
	::UpdateOverlappedStatus(view, vis_dev);
}

CoreView* TouchListener::GetTouchHitView(const OpPoint &point, CoreView* view)
{
	return GetPointHitView(point, view, vis_dev);
}
#endif // TOUCH_EVENTS_SUPPORT
