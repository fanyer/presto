/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/doc/html_doc.h"

#include "modules/hardcore/mem/mem_man.h"
#include "modules/util/tempbuf.h"

#include "modules/display/VisDevListeners.h"
#include "modules/img/image.h"
#include "modules/logdoc/logdoc_util.h"

#include "modules/widgets/OpButton.h"
#include "modules/widgets/OpDropDown.h"
#ifdef DOCUMENT_EDIT_SUPPORT
# include "modules/documentedit/OpDocumentEdit.h"
#endif // DOCUMENT_EDIT_SUPPORT

#include "modules/doc/frm_doc.h"
#include "modules/probetools/probepoints.h"
#include "modules/dochand/fdelm.h"
#include "modules/forms/form.h"
#include "modules/forms/formvaluelistener.h"
#include "modules/forms/formmanager.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/display/vis_dev.h"
#include "modules/forms/form.h"
#include "modules/forms/formvalue.h"
#include "modules/forms/formvalueradiocheck.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_print.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"

#include "modules/dochand/viewportcontroller.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/logdoc/urlimgcontprov.h"
#include "modules/logdoc/selection.h"

#include "modules/windowcommander/src/WindowCommander.h"

#include "modules/dom/domutils.h"

#ifdef _SPAT_NAV_SUPPORT_
# include "modules/spatial_navigation/handler_api.h"
#endif // _SPAT_NAV_SUPPORT_

#ifdef _WML_SUPPORT_ // stighal {
# include "modules/logdoc/wml.h"
#endif // _WML_SUPPORT_ } stighal

#ifdef SVG_SUPPORT
# include "modules/svg/SVGManager.h"
# include "modules/svg/svg_image.h"
#endif // SVG_SUPPORT

#if defined WAND_SUPPORT && defined PREFILLED_FORM_WAND_SUPPORT
# include "modules/wand/wandmanager.h"
#endif//WAND_SUPPORT

#include "modules/display/coreview/coreview.h"
#include "modules/layout/layout_workplace.h"
#include "modules/layout/box/box.h"
#include "modules/layout/content/content.h"
#include "modules/layout/traverse/traverse.h"

#ifdef NEARBY_ELEMENT_DETECTION
# include "modules/widgets/finger_touch/element_expander.h"
#endif // NEARBY_ELEMENT_DETECTION

#ifdef SEARCH_MATCHES_ALL
# include "modules/doc/searchinfo.h"
#endif

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
#include "modules/accessibility/AccessibleDocument.h"
#endif // ACCESSIBILITY_EXTENSION_SUPPORT

#ifdef DRAG_SUPPORT
#include "modules/dragdrop/dragdrop_manager.h"
#endif // DRAG_SUPPORT

OP_STATUS HTML_Document::HandleLoadingFinished()
{
# if defined (WAND_SUPPORT) && defined (PREFILLED_FORM_WAND_SUPPORT)
	if (!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseParanoidMailpassword)
		&& GetDocManager()->GetUserStartedLoading())
	{
		if (g_wand_manager->Usable(GetFramesDocument()))
			g_wand_manager->Use(GetFramesDocument(), NO);
	}
# endif // WAND_SUPPORT && PREFILLED_FORM_WAND_SUPPORT

	if (loading_finished)
	{
		// turn off progressbar for image loading that was started after first time doc was loaded.

		RETURN_IF_MEMORY_ERROR(GetDocManager()->HandleDocFinished());

		return OpStatus::OK;
	}

	loading_finished = TRUE;

	all_loaded_sent = TRUE;
	send_all_loaded = FALSE;

	OP_STATUS stat = GetDocManager()->HandleDocFinished();

#ifdef IMAGE_TURBO_MODE
	if (stat != OpStatus::ERR_NO_MEMORY)
		GetFramesDocument()->DecodeRemainingImages();
#endif

#ifdef WAND_SUPPORT
# ifdef WAND_EXTERNAL_STORAGE
	g_wand_manager->UpdateMatchingFormsFromExternalStorage(frames_doc);
# endif
#endif

	return stat;
}

BOOL HTML_Document::Free()
{
#ifdef DOCUMENT_EDIT_SUPPORT
	OpStatus::Ignore(frames_doc->SetEditable(FramesDocument::DESIGNMODE_OFF));
	OpStatus::Ignore(frames_doc->SetEditable(FramesDocument::CONTENTEDITABLE_OFF));
#endif
	SetCurrentAreaElement(NULL);
	SetCurrentFormElement(NULL);
	ResetFocusedElement();

	url_target_element.Reset();
	active_html_element.Reset();
	active_pseudo_html_element.Reset();
	hover_html_element.Reset();
	hover_pseudo_html_element.Reset();
	navigation_element.Reset();
	saved_scroll_to_element.Reset();
	hover_referencing_html_element.Reset();
#ifdef DRAG_SUPPORT
	prev_immediate_selection_elm.Reset();
	immediate_selection_elm.Reset();
	current_target_elm.Reset();
	g_drag_manager->OnDocumentUnload(this);
#endif // DRAG_SUPPORT

	navigation_element_is_highlighted = FALSE;

	area_selected = FALSE; // ******

	all_loaded_sent = FALSE;

	loading_finished = FALSE;

#ifndef RAMCACHE_ONLY
	frames_doc->GetURL().FreeUnusedResources();
#endif

	if (search_selection)
	{
		OP_DELETE(search_selection);
		search_selection = NULL;
	}

#if defined SEARCH_MATCHES_ALL && !defined HAS_NO_SEARCHTEXT
	selections_map.RemoveAll();
	selections.Clear();
#endif // SEARCH_MATCHES_ALL && !HAS_NO_SEARCHTEXT

	image_form_selected = FALSE;

	return TRUE;
}

HTML_Document::HTML_Document(FramesDocument* frm_doc, DocumentManager* doc_manager)
  : doc_manager(doc_manager)
  , frames_doc(frm_doc)
#if defined SEARCH_MATCHES_ALL && !defined HAS_NO_SEARCHTEXT
  , active_selection(NULL)
#endif // SEARCH_MATCHES_ALL && !HAS_NO_SEARCHTEXT
  , search_selection(NULL)
  , image_form_selected(FALSE)
  , css_handling(CSS_NONE)
  , all_loaded_sent(FALSE)
  , send_all_loaded(TRUE)
  , loading_finished(FALSE)
  , navigation_element_is_highlighted(FALSE)
  , element_with_selection(NULL)
  , current_focused_formobject((void*)0xabcdef00) // Temporary to fix compile problem with spatial_navigation that tries to access this instead FramesDocument::current_focused_formobject
  , area_selected(FALSE)
  , record_mouse_actions(0)
  , replaying_recorded_mouse_actions(FALSE)
  , mouse_action_yielded(FALSE)
{
	show_img = GetWindow()->ShowImages() || frames_doc->GetURL().IsImage();

#ifndef MOUSELESS
	if (CoreView *view = GetVisualDevice()->GetView())
		view->GetMousePos(&last_mousemove_x, &last_mousemove_y);
#endif
}

HTML_Document::~HTML_Document()
{
	SetAsCurrentDoc(FALSE);

	OP_DELETE(search_selection);

	OP_ASSERT(recorded_mouse_actions.Empty()); // Should be emptied by SetAsCurrentDoc(FALSE) above

#ifdef _SPAT_NAV_SUPPORT_
	Window* win = GetWindow();
	if (win->GetSnHandler())
	{
		win->GetSnHandler()->DocDestructed ( this );
	}
#endif // _SPAT_NAV_SUPPORT_
}

OP_DOC_STATUS HTML_Document::Display(const RECT& rect, VisualDevice* vd)
{
	OP_PROBE1(OP_PROBE_HTML_DOCUMENT_DISPLAY);

	HTML_Element* doc_root = frames_doc->GetDocRoot();

	if (doc_root && doc_root->GetLayoutBox())
	{
		int min_color_contrast = 0;
		long light_font_color = OP_RGB(255, 255, 255);
		long dark_font_color = OP_RGB(0, 0, 0);

		LayoutMode layout_mode = frames_doc->GetLayoutMode();

		if (layout_mode != LAYOUT_NORMAL)
		{
			PrefsCollectionDisplay::RenderingModes rendering_mode;
			switch (layout_mode)
			{
			case LAYOUT_SSR:
				rendering_mode = PrefsCollectionDisplay::SSR;
				break;
			case LAYOUT_CSSR:
				rendering_mode = PrefsCollectionDisplay::CSSR;
				break;
			case LAYOUT_AMSR:
				rendering_mode = PrefsCollectionDisplay::AMSR;
				break;
#ifdef TV_RENDERING
			case LAYOUT_TVR:
				rendering_mode = PrefsCollectionDisplay::TVR;
				break;
#endif // TV_RENDERING
			default:
				rendering_mode = PrefsCollectionDisplay::MSR;
				break;
			}

			const uni_char *hostname = frames_doc->GetHostName();
			min_color_contrast = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::MinimumTextColorContrast), hostname);
			light_font_color = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::TextColorLight), hostname);
			dark_font_color = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::TextColorDark), hostname);
		}

#ifdef _PRINT_SUPPORT_
		FramesDocument* old_frames_doc = frames_doc->GetHLDocProfile()->GetFramesDocument();
		VisualDevice* old_vis_dev = frames_doc->GetHLDocProfile()->GetVisualDevice();
		if (frames_doc->IsPrintDocument())
		{
			HLDocProfile* hld_profile = frames_doc->GetHLDocProfile();
			hld_profile->SetFramesDocument(frames_doc);

			LayoutWorkplace *layout_workplace = frames_doc->GetLogicalDocument()->GetLayoutWorkplace();
			layout_workplace->SwitchPrintingDocRootProperties();

			hld_profile->SetVisualDevice(vd);
		}
#endif // _PRINT_SUPPORT

		OP_ASSERT(frames_doc->GetLogicalDocument()); // Since we have a root element
		LayoutWorkplace* wp = frames_doc->GetLogicalDocument()->GetLayoutWorkplace();
		wp->SetCanYield(TRUE);

		PaintObject paint_object(frames_doc,
								 vd,
								 rect,
								 GetTextSelection(),
								 IsNavigationElementHighlighted() ? GetNavigationElement() : NULL,
#ifdef _PRINT_SUPPORT_
								 (GetWindow()->GetPreviewMode() || vd->IsPrinter()) && !g_pcprint->GetIntegerPref(PrefsCollectionPrint::PrintBackground, frames_doc->GetHostName()),
#endif // _PRINT_SUPPORT_
								 min_color_contrast,
								 light_font_color,
								 dark_font_color);

		OP_STATUS stat = paint_object.Traverse(doc_root);

		wp->SetCanYield(FALSE);

		if (stat == OpStatus::ERR_YIELD)
		{
			vd->Update(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, TRUE);
			return DocStatus::DOC_CANNOT_DISPLAY;
		}

#ifdef _PRINT_SUPPORT_
		if (frames_doc->IsPrintDocument())
		{
			HLDocProfile* hld_profile = frames_doc->GetHLDocProfile();
			hld_profile->SetFramesDocument(old_frames_doc);

			LayoutWorkplace *layout_workplace = frames_doc->GetLogicalDocument()->GetLayoutWorkplace();
			layout_workplace->SwitchPrintingDocRootProperties();

			frames_doc->GetHLDocProfile()->SetVisualDevice(old_vis_dev);
		}
#endif //_PRINT_SUPPORT_

		if (IsNavigationElementHighlighted())
		{
			// Invert any highlighted area or image form elements

			stat = DrawHighlight(paint_object.GetHighlightRects(), paint_object.GetNumHighlightRects(), paint_object.GetHiglightClipRect());
			if (OpStatus::IsError(stat))
				return static_cast<OP_DOC_STATUS>(stat);
		}

		return paint_object.IsOutOfMemory() ? (OP_DOC_STATUS) OpStatus::ERR_NO_MEMORY : (OP_DOC_STATUS) DocStatus::DOC_DISPLAYED;
	}
#ifdef GADGET_SUPPORT
	else if (!GetWindow()->IsBackgroundTransparent() && GetWindow()->GetType() != WIN_TYPE_GADGET)
#else // GADGET_SUPPORT
	else if (!GetWindow()->IsBackgroundTransparent())
#endif // GADGET_SUPPORT
	{
		vd->DrawBgColor(rect);
	}

	return DocStatus::DOC_CANNOT_DISPLAY;
}

OP_STATUS HTML_Document::UndisplayDocument()
{
	LogicalDocument* logdoc = frames_doc->GetLogicalDocument();
	if (logdoc && logdoc->GetRoot())
		logdoc->GetRoot()->DisableContent(frames_doc);

#ifndef HAS_NO_SEARCHTEXT
# ifdef SEARCH_MATCHES_ALL
	if (active_selection)
		GetWindow()->ResetSearch();
# endif // SEARCH_MATCHES_ALL
#endif // HAS_NO_SEARCHTEXT

	return(OpStatus::OK);
}

HTML_Element* HTML_Document::GetHTML_Element(int x, int y, BOOL text_nodes /* = TRUE */)
{
	LogicalDocument* logdoc = frames_doc->GetLogicalDocument();

	if (logdoc && logdoc->GetRoot())
	{
		Box* inner_box = logdoc->GetRoot()->GetInnerBox(x, y, frames_doc, text_nodes);

		if (inner_box)
			return (inner_box->GetHtmlElement()->Type() == HE_DOC_ROOT) ? logdoc->GetDocRoot() : inner_box->GetHtmlElement();
	}

	return NULL;
}

void HTML_Document::SetHoverHTMLElementSilently(HTML_Element* hover)
{
	// May only be used when moving the hover upwards in a tree, typically, when part
	// of a tree is going away. Then pseudo classes stay the same and no new events
	// need to be sent.
	OP_ASSERT(!hover || hover->IsAncestorOf(hover_html_element.GetElm()));
	hover_html_element.SetElm(hover);
	if (hover && hover->IsAncestorOf(hover_pseudo_html_element.GetElm()))
		hover_pseudo_html_element.SetElm(hover);
}

void HTML_Document::SetHoverHTMLElement(HTML_Element* hover, BOOL apply_hover_style)
{
	while (hover && hover->GetInserted() == HE_INSERTED_BY_LAYOUT)
		hover = hover->Parent();

	HTML_Element* new_pseudo_elm = NULL;
	HTML_Element* new_top_pseudo_elm = NULL;
	HTML_Element* common_parent = NULL;

	if (hover != hover_html_element.GetElm())
	{
		HTML_Element* old_top_pseudo_elm = NULL;

		if (apply_hover_style)
		{
			// Find the new hover_pseudo_html_element and check if we have a common dynamic pseudo element parent.
			for (HTML_Element* parent = hover; parent; parent = parent->Parent())
				if (parent->HasDynamicPseudo())
				{
					if (!new_pseudo_elm)
						new_pseudo_elm = parent;

					if (hover_pseudo_html_element.GetElm() && parent->IsAncestorOf(hover_pseudo_html_element.GetElm()))
					{
						common_parent = parent;
						break;
					}
					else
						new_top_pseudo_elm = parent;
				}

			// Find the top_pseudo_elm for the old hover element.
			HTML_Element* pseudo = hover_pseudo_html_element.GetElm();
			for (; pseudo; pseudo = pseudo->Parent())
			{
				if (pseudo == common_parent)
					break;
				else if (pseudo->HasDynamicPseudo())
					old_top_pseudo_elm = pseudo;
			}
		}

		// Set the new hover element.
		hover_html_element.SetElm(hover);
		hover_pseudo_html_element.SetElm(new_pseudo_elm);

		if (apply_hover_style)
		{
			// Clear old hover style.
			if (old_top_pseudo_elm)
				OpStatus::Ignore(frames_doc->GetLogicalDocument()->GetLayoutWorkplace()->ApplyPropertyChanges(old_top_pseudo_elm, CSS_PSEUDO_CLASS_GROUP_MOUSE, TRUE));

			// Apply hover style to the new hovered element, starting from top pseudo element of new hover element,
			// or closest common parent with old hovered element.
			if (new_top_pseudo_elm)
				OpStatus::Ignore(frames_doc->GetLogicalDocument()->GetLayoutWorkplace()->ApplyPropertyChanges(new_top_pseudo_elm, CSS_PSEUDO_CLASS_GROUP_MOUSE, TRUE));
		}
	}
}

void HTML_Document::SetActiveHTMLElement(HTML_Element* active)
{
	if (active != active_html_element.GetElm())
	{
		HTML_Element* previous_active_pseudo = active_pseudo_html_element.GetElm();

		active_pseudo_html_element.Reset();

		active_html_element.SetElm(active);

		if (previous_active_pseudo)
			// Turn off old active element with pseudo class
			frames_doc->GetLogicalDocument()->GetLayoutWorkplace()->ApplyPropertyChanges(previous_active_pseudo, CSS_PSEUDO_CLASS_ACTIVE, TRUE);

		if (active_html_element.GetElm())
		{
			// Look for new active element with pseudo class

			HTML_Element* pseudo = active;

			while (pseudo)
			{
				if (pseudo->HasDynamicPseudo())
					break;

				pseudo = pseudo->Parent();
			}

			if (pseudo)
			{
				// Apply if we found it
				active_pseudo_html_element.SetElm(pseudo);
				frames_doc->GetLogicalDocument()->GetLayoutWorkplace()->ApplyPropertyChanges(pseudo, CSS_PSEUDO_CLASS_ACTIVE, TRUE);
			}
		}
	}
}

ScrollableArea *GetScrollableParent(HTML_Element* helm)
{
	while (helm)
	{
		if (Box* box = helm->GetLayoutBox())
			if (box->GetScrollable())
				return box->GetScrollable();

		helm = helm->Parent();
	}

	return NULL;
}

#ifndef MOUSELESS

void HTML_Document::MouseOut()
{
	GetWindow()->ClearMessage();

	RecalculateHoverElement();
}

void HTML_Document::RecalculateHoverElement()
{
	int x, y;
	VisualDevice* vis_dev = GetVisualDevice();

	if (CoreView *view = vis_dev->GetView())
	{
		if (frames_doc->IsReflowing() || frames_doc->IsUndisplaying())
			return;

		view->GetMousePos(&x, &y);
		int view_width, view_height; // not to be confused with the member variables with the same name in the base class.
		view->GetSize(&view_width, &view_height);
		BOOL outside_document = x < 0 || x >= (int)view_width || y < 0 || y >= (int)view_height;
		int view_x = vis_dev->GetRenderingViewX();
		int view_y = vis_dev->GetRenderingViewY();
		x = vis_dev->ScaleToDoc(x) + view_x;
		y = vis_dev->ScaleToDoc(y) + view_y;

		// FIXME: Faking a mouse event has side events that might be
		// unexpected and unwanted. We should figure out what
		// this method wants to do and do it in a less disturbing way.
		if (x != last_mousemove_x || y != last_mousemove_y)
			MouseAction(ONMOUSEMOVE, x, y, frames_doc->GetVisualViewX(), frames_doc->GetVisualViewY(), MOUSE_BUTTON_1, FALSE, FALSE, FALSE, outside_document);
	}
}

// Widgets want the top document position, and correct for internal
// widgets themselves.
static OpRect GetDocumentPosForWidget(OpWidget* widget)
{
	while (widget->GetParent())
		widget = widget->GetParent();

#if 0
	// This assert tries to capture when we are asked to send events to widgets that aren't inside the
	// document. When we do that the coordinates will be wrong since we assume that the widget and
	// this document are conncted.
	// I've disabled the assert since this happes too much and people get the assert when trying to use
	// opera. See for instance bug 233906.
	OP_ASSERT(widget->GetFormObject() ||
			  widget->GetType() == OpTypedObject::WIDGET_TYPE_BUTTON || // HE_BUTTON has no FormObject, but they have a widget
			  widget->GetType() == OpTypedObject::WIDGET_TYPE_SCROLLBAR); // Test that it is the HTML widget or a scrollbar, the only widgets we have on a web page
#endif // 0

	return widget->GetDocumentRect();
}

// Convert point (x, y) in document coordinates into a point that is
// relative to the widget(-hierarchy)
static OpPoint GetInWidgetCoordinates(OpWidget* widget, int x, int y)
{
	// Get the topmost widget (the one holding the [relevant] document position)
	while (widget->GetParent())
		widget = widget->GetParent();

#if 0
	// This assert tries to capture when we are asked to send events to widgets that aren't inside the
	// document. When we do that the coordinates will be wrong since we assume that the widget and
	// this document are conncted.
	// I've disabled the assert since this happes too much and people get the assert when trying to use
	// opera. See for instance bug 233906.
	OP_ASSERT(widget->GetFormObject() ||
			  widget->GetType() == OpTypedObject::WIDGET_TYPE_BUTTON || // HE_BUTTON has no FormObject, but they have a widget
			  widget->GetType() == OpTypedObject::WIDGET_TYPE_SCROLLBAR); // Test that it is the HTML widget or a scrollbar, the only widgets we have on a web page
#endif // 0

	// Transform to the local widget coordinate system
	OpPoint p(x, y);
	AffinePos widget_ctm = widget->GetPosInDocument();
	widget_ctm.ApplyInverse(p);

	return p;
}

static BOOL IsSameHoveredElement(HTML_Element *current, HTML_Element *next)
{
	HTML_Element *iter1 = DOM_Utils::GetActualEventTarget(current), *iter2 = DOM_Utils::GetActualEventTarget(next);

	while (iter1 && iter1 == iter2)
	{
		iter1 = DOM_Utils::GetEventPathParent(iter1, current);
		iter2 = DOM_Utils::GetEventPathParent(iter2, next);
	}

	return iter1 == iter2;
}



/**
 * Normally we just locate the HTML_Element here and send the event to
 * it through FramesDocument::HandleEvent, but then there is
 * scrollbars and all kinds of form widgets to complicate matters.
 *
 * <h1>Element types with special handling<h1>
 *
 * <h2>Button types (&lt;button&gt; and input
 * type=button,submit,add,reset,remove,move-up,move-down</h2>
 *
 * These are to be treated as normal HTML elements visavi the document
 * events, but with a couple of exceptions.  First, a button becomes
 * "active" by pressing the mouse down in it, and any mouseup on it
 * should result in a click. Alsp, even if the mouse is moved out and
 * then in again, it should still click it. To make the button draw
 * correctly, it might have to know about events happening outside
 * it. While &lt;button&gt; and input buttons should act the same for
 * the user, they're very different to us since one has a FormObject
 * (inputs) and the other has only a hidden OpWidget.
 *
 * <h2>Checkboxes and radiobuttons</h2>
 *
 * These works as buttons, but the value should be changed before
 * onclick and restored to the original value if the onclick event is
 * cancelled.
 *
 * <h2>Text types (textarea, input type=text, type=number,url,email....)</h2>
 *
 * These capture mouseevents after a mousedown to be able to handle
 * text selection with a mouse. No events should be sent to anything
 * but the control.
 *
 * <h2>selects as a listbox</h2>
 *
 * mousedown on a option should result in a onmousedown event to the
 * option. After that the select box should capture all events to be
 * able to handle drag selection.  A mouseup on the same option should
 * result in a mouseup on it and an onclick on it. A mouseup outside
 * the listbox should result in a mouseup on the select, and no
 * onclick.
 *
 * <h2>selects as a dropdown</h2>
 *
 * The dropdown should be handled as a button, but it should open on
 * mousedown. When it is open, a click on an option should result in a
 * onmousedown,onmouseup,onclick on the option element.
 *
 * <h2>scrollbars</h2>
 *
 * Many widgets have scrollbars, and even normal html element may have
 * scrollbars (overflow:auto/scroll).  Whenever a scrollbar gets a
 * mousedown it starts capturing mousevents and no events should be
 * sent to the document tree.
 *
 * <h2>disabled elements</h2>
 *
 * Form elements can be disabled with the "disabled" attribute.
 * If a form element is disabled, it should not receive
 * the dom events onmousedown, nor onmouseup or onclick.
 *
 * <h1>Internal state</h1>
 *
 * To our help we have a couple of different pointers:
 *
 * <p> OpWidget::hooked_widget (also known as
 * g_widget_globals->captured_widget)<br>
 * This is a widget wanting to know
 * about all mouse events.
 *
 * <p> active_html_element (not to be mixed up with the local variable
 * |active|)<br>
 * This the element to got the mousedown and is "armed" for
 * a mouseup which will trigger the onclick. Any mouseup elsewhere
 * won't cause a onclick.
 *
 * <p> captured_widget_element<br>
 * This is the form element capturing mouse events.
 *
 * <p> last_mousemove_x, last_mousemove_y<br>
 * Global variables with the coordinates of the last mousemove event.
 *
 * <h1>Communication with elements</h1>
 *
 * Communication directly with a widget is done through
 * OpWidget::GenerateOnMouseLeave, OpWidget::GenerateOnMouseDown and
 * OpWidget::GenerateOnMouseUp. These might have side effects if they
 * have widget listeners.
 *
 * <p>Communication with form elements are done through
 * FormObject::OnMouseDown, FormObject::OnMouseUp and
 * FormObject::OnMouseMove. These also talk to widgets and might have
 * side effects (such as sending dom events).
 *
 * <p>Normal events and elements are called through
 * FramesDocument::HandleEvent.
 *
 * <h1>DocumentEdit</h1>
 *
 * In document edit mode, we intercept all mousedown events to avoid
 * triggering actions and instead we use mousedown events to position
 * the cursor.
 */

class RecordedPointAction
	: public ListElement<RecordedPointAction>
{
public:
	enum Type { Mouse, Touch } type;

	RecordedPointAction(Type type, DOM_EventType event, int x, int y, int visual_viewport_x, int visual_viewport_y, BOOL shift_pressed, BOOL control_pressed, BOOL alt_pressed)
		: type(type), event(event), x(x), y(y), visual_viewport_x(visual_viewport_x), visual_viewport_y(visual_viewport_y), shift_pressed(shift_pressed), control_pressed(control_pressed)
		, alt_pressed(alt_pressed) {}

	virtual DocAction Action(HTML_Document*) { return DOC_ACTION_NONE; }

	DOM_EventType event;
	int x, y;
	int visual_viewport_x, visual_viewport_y;
	BOOL shift_pressed, control_pressed, alt_pressed;
};

class RecordedMouseAction
	: public RecordedPointAction
{
public:
	RecordedMouseAction(DOM_EventType event, int x, int y, int visual_viewport_x, int visual_viewport_y, int sequence_count_and_button_or_delta, BOOL shift_pressed,
			BOOL control_pressed, BOOL alt_pressed, BOOL outside_document, BOOL simulated = FALSE, int radius = 0)
		: RecordedPointAction(Mouse, event, x, y, visual_viewport_x, visual_viewport_y, shift_pressed, control_pressed, alt_pressed)
		, sequence_count_and_button_or_delta(sequence_count_and_button_or_delta)
		, radius(radius), outside_document(outside_document), simulated(simulated) {}

	DocAction Action(HTML_Document* doc) { return doc->MouseAction(event, x, y, visual_viewport_x, visual_viewport_y, sequence_count_and_button_or_delta,
			shift_pressed, control_pressed, alt_pressed, outside_document, simulated, radius); }

	int sequence_count_and_button_or_delta, radius;
	BOOL outside_document, simulated;
};

#ifdef TOUCH_EVENTS_SUPPORT
class RecordedTouchAction
	: public RecordedPointAction
{
public:
	RecordedTouchAction(DOM_EventType event, int id, int x, int y, int visual_viewport_x, int visual_viewport_y, int radius, int sequence_count_and_button_or_delta,
			BOOL shift_pressed, BOOL control_pressed, BOOL alt_pressed, void* user_data)
		: RecordedPointAction(Touch, event, x, y, visual_viewport_x, visual_viewport_y, shift_pressed, control_pressed, alt_pressed)
		, id(id), radius(radius), sequence_count_and_button_or_delta(sequence_count_and_button_or_delta), user_data(user_data) {}

	DocAction Action(HTML_Document* doc) { return doc->TouchAction(event, id, x, y, visual_viewport_x, visual_viewport_y, radius, sequence_count_and_button_or_delta,
			shift_pressed, control_pressed, alt_pressed, user_data); }

	int id, radius, sequence_count_and_button_or_delta;
	void* user_data;
};
#endif // TOUCH_EVENTS_SUPPORT

void HTML_Document::ReplayRecordedMouseActions()
{
	// a yielded point action always gets through
	if (mouse_action_yielded || ((record_mouse_actions == 0 || --record_mouse_actions == 0) && !replaying_recorded_mouse_actions))
	{
		replaying_recorded_mouse_actions = TRUE;

		while (RecordedPointAction *point_action = recorded_mouse_actions.First())
		{
			if (!frames_doc->IsUndisplaying())
				if (point_action->Action(this) == DOC_ACTION_YIELD)
					return;

			point_action->Out();
			OP_DELETE(point_action);

			if (record_mouse_actions != 0)
				break;
		}

		replaying_recorded_mouse_actions = FALSE;
	}
}

void HTML_Document::CleanRecordedMouseActions()
{
	RecordedPointAction* a = recorded_mouse_actions.First();

	while (a)
	{
		RecordedPointAction* suc = a->Suc();

		if (a->event == ONMOUSEMOVE)
		{
			a->Out();
			OP_DELETE(a);
		}
		a = suc;
	}
}

#if defined DRAG_SUPPORT && defined GADGET_SUPPORT
OP_STATUS
HTML_Document::CheckControlRegionOnParents(FramesDocument* frm_doc, int x, int y, BOOL &control_region_found)
{
	control_region_found = FALSE;
	FramesDocument* current_doc = frames_doc;
	while (!control_region_found && current_doc->GetParentDoc())
	{
		FramesDocElm* frm_doc_elem = current_doc->GetDocManager()->GetFrame();
		OP_ASSERT(frm_doc_elem);
		int current_x = x + frm_doc_elem->GetAbsX();
		int current_y = y + frm_doc_elem->GetAbsY();
		current_doc = current_doc->GetParentDoc();

		// Need text boxes so that we can, in the case of broken HTML, be sure
		// of what part of a tree the user clicked on. Two text nodes can be at
		// the same place in the document tree but depending on last_descendant
		// pointers they can have different semantics. See for instance bug 367494.
		const BOOL include_text_nodes = TRUE;
		IntersectionObject current_intersection_obj(current_doc, LayoutCoord(current_x), LayoutCoord(current_y), include_text_nodes) ;
		current_intersection_obj.Traverse(current_doc->GetLogicalDocument()->GetRoot());
		control_region_found = current_intersection_obj.HasControlRegionBeenFound();
	}
	return OpStatus::OK;
}
#endif // defined DRAG_SUPPORT && defined GADGET_SUPPORT

/* virtual */
DocAction HTML_Document::MouseAction(DOM_EventType event, int x, int y, int visual_viewport_x, int visual_viewport_y, int sequence_count_and_button_or_delta, BOOL shift_pressed, BOOL control_pressed, BOOL alt_pressed, BOOL outside_document, BOOL simulated /* = FALSE */, int radius /* = 0 */)
{
#ifdef DRAG_SUPPORT
	/* The input events (mouse, keyboard, touch) must be suppressed while dragging and their queue must be emptied (it's done by ReplayRecordedMouseActions()).
	The exceptions here are to make mouse scroll working durring the drag. ONMOUSEUP is here to balance OMOUSEDOWN sent before d'n'd was started. */
	if (g_drag_manager->IsDragging() && !g_drag_manager->IsBlocked() && event != ONMOUSEWHEEL && event != ONMOUSEWHEELH && event != ONMOUSEWHEELV && event != ONMOUSEUP)
	{
		ReplayRecordedMouseActions();
		return DOC_ACTION_NONE;
	}
#endif // DRAG_SUPPORT
	if (record_mouse_actions != 0 || recorded_mouse_actions.First() && !replaying_recorded_mouse_actions)
	{
		if (RecordedMouseAction *recorded_mouse_action = OP_NEW(RecordedMouseAction, (event, x, y, visual_viewport_x, visual_viewport_y, sequence_count_and_button_or_delta, shift_pressed, control_pressed, alt_pressed, outside_document, simulated, radius)))
		{
#ifdef DRAG_SUPPORT
			if (event == ONMOUSEDOWN)
				g_drag_manager->OnRecordMouseDown();
#endif // DRAG_SUPPORT
			recorded_mouse_action->Into(&recorded_mouse_actions);
		}
		return DOC_ACTION_NONE;
	}

	// We always need a mousemove event before taking care of any other event on a coordinate since mousemove activates whatever is below the mouse cursor
	if ((x != last_mousemove_x || y != last_mousemove_y || (!hover_html_element.GetElm() == !outside_document)) && event != ONMOUSEMOVE)
	{
		// This extra mouse move action must not clear the blocking widget. (DSK-182869)
		OpDropDown* widget = g_widget_globals->m_blocking_popup_dropdown_widget;
		MouseAction(ONMOUSEMOVE, x, y, visual_viewport_x, visual_viewport_y, 0, FALSE, FALSE, FALSE, outside_document);
		g_widget_globals->m_blocking_popup_dropdown_widget = widget;
	}

	ShiftKeyState modifiers = (shift_pressed ? SHIFTKEY_SHIFT : 0) | (control_pressed ? SHIFTKEY_CTRL : 0) | (alt_pressed ? SHIFTKEY_ALT : 0);

	LogicalDocument* logdoc = frames_doc->GetLogicalDocument();

	int sequence_count = EXTRACT_SEQUENCE_COUNT(sequence_count_and_button_or_delta);

#ifdef NEARBY_ELEMENT_DETECTION
	if (event == ONMOUSEUP)
		if (ElementExpander* expander = GetWindow()->GetElementExpander())
			expander->Hide(TRUE, TRUE);

	OpAutoPtr<ElementExpander> element_expander;
#endif // NEARBY_ELEMENT_DETECTION

	int old_last_mousemove_x = last_mousemove_x;
	int old_last_mousemove_y = last_mousemove_y;

	if (event == ONMOUSEMOVE)
	{
		last_mousemove_x = x;
		last_mousemove_y = y;
	}

	if (logdoc && logdoc->GetRoot())
	{
		HTML_Element* h_elm = NULL;
		HTML_Element* referencing_element = NULL;
		int offset_x = 0, offset_y = 0;

		BOOL has_capture = ((event == ONMOUSEMOVE || event == ONMOUSEUP) && captured_widget_element.GetElm() && OpWidget::hooked_widget);
		if (has_capture)
		{
			// Buttons don't capture for real
			if (captured_widget_element->IsMatchingType(HE_BUTTON, NS_HTML))
			{
				has_capture = FALSE;
			}
			else if (captured_widget_element->IsMatchingType(HE_INPUT, NS_HTML))
			{
				// These are the types that don't "capture" events.
				// Generally it's all types which has a meaningful "onclick" event.
				InputType inp_type = captured_widget_element->GetInputType();
				BOOL is_button_type =
					inp_type == INPUT_BUTTON || inp_type == INPUT_RESET ||
					inp_type == INPUT_SUBMIT ||
					inp_type == INPUT_CHECKBOX ||
					inp_type == INPUT_RADIO;
				if (is_button_type)
				{
					has_capture = FALSE;
				}
			}
			else if (captured_widget_element->IsMatchingType(HE_SELECT, NS_HTML))
			{
				BOOL is_dropdown = !captured_widget_element->GetBoolAttr(ATTR_MULTIPLE) &&
					captured_widget_element->GetNumAttr(ATTR_SIZE) <= 1;
				if (is_dropdown)
				{
					has_capture = FALSE;
				}
			}
		}

		OpAutoPtr<IntersectionObject> intersection_owner;
		IntersectionObject* intersection = NULL;

		if (has_capture)
		{
			// If a widget is hooked, we should only run events on that element so no links or hover will change cursor etc.
			h_elm = captured_widget_element.GetElm();

			OpRect r = GetDocumentPosForWidget(OpWidget::hooked_widget);

			offset_x = x - r.x;
			offset_y = y - r.y;
		}
		else if (!outside_document)
		{
#if defined DRAG_SUPPORT && defined GADGET_SUPPORT
			WindowCommander* win_com = GetWindow()->GetWindowCommander();
#endif // DRAG_SUPPORT && GADGET_SUPPORT

			// Need text boxes so that we can, in the case of broken HTML, be sure
			// of what part of a tree the user clicked on. Two text nodes can be at
			// the same place in the document tree but depending on last_descendant
			// pointers they can have different semantics. See for instance bug 367494.
			const BOOL include_text_nodes = TRUE;
			intersection = OP_NEW(IntersectionObject, (frames_doc, LayoutCoord(x), LayoutCoord(y), include_text_nodes));

			if (!intersection)
				goto raise_oom;

			intersection_owner.reset(intersection);

#ifdef NEARBY_ELEMENT_DETECTION
			if (event == ONMOUSEUP && ElementExpander::IsEnabled() &&
				GetWindow()->GetType() != WIN_TYPE_GADGET &&
				(!GetTextSelection() || GetTextSelection()->IsEmpty()))
			{
				OP_ASSERT(frames_doc);
				OP_ASSERT(frames_doc->GetWindow());
				if (radius == 0)
					radius = ElementExpander::GetFingertipPixelRadius(frames_doc->GetWindow()->GetOpWindow());
				element_expander.reset(ElementExpander::Create(frames_doc, OpPoint(x, y), radius));

				if (!element_expander.get())
					goto raise_oom;

				intersection->EnableEoiDetection(element_expander.get(), radius);
			}
#endif // NEARBY_ELEMENT_DETECTION

			LayoutWorkplace* wp = logdoc->GetLayoutWorkplace();

			if (wp)
				wp->SetCanYield(TRUE);

			OP_STATUS status = OpStatus::ERR_YIELD;

			if (!wp || !wp->IsTraversing())
				status = intersection->Traverse(frames_doc->GetDocRoot());

			if (status == OpStatus::ERR_YIELD)
			{
				mouse_action_yielded = TRUE;

				if (!replaying_recorded_mouse_actions)
				{
					if (event == ONMOUSEMOVE)
						CleanRecordedMouseActions();

					if (RecordedMouseAction *recorded_mouse_action = OP_NEW(RecordedMouseAction, (event, x, y, visual_viewport_x, visual_viewport_y, sequence_count_and_button_or_delta, shift_pressed, control_pressed, alt_pressed, outside_document)))
						recorded_mouse_action->Into(&recorded_mouse_actions);
				}

				if (!GetFramesDocument()->GetMessageHandler()->HasCallBack(GetFramesDocument(), MSG_REPLAY_RECORDED_MOUSE_ACTIONS, 0))
					GetFramesDocument()->GetMessageHandler()->SetCallBack(GetFramesDocument(), MSG_REPLAY_RECORDED_MOUSE_ACTIONS, 0);

				GetFramesDocument()->GetMessageHandler()->PostMessage(MSG_REPLAY_RECORDED_MOUSE_ACTIONS, 0, 0);
				return DOC_ACTION_YIELD;
			}

			mouse_action_yielded = FALSE;

			if (wp)
				wp->SetCanYield(FALSE);

			if (intersection->IsOutOfMemory())
				goto raise_oom;

#ifdef DRAG_SUPPORT
# ifdef GADGET_SUPPORT
			if (event == ONMOUSEDOWN &&
			    GetWindow()->GetType() == WIN_TYPE_GADGET)
			{
				BOOL control_region_found = intersection->HasControlRegionBeenFound();
				if (!control_region_found)
					if (OpStatus::IsError(CheckControlRegionOnParents(frames_doc, x, y, control_region_found)))
						goto raise_oom;

				if (!control_region_found)
					win_com->GetDocumentListener()->OnGadgetDragRequest(NULL);
			}
# endif // GADGET_SUPPORT
#endif // DRAG_SUPPORT

			Box* inner_box = intersection->GetInnerBox();

			offset_x = intersection->GetRelativeX();
			offset_y = intersection->GetRelativeY();

			if (inner_box)
			{
				h_elm = inner_box->GetHtmlElement();

				// We sometimes have text nodes here for internal purposes. Externally we'll show an element
				// so the offset needs to be relative that element (which is the parent of the text node).
				if (h_elm->IsText())
				{
					HTML_Element* parent = h_elm->ParentActual();
					if (parent && parent->GetLayoutBox())
					{
						LayoutCoord elm_offset_x;
						LayoutCoord elm_offset_y;
						if (intersection->GetRelativeToBox(parent->GetLayoutBox(), elm_offset_x, elm_offset_y))
						{
							offset_x = elm_offset_x;
							offset_y = elm_offset_y;
						}
					}
				}

#ifdef ADAPTIVE_ZOOM_SUPPORT
				ViewportController* controller = GetWindow()->GetViewportController();
				OpViewportInfoListener* listener = controller->GetViewportInfoListener();

				OpRect container_rect = intersection->GetContainerRect();
				OpRect line_rect = intersection->GetLineRect();
				FramesDocElm* frame = frames_doc->GetDocManager()->GetFrame();

				if (frame)
				{
					// If the element is inside a frames document, compensate for the
					// position of the frame

					container_rect = controller->ConvertToToplevelRect(frames_doc, container_rect);
					line_rect = controller->ConvertToToplevelRect(frames_doc, line_rect);
				}

				int negative_overflow = NegativeOverflow();

				container_rect.x += negative_overflow;
				line_rect.x += negative_overflow;
				listener->OnAreaOfInterestChanged(controller, container_rect, line_rect);
#endif // ADAPTIVE_ZOOM_SUPPORT

#ifdef NEARBY_ELEMENT_DETECTION
				if (element_expander.get() && !element_expander->Empty())
				{
					// No element_expander should be created for gadgets, so we shouldn't be here if it was
					OP_ASSERT(GetWindow()->GetType() != WIN_TYPE_GADGET);

					/* Found one or more nearby elements. Based on the element we are currently
					   over, determine whether we should expand them or not. */

					BOOL expand_nearby_elements;

					if (h_elm->Type() == HE_BUTTON || h_elm->HasAttr(ATTR_USEMAP))
					{
						// Don't expand BUTTON elements or image maps.

						/* BUTTON elements should ideally be expanded, but due to implementation
						   details, this is currently ~impossible. */

						expand_nearby_elements = FALSE;
					}
					else if (h_elm->GetAnchor_HRef(frames_doc) || FormManager::IsFormElement(h_elm))
					{
						// Always expand elements if over an anchor or form element

						expand_nearby_elements = TRUE;
					}
					else
					{
						/* Expand elements only if over an element that doesn't have certain
						   event handlers. If we decide to expand elements here, the element
						   we're currently over will not receive its event.

						   This is a problematic approach. It all depends on what the event
						   handler does. If it only changes the color or font (or if it does
						   nothing at all), nearby elements could still sensibly be
						   expanded. On the other hand, if they open or close menus, or
						   whatever, nearby elements should not be expanded. We should process
						   the event then.

						   Passing TRUE as the last parameter to HasEventHandler() also has
						   issues, since that means that it will not find parent elements with
						   event handlers (which may be cruicial for a web site to operate
						   properly). */

						expand_nearby_elements =
							!h_elm->HasEventHandler(frames_doc, ONCLICK, TRUE) &&
							!h_elm->HasEventHandler(frames_doc, ONMOUSEDOWN, TRUE) &&
							!h_elm->HasEventHandler(frames_doc, ONMOUSEUP, TRUE) &&
							!h_elm->HasEventHandler(frames_doc, ONMOUSEOVER, TRUE) &&
							!h_elm->HasEventHandler(frames_doc, ONMOUSEENTER, TRUE) &&
							!h_elm->HasEventHandler(frames_doc, ONMOUSEMOVE, TRUE) &&
							!h_elm->HasEventHandler(frames_doc, ONMOUSEOUT, TRUE) &&
							!h_elm->HasEventHandler(frames_doc, ONMOUSELEAVE, TRUE) &&
							!h_elm->HasEventHandler(frames_doc, ONMOUSEWHEEL, TRUE) &&
							!h_elm->HasEventHandler(frames_doc, ONMOUSEWHEELH, TRUE) &&
							!h_elm->HasEventHandler(frames_doc, ONMOUSEWHEELV, TRUE) &&
#ifdef TOUCH_EVENTS_SUPPORT
							!h_elm->HasEventHandler(frames_doc, TOUCHSTART, TRUE) &&
							!h_elm->HasEventHandler(frames_doc, TOUCHMOVE, TRUE) &&
							!h_elm->HasEventHandler(frames_doc, TOUCHEND, TRUE) &&
							!h_elm->HasEventHandler(frames_doc, TOUCHCANCEL, TRUE) &&
#endif // TOUCH_EVENTS_SUPPORT
							!h_elm->HasEventHandler(frames_doc, ONDBLCLICK, TRUE);
					}

					if (expand_nearby_elements)
					{
						ElementExpander* ee = element_expander.release();

						GetWindow()->SetElementExpander(ee);
						OP_BOOLEAN expanded = ee->Show();

						if (expanded == OpBoolean::IS_TRUE)
							return DOC_ACTION_HANDLED;

						GetWindow()->SetElementExpander(NULL);

						if (OpStatus::IsMemoryError(expanded))
							goto raise_oom;
					}
				}
#endif // NEARBY_ELEMENT_DETECTION

#ifdef SUPPORT_VISUAL_ADBLOCK
				// we will block all events in Content block edit mode
				if (GetWindow()->GetContentBlockEditMode() && event != ONMOUSEWHEELH && event != ONMOUSEWHEELV)
				{
					if (event == ONMOUSEUP)
					{
						URL *url_ptr = NULL;
						URL img_url = h_elm->GetImageURL(TRUE, frames_doc->GetLogicalDocument());

						if (!img_url.IsEmpty())
						{
							// happens when clicking on a image
							url_ptr = &img_url;
						}
						else
						{
							// happens if clicking on a placeholder image for a plugin
							url_ptr = h_elm->GetEMBED_URL();
						}
						if(url_ptr)
						{
							// ignore further processing of this click and just notify the document listener
							WindowCommander *win_com = GetWindow()->GetWindowCommander();
							const uni_char *image_url = url_ptr->GetAttribute(URL::KUniName, TRUE).CStr();

							if (image_url)
							{
								if (frames_doc->GetIsURLBlocked(*url_ptr))
								{
									frames_doc->RemoveFromURLBlockedList(*url_ptr);
									win_com->GetDocumentListener()->OnContentUnblocked(win_com, image_url);
								}
								else
								{
									frames_doc->AddToURLBlockedList(*url_ptr);
									win_com->GetDocumentListener()->OnContentBlocked(win_com, image_url);
								}
								h_elm->MarkExtraDirty(frames_doc);
							}
						}
					}
					return DOC_ACTION_HANDLED;
				}
#endif // SUPPORT_VISUAL_ADBLOCK
			}

		}

#ifdef MEDIA_HTML_SUPPORT
		// Elements inserted by track (elements that are part of the rendering
		// tree for a cue) should not be hit, so redirect to their non-anonymous
		// parent (the <video> element). Check inserted type or namespace
		// because there will also be HE_INSERTED_BY_LAYOUT (w/ NS_CUE) elements
		// in a cue fragment.
		if (h_elm && (h_elm->GetInserted() == HE_INSERTED_BY_TRACK || h_elm->GetNsType() == NS_CUE))
			h_elm = h_elm->ParentActual();
#endif // MEDIA_HTML_SUPPORT

#ifdef SVG_SUPPORT
		if (h_elm && h_elm->IsMatchingType(Markup::SVGE_SVG, NS_SVG) && frames_doc->GetShowImages())
		{
			HTML_Element* event_target = NULL;

			g_svg_manager->FindSVGElement(h_elm, frames_doc, x, y, &event_target, offset_x, offset_y);
			if (event_target != NULL)
				h_elm = event_target;
		}
#endif // SVG_SUPPORT

		// == Deal with the platformindependent formswidgets =====================
		BOOL is_capturing_element = h_elm && h_elm->GetFormObject() &&
			(h_elm->GetNsType() == NS_HTML &&
			 (h_elm->Type() == HE_INPUT ||
			  h_elm->Type() == HE_TEXTAREA ||
			  h_elm->Type() == HE_SELECT
#ifdef _SSL_SUPPORT_
			  || h_elm->Type() == HE_KEYGEN
#endif
			  ));
		if (is_capturing_element)
		{
			if (event == ONMOUSEMOVE && !captured_widget_element.GetElm())
				if (FormObject *fo = h_elm->GetFormObject())
				{
					fo->OnMouseMove(OpPoint(x, y));
					fo->UpdatePosition();
				}

			h_elm = h_elm->GetFormObject()->GetInnermostElement(x, y);
		}
		else if (OpWidget::over_widget && !OpWidget::hooked_widget)
		{
			OpWidget* owidget = OpWidget::over_widget;

			// Scrollbars in ScrollableContainer and OpButton in <button> element will come here because they are not FormObjects.
			// We must make sure we are really outside the widget, so we won't call OnMouseLeave if hovering it.

			OpPoint widget_point = GetInWidgetCoordinates(owidget, x, y);
			if (!owidget->GetRect().Contains(widget_point))
				owidget->GenerateOnMouseLeave();
		}

		if (event == ONMOUSEUP)
		{
			SetCapturedWidgetElement(NULL);

			if (OpWidget::hooked_widget)
			{
				OpWidget* hwidget = OpWidget::hooked_widget;

				BOOL is_scrollbar = (hwidget->GetType() == OpTypedObject::WIDGET_TYPE_SCROLLBAR);

				// A captured widget which is not a formsobject.
				// Calculate the mousepos relative to the widget.

				if (is_scrollbar ||
					hwidget->GetType() == OpTypedObject::WIDGET_TYPE_EDIT ||
					hwidget->GetType() == OpTypedObject::WIDGET_TYPE_MULTILINE_EDIT ||
					hwidget->GetType() == OpTypedObject::WIDGET_TYPE_LISTBOX)
				{
					OpPoint widget_point = GetInWidgetCoordinates(hwidget, x, y);

					hwidget->GenerateOnMouseUp(widget_point, current_mouse_button, sequence_count);
				}

				if (is_scrollbar)
					// Do not send events if it was a hooked scrollbar.
					return DOC_ACTION_NONE;
			}
		}

		if (event == ONMOUSEMOVE)
			if (captured_widget_element.GetElm())
			{
				if (captured_widget_element->GetFormObject())
				{
					captured_widget_element->GetFormObject()->OnMouseMove(OpPoint(x, y));
				}
			}
			else
				if (OpWidget::hooked_widget && OpWidget::hooked_widget->GetVisualDevice() == GetVisualDevice())
				{
					OpWidget* hwidget = OpWidget::hooked_widget;

					// A captured widget which is not a formsobject.
					// Calculate the mousepos relative to the widget.
					BOOL is_scrollbar = (hwidget->GetType() == OpTypedObject::WIDGET_TYPE_SCROLLBAR);

					// Otherwise it's a widget outside the document. FIXME Check right/bottom limits too
					OpPoint widget_point;
					if (x >= 0 && y >= 0)
						widget_point = GetInWidgetCoordinates(hwidget, x, y);

					// else it will get a mousemove, but the coordinates will be very wrong
					// Might change/null OpWidget::hooked_widget

					hwidget->GenerateOnMouseMove(widget_point);

					if (is_scrollbar)
						return DOC_ACTION_NONE;
				}
		// =======================================================================

		if (!h_elm && !outside_document && frames_doc->GetHLDocProfile())
		{
			// Send event to the body element if there is no element
			// under the mouse. That can happen if CSS has shrunk
			// the body/html elements to be smaller than the viewport
			h_elm = frames_doc->GetHLDocProfile()->GetBodyElm();

			// If there is no body element either, well, then we're
			// out of luck
		}

		BOOL mouse_changed_position = x != old_last_mousemove_x ||
		                              y != old_last_mousemove_y;

		if (h_elm)
		{
			HTML_Element* actual_element = h_elm;

			if (actual_element->GetInserted() == HE_INSERTED_BY_LAYOUT)
				actual_element = actual_element->ParentActual();

			// Image with usemap?
			if (HTML_Element* linked_element = actual_element->GetLinkedElement(frames_doc, offset_x, offset_y, h_elm))
			{
				referencing_element = actual_element;
				h_elm = linked_element;
			}
		}

		BOOL is_new_hover_element = FALSE;
		if (h_elm != hover_html_element.GetElm())
		{
			if (IsSameHoveredElement(hover_html_element.GetElm(), h_elm))
			{
				// Moving between text nodes inside an element
				SetHoverHTMLElement(h_elm);
				OP_ASSERT(referencing_element == hover_referencing_html_element.GetElm());
			}
			else
			{
				HTML_Element* old_hover_element = hover_html_element.GetElm();
				HTML_Element* old_hover_referencing_element = hover_referencing_html_element.GetElm();
				is_new_hover_element = TRUE;

				GetWindow()->DisplayLinkInformation(NULL, ST_ASTRING, NULL); // Removes an existing status bar or tooltip

				HTML_Element *common_ancestor = NULL;
				if (old_hover_element)
				{
					++record_mouse_actions;
					if (frames_doc->HandleMouseEvent(ONMOUSEOUT,
													 h_elm, old_hover_element, old_hover_referencing_element,
													 offset_x, offset_y,
													 x, y,
													 modifiers,
													 0, NULL, TRUE,
													 FALSE, &visual_viewport_x, &visual_viewport_y) == OpStatus::ERR_NO_MEMORY)
					{
						goto raise_oom;
					}

					// Mouseleave fires when the mouse leaves element and all its descendants.
					// Fire after mouse out.
					common_ancestor = HTML_Element::GetCommonAncestorActual(h_elm, old_hover_element);
					HTML_Element *iterator = old_hover_element;
					while (iterator && iterator != common_ancestor)
					{
						LayoutCoord target_offset_x = LayoutCoord(x);
						LayoutCoord target_offset_y = LayoutCoord(y);
						BOOL calculate_offset_lazily = TRUE;
						if (Box* target_box = iterator->GetLayoutBox())
							if (intersection)
								calculate_offset_lazily = !intersection->GetRelativeToBox(target_box, target_offset_x, target_offset_y);

						if (frames_doc->HandleMouseEvent(ONMOUSELEAVE,
													 h_elm, iterator, old_hover_referencing_element,
													 target_offset_x, target_offset_y,
													 x, y,
													 modifiers,
													 0, NULL, TRUE,
													 calculate_offset_lazily,
													 &visual_viewport_x, &visual_viewport_y) == OpStatus::ERR_NO_MEMORY)
						{
							goto raise_oom;
						}
						iterator = iterator->ParentActual();
					}
				}

				SetHoverHTMLElement(h_elm);
				SetHoverReferencingHTMLElement(referencing_element);

				if (h_elm)
				{
					++record_mouse_actions;

					if (!common_ancestor)
						common_ancestor = HTML_Element::GetCommonAncestorActual(h_elm, old_hover_element);
					// Mouseenter fires when mouse enters an element or any of its descendants
					// for the first time, meaning the related target is not itself nor a descendant.
					// Fire before mouse over, and need to fire from parent to child.
					HE_AncestorToDescendantIterator iterator;
					if (OpStatus::IsMemoryError(iterator.Init(common_ancestor, h_elm, TRUE)))
						goto raise_oom;
					for (HTML_Element *target; (target = iterator.GetNextActual()) != NULL;)
					{
						LayoutCoord target_offset_x = LayoutCoord(0);
						LayoutCoord target_offset_y = LayoutCoord(0);
						if (Box* target_box = target->GetLayoutBox())
							if (intersection)
								intersection->GetRelativeToBox(target_box, target_offset_x, target_offset_y);

						if (frames_doc->HandleMouseEvent(ONMOUSEENTER, old_hover_element,
								target, referencing_element, target_offset_x, target_offset_y, x, y,
								modifiers, 0, NULL, TRUE, FALSE, &visual_viewport_x, &visual_viewport_y) == OpStatus::ERR_NO_MEMORY)
							goto raise_oom;
					}

					if (frames_doc->HandleMouseEvent(ONMOUSEOVER,
													 old_hover_element, h_elm, referencing_element,
													 offset_x, offset_y,
													 x, y,
													 modifiers,
													 0, NULL, TRUE,
													 FALSE, &visual_viewport_x, &visual_viewport_y) == OpStatus::ERR_NO_MEMORY)
					{
						goto raise_oom;
					}
				}
			}
		}

		OP_ASSERT(hover_html_element.GetElm() == h_elm || !h_elm->IsIncludedActual() && h_elm->ParentActual() == hover_html_element.GetElm());

		switch (event)
		{
		case ONCLICK: // Who sends onclick directly? Isn't that always generated internally from mousedown/mouseup pairs?
		case ONMOUSEWHEELH:
		case ONMOUSEWHEELV:
			if (hover_html_element.GetElm() || event != ONCLICK)
			{
				if (hover_html_element.GetElm())
				{
					HTML_Element *target = hover_html_element.GetElm();
					HTML_Element *reftarget = hover_referencing_html_element.GetElm();
					int& delta = sequence_count_and_button_or_delta;

					if (event != ONCLICK)
						++record_mouse_actions;

					if (frames_doc->HandleMouseEvent(event, NULL, target, reftarget, offset_x, offset_y, x, y, modifiers, delta, NULL, FALSE, FALSE, &visual_viewport_x, &visual_viewport_y) == OpStatus::ERR_NO_MEMORY)
						goto raise_oom;
				}

				return DOC_ACTION_HANDLED;
			}
			break;

		case ONDBLCLICK:
			break;

		case ONMOUSEUP:
			{
				if (hover_html_element.GetElm())
				{
					GetWindow()->ClearMessage();

					++record_mouse_actions;
					if (frames_doc->HandleMouseEvent(ONMOUSEUP,
												 NULL, hover_html_element.GetElm(), hover_referencing_html_element.GetElm(),
												 offset_x, offset_y,
												 x, y,
												 modifiers,
												 sequence_count_and_button_or_delta, NULL,
												 FALSE, FALSE,
												 &visual_viewport_x, &visual_viewport_y) == OpStatus::ERR_NO_MEMORY)
					{
						goto raise_oom;
					}
					// Note that hover_html_element may have been deleted (and pointer set to NULL)
					// by the call to HandleMouseEvent, through a chain of mouseup->click->default
					// action on a WF2 remove button (though that one doesn't exist anymore)
				}

#if defined(PEN_DEVICE_MODE) && !defined(DOC_TOUCH_SMARTPHONE_COMPATIBILITY)
				SetHoverHTMLElement(NULL);
#endif // PEN_DEVICE_MODE && !DOC_TOUCH_SMARTPHONE_COMPATIBILITY

#ifndef DOC_TOUCH_SMARTPHONE_COMPATIBILITY
				if (simulated)
					SetHoverHTMLElement(NULL);
#endif // !DOC_TOUCH_SMARTPHONE_COMPATIBILITY
			}
			break;

		case ONMOUSEDOWN:
			{
				if (hover_html_element.GetElm())
				{
#ifdef DRAG_SUPPORT
					g_drag_manager->OnSendMouseDown();
#endif // DRAG_SUPPORT
					++record_mouse_actions;
					if (frames_doc->HandleMouseEvent(ONMOUSEDOWN,
												 NULL, hover_html_element.GetElm(), hover_referencing_html_element.GetElm(),
												 offset_x, offset_y,
												 x, y,
												 modifiers,
												 sequence_count_and_button_or_delta, NULL,
												 FALSE, FALSE,
												 &visual_viewport_x, &visual_viewport_y) == OpStatus::ERR_NO_MEMORY)
					{
						goto raise_oom;
					}
				}
			}
			break;

		case ONMOUSEMOVE:
			{
				if (hover_html_element.GetElm() && (mouse_changed_position || is_new_hover_element))
				{
					++record_mouse_actions;
					if (frames_doc->HandleMouseEvent(ONMOUSEMOVE,
						NULL, hover_html_element.GetElm(),
						hover_referencing_html_element.GetElm(),
						offset_x, offset_y,
						x, y,
						modifiers,
						0, NULL,
						FALSE, FALSE,
						&visual_viewport_x, &visual_viewport_y) == OpStatus::ERR_NO_MEMORY)
					{
						goto raise_oom;
					}
				}
			}
			break;

		default:
				break;

		}
	}

	return DOC_ACTION_NONE;

raise_oom:
	record_mouse_actions = 0;
	GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
	return DOC_ACTION_NONE;
}

#endif // !MOUSELESS

#ifdef TOUCH_EVENTS_SUPPORT
DocAction HTML_Document::TouchAction(DOM_EventType event, int id, int x, int y, int visual_viewport_x, int visual_viewport_y, int radius, int sequence_count_and_button_or_delta, BOOL shift_pressed, BOOL control_pressed, BOOL alt_pressed, void* user_data /* = NULL */)
{
#ifdef DRAG_SUPPORT
	// The input event (mouse, keyboard, touch) must be suppressed while dragging. Empty the recorderd events by replaying them and return.
	if (g_drag_manager->IsDragging() && !g_drag_manager->IsBlocked())
	{
		ReplayRecordedMouseActions();
		return DOC_ACTION_NONE;
	}
#endif // DRAG_SUPPORT

	if (record_mouse_actions != 0 || recorded_mouse_actions.First() && !replaying_recorded_mouse_actions)
	{
		if (RecordedTouchAction *recorded_touch_action = OP_NEW(RecordedTouchAction, (event, id, x, y, visual_viewport_x, visual_viewport_y, radius, sequence_count_and_button_or_delta, shift_pressed, control_pressed, alt_pressed, user_data)))
			recorded_touch_action->Into(&recorded_mouse_actions);
		return DOC_ACTION_NONE;
	}

	ShiftKeyState modifiers = (shift_pressed ? SHIFTKEY_SHIFT : 0) | (control_pressed ? SHIFTKEY_CTRL : 0) | (alt_pressed ? SHIFTKEY_ALT : 0);
	LogicalDocument* logdoc = frames_doc->GetLogicalDocument();

	if (logdoc && logdoc->GetRoot())
	{
		HTML_Element* target = NULL;

		int offset_x = 0;
		int offset_y = 0;

		/*
		 * A sequence of touch events is delivered to the element that received
		 * the touchstart event, regardless of the current location of the touch.
		 */
		if (event == TOUCHSTART)
		{
			/* Need text boxes so that we can, in the case of broken HTML, be sure
			   of what part of a tree the user clicked on. Two text nodes can be at
			   the same place in the document tree but depending on last_descendant
			   pointers they can have different semantics. See for instance bug 367494. */
			IntersectionObject intersection(frames_doc, LayoutCoord(x), LayoutCoord(y), TRUE);

			if (LayoutWorkplace* wp = logdoc->GetLayoutWorkplace())
				wp->SetCanYield(FALSE);

			intersection.Traverse(logdoc->GetRoot());
			mouse_action_yielded = FALSE;

			if (intersection.IsOutOfMemory())
				goto raise_oom;

			offset_x = intersection.GetRelativeX();
			offset_y = intersection.GetRelativeY();

			if (Box* inner_box = intersection.GetInnerBox())
			{
				target = inner_box->GetHtmlElement();

				/* Don't send events to text elements. */
				while (target->IsText())
					target = target->Parent();

#ifdef ADAPTIVE_ZOOM_SUPPORT
				ViewportController* controller = GetWindow()->GetViewportController();
				OpViewportInfoListener* listener = controller->GetViewportInfoListener();

				OpRect container_rect = intersection.GetContainerRect();
				OpRect line_rect = intersection.GetLineRect();
				FramesDocElm* frame = frames_doc->GetDocManager()->GetFrame();

				if (frame)
				{
					// If the element is inside a frames document, compensate for the
					// position of the frame

					container_rect = controller->ConvertToToplevelRect(frames_doc, container_rect);
					line_rect = controller->ConvertToToplevelRect(frames_doc, line_rect);
				}

				int negative_overflow = NegativeOverflow();

				container_rect.x += negative_overflow;
				line_rect.x += negative_overflow;
				listener->OnAreaOfInterestChanged(controller, container_rect, line_rect);
#endif // ADAPTIVE_ZOOM_SUPPORT

				if (target->Type() == HE_DOC_ROOT)
					target = logdoc->GetDocRoot();
			}

#ifdef MEDIA_HTML_SUPPORT
			// Elements inserted by track (elements that are part of the
			// rendering tree for a cue) should not be hit, so redirect to their
			// non-anonymous parent (the <video> element). Check inserted type
			// or namespace because there will also be HE_INSERTED_BY_LAYOUT (w/
			// NS_CUE) elements in a cue fragment.
			if (target && (target->GetInserted() == HE_INSERTED_BY_TRACK || target->GetNsType() == NS_CUE))
				target = target->ParentActual();
#endif // MEDIA_HTML_SUPPORT

#ifdef SVG_SUPPORT
			if (target && target->IsMatchingType(Markup::SVGE_SVG, NS_SVG))
			{
				HTML_Element* event_target = NULL;

				g_svg_manager->FindSVGElement(target, frames_doc, x, y, &event_target, offset_x, offset_y);
				if (event_target != NULL)
					target = event_target;
			}
#endif // SVG_SUPPORT

			if (!target && frames_doc->GetHLDocProfile())
			{
				// Send event to the body element if there is no element
				// under the mouse. That can happen if CSS has shrunk
				// the body/html elements to be smaller than the viewport
				target = frames_doc->GetHLDocProfile()->GetBodyElm();

				// If there is no body element either, well, then we're
				// out of luck
			}

			if (target)
			{
				HTML_Element* actual_element = target;

				if (actual_element->GetInserted() == HE_INSERTED_BY_LAYOUT)
					actual_element = actual_element->ParentActual();

				// Image with usemap?
				if (HTML_Element* linked_element = actual_element->GetLinkedElement(frames_doc, offset_x, offset_y, target))
					target = linked_element;
			}
		}

		++record_mouse_actions;
		if (frames_doc->HandleTouchEvent(event, target, id, offset_x, offset_y, x, y, visual_viewport_x, visual_viewport_y, radius, sequence_count_and_button_or_delta, modifiers, user_data) == OpStatus::OK)
			return DOC_ACTION_HANDLED;
	}

	return DOC_ACTION_NONE;

raise_oom:
	record_mouse_actions = 0;
	GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
	return DOC_ACTION_NONE;
}
#endif // TOUCH_EVENTS_SUPPORT

int HTML_Document::Width()
{
	if (LogicalDocument* logdoc = frames_doc->GetLogicalDocument())
		return logdoc->GetLayoutWorkplace()->GetDocumentWidth();
	else
		return 0;
}

long HTML_Document::Height()
{
	if (LogicalDocument* logdoc = frames_doc->GetLogicalDocument())
		return logdoc->GetLayoutWorkplace()->GetDocumentHeight();
	else
		return 0;
}

int HTML_Document::NegativeOverflow() const
{
#ifdef SUPPORT_TEXT_DIRECTION
	if (LogicalDocument* logdoc = frames_doc->GetLogicalDocument())
		return logdoc->GetLayoutWorkplace()->GetNegativeOverflow();
#endif // SUPPORT_TEXT_DIRECTION

	return 0;
}

OP_STATUS HTML_Document::StopLoading(BOOL format, BOOL aborted)
{
	LogicalDocument* logdoc = frames_doc->GetLogicalDocument();
	send_all_loaded = FALSE;
	all_loaded_sent = TRUE;

	if (logdoc)
		logdoc->StopLoading(aborted);

	if (format)
		if (FramesDocElm* parent = GetDocManager()->GetFrame())
			return FramesDocument::CheckOnLoad(NULL, parent);

	return OpStatus::OK;
}

URL HTML_Document::GetCurrentURL(const uni_char* &win_name, int unused_parameter /* = 0 */)
{
	if (navigation_element.GetElm())
	{
		if (current_area_element.GetElm())
		{
			URL* curl = current_area_element->GetAREA_URL(frames_doc->GetLogicalDocument());

			win_name = current_area_element->GetAREA_Target();

			if (!win_name || !*win_name)
				win_name = GetCurrentBaseTarget(current_area_element.GetElm());

			return curl ? *curl : URL();
		}
		else
		{
			URL curl = navigation_element->GetAnchor_URL(this); // this call now includes all anchor elements: HE_A, WE_ANCHOR, etc. (was: GetA_HRef())

			win_name = navigation_element->GetA_Target();

			if (!win_name || !*win_name)
				win_name = GetCurrentBaseTarget(navigation_element.GetElm());

			return curl; // ? *curl : URL();
		}
	}

	return URL();
}

OP_STATUS HTML_Document::ReactivateDocument()
{
	LogicalDocument* logdoc = frames_doc->GetLogicalDocument();

	if (logdoc && logdoc->GetRoot())
		return logdoc->GetRoot()->EnableContent(frames_doc);

	return OpStatus::OK;
}

void HTML_Document::SetAsCurrentDoc(BOOL state)
{
	if (state)
	{
		GetVisualDevice()->SetColor(0, 0, 0); // set default text color black
		GetVisualDevice()->SetDefaultBgColor();
		GetVisualDevice()->UpdateAll();
	}
	else
	{
#ifdef NEARBY_ELEMENT_DETECTION
		GetWindow()->SetElementExpander(NULL);
#endif // NEARBY_ELEMENT_DETECTION

		recorded_mouse_actions.Clear();

#ifdef DRAG_SUPPORT
		g_drag_manager->OnDocumentUnload(this);
#endif // DRAG_SUPPORT

	}
}

URL HTML_Document::GetBGImageURL()
{
	LogicalDocument* logdoc = frames_doc->GetLogicalDocument();
	HTML_Element* body_he = logdoc ? logdoc->GetBodyElm() : NULL;

	return body_he ? body_he->GetImageURL(TRUE, logdoc) : URL();
}


OP_STATUS HTML_Document::SetRelativePos(const uni_char* rel_name, const uni_char* alternative_rel_name, BOOL scroll)
{
	OP_ASSERT(rel_name);

	// set y positions for HTML and viewports for SVG

	LogicalDocument* logdoc = frames_doc->GetLogicalDocument();
	if (logdoc && logdoc->IsLoaded())
	{
		// empty relative name "#" should go to top of page (stighal, 2004-09-20)
		// and so should "#top" if not found in page
		BOOL jump_to_top = !*rel_name;

		HTML_Element* element = NULL;
		if (!jump_to_top)
		{
			// Find a suitable target. It's identified by its id except
			// for HE_A and HE_MAP that are also identified by their name.
			// So we use the global table and check if it's name or id.
			NamedElementsIterator iterator;
			int found = logdoc->SearchNamedElements(iterator, NULL, rel_name, TRUE, TRUE);
			BOOL used_alternative_rel_name = FALSE;
			if (found == 0 && alternative_rel_name && *alternative_rel_name)
			{
				used_alternative_rel_name = TRUE;
				found = logdoc->SearchNamedElements(iterator, NULL, alternative_rel_name, FALSE, TRUE); // only <a name> should be found
			}

			if (found == 0 && uni_stricmp(rel_name, "top") == 0)
				jump_to_top = TRUE;
			else
			{
				// Find a visible element in the list, matching on id first, and on name second.
				HTML_Element* best_name_candidate = NULL;
				while (HTML_Element* candidate = iterator.GetNextElement())
				{
					BOOL was_only_match_by_name = TRUE;
					if (!used_alternative_rel_name)
					{
						const uni_char* elem_id = candidate->GetId();
						was_only_match_by_name = !elem_id || !uni_str_eq(elem_id, rel_name);
					}

					if (was_only_match_by_name && best_name_candidate)
						continue;

					// The name attribute should only work on some elements.
					if (was_only_match_by_name)
					{
						if (candidate->GetNsType() != NS_HTML ||
						    candidate->Type() != HE_A && candidate->Type() != HE_MAP ||
						    used_alternative_rel_name && candidate->Type() != HE_A)
						{
							// You can only find html:a and html:map by name in general,
							// and only <a name should be found> by the alternative name.
							continue;
						}
					}

					OP_ASSERT(!used_alternative_rel_name || candidate->IsMatchingType(HE_A, NS_HTML) && candidate->GetName() && uni_str_eq(candidate->GetName(), alternative_rel_name));

					element = candidate;
					if (element->GetLayoutBox()
#ifdef SVG_SUPPORT
						|| element->GetNsType() == NS_SVG && g_svg_manager->IsVisible(element)
#endif // SVG_SUPPORT
						)
					{
						if (was_only_match_by_name)
							best_name_candidate = candidate;
						else
							break;
					}
				}
				if (!element)
					element = best_name_candidate;
			}
		}

		if (jump_to_top && scroll)
		{
			frames_doc->RequestSetVisualViewPos(0, 0, VIEWPORT_CHANGE_REASON_JUMP_TO_RELATIVE_POS);
		}

		ApplyTargetPseudoClass(element);

		// Scroll
		if (element && element->GetLayoutBox() && scroll)
		{
			HTML_Element* named_element = element;
			RECT element_rect;

			for (; element; element = element->NextSiblingActual())
			{
				if (logdoc->GetBoxRect(element, BOUNDING_BOX, element_rect))
				{
					ScrollToElement(element, SCROLL_ALIGN_TOP, TRUE, VIEWPORT_CHANGE_REASON_JUMP_TO_RELATIVE_POS);
					return OpStatus::OK;
				}
			}

			// Found nothing visible after the named element. Try the other way
			element = named_element->PrevActual();
			for (; element; element = element->PrevActual())
			{
				if (logdoc->GetBoxRect(element, BOUNDING_BOX, element_rect))
				{
					ScrollToElement(element, SCROLL_ALIGN_TOP, TRUE, VIEWPORT_CHANGE_REASON_JUMP_TO_RELATIVE_POS);
					return OpStatus::OK;
				}
			}
		}

#ifdef SVG_SUPPORT
		if (logdoc->GetDocRoot() &&
			logdoc->GetDocRoot()->IsMatchingType(Markup::SVGE_SVG, NS_SVG))
		{
			// some fragments are not id:s, and we need to repaint those cases too
			if (SVGImage* svg_image = g_svg_manager->GetSVGImage(logdoc, logdoc->GetDocRoot()))
				return svg_image->SetURLRelativePart(rel_name);
		}
#endif // SVG_SUPPORT
	}

	return OpStatus::OK;
}

void HTML_Document::ApplyTargetPseudoClass(HTML_Element *target_element)
{
	LogicalDocument* logdoc = frames_doc->GetLogicalDocument();

	if (logdoc)
	{
		// Apply the :target pseudo class
		if (url_target_element.GetElm() != target_element)
		{
			// First remove the :target pseudo class from the old target
			if (HTML_Element* prev_target_element = url_target_element.GetElm())
			{
				url_target_element.Reset();
				logdoc->GetLayoutWorkplace()->ApplyPropertyChanges(prev_target_element, 0, TRUE);
			}

			if (target_element)
			{
				url_target_element.SetElm(target_element);
				logdoc->GetLayoutWorkplace()->ApplyPropertyChanges(target_element, 0, TRUE);
			}
		}
	}
}

void HTML_Document::ClearFocusAndHighlight(BOOL send_event, BOOL clear_textselection/*=TRUE*/, BOOL clear_highlight/*=TRUE*/)
{
	if (HTML_Element *foc_elm = GetFocusedElement())
	{
		FormObject* form_obj = foc_elm->GetFormObject();
		if (form_obj)
		{
			OpWidget* widget = form_obj->GetWidget();
			if (widget->IsFocused() || widget->IsFocused(TRUE))
				form_obj->ReleaseFocus(send_event ? FOCUS_REASON_ACTION : FOCUS_REASON_RELEASE);
			send_event = FALSE; // Forms handles focusevents
		}
		else if (foc_elm->IsMatchingType(HE_BUTTON, NS_HTML))
		{
			if (Box* box = foc_elm->GetLayoutBox())
				if (ButtonContainer* bc = box->GetButtonContainer())
					if (OpButton* button = bc->GetButton())
						button->ReleaseFocus(send_event ? FOCUS_REASON_ACTION : FOCUS_REASON_RELEASE);
		}
#ifdef DOCUMENT_EDIT_SUPPORT
		else if (frames_doc->GetDocumentEdit() && foc_elm->IsContentEditable())
		{
			frames_doc->GetDocumentEdit()->ReleaseFocus();
		}
#endif // DOCUMENT_EDIT_SUPPORT
	}
	else if (HTML_Element *nav_elm = navigation_element.GetElm())
	{
		if (nav_elm->GetNsType() == NS_HTML)
		{
			switch (nav_elm->Type())
			{
			case HE_INPUT:
#ifdef _SSL_SUPPORT_
			case HE_KEYGEN:
#endif
			case HE_SELECT:
			case HE_TEXTAREA:
				send_event = FALSE; // No ONFOCUS for these, so we should not send ONBLUR
			}
		}
#ifdef SVG_SUPPORT
		else if (nav_elm->GetNsType() == NS_SVG)
		{
			// In this case we would get ONBLUR followed by an ONFOCUS on the first element
			// that we wanted to focus. We should not send ONBLUR unless something was blurred,
			// see bug 218815.
			if (!GetFocusedElement())
				send_event = FALSE;
		}
#endif // SVG_SUPPORT
	}

	HTML_Element *helm = focused_element.GetElm() ? focused_element.GetElm() : (clear_highlight ? navigation_element.GetElm() : NULL);

	if (helm && send_event)
	{
		if (frames_doc->HandleEvent(ONBLUR, NULL, helm, SHIFTKEY_NONE) == OpStatus::ERR_NO_MEMORY)
			GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
	}

	InvalidateHighlightArea();

	focused_element.Reset();
	if (clear_highlight)
	{
		navigation_element.Reset();
		navigation_element_is_highlighted = FALSE;
	}

	SetCurrentAreaElement(NULL);
	image_form_selected = FALSE;
	area_selected = FALSE;

	if (clear_textselection)
		frames_doc->ClearDocumentSelection(FALSE, FALSE, FALSE);

	if (helm && !helm->IsText() && !frames_doc->IsReflowing() && !frames_doc->IsBeingDeleted() && !frames_doc->IsUndisplaying())
	{
		helm->SetIsPreFocused(FALSE);
		frames_doc->GetLogicalDocument()->GetLayoutWorkplace()->ApplyPropertyChanges(helm, CSS_PSEUDO_CLASS_GROUP_MOUSE, TRUE);
	}

	GetWindow()->DisplayLinkInformation(NULL, ST_ASTRING, NULL);
}

/**
 * Helper function for FindNearbySaveScrollToElement, to find the
 * distance from a point to the nearest left or right edge of a
 * range. Will return 0 if point is inside range.
 *
 * @param x point to find distance from
 * @param rx left edge of range
 * @param rw width of range
 */
static int DistanceToRange(int x, int rx, int rw)
{
	if (x < rx)
		return rx - x;
	else if (x > rx + rw)
		return x - (rx + rw);
	else
		return 0;
}

OP_STATUS HTML_Document::FindNearbySaveScrollToElement(const OpPoint& p, HTML_Element** elm, int& text_offset, OpViewportRequestListener::Align& text_align)
{
	FramesDocument* frames_doc = GetFramesDocument();
	OP_ASSERT(elm != NULL);

	// We want to find nearby paragraph rects. Prioritization is as follows:
	// 1. Rects located directly above or below point of interest (POI).
	// 2. Rects located to the left of POI.
	// 3. Rects located to the right.
	const int PARAGRAPH_DETECTION_RADIUS = 100;
	AutoDeleteHead paragraph_list;
	OpRect r = OpRect(p.x - PARAGRAPH_DETECTION_RADIUS, p.y - PARAGRAPH_DETECTION_RADIUS, PARAGRAPH_DETECTION_RADIUS * 2, PARAGRAPH_DETECTION_RADIUS * 2);
	frames_doc->GetLogicalDocument()->GetLayoutWorkplace()->GetParagraphList(r, paragraph_list);

	BOOL found_rect_vertically = FALSE;
	BOOL found_rect_horizontally = FALSE;
	OpRectListItem* li = 0;
	OpRectListItem* closest_rect_item = 0;
	for (li = static_cast<OpRectListItem*>(paragraph_list.First());
		 li; li = li->Suc())
	{
		// Make sure the rectangle is sane.
		if (li->rect.IsEmpty())
			continue;

		// Check that the rectangle is within specified distance.
		int horizontal_distance = DistanceToRange(p.x, li->rect.x, li->rect.width);
		int vertical_distance = DistanceToRange(p.y, li->rect.y, li->rect.height);
		if (horizontal_distance > PARAGRAPH_DETECTION_RADIUS ||
			vertical_distance > PARAGRAPH_DETECTION_RADIUS)
			continue;

		// If this rectangle is over or below point of interest,
		// select it if it's the closest one so far.
		if (horizontal_distance == 0)
		{
			if (found_rect_vertically && vertical_distance >
				DistanceToRange(p.y, closest_rect_item->rect.y, closest_rect_item->rect.height))
				continue;
			closest_rect_item = li;
			found_rect_vertically = TRUE;
		}

		// If this rectangle is to the left or right of point of
		// interest, and we haven't found a rectangle above or below,
		// check if it should be selected.
		if (!found_rect_vertically)
		{
			if (found_rect_horizontally)
			{
				// How much closer the new rect is than the current one, horizontally.
				int horizontal_delta = DistanceToRange(p.x, closest_rect_item->rect.x, closest_rect_item->rect.width) -
					horizontal_distance;

				// How much closer the new rect is than the current one, vertically.
				int vertical_delta = DistanceToRange(p.y, closest_rect_item->rect.y, closest_rect_item->rect.height) -
					vertical_distance;

				// Check if rect is closer (prioritize horizontal distance).
				if (horizontal_delta < 0 || (horizontal_delta == 0 && vertical_delta < 0))
					continue;
			}
			closest_rect_item = li;
			found_rect_horizontally = TRUE;
		}
	}

	// If we have found a nearby rect, find the HTML_Element.
	if (closest_rect_item)
	{
		IntersectionObject io(frames_doc, LayoutCoord(closest_rect_item->rect.x), LayoutCoord(closest_rect_item->rect.y), TRUE);
		io.Traverse(frames_doc->GetLogicalDocument()->GetRoot());
		if (io.IsOutOfMemory()) // not fatal
			return OpStatus::ERR_NO_MEMORY;
		*elm = io.GetInnerBox() ? io.GetInnerBox()->GetHtmlElement() : NULL;
		if (*elm && (*elm)->IsText())
		{
			text_offset = io.GetWord() ? io.GetWord() - (*elm)->TextContent() : 0;
			OP_ASSERT(text_offset >= 0);
		}
		else
		{
			text_offset = -1;
		}
#ifdef LAYOUT_INTERSECT_CONTAINER_INFO
		text_align = io.GetTextAlign();
#endif // LAYOUT_INTERSECT_CONTAINER_INFO
	}
	return OpStatus::OK;
}

/**
 * Convenience wrapper for Box::GetRect(), that check bounds for text_offset.
 * @param elm Element to get rectangle for.
 * @param frames_doc Document hosting the element.
 * @param type Type of rectangle to get.
 * @param text_offset What will be sent as param element_text_start to
 *                    Box::GetRect(), or -1 if no text_offset
 * @param rect Element rectangle.
 * @return TRUE if successful and false otherwise.
 */
static BOOL GetElementRect(HTML_Element* elm, FramesDocument* frames_doc, BoxRectType type, int text_offset, RECT& rect)
{
	BOOL found = FALSE;
	int tlen = 0;

	if (elm && elm->GetLayoutBox() && frames_doc && elm->Type() != Markup::HTE_DOC_ROOT)
	{
		// Some obfuscation here, but avoid calling GetTextContentLength and
		// other methods twice.
		if (text_offset >= 0 && elm->Type() == Markup::HTE_TEXT)
			tlen = elm->GetTextContentLength();

		// we want to find a specific position inside a text element.
		if (tlen > 0)
		{
			OP_ASSERT(text_offset <= tlen);

			// check bounds
			if (text_offset == tlen)
				text_offset--;

			OP_ASSERT(text_offset < tlen && text_offset >= 0);

			found = elm->GetLayoutBox()->GetRect(frames_doc, type, rect, text_offset, text_offset + 1);
		}
		else
		{
			found = elm->GetLayoutBox()->GetRect(frames_doc, type, rect);
		}
	}

	return found;
}

void HTML_Document::SaveScrollToElement(OpPoint& p)
{
	FramesDocument* frames_doc = GetFramesDocument();

	if (!frames_doc->GetLogicalDocument() || !frames_doc->GetLogicalDocument()->GetRoot())
		return;

	IntersectionObject io(frames_doc, LayoutCoord(p.x), LayoutCoord(p.y), TRUE);

	io.Traverse(frames_doc->GetLogicalDocument()->GetRoot());

	if (io.IsOutOfMemory()) // not fatal
		GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);

	HTML_Element* el = io.GetInnerBox() ? io.GetInnerBox()->GetHtmlElement() : NULL;
	int offset = -1;
	OpViewportRequestListener::Align text_align =
#ifdef LAYOUT_INTERSECT_CONTAINER_INFO
		io.GetTextAlign();
#else
		OpViewportRequestListener::VIEWPORT_ALIGN_NONE;
#endif

	if (el && el->IsText())
	{
		offset = io.GetWord() ? io.GetWord() - el->TextContent() : 0;
		OP_ASSERT(offset >= 0);
	}

	// If we don't find an interesting element, look for something in the vicinity.
	if (!el || (el->GetLayoutBox() && el->GetLayoutBox()->GetContainer()))
		if (OpStatus::IsMemoryError(FindNearbySaveScrollToElement(p, &el, offset, text_align)))
			GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);

	if (!el || el->Type() == HE_DOC_ROOT)
		el = frames_doc->GetLogicalDocument()->GetDocRoot();

	RECT rect = { 0, 0, 0, 0 };
	GetElementRect(el, frames_doc, BOUNDING_BOX, offset, rect);

	SaveScrollToElement(el, offset, OpPoint(p.x - rect.left, p.y - rect.top),
						OpRect(&rect), text_align);
}

void HTML_Document::SaveScrollToElement(HTML_Element* helm, int text_offset, const OpPoint& point_offset, const OpRect& rect, OpViewportRequestListener::Align text_align)
{
	saved_scroll_to_element.SetElm(helm);
	saved_scroll_to_element.SetScrollToTextOffset(text_offset);
	saved_scroll_to_element.SetScrollToPointOffset(point_offset);
	saved_scroll_to_element.SetScrollToRect(rect);
	saved_scroll_to_element.SetScrollToTextAlign(text_align);
}

BOOL HTML_Document::ScrollToElement(HTML_Element *helm, SCROLL_ALIGN align, BOOL strict_align, OpViewportChangeReason reason, int text_offset)
{
	OP_ASSERT(helm);

	OP_ASSERT(helm->IsText() || text_offset < 0);
	OpRect oprect;
	BOOL can_scroll = FALSE;
	if (helm->GetLayoutBox())
	{
		RECT rect = { 0, 0, 0, 0 };
		if (GetElementRect(helm, frames_doc, BORDER_BOX, text_offset, rect))
		{
			oprect = OpRect(&rect);
			can_scroll = TRUE;
		}
	}
#ifdef SVG_SUPPORT
	else if (helm->GetNsType() == NS_SVG && OpStatus::IsSuccess(g_svg_manager->GetElementRect(helm, oprect)))
		can_scroll = TRUE;
#endif // SVG_SUPPORT

	BOOL scroll_needed = FALSE;

	if (can_scroll)
	{
		scroll_needed = frames_doc->ScrollToRect(oprect, align, strict_align, reason, helm);

		VisualDevice* vis_dev = frames_doc->GetVisualDevice();
		if (OpView *opview = vis_dev->GetOpView())
		{
			OpRect hlrect(oprect.x - vis_dev->GetRenderingViewX(),
			              oprect.y - vis_dev->GetRenderingViewY(),
			              oprect.width,
			              oprect.height);
			hlrect = vis_dev->ScaleToScreen(hlrect);
			hlrect = vis_dev->OffsetToContainer(hlrect);
			opview->OnHighlightRectChanged(hlrect);
		}
	}

	return scroll_needed;
}

HTML_Element* HTML_Document::SetAreaElementAsNavigationElement(HTML_Element* helm)
{
	OP_ASSERT(helm);
	OP_ASSERT(helm->IsMatchingType(HE_AREA, NS_HTML));

	area_selected = TRUE;
	SetCurrentAreaElement(helm);

	HTML_Element *map = helm->ParentActual();
	while (map && map->Type() != HE_MAP)
		map = map->ParentActual();

	HTML_Element *usemap_elm = FindAREAObjectElement(map, frames_doc->GetLogicalDocument()->GetRoot());
	SetNavigationElement(usemap_elm, FALSE);
	return usemap_elm;
}

void HTML_Document::ScrollToSavedElement()
{
	if (HTML_Element* element = saved_scroll_to_element.GetElm())
	{
		FramesDocument* top_doc = frames_doc->GetTopFramesDoc();

		if (HTML_Element* root = top_doc->GetDocRoot())
			if (root->IsDirty())
				/* The top document is dirty. In smart frames mode this typically happens when a
				   document in a sub-frame has got a new size during reflow, so that the frame
				   sizes need to be recalculated. In smart frames mode, sub-frames should never be
				   scrolled; it is always the top frameset that is scrolled. Since the frameset
				   size needs to be recalculated first, we cannot scroll yet. Try again later, when
				   traversing this document for the first time after reflow. */

				return;

		// Reset saved_scroll_to_element now, or we might cause an infinite traversal recursion.

		saved_scroll_to_element.Reset();

		OP_ASSERT(element->IsText() || saved_scroll_to_element.GetScrollToTextOffset() < 0);

#ifdef DOC_SEND_POIMOVED
		HTML_Element* container = element;
		while (container->GetLayoutBox() && !container->GetLayoutBox()->GetContainer() && container->Parent())
		{
			container = container->Parent();
		}

		if (container->GetLayoutBox())
		{
			RECT element_rect = { 0, 0, 0, 0 };
			GetElementRect(element, frames_doc, BOUNDING_BOX, saved_scroll_to_element.GetScrollToTextOffset(), element_rect);

			RECT container_rect = { 0, 0, 0, 0 };
			container->GetLayoutBox()->GetRect(frames_doc, BOUNDING_BOX, container_rect);

			ViewportController* controller = GetWindow()->GetViewportController();

			int dx = element_rect.left - saved_scroll_to_element.GetScrollToRect().x;
			int dy = element_rect.top - saved_scroll_to_element.GetScrollToRect().y;

			// If not moving according to word inside element, move relatively
			// to the size-change of element, to try and stay close to the
			// approx the same position.
			if (saved_scroll_to_element.GetScrollToTextOffset() < 0)
			{
				dx += saved_scroll_to_element.GetScrollToPointOffset().x * (element_rect.right - element_rect.left + 1) / MAX(1, saved_scroll_to_element.GetScrollToRect().width) - saved_scroll_to_element.GetScrollToPointOffset().x;
				dy += saved_scroll_to_element.GetScrollToPointOffset().y * (element_rect.bottom - element_rect.top + 1) / MAX(1, saved_scroll_to_element.GetScrollToRect().height) - saved_scroll_to_element.GetScrollToPointOffset().y;
			}

			OpViewportRequestListener::POIData poi_data;
			poi_data.movement = OpPoint(dx, dy);
			poi_data.container = controller->ConvertToToplevelRect(frames_doc, OpRect(&container_rect));

			if (element->Type() == HE_IMG)
				poi_data.element_type = OpViewportRequestListener::ELEMENT_TYPE_IMAGE;
			else if (element->Type() == HE_P)
				poi_data.element_type = OpViewportRequestListener::ELEMENT_TYPE_PARAGRAPH;
			else if (element->GetLayoutBox()->GetContainer() != NULL)
				poi_data.element_type = OpViewportRequestListener::ELEMENT_TYPE_CONTAINER;
			else
				poi_data.element_type = OpViewportRequestListener::ELEMENT_TYPE_OTHER;

			poi_data.text_align = saved_scroll_to_element.GetScrollToTextAlign();

			OpViewportRequestListener* listener = controller->GetViewportRequestListener();
			listener->OnPOIMoved(controller, poi_data);
		}
#else // DOC_SEND_POIMOVED
		if (element->Type() == HE_INPUT || element->Type() == HE_TEXTAREA)
		{
			FormObject* formObject = element->GetFormObject();
			if (formObject)
				formObject->EnsureActivePartVisible(TRUE);
		}
		else
		{
			ScrollToElement(element, SCROLL_ALIGN_CENTER, TRUE, VIEWPORT_CHANGE_REASON_REFLOW, saved_scroll_to_element.GetScrollToTextOffset());
		}
#endif // DOC_SEND_POIMOVED
	}
	saved_scroll_to_element.SetScrollToTextOffset(-1);
}

static FOCUS_REASON ConvertFocusOriginToFocusReason(HTML_Document::FocusOrigin origin)
{
	switch (origin)
	{
	case HTML_Document::FOCUS_ORIGIN_DOM:                return FOCUS_REASON_OTHER;
	case HTML_Document::FOCUS_ORIGIN_STORED_STATE:       return FOCUS_REASON_RESTORE;
	case HTML_Document::FOCUS_ORIGIN_SPATIAL_NAVIGATION:
	case HTML_Document::FOCUS_ORIGIN_KEYBOARD:           return FOCUS_REASON_KEYBOARD;
	case HTML_Document::FOCUS_ORIGIN_MOUSE:              return FOCUS_REASON_MOUSE;
	default:                                             return FOCUS_REASON_ACTION;
	}
}

BOOL HTML_Document::FocusElement(HTML_Element* helm, FocusOrigin origin, BOOL clear_textselection/*=TRUE*/, BOOL allow_scroll/*=TRUE*/, BOOL do_highlight/*=FALSE*/)
{
	if (!helm || helm == focused_element.GetElm())
		return FALSE;

	if (!helm->IsMatchingType(HE_AREA, NS_HTML))
	{
		// Don't focus if the element is not visible. <area> elements don't follow this behaviour, though.
		Head prop_list;
		LayoutProperties* lprops = LayoutProperties::CreateCascade(helm, prop_list, frames_doc->GetHLDocProfile());
		BOOL visible = TRUE;
		if (lprops && lprops->GetProps()->visibility != CSS_VALUE_visible)
			visible = FALSE;
		while (lprops && visible)
		{
			if (lprops->GetProps()->display_type == CSS_VALUE_none)
				visible = FALSE;
			lprops = lprops->Pred();
		}
		prop_list.Clear();
		if (!visible)
			return FALSE;
	}

#ifdef _SPAT_NAV_SUPPORT_
	if (origin != FOCUS_ORIGIN_SPATIAL_NAVIGATION)
	{
		OpSnHandler* handler = GetWindow()->GetSnHandler();
		if (handler)
			handler->ElementNavigated(this, helm);
	}
#endif // _SPAT_NAV_SUPPORT_

	FOCUS_REASON focus_reason = ConvertFocusOriginToFocusReason(origin);

	if (FramesDocElm *fdelm = frames_doc->GetDocManager()->GetFrame())
		frames_doc->GetTopDocument()->SetActiveFrame(fdelm, TRUE, focus_reason);
	else
		frames_doc->SetNoActiveFrame(focus_reason);

	if(helm->GetNsType() == NS_HTML)
	{
		switch (helm->Type())
		{
#ifdef DOCUMENT_EDIT_SUPPORT
		case HE_IFRAME:
			{
			FramesDocElm *fde = FramesDocElm::GetFrmDocElmByHTML(helm);
			if (!fde || !fde->GetCurrentDoc() || !fde->GetCurrentDoc()->GetDocumentEdit())
				return FALSE;

			if (allow_scroll && origin != FOCUS_ORIGIN_DOM)
			{
				ScrollToElement(helm, SCROLL_ALIGN_LAZY_TOP, FALSE, VIEWPORT_CHANGE_REASON_DOCUMENTEDIT);
			}

			frames_doc->SetSuppressFocusEvents(TRUE);
			ClearFocusAndHighlight(origin != FOCUS_ORIGIN_DOM, clear_textselection);

			fde->GetCurrentDoc()->GetDocumentEdit()->SetFocus(focus_reason);
			frames_doc->SetSuppressFocusEvents(FALSE);

			focused_element.SetElm(helm);
			return TRUE;
			}
#endif // DOCUMENT_EDIT_SUPPORT
		case HE_INPUT:
#ifdef _SSL_SUPPORT_
		case HE_KEYGEN:
#endif
		case HE_SELECT:
		case HE_TEXTAREA:
			if (helm->GetInputType() != INPUT_IMAGE)
			{
				if (FormObject* form_obj = helm->GetFormObject())
				{
					// What is the purpose of this? Indirectly affecting/updating the form-widget position?
					AffinePos doc_pos;
					form_obj->GetPosInDocument(&doc_pos);
					OpRect rect(0, 0, form_obj->Width(), form_obj->Height());

					if (allow_scroll)
					{
						ScrollToElement(helm, SCROLL_ALIGN_LAZY_TOP, FALSE, VIEWPORT_CHANGE_REASON_FORM_FOCUS);
					}
					form_obj = helm->GetFormObject(); // Update pointer since ScrollToElement may have reflowed (and recreated formobject).

					frames_doc->SetSuppressFocusEvents(TRUE);
					ClearFocusAndHighlight(origin != FOCUS_ORIGIN_DOM, clear_textselection);

					form_obj->SetFocus(focus_reason);
					// form_obj might now be rewritten so don't use it after this line without fetching
					// it again from the HTML_Element.
					frames_doc->SetSuppressFocusEvents(FALSE);

					focused_element.SetElm(helm);
					return TRUE;
				}
				return FALSE;
			}
			/* fall through */

		default:
			if (!do_highlight || HighlightElement(helm, origin, FALSE, allow_scroll, NULL, clear_textselection))
			{
				if (!do_highlight) // Otherwise we may clear what has just been selected by HighlightElement().
				{
					frames_doc->SetSuppressFocusEvents(TRUE);
					ClearFocusAndHighlight(origin != FOCUS_ORIGIN_DOM, clear_textselection);
					frames_doc->SetSuppressFocusEvents(FALSE);
				}

				focused_element.SetElm(helm);
				SetNavigationElement(helm, FALSE);

				if (helm->GetNsType() == NS_HTML)
				{
					switch (helm->Type())
					{
					case HE_AREA:
						{
							SetAreaElementAsNavigationElement(helm);

							const uni_char* alt = helm->GetStringAttr(ATTR_ALT);
							const uni_char* title = helm->GetStringAttr(ATTR_TITLE);
							if (alt)
								GetWindow()->DisplayLinkInformation(alt, ST_ALINK, title);
						}
						break;
					}
				}

#ifdef DOCUMENT_EDIT_SUPPORT
				if (helm->IsContentEditable() && frames_doc->GetDocumentEdit())
				{
					allow_scroll = origin != FOCUS_ORIGIN_DOM;
					if (allow_scroll)
					{
						ScrollToElement(helm, SCROLL_ALIGN_LAZY_TOP, FALSE, VIEWPORT_CHANGE_REASON_DOCUMENTEDIT);
					}

					frames_doc->GetDocumentEdit()->FocusEditableRoot(helm, focus_reason);

					focused_element.SetElm(helm);
				}
#endif // DOCUMENT_EDIT_SUPPORT

				if (!do_highlight && allow_scroll)
				{
					ScrollToElement(helm, SCROLL_ALIGN_LAZY_TOP, FALSE, VIEWPORT_CHANGE_REASON_FORM_FOCUS); // FIXME: not necessarily a form element
				}

				return TRUE;
			}
			else
				return FALSE;
		}
	}
#ifdef SVG_SUPPORT
	else if(helm->GetNsType() == NS_SVG)
	{
		if (!do_highlight || HighlightElement(helm, origin, FALSE, allow_scroll, NULL, clear_textselection))
		{
			frames_doc->SetSuppressFocusEvents(TRUE);
			ClearFocusAndHighlight(origin != FOCUS_ORIGIN_DOM, clear_textselection);
			frames_doc->SetSuppressFocusEvents(FALSE);

			focused_element.SetElm(helm);
			SetNavigationElement(helm, FALSE);
			if (helm->IsMatchingType(Markup::SVGE_A, NS_SVG))
			{
				URL url = helm->GetAnchor_URL(this);
				const uni_char* title = helm->GetElementTitle();

				GetWindow()->DisplayLinkInformation(url, ST_ALINK, title);
			}
			if (g_svg_manager->IsEditableElement(frames_doc, helm))
			{
				g_svg_manager->BeginEditing(frames_doc, helm, origin == FOCUS_ORIGIN_DOM ?
											FOCUS_REASON_OTHER : FOCUS_REASON_ACTION);
			}

			return TRUE;
		}

		return FALSE;
	}
#endif // SVG_SUPPORT

	return FALSE;
}

void HTML_Document::ResetFocusedElement()
{
	focused_element.Reset();
}

void HTML_Document::SetFocusedElement(HTML_Element* helm, BOOL clear_textselection)
{
	if (helm != focused_element.GetElm())
	{
		HTML_Element *old = focused_element.GetElm();

		BOOL clear_it = clear_textselection;
		if (helm && helm->GetNsType() == NS_HTML && (helm->Type() == HE_BUTTON || (helm->Type() == HE_INPUT && helm->GetInputType() == INPUT_BUTTON)))
			clear_it = FALSE;

		// No point in clearing highlight if we're reapplying it to the same element
		// Avoids problem with it sending events for losing focus and regaining
		// it immediately
		BOOL clear_highlight = helm != navigation_element.GetElm();

		ClearFocusAndHighlight(TRUE, clear_it, clear_highlight);

		if (helm)
			GetFramesDocument()->OnFocusChange(GetFramesDocument());

		if( old )
		{
			old->SetIsPreFocused(FALSE);
		}
		if ( helm )
		{
			helm->SetIsPreFocused(FALSE);

			focused_element.SetElm(helm);
			GetFramesDocument()->GetLogicalDocument()->GetLayoutWorkplace()->ApplyPropertyChanges(helm, CSS_PSEUDO_CLASS_GROUP_MOUSE, TRUE);
		}
	}

	SetNavigationElement(helm, FALSE);
}

void HTML_Document::SetElementWithSelection(HTML_Element* helm)
{
	SET_NEW_ELEMENT(element_with_selection, helm);
}

HTML_Element* HTML_Document::GetElementWithSelection()
{
	return element_with_selection;
}

HTML_Element *HTML_Document::FindAREAObjectElement(HTML_Element *map, HTML_Element *helm)
{
	if (map)
	{
		const uni_char *map_name = map->GetStringAttr(ATTR_NAME);
		const uni_char *map_id = map->GetId();

		if (map_name || map_id)
		{
			while (helm)
			{
				HTML_ElementType type = helm->Type();

				if (type == HE_IMG || type == HE_OBJECT || (type == HE_INPUT && helm->GetInputType() == INPUT_IMAGE))
				{
					URL *usemap_url;

					if (type == HE_INPUT || type == HE_OBJECT)
						usemap_url = helm->GetUrlAttr(ATTR_USEMAP, NS_IDX_HTML, frames_doc->GetLogicalDocument());
					else
						usemap_url = helm->GetIMG_UsemapURL(frames_doc->GetLogicalDocument());

					if (usemap_url)
					{
						const uni_char *usemap_name = usemap_url->UniRelName();

						if (usemap_name)
							if ( map_name && uni_str_eq(map_name, usemap_name)
								|| map_id && uni_str_eq(map_id, usemap_name))
							{
								return helm;
							}
					}
				}

				helm = helm->NextActual();
			}
		}
	}

	return NULL;
}

BOOL HTML_Document::HighlightElement(HTML_Element* helm, BOOL send_event, BOOL focusFormElm, BOOL scroll, OpRect* update_rect, BOOL clear_textselection, BOOL invert_form_border_rect)
{
	return HighlightElement(helm, !send_event ? FOCUS_ORIGIN_DOM : FOCUS_ORIGIN_UNKNOWN, focusFormElm, scroll, update_rect, clear_textselection, invert_form_border_rect);
}

BOOL HTML_Document::HighlightElement(HTML_Element* helm, FocusOrigin origin, BOOL focusFormElm, BOOL scroll, OpRect* update_rect, BOOL clear_textselection, BOOL invert_form_border_rect)
{
	if (helm == NULL || (!helm->GetLayoutBox() && !helm->IsMatchingType(HE_AREA, NS_HTML)
#ifdef SVG_SUPPORT
		&& helm->GetNsType() != NS_SVG
#endif // SVG_SUPPORT
		))
		return FALSE;

#ifdef _SPAT_NAV_SUPPORT_
	if (origin != FOCUS_ORIGIN_SPATIAL_NAVIGATION)
	{
		OpSnHandler* handler = GetWindow()->GetSnHandler();
		if (handler)
			handler->ElementNavigated(this, helm);
	}
#endif // _SPAT_NAV_SUPPORT_

	HTML_Element *scroll_to_elm = helm;
	BOOL ret_val = TRUE;
	BOOL invalidate_highlight_area = FALSE;
	BOOL is_oom = FALSE;

	// Only send OnFocus when navigation_element changes (refer bug #279361).
	BOOL send_event = origin != FOCUS_ORIGIN_DOM && helm != navigation_element.GetElm();

	frames_doc->SetSuppressFocusEvents(TRUE);
	ClearFocusAndHighlight(send_event, clear_textselection);
	frames_doc->SetFocus();
	frames_doc->SetSuppressFocusEvents(FALSE);

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	if (AccessibleDocument *accessible_document = GetVisualDevice()->GetAccessibleDocument())
		accessible_document->HighlightElement(helm);
#endif // ACCESSIBILITY_EXTENSION_SUPPORT

	SetNavigationElement(helm, FALSE);
	SetNavigationElementHighlighted(TRUE);

#ifdef SVG_SUPPORT
	// FIXME: Hack to make spatnav in SVG work.
	if (helm->GetNsType() == NS_SVG)
		GetVisualDevice()->SetDrawFocusRect(TRUE);
#endif // SVG_SUPPORT

	if (FramesDocElm *fdelm = frames_doc->GetDocManager()->GetFrame())
		frames_doc->GetTopDocument()->SetActiveFrame(fdelm);
	else
		frames_doc->SetNoActiveFrame();

	BOOL elm_handled = FALSE;

	if (helm->GetNsType() == NS_HTML)
	{
		switch (helm->Type())
		{
		case HE_INPUT:
			if (helm->GetInputType() == INPUT_IMAGE)
				image_form_selected = TRUE;
			/* fall through */

#ifdef _SSL_SUPPORT_
		case HE_KEYGEN:
#endif
		case HE_SELECT:
		case HE_TEXTAREA:
			SetCurrentFormElement(helm);
			if (invert_form_border_rect)
				InvertFormBorderRect(helm);
			if (focusFormElm)
			{
				FormObject* form_obj = helm->GetFormObject();
				if (form_obj)
				{
					/* FormObject::Focus calls SetFocusedElement which calls
					ClearFocusAndHighlight, so highlighted_element needs
					to be set to NULL during that call or it will be cleared
					again and a blur event will be sent. */

					SetNavigationElement(NULL, FALSE);
					form_obj->SetFocus(send_event ? FOCUS_REASON_ACTION : FOCUS_REASON_OTHER);
					send_event = FALSE;
					focused_element.SetElm(helm);
					SetNavigationElement(helm, FALSE);
					SetNavigationElementHighlighted(TRUE);
				}
			}
			invalidate_highlight_area = helm->GetInputType() == INPUT_IMAGE;

			// Fix if we want to draw borders around focused area with tabindex.

			elm_handled = TRUE;
			break;

		case HE_AREA:
			{
				HTML_Element* usemap_elm = SetAreaElementAsNavigationElement(helm);
				invalidate_highlight_area = TRUE;
				scroll_to_elm = usemap_elm;
			}
			elm_handled = TRUE;
			break;
		}
	}

#ifdef _WML_SUPPORT_
	else if (helm->GetNsType() == NS_WML)
	{
		switch (helm->Type())
		{
		case WE_GO:
		case WE_PREV:
		case WE_REFRESH:
			{
				HTML_Element *candidate = helm->Parent();
				while (candidate)
				{
					if (candidate->IsMatchingType(WE_ANCHOR, NS_WML)
						|| candidate->IsMatchingType(WE_DO, NS_WML))
						break;
				}

				if (candidate)
				{
					SetNavigationElement(candidate, FALSE);
					invalidate_highlight_area = TRUE;
					scroll_to_elm = candidate;
					elm_handled = TRUE;
				}
			}
			break;
		}
	}
#endif // _WML_SUPPORT_

#ifdef SVG_SUPPORT
	else if (helm->GetNsType() == NS_SVG)
	{
		SetFocusedElement(helm);
		SetNavigationElementHighlighted(TRUE);
	}
#endif // SVG_SUPPORT

	if (!elm_handled)
	{
		Box* box = helm->GetLayoutBox();
#ifdef DOCUMENT_EDIT_SUPPORT
		FramesDocElm *fde = helm->IsMatchingType(HE_IFRAME, NS_HTML) ? FramesDocElm::GetFrmDocElmByHTML(helm) : NULL;
#endif // DOCUMENT_EDIT_SUPPORT

		if (box && box->IsContentReplaced()
			&& (box->IsContentImage() || box->IsContentEmbed()))
		{
			SetFocusedElement(helm);
			image_form_selected = TRUE;
			SetCurrentFormElement(helm);
			invalidate_highlight_area = TRUE;
			SetNavigationElementHighlighted(TRUE);
		}
#ifdef DOCUMENT_EDIT_SUPPORT
		else if ((helm->IsContentEditable() && frames_doc->GetDocumentEdit()) ||
				 (helm->IsMatchingType(HE_IFRAME, NS_HTML) && fde && fde->GetCurrentDoc() && fde->GetCurrentDoc()->GetDesignMode()))
		{
			invalidate_highlight_area = TRUE;
		}
#endif // DOCUMENT_EDIT_SUPPORT
		// The BUTTON element is painted with a buttonwidget. We don't need to select
		// the text in it to show that it is focused. It gets the defaultborder-look instead.
		else if (helm->IsMatchingType(HE_BUTTON, NS_HTML))
		{
			if (box)
				if (ButtonContainer* bc = box->GetButtonContainer())
					bc->SetButtonFocus(helm, ConvertFocusOriginToFocusReason(origin));

			FormObject::ResetDefaultButton(frames_doc);
			invalidate_highlight_area = TRUE;
		}
#ifdef MEDIA_HTML_SUPPORT
		else if (helm->IsMatchingType(HE_VIDEO, NS_HTML) || helm->IsMatchingType(HE_AUDIO, NS_HTML))
		{
			SetFocusedElement(helm);
			SetNavigationElementHighlighted(TRUE);
			invalidate_highlight_area = TRUE;
		}
#endif // MEDIA_HTML_SUPPORT
		else
		{
#ifndef SKIN_HIGHLIGHTED_ELEMENT
			frames_doc->ClearDocumentSelection(FALSE, FALSE, TRUE);
			OP_ASSERT(!GetTextSelection() || !"We're leaking text selection because we're overwriting a live pointer");
			frames_doc->SetTextSelectionPointer(OP_NEW(TextSelection, ()));
			if (GetTextSelection())
				GetTextSelection()->SetNewSelection(frames_doc, helm, TRUE, FALSE, TRUE);
			else
				is_oom = TRUE;
#else // !SKIN_HIGHLIGHTED_ELEMENT
			invalidate_highlight_area = TRUE;
			image_form_selected = TRUE;
			SetCurrentFormElement(helm);
#endif // !SKIN_HIGHLIGHTED_ELEMENT

			// It may be an image in the HE_A element
#ifdef _WML_SUPPORT_
			if (!is_oom && (helm->IsMatchingType(HE_A, NS_HTML) || helm->IsMatchingType(WE_ANCHOR, NS_WML)))
#else // _WML_SUPPORT_
			if (!is_oom && (helm->IsMatchingType(HE_A, NS_HTML)))
#endif // _WML_SUPPORT_
			{
				URL* url = helm->GetA_URL(frames_doc->GetLogicalDocument());

				if (url)
					GetWindow()->DisplayLinkInformation(*url, ST_ASTRING, NULL);

				HTML_Element *child = helm->FirstChild(), *image = NULL;

				while (child && helm->IsAncestorOf(child))
				{
					Box* box = child->GetLayoutBox();
					if (box)
					{
						if (!image && box->IsContentReplaced() && (box->IsContentImage() || box->IsContentEmbed()))
							image = child;
						/* Check if object is an image */
						else if (!image && child->Type() == HE_OBJECT)
						{
							OpStringS8 str;
							if (child->HasAttr(ATTR_TYPE))
								if (OpStatus::IsSuccess(str.SetUTF8FromUTF16(child->GetStringAttr(ATTR_TYPE), uni_strlen(child->GetStringAttr(ATTR_TYPE)))))
									if (str.CompareI("image/", 6) == 0 || str.CompareI("text/svg", 8) == 0)
										image = child;
						}
						else if (child->Type() == HE_TEXT && !child->HasWhiteSpaceOnly())
						{
							image = NULL;
							break;
						}
					}

					child = (HTML_Element *) child->Next();
				}

#ifdef SKIN_HIGHLIGHTED_ELEMENT
				if (!image)
					image = helm;
#endif // SKIN_HIGHLIGHTED_ELEMENT

				if (image)
				{
					image_form_selected = TRUE;
					SetCurrentFormElement(image);
					scroll_to_elm = image;
					invalidate_highlight_area = TRUE;
				}
				else
					ret_val = GetTextSelection() && !GetTextSelection()->IsEmpty();
			}
		}
	}

	if (scroll && scroll_to_elm)
	{
		ScrollToElement(scroll_to_elm, SCROLL_ALIGN_LAZY_TOP, FALSE, VIEWPORT_CHANGE_REASON_HIGHLIGHT);
	}

	if (invalidate_highlight_area)
	{
		OP_ASSERT(!frames_doc->IsReflowing());
#ifndef SKIN_HIGHLIGHTED_ELEMENT // Skinned highlighting may need to repaint a bigger area than the element, so we can't just rely on the MarkDirty repaint
		if (frames_doc->GetLogicalDocument()->GetRoot()->IsDirty())
			helm->MarkDirty(frames_doc, TRUE, TRUE);
		else
#endif // SKIN_HIGHLIGHTED_ELEMENT
			InvalidateHighlightArea();
	}

	if (send_event)
		is_oom |= frames_doc->HandleEvent(ONFOCUS, NULL, helm, SHIFTKEY_NONE) == OpStatus::ERR_NO_MEMORY;

	// The highlighted element gets the :focus pseudo class set
	GetFramesDocument()->GetLogicalDocument()->GetLayoutWorkplace()->ApplyPropertyChanges(helm, CSS_PSEUDO_CLASS_GROUP_MOUSE, TRUE);

	if (is_oom)
	{
		GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);

		ret_val = FALSE;
	}

	return ret_val;
}

void HTML_Document::SetNavigationElement(HTML_Element* helm, BOOL unset_focus_element)
{
	if (unset_focus_element)
	{
		navigation_element.Reset();
		navigation_element_is_highlighted = FALSE;
		if (GetFocusedElement())
			ClearFocusAndHighlight();
	}

	// The navigation element is given the :focus pseudo class (see ElementHasFocus())
	// so we must tell the layout engine when we change the navigation element
	// in case it affects style
	if (navigation_element.GetElm() != helm)
	{
		HTML_Element* previous = navigation_element.GetElm();
		navigation_element.SetElm(helm);
		LayoutWorkplace* layout = GetFramesDocument()->GetLogicalDocument()->GetLayoutWorkplace();
		if (previous)
			layout->ApplyPropertyChanges(previous, CSS_PSEUDO_CLASS_FOCUS, TRUE);
		if (navigation_element.GetElm())
			layout->ApplyPropertyChanges(navigation_element.GetElm(), CSS_PSEUDO_CLASS_FOCUS, TRUE);
		else
			SetNavigationElementHighlighted(FALSE);
#ifdef DOC_SHOW_SPATIAL_NAVIGATION_IN_FORM
		if (navigation_element.GetElm() && navigation_element->IsFormElement())
			InvertFormBorderRect(navigation_element.GetElm());
#endif // DOC_SHOW_SPATIAL_NAVIGATION_IN_FORM
	}
}

OP_BOOLEAN HTML_Document::ActivateElement(HTML_Element* helm, ShiftKeyState keystate)
{
	if (!helm)
		helm = navigation_element.GetElm();

	if (helm)
	{
		if (helm->GetNsType() == NS_HTML)
		{
			switch (helm->Type())
			{
			case HE_LABEL: // Later we simulate a click on the label that will activate the label's form element
			case HE_LEGEND:
				ScrollToElement(helm, SCROLL_ALIGN_CENTER, FALSE, VIEWPORT_CHANGE_REASON_FORM_FOCUS);
				break;

#ifdef _SSL_SUPPORT_
			case HE_KEYGEN:
#endif
			case HE_SELECT:
			case HE_TEXTAREA:
				return FocusElement(helm, FOCUS_ORIGIN_UNKNOWN) ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;

			case HE_INPUT:
				switch (helm->GetInputType())
				{
				case INPUT_RADIO:
				case INPUT_CHECKBOX:
					{
						ScrollToElement(helm, SCROLL_ALIGN_CENTER, FALSE, VIEWPORT_CHANGE_REASON_FORM_FOCUS);
						// Since many pages have code in onclick handlers we have to trigger such an event
						// to make those pages behave correctly.
						// When we make this the right way, the actual toggle will be done by the click/activate event
						// so that it's possible to block it with an onclick handler
						FormValueRadioCheck* radiocheck_val = FormValueRadioCheck::GetAs(helm->GetFormValue());
						radiocheck_val->SaveStateBeforeOnClick(helm);
						// Check unless it's already a set checkbox. In that case uncheck
						BOOL set_to_state = !(helm->GetInputType() == INPUT_CHECKBOX && radiocheck_val->IsChecked(helm));
						FramesDocument* doc = GetFramesDocument();
						ES_Thread* thread = NULL;
						radiocheck_val->SetIsChecked(helm, set_to_state, doc, TRUE, thread);
						RETURN_IF_ERROR(FormValueListener::HandleValueChanged(doc, helm, TRUE, TRUE, thread));
						OP_STATUS status = doc->HandleMouseEvent(ONCLICK, NULL, helm, NULL,
								0, 0,       // x and y relative to the element
								0, 0, // x and y relative to the document
								keystate,
								0);
						RETURN_IF_ERROR(status);

						return OpBoolean::IS_TRUE;
					}

				case INPUT_SUBMIT:
				case INPUT_BUTTON:
				case INPUT_IMAGE:
					/* Fall through. */
					break;

				default:
					return FocusElement(helm, FOCUS_ORIGIN_UNKNOWN) ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
				}
#ifdef DOCUMENT_EDIT_SUPPORT
			default:
				if (helm->IsContentEditable() && frames_doc->GetDocumentEdit())
				{
					return FocusElement(helm, FOCUS_ORIGIN_UNKNOWN) ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
				}
				if (helm->Type() == HE_IFRAME)
				{
					FramesDocElm *fde = FramesDocElm::GetFrmDocElmByHTML(helm);
					if (fde && fde->GetCurrentDoc() && fde->GetCurrentDoc()->GetDesignMode())
						return FocusElement(helm, FOCUS_ORIGIN_UNKNOWN) ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
				}
#endif // DOCUMENT_EDIT_SUPPORT
			}
		}

		// First give it focus, then click it
		if (helm->IsFocusable(GetFramesDocument()))
			FocusElement(helm, FOCUS_ORIGIN_UNKNOWN);
		int document_x = 0, document_y = 0;
		RECT rect;
		if (helm->GetBoxRect(frames_doc, CONTENT_BOX, rect))
		{
			document_x = rect.left;
			document_y = rect.top;
		}
		OP_STATUS res = GetFramesDocument()->HandleMouseEvent(ONCLICK, NULL, helm, NULL, 0, 0, document_x, document_y, keystate, MOUSE_BUTTON_1);
		if (OpStatus::IsError(res))
			return res;
		else
			return OpBoolean::IS_TRUE;
	}
	return OpBoolean::IS_FALSE;
}

void HTML_Document::DeactivateElement()
{
	if (HTML_Element *focus_elm = GetFocusedElement())
	{
		if (focus_elm->GetNsType() == NS_HTML)
		{
			switch (focus_elm->Type())
			{
			case HE_INPUT:
#ifdef _SSL_SUPPORT_
			case HE_KEYGEN:
#endif
			case HE_SELECT:
			case HE_TEXTAREA:
				{
					if (focus_elm->GetInputType() != INPUT_IMAGE)
						HighlightElement(focus_elm, FOCUS_ORIGIN_DEACTIVATED);
					else
						GetWindow()->GetWindowCommander()->GetDocumentListener()
							->OnLinkNavigated(GetWindow()->GetWindowCommander(), NULL, NULL, FALSE);
					return;
				}
				break;

			default: break; // other types don't support focus anyway
			}
		}
	}
}

void HTML_Document::CleanReferencesToElement(HTML_Element* element)
{
	OP_ASSERT(element);

	if (element_with_selection == element)
		SetElementWithSelection(NULL);

#if defined SEARCH_MATCHES_ALL && !defined HAS_NO_SEARCHTEXT
	RemoveElementFromSearchHit(element);
#endif // SEARCH_MATCHES_ALL && !HAS_NO_SEARCHTEXT

#ifdef NEARBY_ELEMENT_DETECTION
	if (ElementExpander* expander = GetWindow()->GetElementExpander())
		expander->OnElementRemoved(element);
#endif // NEARBY_ELEMENT_DETECTION
}

void HTML_Document::BeforeTextDataChange(HTML_Element* text_node)
{
	if (search_selection)
		search_selection->BeforeTextDataChange(text_node);

#ifndef HAS_NO_SEARCHTEXT
# ifdef SEARCH_MATCHES_ALL
	if (!selections.Empty())
	{
		SelectionElm *affected_selection = NULL;
		if (OpStatus::IsSuccess(selections_map.GetData(text_node, &affected_selection)))
		{
			while (affected_selection &&
				    (affected_selection->GetSelection()->GetStartElement() == text_node ||
				     affected_selection->GetSelection()->GetEndElement() == text_node))
			{
				affected_selection->GetSelection()->BeforeTextDataChange(text_node);
				affected_selection = affected_selection->Suc();
			}
		}
	}
# endif // SEARCH_MATCHES_ALL
#endif // HAS_NO_SEARCHTEXT
}

void HTML_Document::AbortedTextDataChange(HTML_Element* text_node)
{
	if (search_selection)
		search_selection->AbortedTextDataChange(text_node);

#ifndef HAS_NO_SEARCHTEXT
# ifdef SEARCH_MATCHES_ALL
	if (!selections.Empty())
	{
		SelectionElm *affected_selection = NULL;
		if (OpStatus::IsSuccess(selections_map.GetData(text_node, &affected_selection)))
		{
			while (affected_selection &&
				    (affected_selection->GetSelection()->GetStartElement() == text_node ||
				     affected_selection->GetSelection()->GetEndElement() == text_node))
			{
				affected_selection->GetSelection()->AbortedTextDataChange(text_node);
				affected_selection = affected_selection->Suc();
			}
		}
	}
# endif // SEARCH_MATCHES_ALL
#endif // HAS_NO_SEARCHTEXT
}

void HTML_Document::TextDataChange(HTML_Element* text_element, HTML_Element::ValueModificationType modification_type, unsigned offset, unsigned length1, unsigned length2)
{
	if (search_selection)
		search_selection->TextDataChange(text_element, modification_type, offset, length1, length2);

#ifndef HAS_NO_SEARCHTEXT
# ifdef SEARCH_MATCHES_ALL
	if (!selections.Empty())
	{
		SelectionElm *affected_selection = NULL;
		if (OpStatus::IsSuccess(selections_map.GetData(text_element, &affected_selection)))
		{
			while (affected_selection &&
				    (affected_selection->GetSelection()->GetStartElement() == text_element ||
				     affected_selection->GetSelection()->GetEndElement() == text_element))
			{
				affected_selection->GetSelection()->TextDataChange(text_element, modification_type, offset, length1, length2);
				affected_selection = affected_selection->Suc();
			}
		}
	}
# endif // SEARCH_MATCHES_ALL
#endif // HAS_NO_SEARCHTEXT
}

void HTML_Document::OnTextConvertedToTextGroup(HTML_Element* elm)
{
	// Selections point to text elements and elements, not to textgroups. If a text element
	// changes we need to modify the selection as well.
	if (search_selection)
		search_selection->OnTextConvertedToTextGroup(elm);

#ifndef HAS_NO_SEARCHTEXT
# ifdef SEARCH_MATCHES_ALL
	if (!selections.Empty())
	{
		SelectionElm *affected_selection = NULL;
		if (OpStatus::IsSuccess(selections_map.GetData(elm, &affected_selection)))
		{
			MapSearchHit(elm->FirstChild(), affected_selection);
			while (affected_selection &&
				    (affected_selection->GetSelection()->GetStartElement() == elm ||
				     affected_selection->GetSelection()->GetEndElement() == elm))
			{
				affected_selection->GetSelection()->OnTextConvertedToTextGroup(elm);
				affected_selection = affected_selection->Suc();
			}
		}
	}
# endif // SEARCH_MATCHES_ALL
#endif // HAS_NO_SEARCHTEXT
}

#ifdef NEARBY_INTERACTIVE_ITEM_DETECTION
OP_STATUS HTML_Document::FindNearbyInteractiveItems(const OpRect& rect, List<InteractiveItemInfo>& list)
{
	LayoutWorkplace* layout_workplace = frames_doc->GetLogicalDocument() ? frames_doc->GetLogicalDocument()->GetLayoutWorkplace() : NULL;

	if (!layout_workplace)
		return OpStatus::OK;

	DocumentElementCollection element_collection;
	VisualDevice* vis_dev = GetVisualDevice();
	OpRect visible_rect = vis_dev->GetVisibleDocumentRect();
	OP_STATUS status;

	OpRect rect_limited = rect;
	/* According to the documentation of the method, rect should be fully inside visible_rect,
	   but instead of asserting it, it's simpler to just ensure it. */
	rect_limited.IntersectWith(visible_rect);
	if (rect_limited.IsEmpty())
		return OpStatus::OK;

	status = layout_workplace->FindNearbyInteractiveItems(rect_limited, element_collection, &visible_rect);

	RETURN_IF_ERROR(status);

	int elm_count = element_collection.GetCount();
	// Used to restrict the found element's rectangles. We don't want them to overlap some parent view (if such exists).
	OpRect clip_rect = vis_dev->GetDocumentInnerViewClipRect();

	// We must have non empty clip_rect if the view of this document is at least partially visible.
	OP_ASSERT(!clip_rect.IsEmpty());

	for (int i = 0; i < elm_count; i++)
	{
		OpDocumentElement* element = element_collection.Get(i);
		const DocumentElementRegion& element_region = element->GetRegion();
		unsigned int rect_count = static_cast<unsigned int>(element_region.GetCount());
		InteractiveItemInfo* item_info;

		if (!rect_count)
			continue;

		if (element->GetType() != DOCUMENT_ELEMENT_TYPE_HTML_ELEMENT) // Scrollbar.
		{
			item_info = InteractiveItemInfo::CreateInteractiveItemInfo(rect_count, InteractiveItemInfo::INTERACTIVE_ITEM_TYPE_SCROLLBAR);
			if (!item_info)
				return OpStatus::ERR_NO_MEMORY;
		}
		else // Logical element.
		{
			HTML_Element* helm = element->GetHtmlElement();

			item_info = NULL;
			HTML_ElementType type = helm->Type();
			NS_Type helm_ns_type = helm->GetNsType();
			BOOL context_attached_elm_handled = FALSE;

			if (helm_ns_type == NS_HTML && type == HE_INPUT)
			{
				InputType inputType = helm->GetInputType();

				if (inputType == INPUT_TEXT || inputType == INPUT_SEARCH || inputType == INPUT_NUMBER ||
					inputType == INPUT_EMAIL || inputType == INPUT_URI)
				{
					context_attached_elm_handled = TRUE;

					item_info = InteractiveItemInfo::CreateInteractiveItemInfo(rect_count, InteractiveItemInfo::INTERACTIVE_ITEM_TYPE_FORM_INPUT);
					if (!item_info)
						return OpStatus::ERR_NO_MEMORY;

					OpRect& first_rect = element_region.Get(0)->rect;
					OpPoint doc_point(first_rect.x, first_rect.y);

					OpDocumentContext* ctx = frames_doc->CreateDocumentContextForDocPos(doc_point, helm);
					if (!ctx)
					{
						OP_DELETE(item_info);
						return OpStatus::ERR_NO_MEMORY;
					}

					item_info->SetDocumentContext(ctx);

					if (OpStatus::IsMemoryError(item_info->SetURL(UNI_L(""))))
					{
						OP_DELETE(item_info);
						return OpStatus::ERR_NO_MEMORY;
					}
				}
			}

			if (!context_attached_elm_handled)
			{
				if (helm_ns_type == NS_HTML)
				{
					if (type == HE_INPUT && helm->GetInputType() == INPUT_CHECKBOX)
					{
						item_info = InteractiveItemInfo::CreateInteractiveItemInfo(rect_count,
							InteractiveItemInfo::INTERACTIVE_ITEM_TYPE_FORM_CHECKBOX);
						if (item_info)
						{
							FormValue* value = helm->GetFormValue();
							FormValueRadioCheck* checkbox_value = FormValueRadioCheck::GetAs(value);
							item_info->SetCheckboxState(checkbox_value && checkbox_value->IsChecked(helm));
						}
					}
					else if (type == HE_INPUT && helm->GetInputType() == INPUT_RADIO)
					{
						item_info = InteractiveItemInfo::CreateInteractiveItemInfo(rect_count,
							InteractiveItemInfo::INTERACTIVE_ITEM_TYPE_FORM_RADIO);
					}
					else
					{
						InteractiveItemInfo::Type item_type;

						// HE_INPUT with input type INPUT_TEXT, INPUT_SEARCH, INPUT_EMAIL, INPUT_NUMBER and INPUT_URI is handled earlier.
						if (FormManager::IsButton(helm))
							item_type = InteractiveItemInfo::INTERACTIVE_ITEM_TYPE_BUTTON;
						else if (type == HE_TEXTAREA)
							item_type = InteractiveItemInfo::INTERACTIVE_ITEM_TYPE_FORM_INPUT;
						else if (type == HE_INPUT)
							item_type = InteractiveItemInfo::INTERACTIVE_ITEM_TYPE_FORM_INPUT;
						else if (type == HE_SELECT)
							item_type = InteractiveItemInfo::INTERACTIVE_ITEM_TYPE_FORM_SELECT;
						else if (type == HE_A)
						{
							if (helm->GetAncestorMapElm())
								item_type = InteractiveItemInfo::INTERACTIVE_ITEM_TYPE_USEMAP_AREA;
							else
								item_type = InteractiveItemInfo::INTERACTIVE_ITEM_TYPE_ANCHOR;
						}
						else if (type == HE_AREA)
							item_type = InteractiveItemInfo::INTERACTIVE_ITEM_TYPE_USEMAP_AREA;
						else if (helm->HasEventHandler(frames_doc, ONCLICK, FALSE) ||
								 helm->HasEventHandler(frames_doc, ONMOUSEDOWN, FALSE) ||
								 helm->HasEventHandler(frames_doc, ONMOUSEUP, FALSE) ||
								 helm->HasEventHandler(frames_doc, ONDBLCLICK, FALSE))
							item_type = InteractiveItemInfo::INTERACTIVE_ITEM_TYPE_CUSTOM_POINTER_ACTION;
#ifdef TOUCH_EVENTS_SUPPORT
						else if (helm->HasEventHandler(frames_doc, TOUCHSTART, FALSE) ||
								 helm->HasEventHandler(frames_doc, TOUCHMOVE, FALSE) ||
								 helm->HasEventHandler(frames_doc, TOUCHEND, FALSE) ||
								 helm->HasEventHandler(frames_doc, TOUCHCANCEL, FALSE))
							item_type = InteractiveItemInfo::INTERACTIVE_ITEM_TYPE_CUSTOM_TOUCH_ACTION;
#endif // TOUCH_EVENTS_SUPPORT
						else
							item_type = InteractiveItemInfo::INTERACTIVE_ITEM_TYPE_OTHER;

						item_info = InteractiveItemInfo::CreateInteractiveItemInfo(rect_count, item_type);

						if (item_info != NULL && item_type == InteractiveItemInfo::INTERACTIVE_ITEM_TYPE_FORM_INPUT)
						{
							OpRect& first_rect = element_region.Get(0)->rect;
							OpPoint doc_point(first_rect.x, first_rect.y);

							OpDocumentContext* ctx = frames_doc->CreateDocumentContextForDocPos(doc_point, helm);
							if (!ctx)
							{
								OP_DELETE(item_info);
								return OpStatus::ERR_NO_MEMORY;
							}

							item_info->SetDocumentContext(ctx);
						}
					}
				}
#ifdef SVG_SUPPORT
				else if (helm_ns_type == NS_SVG)
				{
					if (type == Markup::SVGE_A)
					{
						item_info = InteractiveItemInfo::CreateInteractiveItemInfo(rect_count,InteractiveItemInfo::INTERACTIVE_ITEM_TYPE_ANCHOR);
					}
				}
#endif // SVG_SUPPORT
#ifdef _WML_SUPPORT_
				else if (helm_ns_type == NS_WML)
				{
					if (static_cast<WML_ElementType>(type) == WE_ANCHOR)
					{
						item_info = InteractiveItemInfo::CreateInteractiveItemInfo(rect_count, InteractiveItemInfo::INTERACTIVE_ITEM_TYPE_ANCHOR);
					}
				}
#endif // _WML_SUPPORT_
				else
				{
					item_info = InteractiveItemInfo::CreateInteractiveItemInfo(rect_count, InteractiveItemInfo::INTERACTIVE_ITEM_TYPE_OTHER);
				}

				if (!item_info)
					return OpStatus::ERR_NO_MEMORY;

				status = helm->GetAnchor_URL(frames_doc, item_info->GetURL());

				if (OpStatus::IsError(status))
				{
					OP_DELETE(item_info);
					return status;
				}
			}
		}

		InteractiveItemInfo::ItemRect* item_rect_array = item_info->GetRects();

		for (unsigned int j = 0; j < rect_count; j++)
		{
			OpTransformedRect* region_rect = element_region.Get(j);
			const AffinePos* affine_pos = region_rect->affine_pos;
			if (affine_pos)
			{
				item_rect_array[j].affine_pos = OP_NEW(AffinePos, (*affine_pos));
				if (!item_rect_array[j].affine_pos)
				{
					OP_DELETE(item_info);
					return OpStatus::ERR_NO_MEMORY;
				}
			}

			item_rect_array[j].rect = region_rect->rect;
			if (!affine_pos)
				item_rect_array[j].rect.IntersectWith(clip_rect);
		}

		item_info->Into(&list);
	}

	return OpStatus::OK;
}

OP_STATUS HTML_Document::FindNearbyLinks(const OpRect& rect, OpVector<HTML_Element>& list)
{
	LayoutWorkplace* layout_workplace = frames_doc->GetLogicalDocument() ? frames_doc->GetLogicalDocument()->GetLayoutWorkplace() : NULL;

	if (!layout_workplace)
		return OpStatus::OK;

	DocumentElementCollection element_collection;
	VisualDevice* vis_dev = GetVisualDevice();
	OpRect visible_rect = vis_dev->GetVisibleDocumentRect();

	OpRect rect_limited = rect;
	/* According to the documentation of the method, rect should be fully inside visible_rect,
	   but instead of asserting it, it's simpler to just ensure it. */
	rect_limited.IntersectWith(visible_rect);
	if (rect_limited.IsEmpty())
		return OpStatus::OK;

	RETURN_IF_ERROR(layout_workplace->FindNearbyInteractiveItems(rect_limited, element_collection, &visible_rect));

	int elm_count = element_collection.GetCount();
	for (int i = 0; i < elm_count; i++)
	{
		OpDocumentElement* element = element_collection.Get(i);
		const DocumentElementRegion& element_region = element->GetRegion();
		unsigned int rect_count = static_cast<unsigned int>(element_region.GetCount());

		if (!rect_count)
			continue;

		HTML_Element* helm = element->GetHtmlElement();

		if (helm->IsMatchingType(HE_A, NS_HTML))
			list.Add(helm);
#ifdef SVG_SUPPORT
		else if (helm->IsMatchingType(Markup::SVGE_A, NS_SVG))
			list.Add(helm);
#endif // SVG_SUPPORT
#ifdef _WML_SUPPORT_
		else if (helm->IsMatchingType(WE_ANCHOR, NS_WML))
			list.Add(helm);
#endif // _WML_SUPPORT_
	}

	return OpStatus::OK;
}
#endif // NEARBY_INTERACTIVE_ITEM_DETECTION

#if defined SEARCH_MATCHES_ALL && !defined HAS_NO_SEARCHTEXT
void HTML_Document::RemoveElementFromSearchHit(HTML_Element *element)
{
	if (!selections.Empty())
	{
		SelectionElm *removed_match = NULL;
		selections_map.Remove(element, &removed_match);
		OP_ASSERT(removed_match || !element->IsInSelection());

		while (removed_match)
		{
			SelectionElm *next_match = static_cast<SelectionElm*>(removed_match->Suc());
			HTML_Element *next_match_start = next_match ? next_match->GetSelection()->GetStartElement() : NULL;
			TextSelection *removed_selection = removed_match->GetSelection();
			HTML_Element *last_elm = removed_selection->GetEndElement()->NextSibling();

			element->SetIsInSelection(FALSE);

			for (HTML_Element *iter_elm = removed_selection->GetStartElement();
				iter_elm && iter_elm != last_elm;
				iter_elm = iter_elm->Next())
			{
				if (iter_elm != element)
				{
					SelectionElm *dummy_match = NULL;
					selections_map.Remove(iter_elm, &dummy_match);

					if (iter_elm == next_match_start)
					{
						MapSearchHit(iter_elm, next_match);
					}
					else
					{
						iter_elm->SetIsInSelection(FALSE);
					}
				}

			}

			if (active_selection == removed_match)
				active_selection = static_cast<SelectionElm*>(active_selection->Suc());

#ifdef _DEBUG
			OpHashIterator* it = selections_map.GetIterator();
			if (it)
			{
				OP_STATUS status = it->First();
				while (OpStatus::IsSuccess(status))
				{
					OP_ASSERT(it->GetData() != removed_match || !"Failed to clear all references to the SelectionElm");
					status = it->Next();
				}
				OP_DELETE(it);
			}
#endif // _DEBUG
			removed_match->Out();
			OP_DELETE(removed_match);

			if (next_match_start == element)
				removed_match = next_match;
			else
				break;
		}
	}
}
#endif // SEARCH_MATCHES_ALL && !HAS_NO_SEARCHTEXT

void HTML_Document::UpdateSecurityState(BOOL include_loading_docs)
{
	BYTE document_security = frames_doc->GetURL().GetAttribute(URL::KSecurityStatus);
	if (document_security != SECURITY_STATE_UNKNOWN) // Can be unknown for about:blank for instance
	{
		GetWindow()->SetSecurityState(document_security, FALSE, frames_doc->GetURL().GetAttribute(URL::KSecurityText).CStr());
	}
}
