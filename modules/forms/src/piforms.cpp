/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 */

#include "core/pch.h"

#include "modules/forms/piforms.h"
#include "modules/forms/formvaluetext.h"

#include "modules/layout/cascade.h"
#include "modules/layout/cssprops.h"
#include "modules/layout/layout_workplace.h"
#include "modules/layout/layoutprops.h"
#include "modules/logdoc/urlimgcontprov.h"
#include "modules/forms/formsuggestion.h"
#include "modules/forms/formvaluelistener.h"
#include "modules/forms/formmanager.h"
#include "modules/forms/formsenum.h"
#include "modules/forms/formvaluelist.h"
#include "modules/forms/webforms2support.h"
#include "modules/forms/webforms2number.h"
#include "modules/forms/datetime.h"
#include "modules/forms/src/validationerrorwindow.h"
#include "modules/widgets/OpColorField.h"
#include "modules/widgets/OpWidgetFactory.h"
#include "modules/widgets/OpFileChooserEdit.h"
#include "modules/widgets/OpScrollbar.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/widgets/OpNumberEdit.h"
#include "modules/widgets/OpCalendar.h"
#include "modules/widgets/OpDateTime.h"
#include "modules/widgets/OpTime.h"
#include "modules/widgets/OpSlider.h"

#ifdef NEARBY_ELEMENT_DETECTION
# include "modules/widgets/finger_touch/element_expander.h"
#endif // NEARBY_ELEMENT_DETECTION

#ifdef WAND_SUPPORT
# include "modules/wand/wandmanager.h"
#endif // WAND_SUPPORT

#include "modules/inputmanager/inputmanager.h"
#include "modules/forms/pi_forms_listeners.h"

#include "modules/hardcore/mh/messages.h"
#include "modules/util/opfile/opfile.h"
#include "modules/dochand/win.h"
#include "modules/locale/oplanguagemanager.h"

#include "modules/forms/formvalue.h"
#include "modules/forms/formvalueradiocheck.h"
#include "modules/dochand/docman.h"
#include "modules/doc/frm_doc.h"
#include "modules/doc/html_doc.h"
#include "modules/display/styl_man.h"

#include "modules/url/url_sn.h"

#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"

#define FORM_VALUE_REBUILD_DELAY (5) // ms

static JUSTIFY GetJustify(const HTMLayoutProperties& props, FormObject* fobj)
{
	JUSTIFY justify;
	switch(props.text_align)
	{
		default: // This includes CSS_VALUE_left but also illegal values as CSS_VALUE_top or CSS_VALUE_bottom
			justify = JUSTIFY_LEFT;
			break;
		case CSS_VALUE_right:
			justify = JUSTIFY_RIGHT;
			break;
		case CSS_VALUE_center:
			justify = JUSTIFY_CENTER;
			break;
	}

	return justify;
}

// == FormObject =================================================

/* static */
BOOL FormObject::IsEditFieldType(InputType type)
{
	switch (type)
	{
	case INPUT_TEXT:
	case INPUT_PASSWORD:
	case INPUT_URI:
	case INPUT_EMAIL:
	case INPUT_TEL:
	case INPUT_SEARCH:
		return TRUE;
	default:
		return FALSE;
	}
}

void FormObject::OnOptionsCollected()
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
}

FormObject::FormObject(VisualDevice *vd, HTML_Element* he, FramesDocument* doc)
     :
	opwidget(NULL)
	, m_doc(doc)
	, m_vis_dev(vd)
	, m_listener(NULL)
	, m_width(0)
	, m_height(0)
	, m_displayed(FALSE)
	, m_helm(he)
	, m_update_properties(TRUE)
	, m_locked(FALSE)
	, m_validation_error(NULL)
	, m_autofocus_attribute_processed(FALSE)
	, m_memsize(0)
	, css_border_left_width(0)
	, css_border_top_width(0)
	, css_border_right_width(0)
	, css_border_bottom_width(0)
{
	OP_ASSERT(he->GetInserted() != HE_INSERTED_BY_LAYOUT);
}

FormObject::~FormObject()
{
#ifdef _DEBUG
	m_helm->GetFormValue()->AssertNotExternal();
#endif // _DEBUG

	if (m_validation_error)
	{
		m_validation_error->Close();
		OP_ASSERT(!m_validation_error); // Should have been cleared by the |Close()|
	}

	HTML_Document* html_doc = m_doc->GetHtmlDocument();

	if (html_doc)
	{
		if (html_doc->GetFocusedElement() == m_helm && !m_locked)
			html_doc->ResetFocusedElement();
		if (html_doc->GetElementWithSelection() == m_helm)
			html_doc->SetElementWithSelection(NULL);
	}

#ifndef HAS_NOTEXTSELECTION
	// FIXME: Why? Check cvs logs and see why this was put here
	m_doc->RemoveFromSelection(m_helm);
#endif

	// We don't want anyone trying to mess with the widget focus while we
	// delete it so we do this first to let things settle
	if (g_input_manager)
	{
		g_input_manager->ReleaseInputContext(this);
	}

	// Deleting opwidget will result in a focusrequest on this
	// formobject. Setting opwidget to NULL before, will prevent us
	// from trying to set the focus back to opwidget.
	OpWidget* widget_to_delete = opwidget;
	if(widget_to_delete)
	{
		opwidget = NULL;

		widget_to_delete->Delete();
	}

	OP_DELETE(m_listener);

	if (m_doc->unfocused_formelement == m_helm)
		m_doc->unfocused_formelement = NULL;

	if (m_doc->current_focused_formobject == this)
		m_doc->current_focused_formobject = NULL;

	if (m_doc->current_default_formelement == m_helm)
		m_doc->current_default_formelement = NULL;
	// Re-resolve CSS3 UI pseudo class? It shouldn't be necessary since we're going away.

	FormObject* the_backup = m_helm->GetFormObjectBackup();
	if (the_backup != NULL && the_backup != this)
		m_helm->DestroyFormObjectBackup();

	if (m_memsize)
		REPORT_MEMMAN_DEC(m_memsize);
}

void FormObject::GetPosInDocument(AffinePos* pos)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	if (IsDisplayed())
	{
		*pos = opwidget->GetPosInDocument();
	}
	else // If it hasn't been displayed, it can't possible know its position. We need to traverse to get it.
	{
		RECT rect;
		BOOL status = m_doc->GetLogicalDocument()->GetBoxRect(m_helm, BOUNDING_BOX, rect);
		if (status)
		{
			pos->SetTranslation(rect.left, rect.top);

			opwidget->SetPosInDocument(*pos);
		}
		else
		{
			pos->SetTranslation(0, 0);
		}
	}
}

#ifdef OP_KEY_CONTEXT_MENU_ENABLED
BOOL FormObject::GetCaretPosInDocument(INT32* x, INT32* y)
{
	AffinePos widget_pos;
	GetPosInDocument(&widget_pos);

	if (m_helm->IsMatchingType(HE_TEXTAREA, NS_HTML))
	{
		OpMultilineEdit* multiedit = static_cast<OpMultilineEdit*>(opwidget);

		OpPoint widget_local_caret_pos = multiedit->GetCaretCoordinates();

		widget_pos.Apply(widget_local_caret_pos);
		*x = widget_local_caret_pos.x;
		*y = widget_local_caret_pos.y;
		return TRUE;
	}

	if (m_helm->IsMatchingType(HE_INPUT, NS_HTML) &&
		IsEditFieldType(m_helm->GetInputType()))
	{
		OpEdit* edit = static_cast<OpEdit*>(opwidget);
		OpPoint widget_local_caret_pos = edit->GetCaretCoordinates();

		widget_pos.Apply(widget_local_caret_pos);
		*x = widget_local_caret_pos.x;
		*y = widget_local_caret_pos.y;
		return TRUE;
	}

	return FALSE;
}
#endif // OP_KEY_CONTEXT_MENU_ENABLED

/* static */
OpPoint FormObject::ToWidgetCoords(OpWidget* widget, const OpPoint& doc_point)
{
	OpPoint widget_point = doc_point;
	AffinePos doc_pos = widget->GetPosInDocument();

	doc_pos.ApplyInverse(widget_point);
	return widget_point;
}

void FormObject::SetCssBorders(const HTMLayoutProperties& props)
{
	BOOL hasborder = HasSpecifiedBorders(props, m_helm);
	css_border_left_width = (hasborder ? props.border.left.width : 0);
	css_border_top_width = (hasborder ? props.border.top.width : 0);
	css_border_right_width = (hasborder ? props.border.right.width : 0);
	css_border_bottom_width = (hasborder ? props.border.bottom.width : 0);
}

/* static */
BOOL FormObject::HasSpecifiedBorders(const HTMLayoutProperties& props, HTML_Element* helm)
{
	BOOL has_border_style =
		props.border.left.style != BORDER_STYLE_NOT_SET ||
		props.border.top.style != BORDER_STYLE_NOT_SET ||
		props.border.right.style != BORDER_STYLE_NOT_SET ||
		props.border.bottom.style != BORDER_STYLE_NOT_SET;

	BOOL has_css_border;
	if (helm->Type() == HE_INPUT &&
		(helm->GetInputType() == INPUT_CHECKBOX ||
		helm->GetInputType() == INPUT_RADIO))
	{
		// So many people set border:none for the input tag and
		// don't realize that it will affect checkboxes and radio buttons
		// that we can't accept that setting. Other browsers either don't
		// supports removing borders on checkboxes/radio buttons (NS4/MSIE)
		// or actively disallows it (Mozilla).
		has_css_border = has_border_style &&
			!(props.border.left.style == CSS_VALUE_none ||
			  props.border.top.style == CSS_VALUE_none ||
			  props.border.right.style == CSS_VALUE_none ||
			  props.border.bottom.style == CSS_VALUE_none);
		return has_css_border;
	}
	else // Normal input elements
	{
		has_css_border =
			has_border_style ||
			props.HasBorderImage() ||
			props.border.left.width == 0 ||
			props.border.top.width == 0 ||
			props.border.right.width == 0 ||
			props.border.bottom.width == 0;
	}
	return has_css_border;
}

/* static */
BOOL FormObject::HasSpecifiedBackgroundImage(FramesDocument* frm_doc, const HTMLayoutProperties& props, HTML_Element* helm)
{
	if (props.GetBgIsTransparent())
		return TRUE;

	if (props.HasBorderRadius() && HasSpecifiedBorders(props, helm))
	{
		/* border-radius shrinks the background area so forms can't
		   paint it. It would paint over the rounded corners. */

		return TRUE;
	}

	if (frm_doc)
	{
		if (props.HasBoxShadow() && props.HasInsetBoxShadow(frm_doc->GetVisualDevice()))
			return TRUE;

		HLDocProfile* hld_profile = frm_doc->GetHLDocProfile();
		if (hld_profile)
		{
			URL* base_url = hld_profile->BaseURL();
			if (base_url)
				return props.bg_images.IsAnyImageLoaded(*base_url);
		}
	}

	return props.bg_images.IsAnyImageLoaded(URL());
}

HTML_Element* FormObject::GetInnermostElement(int document_x, int document_y)
{
	if (m_helm->Type() == HE_SELECT)
	{
		SelectionObject* sel_obj = static_cast<SelectionObject*>(this);
		if (sel_obj->IsListbox())
		{
			HTML_Element* option = sel_obj->GetOptionAtPos(document_x, document_y);
			if (option)
			{
				return option;
			}
		}
	}

	return m_helm;
}

BOOL FormObject::FocusNextInternalTabStop(BOOL forward)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	OP_ASSERT(!IsAtLastInternalTabStop(forward));

	OpWidget *next = opwidget->GetNextInternalTabStop(forward);
	if (next)
	{
		next->SetFocus(FOCUS_REASON_KEYBOARD);
		return TRUE;
	}

	return FALSE;
}

void FormObject::FocusInternalEdge(BOOL front)
{
	OpWidget* widget = opwidget->GetFocusableInternalEdge(front);
	if (!widget)
		return;
	OP_ASSERT(widget);

	// Found the edge
	widget->SetFocus(FOCUS_REASON_KEYBOARD);
}

BOOL InputObject::GetDefaultCheck() const
{
	return m_is_checked_by_default;
}

void	InputObject::SetDefaultCheck(BOOL chk)
{
    m_is_checked_by_default = chk;
}

// == Hooks ==============================================

#ifndef MOUSELESS

void FormObject::OnSetCursor(const OpPoint &point)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	opwidget->GenerateOnSetCursor(ToWidgetCoords(opwidget, point));
}

void FormObject::OnMouseMove(const OpPoint &point)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	if(/*opwidget->IsEnabled() &&*/ !m_locked)
	{
		opwidget->GenerateOnMouseMove(ToWidgetCoords(opwidget, point));
	}
}

#ifdef WAND_HAS_MATCHES_WARNING
/**
 * Sometimes we don't want to let the user enter a text field until we have informed the user
 * about the availability of wand data. This is the case for instance in the BREW version.
 * In those cases this method will return TRUE and display a warning. Otherwise it
 * will return FALSE and do nothing.
 */
BOOL FormObject::WarnIfWandHasData()
{
	if (m_doc && m_doc->GetHasWandMatches() && m_helm &&
		m_helm->Type() == HE_INPUT && (m_helm->GetInputType() == INPUT_TEXT || m_helm->GetInputType() == INPUT_PASSWORD))
	{
		if (g_wand_manager->HasMatch(m_doc, m_helm) != 0)
		{
			g_wand_manager->ShowHasMatchesWarning(this, m_doc->GetHTML_Document(), HTML_Document::FOCUS_ORIGIN_UNKNOWN);
			return TRUE;
		}
	}
	return FALSE;
}

#endif // WAND_HAS_MATCHES_WARNING

BOOL FormObject::OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks, BOOL script_cancelled /* = FALSE */)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	if(opwidget->IsEnabled())
	{
		OpPoint widget_point = ToWidgetCoords(opwidget, point);
		OpWidget* best_candidate = opwidget;

#if defined(WIDGETS_IME_SUPPORT) && defined(WIDGETS_IME_SUPPRESS_ON_MOUSE_DOWN)
		opwidget->TrySuppressIMEOnMouseDown(widget_point);
#endif

		OpWidget* child = best_candidate->GetWidget(widget_point, TRUE);
		if (child && child->GetType() != OpTypedObject::WIDGET_TYPE_SCROLLBAR)
		{
			if (child->IsInternalChild() && !child->IsTabStop())
			{
				// It's a child that doesn't want focus. If this form object already has focus (on some child) don't
				// change it. If it hasn't, find a better candidate
				if (IsFocused(TRUE))
					best_candidate = OpWidget::GetFocused();
				else
				{
					// Find a parent to this child that wants focus, or eventually find the opwidget itself which will result
					// in focus on the default child for that opWidget.
					while (child != opwidget)
					{
						if (child->IsTabStop())
							break;
						child = child->GetParent();
					}
					best_candidate = child;
				}
			}
			else
				best_candidate = child;
		}

		if (!script_cancelled)
		{
			if (!child || child->GetType() != OpTypedObject::WIDGET_TYPE_SCROLLBAR)
			{
#ifdef WAND_HAS_MATCHES_WARNING
				if (WarnIfWandHasData())
				{
					return TRUE;
				}
#endif // WAND_HAS_MATCHES_WARNING
			}
			else if (child->GetType() == OpTypedObject::WIDGET_TYPE_SCROLLBAR && GetWidget() && GetWidget()->GetType() == WIDGET_TYPE_MULTILINE_EDIT)
				best_candidate = child;

			BOOL set_focus = TRUE;

#ifdef NEARBY_ELEMENT_DETECTION
			if (best_candidate->IsCloneable() && ElementExpander::IsEnabled())
			{
				set_focus = FALSE;
			}
#endif
			if (set_focus)
			{
				// Activate the proper document.
				FramesDocElm* frame = m_doc->GetDocManager()->GetFrame();
				if (frame)
					m_doc->GetTopDocument()->SetActiveFrame(frame, FALSE);
				else
					m_doc->GetTopDocument()->SetNoActiveFrame(FOCUS_REASON_MOUSE);
				best_candidate->SetFocus(FOCUS_REASON_MOUSE);
			}
		}

		// You should be able to interact with dropdowns, checkboxes and
		// radio buttons even if the mousedown is canceled.
		BOOL has_actions_even_if_cancelled = m_helm->IsMatchingType(HE_SELECT, NS_HTML) ||
			(m_helm->IsMatchingType(HE_INPUT, NS_HTML) && (m_helm->GetInputType() == INPUT_CHECKBOX ||
			m_helm->GetInputType() == INPUT_RADIO || m_helm->GetInputType() == INPUT_FILE));
		if (!script_cancelled || has_actions_even_if_cancelled)
		{
			// OnMouseDown is propagated down to all widgets
			opwidget->GenerateOnMouseDown(widget_point, button, nclicks);
		}

		// We have always returned FALSE for scrollbars. At first it had to
		// do with all event sending, but now it's only used by MouseListener::OnMouseLeftDblClk
		// to handle double click.
		if (best_candidate->GetType() == OpTypedObject::WIDGET_TYPE_SCROLLBAR)
			return FALSE;
	}
	return TRUE;
}

void FormObject::OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks /*= 1*/)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	if(opwidget->IsEnabled())
	{
		opwidget->GenerateOnMouseUp(ToWidgetCoords(opwidget, point), button, nclicks);
	}
}

BOOL FormObject::OnMouseWheel(const OpPoint &point, INT32 delta, BOOL vertical)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.

	UpdatePosition();

	OpPoint tp = ToWidgetCoords(opwidget, point);

	// Get target child widget in the form, or use the form itself is there are no intersecting child.
	OpWidget *target_widget = opwidget->GetWidget(tp, TRUE, FALSE);
	if (!target_widget)
		target_widget = opwidget;

	if (target_widget->GenerateOnMouseWheel(delta, vertical))
		return TRUE;
	if (opwidget->IsScrollable(vertical))
		return TRUE;
	return FALSE;
}

#endif // !MOUSELESS

BOOL FormObject::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
	case OpInputAction::ACTION_LOWLEVEL_PREFILTER_ACTION:
		{
			action = action->GetChildAction();
			switch (action->GetAction())
			{
			case OpInputAction::ACTION_LOWLEVEL_KEY_DOWN:
			case OpInputAction::ACTION_LOWLEVEL_KEY_UP:
			case OpInputAction::ACTION_LOWLEVEL_KEY_PRESSED:
				if (action->IsKeyboardInvoked())
				{
					UpdatePosition();
					OpKey::Code key = action->GetActionKeyCode();

					DOM_EventType event_type = ONKEYPRESS;
					if (action->GetAction() == OpInputAction::ACTION_LOWLEVEL_KEY_DOWN)
						event_type = ONKEYDOWN;
					else if (action->GetAction() == OpInputAction::ACTION_LOWLEVEL_KEY_UP)
						event_type = ONKEYUP;
#ifdef ACCESS_KEYS_SUPPORT
					else if (m_doc->GetWindow()->GetAccesskeyMode())
					{
#ifdef DONT_ALLOW_ACCESSKEYS_IN_FORMS
						// Still allow access keys in some types of form elements that have no
						// key handling of their own.
						if ((m_helm->Type() != HE_INPUT || GetInputType() == INPUT_BUTTON) && m_helm->Type() != HE_TEXTAREA)
#endif // DONT_ALLOW_ACCESSKEYS_IN_FORMS
							return FALSE; // Let the document process the key press as an access key
					}
#endif // ACCESS_KEYS_SUPPORT

					OP_BOOLEAN result = FramesDocument::SendDocumentKeyEvent(m_doc, m_helm, event_type, key, action->GetKeyValue(), action->GetPlatformKeyEventData(), action->GetShiftKeys(), action->GetKeyRepeat(), action->GetKeyLocation(), 0);
					RAISE_IF_MEMORY_ERROR(result);
					return result == OpBoolean::IS_TRUE;
				}
				break;
			}
		}
		break;

		case OpInputAction::ACTION_FOCUS_NEXT_RADIO_WIDGET:
		case OpInputAction::ACTION_FOCUS_PREVIOUS_RADIO_WIDGET:
		{
			return CheckNextRadio(action->GetAction() == OpInputAction::ACTION_FOCUS_NEXT_RADIO_WIDGET);
		}
	}

	return FALSE;
}

OpRect FormObject::GetBorderRect()
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	OpRect r(0, 0, Width(), Height());

	if (opwidget->HasCssBorder())
	{
		r.x -= css_border_left_width;
		r.y -= css_border_top_width;
		r.width += css_border_left_width + css_border_right_width;
		r.height += css_border_top_width + css_border_bottom_width;
	}
	return r;
}

void ConfigureScrollbars(OpMultilineEdit* multiedit, const HTMLayoutProperties& props)
{
	WIDGET_SCROLLBAR_STATUS status_x = SCROLLBAR_STATUS_AUTO;
	WIDGET_SCROLLBAR_STATUS status_y = SCROLLBAR_STATUS_AUTO;
	if (props.overflow_x == CSS_VALUE_hidden)
		status_x = SCROLLBAR_STATUS_OFF;
	else if (props.overflow_x == CSS_VALUE_scroll)
		status_x = SCROLLBAR_STATUS_ON;
	if (props.overflow_y == CSS_VALUE_hidden)
		status_y = SCROLLBAR_STATUS_OFF;
	else if (props.overflow_y == CSS_VALUE_scroll)
		status_y = SCROLLBAR_STATUS_ON;

	multiedit->SetScrollbarStatus(status_x, status_y);
}

void FormObject::SetWidgetPosition(VisualDevice* vis_dev)
{
	opwidget->SetVisualDevice(vis_dev);
	opwidget->SetRect(OpRect(0, 0, Width(), Height()), FALSE);
	opwidget->SetPosInDocument(vis_dev->GetCTM());
}

static BOOL GetTextAreaWrapping(HTML_Element* helm, const HTMLayoutProperties& props)
{
	OP_ASSERT(helm);
	BOOL use_wrapping;
	const uni_char* str = helm->GetStringAttr(ATTR_WRAP);
	if (str && uni_stri_eq(str, "OFF"))
		use_wrapping = FALSE;
	else
		use_wrapping = TRUE;

	if (props.white_space == CSS_VALUE_nowrap)
		use_wrapping = FALSE;

	return use_wrapping;
}

static WIDGET_RESIZABILITY GetResizability(const HTMLayoutProperties& props)
{
	switch (props.resize)
	{
	default:
	case CSS_VALUE_none:
		return WIDGET_RESIZABILITY_NONE;
	case CSS_VALUE_both:
		return WIDGET_RESIZABILITY_BOTH;
	case CSS_VALUE_vertical:
		return WIDGET_RESIZABILITY_VERTICAL;
	case CSS_VALUE_horizontal:
		return WIDGET_RESIZABILITY_HORIZONTAL;
	}
}

OP_STATUS FormObject::Display(const HTMLayoutProperties& props, VisualDevice* vis_dev)
{
	// Anything might have changed so we reread as much as possible from the element

	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	COLORREF oldcol = vis_dev->GetColor();

	opwidget->BeginChangeProperties();

	BOOL need_to_move_popup = m_validation_error && !(vis_dev->GetCTM() == opwidget->GetPosInDocument());
	// Set required information on OpWidget before we do any drawing..
	VisualDevice* old_vis_dev = opwidget->GetVisualDevice();

	SetWidgetPosition(vis_dev);

	OP_ASSERT(props.font_color != USE_DEFAULT_COLOR);
	opwidget->SetForegroundColor(HTM_Lex::GetColValByIndex(props.font_color));

	if (need_to_move_popup)
	{
		m_validation_error->HandleNewWidgetPosition();
	}
	// bg_color is a COLORREF (UINT32) and USE_DEFAULT_COLOR is signed long

	if (props.bg_color != (COLORREF)USE_DEFAULT_COLOR)
		opwidget->SetBackgroundColor(HTM_Lex::GetColValByIndex(props.bg_color));
	else
		opwidget->UnsetBackgroundColor();

	BOOL needs_css_borders = HasSpecifiedBorders(props, m_helm);
	opwidget->SetHasCssBorder(needs_css_borders);

	opwidget->SetHasCssBackground(HasSpecifiedBackgroundImage(m_doc, props, m_helm));
	// Maybe do MarkDirty in DOM instead?
	JUSTIFY justify = GetJustify(props, this);

	opwidget->SetProps(props);

	int fontnr = vis_dev->GetCurrentFontNumber();
	opwidget->SetFontInfo(styleManager->GetFontInfo(fontnr),
							vis_dev->GetFontSize(),
							vis_dev->IsTxtItalic(),
							vis_dev->GetFontWeight(),
							justify,
							vis_dev->GetCharSpacingExtra());

	if (GetInputType() == INPUT_TEXTAREA)
	{
		OpMultilineEdit* multiedit = (OpMultilineEdit*) opwidget;
		multiedit->SetLineHeight(props.GetCalculatedLineHeight(vis_dev));
		ConfigureScrollbars(multiedit, props);

		const BOOL use_wrapping = GetTextAreaWrapping(m_helm, props);
		multiedit->SetWrapping(use_wrapping);
		multiedit->SetAggressiveWrapping(use_wrapping && (props.overflow_wrap == CSS_VALUE_break_word));
	}

	ConfigureWidgetFromHTML(FALSE);

#ifdef WIDGETS_IME_SUPPORT
	UpdateIMStyle();
#endif // WIDGETS_IME_SUPPORT

	opwidget->SetResizability(GetResizability(props));

	opwidget->EndChangeProperties();

	if (m_update_properties)
	{
		// Fix: update font here

		m_update_properties = FALSE;
	}

	OpRect current_cliprect = vis_dev->GetClipping();
	current_cliprect.OffsetBy(vis_dev->GetRenderingViewX(), vis_dev->GetRenderingViewY());

	OpRect widget_doc_rect = opwidget->GetDocumentRect();
	if (widget_doc_rect.Intersecting(current_cliprect))
		opwidget->GenerateOnPaint(opwidget->GetBounds());

	PaintSpecialBorders(props, vis_dev);

	// Must restore the old visualdevice. If the user prints directly from printpreview the formsobjects in the printlayout will
	// temporarily swap to the print_device and if they aren't displayed in printprev again, they are stuck with a deleted printdev.
	if (old_vis_dev && old_vis_dev != vis_dev)
		opwidget->SetVisualDevice(old_vis_dev);

	m_displayed = TRUE;

	vis_dev->SetColor(oldcol);

	return OpStatus::OK;
}

void PaintSpecialBorder(const HTMLayoutProperties& props, VisualDevice* vis_dev, const OpRect& r, COLORREF color, short border_style, BOOL border_inset)
{
#ifdef VEGA_OPPAINTER_SUPPORT
	Border b = props.border;
	b.SetWidth(1);
	b.SetStyle(CSS_VALUE_solid);
	b.SetColor(color);
	// Band-aid to make forms with radius not look too crappy when we draw special borders.
	// Instead of drawing rectangle, we will draw a radius stroke that nearly matches the border.
	// This doesn't work for form fields rounded by background images and this code needs to be go away in the future.

	// Decrease radius (on corners we have radius on) since we inset the special border with 1px.
	if (border_inset)
	{
		b.left.radius_start = MAX(0, b.left.radius_start - 1);
		b.left.radius_end = MAX(0, b.left.radius_end - 1);
		b.top.radius_start = MAX(0, b.top.radius_start - 1);
		b.top.radius_end = MAX(0, b.top.radius_end - 1);
		b.right.radius_start = MAX(0, b.right.radius_start - 1);
		b.right.radius_end = MAX(0, b.right.radius_end - 1);
		b.bottom.radius_start = MAX(0, b.bottom.radius_start - 1);
		b.bottom.radius_end = MAX(0, b.bottom.radius_end - 1);
	}
	vis_dev->Translate(r.x, r.y);
	vis_dev->DrawBorderWithRadius(r.width, r.height, &b);
	vis_dev->Translate(-r.x, -r.y);
#else
	vis_dev->SetColor(color);
	vis_dev->RectangleOut(r.x, r.y, r.width, r.height, border_style);
#endif
}

void FormObject::PaintSpecialBorders(const HTMLayoutProperties& props, VisualDevice* vis_dev)
{
	InputType input_type = GetInputType();

	BOOL need_special_border = FALSE;
	COLORREF special_border_color = 0; // Init to silence stupid MSVC compiler

#ifdef WAND_SUPPORT
	FormValue* form_value = GetHTML_Element()->GetFormValue();
	int exists_in_wand = form_value->ExistsInWand();
	if (exists_in_wand)
	{
		need_special_border = TRUE;
		special_border_color = OP_RGB(255, 205, 0); // orange
	}
#endif // WAND_SUPPORT

	// It the object has a match in the wand profile.
	if (need_special_border)
	{
		OpRect r = OpRect(0, 0, Width(), Height());
		BOOL add_border = input_type == INPUT_TEXT || input_type == INPUT_PASSWORD;
		add_border = add_border ||
			input_type == INPUT_URI || input_type == INPUT_DATE ||
			input_type == INPUT_WEEK || input_type == INPUT_TIME ||
			input_type == INPUT_EMAIL || input_type == INPUT_NUMBER || input_type == INPUT_RANGE ||
			input_type == INPUT_MONTH || input_type == INPUT_DATETIME ||
			input_type == INPUT_DATETIME_LOCAL ||
			input_type == INPUT_COLOR || input_type == INPUT_TEL || input_type == INPUT_SEARCH;

		if (add_border)
		{
			opwidget->GetInfo()->AddBorder(opwidget, &r);
		}

		PaintSpecialBorder(props, vis_dev, r, special_border_color, CSS_VALUE_solid, TRUE);

#ifdef WAND_HAS_MATCH_IN_ECOMMERCE
		if (exists_in_wand & WAND_HAS_MATCH_IN_ECOMMERCE)
		{
			PaintSpecialBorder(props, vis_dev, r, OP_RGB(255, 240, 0), CSS_VALUE_dashed, TRUE);
		}
#endif // WAND_HAS_MATCH_IN_ECOMMERCE
	}

	// If it is a defaultbutton with a CSS border, we will draw the black border (defaultlook) here.
	if (m_doc->current_default_formelement == m_helm &&
			FormManager::IsButton(input_type) &&
			 (css_border_left_width > 0 || css_border_top_width > 0 ||
			 css_border_right_width > 0 || css_border_bottom_width > 0))
	{
		OpRect r = GetBorderRect();

		PaintSpecialBorder(props, vis_dev, r, OP_RGB(0, 0, 0), CSS_VALUE_solid, FALSE);
	}
}

COLORREF FormObject::GetDefaultBackgroundColor() const
{
	if (OpWidgetInfo* widget_info = opwidget->GetInfo())
	{
		return widget_info->GetSystemColor(opwidget->IsEnabled() ? OP_SYSTEM_COLOR_BACKGROUND : OP_SYSTEM_COLOR_BACKGROUND_DISABLED);
	}

	return OP_RGB(0xff, 0xff, 0xff);
}

void FormObject::SetFocus(FOCUS_REASON reason)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
#ifdef WAND_HAS_MATCHES_WARNING
	if (WarnIfWandHasData())
	{
		return;
	}
#endif // WAND_HAS_MATCHES_WARNING
	opwidget->SetFocus(reason);
}

void FormObject::OnFocus(BOOL focus, FOCUS_REASON reason)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully or is (partially) destructed. Check construction code.

	if (focus)
	{
		if (reason==FOCUS_REASON_RELEASE)
		{
			// The only reason we should get focus because something released
			// is if it was our own widget that tried to get rid of focus.
			// In that case it's best to bubble the focus up to the document.
			GetParentInputContext()->SetFocus(reason);
		}
		else
		{
			opwidget->SetFocus(reason);
		}
	}
}


/***********************************************************************************
**
**	OnKeyboardInputGained
**
***********************************************************************************/

void FormObject::OnKeyboardInputGained(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully or is (partially) destructed. Check construction code.

	OpInputContext::OnKeyboardInputGained(new_input_context, old_input_context, reason);

//	if (reason == FOCUS_REASON_RECREATING)
//		recreating = FALSE;

		HandleFocusGained(reason);

		if (m_helm->IsWidgetWithCaret())
		{
			HTML_Document* html_doc = m_doc->GetHtmlDocument();
			if (html_doc) // Can be NULL for OBML2D documents
				html_doc->SetElementWithSelection(m_helm);
		}

		if (reason == FOCUS_REASON_KEYBOARD || reason == FOCUS_REASON_MOUSE)
		{
			// if focus is set manually, then we should ignore any autofocus flags coming up,
			m_doc->SetObeyNextAutofocus(FALSE);
		}
}

void FormObject::HandleFocusGained(FOCUS_REASON reason)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	if (m_doc->current_focused_formobject != this)
	{
		opwidget->SetHasFocusRect(m_vis_dev->GetDrawFocusRect());
		m_doc->current_focused_formobject = this;
		m_doc->current_focused_formobject->UpdatePosition();
		UpdateDefButton();

		if (!m_locked)
		{
			HTML_Document* html_doc = m_doc->GetHtmlDocument();
			if (html_doc)
			{
				html_doc->SetFocusedElement(m_helm);
			}
		}

		if (reason == FOCUS_REASON_MOUSE || reason == FOCUS_REASON_KEYBOARD || reason == FOCUS_REASON_ACTION || reason == FOCUS_REASON_SIMULATED || reason == FOCUS_REASON_RESTORE)
		{
			OP_STATUS status = m_doc->HandleEvent(ONFOCUS, NULL, m_helm, SHIFTKEY_NONE);

			if (OpStatus::IsMemoryError(status))
				m_doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		}
	}
}

/***********************************************************************************
**
**	OnKeyboardInputLost
**
***********************************************************************************/

void FormObject::OnKeyboardInputLost(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully or is (partially) destructed. Check construction code.

	OpInputContext::OnKeyboardInputLost(new_input_context, old_input_context, reason);

//	if (reason == FOCUS_REASON_RECREATING)
//		recreating = TRUE;

	if (!(this == new_input_context || this->IsParentInputContextOf(new_input_context)))
	{
		HandleFocusLost(reason);

		if (m_helm->IsWidgetWithCaret())
		{
			HTML_Document* html_doc = m_doc->GetHtmlDocument();
			if (html_doc) // Can be NULL for OBML2D documents
				html_doc->SetElementWithSelection(NULL);
		}
	}
}

void FormObject::HandleFocusLost(FOCUS_REASON reason)
{
	if (m_doc->current_focused_formobject == this)
	{
		m_doc->current_focused_formobject->UpdatePosition();
		m_doc->current_focused_formobject = NULL;

		if (!m_locked)
		{
			HTML_Document* html_doc = m_doc->GetHtmlDocument();

			if (html_doc)
			{
				html_doc->SetFocusedElement(NULL, FALSE);
			}
		}
		if (reason == FOCUS_REASON_MOUSE || reason == FOCUS_REASON_KEYBOARD || reason == FOCUS_REASON_ACTION || reason == FOCUS_REASON_SIMULATED || reason == FOCUS_REASON_RESTORE)
		{
			OP_STATUS status = m_doc->HandleEvent(ONBLUR, NULL, m_helm, SHIFTKEY_NONE);

			if (OpStatus::IsMemoryError(status))
				m_doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		}
	}

	if (m_validation_error)
	{
		m_validation_error->Close();
		OP_ASSERT(!m_validation_error); // Should have been cleared by the |Close()|
	}
}

void FormObject::SelectText()
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	InputType type = GetInputType();
	if (IsEditFieldType(type))
		((OpEdit*)opwidget)->SelectText();
	else if (type == INPUT_TEXTAREA)
		((OpMultilineEdit*)opwidget)->SelectText();
	else if (type == INPUT_NUMBER)
		((OpNumberEdit*)opwidget)->SelectText();
	else if (type == INPUT_DATE)
	{
		// XXX Select date?
	}
	else if (type == INPUT_DATETIME || type == INPUT_DATETIME_LOCAL)
	{
		// XXX Select date time?
	}
	else if (type == INPUT_TIME)
	{
		// XXX Select time?
	}
	else if (type == INPUT_WEEK)
	{
		// XXX Select date?
	}
	else if (type == INPUT_MONTH)
	{
		// XXX Select date?
	}
}


/* virtual */
void FormObject::GetSelection(INT32& start_ofs, INT32& stop_ofs, SELECTION_DIRECTION& direction)
{
	OP_ASSERT(opwidget);
	opwidget->GetSelection(start_ofs, stop_ofs, direction);
}

/* virtual */
void FormObject::SetSelection(INT32 start_ofs, INT32 stop_ofs, SELECTION_DIRECTION direction)
{
	OP_ASSERT(opwidget);
	opwidget->SetSelection(start_ofs, stop_ofs, direction);
	opwidget->ScrollIfNeeded();
}

void FormObject::SelectSearchHit(INT32 start_ofs, INT32 stop_ofs, BOOL is_active_hit)
{
	OP_ASSERT(opwidget);
	opwidget->SelectSearchHit(start_ofs, stop_ofs, is_active_hit);
	opwidget->ScrollIfNeeded();
}

BOOL FormObject::IsSearchHitSelected()
{
	OP_ASSERT(opwidget);
	return opwidget->IsSearchHitSelected();
}

BOOL FormObject::IsActiveSearchHitSelected()
{
	OP_ASSERT(opwidget);
	return opwidget->IsActiveSearchHitSelected();
}

void FormObject::ClearSelection()
{
	opwidget->SelectNothing();
}

/* virtual */
INT32 FormObject::GetCaretOffset()
{
    return opwidget->GetCaretOffset();
}

/* virtual */
void FormObject::SetCaretOffset(INT32 caret_ofs)
{
    opwidget->SetCaretOffset(caret_ofs);
}

// Searches for the first SUBMIT button in the form
HTML_Element* FindDefButtonInternal(FramesDocument* frames_doc, HTML_Element* helm, HTML_Element* form_elm)
{
	OP_ASSERT(frames_doc);
	OP_ASSERT(helm);

	if (!form_elm)
	{
		return NULL;
	}

	// Use FormIterator?
	while (helm)
	{
		InputType inp_type = helm->GetInputType();
		BOOL is_submit_button = inp_type == INPUT_SUBMIT || inp_type == INPUT_IMAGE;
		if (is_submit_button)
		{
			if (FormManager::BelongsToForm(frames_doc, form_elm, helm))
			{
				break;
			}
		}

		helm = helm->NextActual();
	}

	return helm;
}

HTML_Element* FormObject::GetDefaultElement()
{
	// Find form
	HTML_Element* form_elm = FormManager::FindFormElm(m_doc, m_helm);
	HTML_Element* search_under = form_elm;
	if (!search_under)
	{
		search_under = m_doc->GetLogicalDocument()->GetRoot();
	}

	// If it's a free input field form_elm will be NULL so we
	// accept any free submit button.
	return FindDefButtonInternal(m_doc, search_under, form_elm);
}

void FormObject::UpdateDefButton()
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.

	HTML_Element* def_elm = NULL;

// On mac we effectively have no default button so we shouldn't try to change to another default button if we didn't have one in the first place
#ifndef _MACINTOSH_
	if (FormManager::IsButton(GetInputType()))
		def_elm = m_helm;
	else
		def_elm = GetDefaultElement();
#endif

	if (def_elm == m_doc->current_default_formelement)
		return;

	FormObject::ResetDefaultButton(m_doc);

	// Set new defaultbutton, if found.
	FormObject* fo = def_elm ? def_elm->GetFormObject() : NULL;
	if(fo)
	{
		OP_ASSERT(fo->opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
		((OpButton*)fo->opwidget)->SetDefaultLook(TRUE);
		fo->opwidget->Invalidate(fo->GetBorderRect(), FALSE);
		m_doc->current_default_formelement = def_elm;
		HTML_Element* old_def = m_doc->current_default_formelement;
		m_doc->current_default_formelement = def_elm;
		if (old_def)
		{
			m_doc->GetLogicalDocument()->GetLayoutWorkplace()->ApplyPropertyChanges(old_def, CSS_PSEUDO_CLASS_GROUP_FORM, TRUE);
		}
		if (def_elm)
		{
			m_doc->GetLogicalDocument()->GetLayoutWorkplace()->ApplyPropertyChanges(def_elm, CSS_PSEUDO_CLASS_GROUP_FORM, TRUE);
		}
	}
}

// Compatibility function
void FormObject::ResetDefaultButton()
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	ResetDefaultButton(m_doc);
}

/* static */
void FormObject::ResetDefaultButton(FramesDocument* doc)
{
	HTML_Element* current_default_form_elm = doc->current_default_formelement;
	if (current_default_form_elm)
	{
		FormObject* current_default_form_obj = current_default_form_elm->GetFormObject();
		if (current_default_form_obj)
		{
			// Only buttons can be the default elements
			OpButton* button = static_cast<OpButton*>(current_default_form_obj->opwidget);
			button->SetDefaultLook(FALSE);
			button->Invalidate(doc->current_default_formelement->GetFormObject()->GetBorderRect(), FALSE);
			doc->current_default_formelement = NULL;
			doc->GetLogicalDocument()->GetLayoutWorkplace()->ApplyPropertyChanges(current_default_form_elm, CSS_PSEUDO_CLASS_GROUP_FORM, TRUE);
		}
	}
}

void FormObject::UpdatePosition()
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	OpRect rect = opwidget->GetRect();

	rect.width = Width();
	rect.height = Height();

	opwidget->SetRect(rect);

	if (m_validation_error)
	{
		m_validation_error->HandleNewWidgetPosition();
	}
}

void FormObject::SetSize(int new_width, int new_height)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.

	m_width = new_width;
	m_height = new_height;

	OpRect rect = opwidget->GetRect();

	rect.width = Width();
	rect.height = Height();

	opwidget->SetRect(rect, FALSE);
}

void FormObject::GetPreferredSize(int &preferred_width, int &preferred_height, int html_cols, int html_rows, int html_size, const HTMLayoutProperties& props)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.

	opwidget->BeginChangeProperties();

	if (!g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::EnableStylingOnForms, m_doc->GetHostName()))
	{
		opwidget->SetHasCssBorder(FALSE);
		opwidget->UnsetForegroundColor();
		opwidget->UnsetBackgroundColor();
	}

	JUSTIFY justify = GetJustify(props, this);
	opwidget->SetPadding(props.padding_left, props.padding_top, props.padding_right, props.padding_bottom);

	BOOL needs_css_borders = HasSpecifiedBorders(props, m_helm);
	opwidget->SetHasCssBorder(needs_css_borders);

	int fontnr = m_vis_dev->GetCurrentFontNumber();
	opwidget->SetFontInfo(styleManager->GetFontInfo(fontnr),
							m_vis_dev->GetFontSize(),
							m_vis_dev->IsTxtItalic(),
							m_vis_dev->GetFontWeight(),
							justify,
							m_vis_dev->GetCharSpacingExtra());

	int suggested_width = 0;
	int suggested_height = 0;

	// Make a size suggestion. It should basically not be used since OpWidget::GetPreferedSize should do the job.

	InputType input_type = GetInputType();

	if (FormManager::IsButton(input_type))
	{
		// Get width from the OpWidgetString to get it using fontswitching
		OpButton* button = (OpButton*) opwidget;
		suggested_width = button->string.GetWidth();
		suggested_height = m_vis_dev->GetFontHeight();
	}
	else
		switch (input_type)
		{
		case INPUT_TEXT:
		case INPUT_PASSWORD:
		case INPUT_URI:
		case INPUT_EMAIL:
		case INPUT_DATE: // XXX Decide size some better way
		case INPUT_DATETIME: // XXX Decide size some better way
		case INPUT_DATETIME_LOCAL: // XXX Decide size some better way
		case INPUT_TIME: // XXX Decide size some better way
		case INPUT_NUMBER: // XXX Decide size some better way
		case INPUT_WEEK: // XXX Decide size some better way
		case INPUT_MONTH: // XXX Decide size some better way
		case INPUT_COLOR: // XXX Decide size some better way
		case INPUT_TEL: // XXX Decide size some better way
		case INPUT_SEARCH: // XXX Decide size some better way
				if (html_size)
				{
					int max_char_w = m_vis_dev->GetFontStringWidth(UNI_L("W"), 1);

					if (html_size > 2)
						suggested_width = 2 * max_char_w + (html_size - 2) * m_vis_dev->GetFontAveCharWidth();
					else
						suggested_width = html_size * max_char_w;
				}
				else
					suggested_width = 100; // Does this ever happen?

			suggested_height = m_vis_dev->GetFontHeight();
			break;

		case INPUT_FILE:
			if (html_size == 0)
				html_size = 20; // Default size of 20 seems to be mozilla and IE's choice.

			suggested_width = html_size * m_vis_dev->GetFontAveCharWidth() + 6;
			suggested_height = m_vis_dev->GetFontHeight();

			// Add space for the button. We don't include it in the given size attribute.
			suggested_width += ((OpFileChooserEdit*)opwidget)->GetButtonWidth();

			break;
		case INPUT_RANGE:
			// Nothing?

			break;

		default:
			switch (m_helm->Type())
			{
			case HE_SELECT:
				{
					int nr_visible_items = html_size;

					if (nr_visible_items < 1)
						nr_visible_items = 1;

					suggested_width = 25;
					suggested_height = nr_visible_items * m_vis_dev->GetFontHeight();

					SelectionObject* this_sel_obj = static_cast<SelectionObject*>(this);
					this_sel_obj->RecalculateWidestItem();

					break;
				}

			case HE_TEXTAREA:
				{
					OpMultilineEdit* multiedit = (OpMultilineEdit*) opwidget;
					multiedit->SetLineHeight(props.GetCalculatedLineHeight(m_vis_dev));
					break;
				}
			}

			break;
		}

	opwidget->EndChangeProperties();

 	if (HasSpecifiedBorders(props, m_helm))
		; // Borders will be added in content.cpp
	else
	{
		suggested_width += 4;
		suggested_height += 4;
	}

	suggested_width += opwidget->GetPaddingLeft() + opwidget->GetPaddingRight();
	suggested_height += opwidget->GetPaddingTop() + opwidget->GetPaddingBottom();

	// See if there is a preferred size

	int widget_preferred_width = 0;
	int widget_preferred_height = 0;

	OP_ASSERT(m_helm->GetNsType() == NS_HTML);
	int cols = m_helm->Type() == HE_TEXTAREA ? html_cols : html_size;
	int rows = m_helm->Type() == HE_SELECT ? html_size : html_rows;
	opwidget->GetPreferedSize(&widget_preferred_width, &widget_preferred_height, cols, rows);

	if (suggested_width < 0) // Detected overflow
		suggested_width = INT_MAX;

	if (!widget_preferred_width)
		preferred_width = suggested_width;
	else
		preferred_width = widget_preferred_width;

	if (!widget_preferred_height)
		preferred_height = suggested_height;
	else
		preferred_height = widget_preferred_height;
}

void FormObject::SetEnabled(BOOL enabled, BOOL regain_focus)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	opwidget->SetEnabled(enabled);
	if (regain_focus)
	{
		OP_ASSERT(enabled);
		SetFocus(FOCUS_REASON_OTHER);
	}
	opwidget->Invalidate(opwidget->GetBounds());
	// Affect the read-only/read-write pseudo class
	m_doc->GetLogicalDocument()->GetLayoutWorkplace()->ApplyPropertyChanges(m_helm, CSS_PSEUDO_CLASS_GROUP_FORM, TRUE);
}

#ifdef INTERNAL_SPELLCHECK_SUPPORT
int FormObject::CreateSpellSessionId(OpPoint *point /* = NULL */)
{
	OP_ASSERT(m_helm->GetNsType() == NS_HTML);

	if (m_helm->Type() == HE_TEXTAREA && !IsDisabled())
	{
		OpMultilineEdit* multiedit = static_cast<OpMultilineEdit*>(opwidget);
		return multiedit->CreateSpellSessionId(point);
	}

	if (m_helm->Type() == HE_INPUT &&
		IsEditFieldType(m_helm->GetInputType()) && !IsDisabled())
	{
		OpEdit* edit = static_cast<OpEdit*>(opwidget);
		return edit->CreateSpellSessionId(point);
	}

	return 0;
}
#endif // INTERNAL_SPELLCHECK_SUPPORT

BOOL FormObject::IsUserEditableText()
{
	OP_ASSERT(m_helm->GetNsType() == NS_HTML);

	BOOL has_editable_text = FALSE;
	if (m_helm->Type() == HE_TEXTAREA)
	{
		has_editable_text = TRUE;
	}
	else if (m_helm->Type() == HE_INPUT)
	{
		InputType type = m_helm->GetInputType();
		has_editable_text = IsEditFieldType(type) || type == INPUT_NUMBER;
	}

	if (has_editable_text && !IsDisabled())
	{
		return TRUE;
	}

	return FALSE;
}

void FormObject::SetReadOnly(BOOL readonly)
{
	// ReadOnly handling moved to ConfigureWidgetFromHTML for InputObject (the rest to come)
	switch (m_helm->Type())
	{
	case HE_INPUT:
#ifdef _FILE_UPLOAD_SUPPORT_
		if (GetInputType() == INPUT_FILE)
		{
			((OpFileChooserEdit*)opwidget)->SetReadOnly(readonly);
		}
#endif // _FILE_UPLOAD_SUPPORT_
		return;

	case HE_TEXTAREA:
		((OpMultilineEdit *) opwidget)->SetReadOnly(readonly);
	}
}

void FormObject::SetMinMax(double *new_min, double *new_max)
{
	if (GetInputType() == INPUT_RANGE)
	{
		OpSlider* slider = static_cast<OpSlider*>(opwidget);
		if (new_min)
			slider->SetMin(*new_min);
		if (new_max)
			slider->SetMax(*new_max);
	}
}

void FormObject::SetMultiple(BOOL multiple)
{
	if (GetInputType() == INPUT_FILE)
	{
		OpFileChooserEdit *file = static_cast<OpFileChooserEdit *>(opwidget);
		file->SetMaxNumberOfFiles(multiple ? FileUploadObject::MaxNumberOfUploadFiles : 1);
	}
}

void FormObject::SetIndeterminate(BOOL indeterminate)
{
	if (GetInputType() == INPUT_CHECKBOX)
	{
		OpCheckBox* checkbox = static_cast<OpCheckBox*>(opwidget);
		checkbox->SetIndeterminate(indeterminate);
	}
}

void FormObject::InitialiseWidget(const HTMLayoutProperties& props)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	// Set alignment and font
	JUSTIFY justify = GetJustify(props, this);

	int fontnr = m_vis_dev->GetCurrentFontNumber();
	opwidget->SetFontInfo(styleManager->GetFontInfo(fontnr),
							m_vis_dev->GetFontSize(),
							m_vis_dev->IsTxtItalic(),
							m_vis_dev->GetFontWeight(),
							justify,
							m_vis_dev->GetCharSpacingExtra());

	// Set other stuff
	opwidget->SetFormObject(this);
	opwidget->SetListener(m_listener);
	BOOL needs_css_borders = HasSpecifiedBorders(props, m_helm);
	opwidget->SetHasCssBorder(needs_css_borders);
	opwidget->SetHasCssBackground(HasSpecifiedBackgroundImage(m_doc, props, m_helm));
#ifdef SUPPORT_TEXT_DIRECTION
	opwidget->SetRTL(props.direction == CSS_VALUE_rtl);
#endif

	if (props.bg_color != USE_DEFAULT_COLOR)
		opwidget->SetBackgroundColor(HTM_Lex::GetColValByIndex(props.bg_color));

	opwidget->SetProps(props);

	opwidget->SetVisualDevice(m_vis_dev);

#ifdef WAND_SUPPORT
	// Is this dangerous when we are restoring the form (layout/paint) ???
	// Do somewhere else?
	int exists_in_wand = g_wand_manager->HasMatch(m_doc, GetHTML_Element());
	GetHTML_Element()->GetFormValue()->SetExistsInWand(exists_in_wand);

	if (exists_in_wand)
	{
		m_doc->SetHasWandMatches(TRUE);
	}
#endif // WAND_SUPPORT

	if (m_helm->Type() == HE_SELECT)
	{
		// Selectitems have cached widths/heights in their items. They
		// will need this to recalculate them if the font has changed.
		SelectionObject* select = static_cast<SelectionObject*>(this);
		select->RecalculateWidestItem();
	}

	if (!m_autofocus_attribute_processed
		&& opwidget->IsEnabled()
		&& m_doc->GetObeyNextAutofocus()
		&& GetHTML_Element()->HasAttr(ATTR_AUTOFOCUS)
		&& g_pcdoc->GetIntegerPref(PrefsCollectionDoc::AllowAutofocusFormElement, m_doc->GetHostName()))
	{
		m_doc->SetElementToAutofocus(GetHTML_Element());
	}
	m_autofocus_attribute_processed = TRUE;

#ifdef WIDGETS_IME_SUPPORT
	UpdateIMStyle();
#endif // WIDGETS_IME_SUPPORT
}

#ifdef WIDGETS_IME_SUPPORT
void FormObject::UpdateIMStyle()
{
	// Set the IME style from the ISTYLE attribute or the input-format css property
	InputType type = GetInputType();

	BOOL is_edit_field_type = IsEditFieldType(type);

	if (is_edit_field_type || type == INPUT_TEXTAREA)
	{
		const uni_char* im_style = GetHTML_Element()->GetStringAttr(ATTR_INPUTMODE);
		if (!im_style)
		{
			im_style = GetHTML_Element()->GetAttrValue(UNI_L("ISTYLE"));
		}

		if (!im_style)
		{
			im_style = GetHTML_Element()->GetSpecialStringAttr(FORMS_ATTR_CSS_INPUT_FORMAT, SpecialNs::NS_FORMS);
		}

#ifdef _WML_SUPPORT_
		if (!im_style && m_doc->GetDocManager()->WMLGetContext())
		{
			im_style = GetHTML_Element()->GetStringAttr(WA_FORMAT, NS_IDX_WML);
		}
#endif //_WML_SUPPORT_

		if (is_edit_field_type)
		{
			static_cast<OpEdit*>(opwidget)->SetIMStyle(im_style);
		}
		else
		{
			static_cast<OpMultilineEdit*>(opwidget)->SetIMStyle(im_style);
		}
	}
}

void FormObject::HandleFocusEventFinished()
{
	opwidget->IMEHandleFocusEvent(TRUE);
}
#endif // WIDGETS_IME_SUPPORT

/* virtual */
OP_STATUS FormObject::ConfigureWidgetFromHTML(BOOL set_start_value)
{
	BOOL widget_enabled = !m_helm->IsDisabled(m_doc);
#ifndef _FILE_UPLOAD_SUPPORT_
	if (m_helm->GetInputType() == INPUT_FILE)
	{
		widget_enabled = FALSE;
	}
#endif // !_FILE_UPLOAD_SUPPORT_
	if (opwidget->IsEnabled() != widget_enabled)
	{
		opwidget->SetEnabled(widget_enabled);
	}
	return OpStatus::OK;
}


void FormObject::CheckRadio(HTML_Element *radio_elm, OpWidget* widget)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.

#ifdef _PRINT_SUPPORT_
	DocumentManager* doc_man = m_vis_dev->GetDocumentManager();
	if (doc_man->GetPrintDoc())
	{
		// We don't uncheck anything if this is in print preview, since we shouldn't
		// touch the original document, just because we print preview it. Also, the
		// he we get here is in a different tree so we would have to get a different
		// root if it should have a chance to work.
		return;
	}
#endif
	FormValueRadioCheck* form_value = FormValueRadioCheck::GetAs(radio_elm->GetFormValue());
	form_value->SetIsInCheckedRadioGroup(TRUE);
	form_value->UncheckRestOfRadioGroup(radio_elm, GetDocument(), NULL);

	// Then this is coming from the widget and we're
	// already handling a click, and are probably inside
	// a FormValueRadioCheck::SetIsChecked
	static_cast<OpRadioButton*>(widget)->SetValue(TRUE);

	FormValueListener::HandleValueChanged(GetDocument(), radio_elm,
		FALSE,
		TRUE, /* User origin... we don't know but it's likely (or from the click() method in script) -/ */
		NULL); // Thread
}

BOOL FormObject::CheckNextRadio(BOOL forward)
{
	m_vis_dev->SetDrawFocusRect(TRUE);

	UpdatePosition();

	if (m_helm->GetInputType() == INPUT_RADIO)
	{
		HTML_Element *radio;
		const uni_char *this_name, *other_name;
		int this_formnr = m_helm->GetFormNr(m_doc);

		this_name = m_helm->GetStringAttr(ATTR_NAME);
		if (this_name && uni_strlen(this_name) == 0)
			this_name = NULL;

		radio = forward ? (HTML_Element *) m_helm->Next() : (HTML_Element *) m_helm->Prev();

		while (radio)
		{
			if (radio && radio->Type() == HE_INPUT && radio->GetFormNr(m_doc) == this_formnr && radio->GetInputType() == INPUT_RADIO)
			{
				other_name = radio->GetStringAttr(ATTR_NAME);
				if (other_name && uni_strlen(other_name) == 0)
					other_name = NULL;

				// Same name: both non-null and equal or both null
				// (unnamed radiobuttons are probably meant to be
				// grouped together.)
				if ((this_name && other_name && uni_stricmp(this_name, other_name) == 0) || (this_name == other_name))
				{
					FormObject* form_obj = radio->GetFormObject();
					if (form_obj && !form_obj->IsDisabled())
					{
						form_obj->SetFocus(FOCUS_REASON_KEYBOARD);
						FormValueRadioCheck* radiocheck_val = FormValueRadioCheck::GetAs(radio->GetFormValue());
						radiocheck_val->SaveStateBeforeOnClick(radio);
						radiocheck_val->SetIsChecked(radio, TRUE, GetDocument(), TRUE);
						GetDocument()->HandleMouseEvent(ONCLICK, NULL, radio, NULL, 0, 0, 0, 0, 0,
							MAKE_SEQUENCE_COUNT_AND_BUTTON(1, MOUSE_BUTTON_1));
 						return TRUE;
					}
				}
			}

			radio = forward ? (HTML_Element *) radio->Next() : (HTML_Element *) radio->Prev();
		}
	}
	return FALSE;
}


BOOL InputObject::ChangeTypeInplace(HTML_Element *he)
{
	/* Determine if we can transition this OpEdit-backed
	   input object into another in-place. Do it, if so. */
	if (FormObject *form_object = he->GetFormObject())
		if (FormObject::IsEditFieldType(he->GetInputType()))
		{
			ConfigureEditWidgetFromHTML(he, static_cast<OpEdit *>(form_object->GetWidget()), NULL);
			return TRUE;
		}

	return FALSE;
}

/* static */
void InputObject::ConfigureEditWidgetFromHTML(HTML_Element *he, OpEdit *edit, const uni_char *placeholder)
{
	InputType input_type = he->GetInputType();

	if (placeholder)
		edit->SetGhostText(placeholder);

	if (input_type == INPUT_PASSWORD)
		edit->SetPasswordMode(TRUE);
	else
	{
		AUTOCOMPL_TYPE type = AUTOCOMPLETION_FORM;
		edit->autocomp.SetType(type);
	}

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	if (input_type == INPUT_TEXT && !edit->SpellcheckByUser())
	{
		switch (he->SpellcheckEnabledByAttr())
		{
		case HTML_Element::SPC_DISABLE:
			edit->DisableSpellcheck(FALSE /*force*/);
			break;
		case HTML_Element::SPC_ENABLE:
			edit->EnableSpellcheck();
			break;
		case HTML_Element::SPC_DISABLE_DEFAULT:
			/* Let the widget decide. It leaves the spellchecker disabled until the user enables it */
			break;
		default:
			OP_ASSERT(!"Input spellcheck attribute default is incorrect");
			break;
		}
	}
#endif // INTERNAL_SPELLCHECK_SUPPORT

	int maxlength = he->GetMaxLength();
#ifdef FORMS_LIMIT_INPUT_SIZE
	int force_maxlen;
	if (input_type == INPUT_PASSWORD)
		force_maxlen = g_pcdoc->GetIntegerPref(PrefsCollectionDoc::MaximumBytesInputPassword);
	else
		force_maxlen = g_pcdoc->GetIntegerPref(PrefsCollectionDoc::MaximumBytesInputText);

	maxlength = MIN(maxlength, force_maxlen / sizeof(uni_char));
#endif // FORMS_LIMIT_INPUT_SIZE

	if (maxlength >= 0)
		edit->SetMaxLength(maxlength);

	edit->SetReadOnly(he->GetReadOnly());
}

// == InputObject ====================================================

InputObject::InputObject(VisualDevice* vd,
						 FramesDocument* doc,
                         InputType type,
                         HTML_Element* he,
						 BOOL read_only)
	: FormObject(vd, he, doc)
	, object_type(type)
	, m_default_text(NULL)
{
}

// Construct radio check thing
OP_STATUS InputObject::Construct(VisualDevice* vd,
								 const HTMLayoutProperties& props,
								 InputType type,
								 HTML_Element* he)
{
	OP_ASSERT(type == INPUT_CHECKBOX || type == INPUT_RADIO);
	switch(type)
	{
		case INPUT_CHECKBOX:
		{
			// Create the widget
			OpCheckBox* new_object;
			RETURN_IF_ERROR(OpCheckBox::Construct(&new_object));
			opwidget = new_object;

			m_listener = OP_NEW(WidgetListener, (vd->GetDocumentManager(), he));

			m_memsize = sizeof(OpCheckBox) + sizeof(WidgetListener);

			break;
		}
		case INPUT_RADIO:
		{
			// Create the widget
			OpRadioButton* new_object;
			RETURN_IF_ERROR(OpRadioButton::Construct(&new_object));
			opwidget = new_object;

			m_listener = OP_NEW(RadioButtonListener, (vd->GetDocumentManager(), he));

			m_memsize = sizeof(OpRadioButton) + sizeof(RadioButtonListener);
			break;
		}
	}

	if (m_memsize > 0)
		// We don't care if OOM occurred. The important part is to use the same number as in the destructor.

		REPORT_MEMMAN_INC(m_memsize);

	if (!m_listener && opwidget)
	{
		opwidget->Delete();
		opwidget = NULL;
	}

	if(!opwidget)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	// Initialize it
	InitialiseWidget(props);

	return OpStatus::OK;
}

OP_STATUS InputObject::Construct(VisualDevice* vd,
						 const HTMLayoutProperties& props,
                         InputType type,
                         int maxlength,
						 HTML_Element* he,
						 const uni_char* label)
{
	if (FormManager::IsButton(type))
	{
		type = INPUT_BUTTON; // To simplify somewhat
	}
	switch(type)
	{
		case INPUT_TEXT:
		case INPUT_PASSWORD:
		case INPUT_URI:
		case INPUT_EMAIL:
		case INPUT_TEL:
		case INPUT_SEARCH:
		{
			// Create the widget
			OpEdit* new_object;
			RETURN_IF_ERROR(OpEdit::Construct(&new_object));
			opwidget = new_object;

			m_listener = OP_NEW(WidgetListener, (vd->GetDocumentManager(), he));

			if (!m_listener)
			{
				opwidget->Delete();
				opwidget = NULL;
				break;
			}

#ifdef SKIN_SUPPORT
			if (type == INPUT_SEARCH)
			{
				opwidget->GetBorderSkin()->SetImage("Edit Search Skin", "Edit Skin");
			}
#endif

			m_memsize = sizeof(OpEdit) + sizeof(WidgetListener);

			break;
		}
		case INPUT_RANGE:
		{
			// Create the widget
			OpSlider* new_object;
			RETURN_IF_ERROR(OpSlider::Construct(&new_object));
			opwidget = new_object;

			m_listener = OP_NEW(WidgetListener, (vd->GetDocumentManager(), he));

			if (!m_listener)
			{
				opwidget->Delete();
				opwidget = NULL;
				break;
			}

			m_memsize = sizeof(OpSlider) + sizeof(WidgetListener);
		}
		break;
		case INPUT_NUMBER:
		{
			// Create the widget
			OpNumberEdit* new_object;
			RETURN_IF_ERROR(OpNumberEdit::Construct(&new_object));
			opwidget = new_object;

			m_listener = OP_NEW(WidgetListener, (vd->GetDocumentManager(), he));

			if (!m_listener)
			{
				opwidget->Delete();
				opwidget = NULL;
				break;
			}

			m_memsize = sizeof(OpNumberEdit) + sizeof(WidgetListener);
		}
		break;
		case INPUT_WEEK:
		case INPUT_MONTH:
		case INPUT_DATE:
		{
			// Create the widget
			OpCalendar* new_object;
			RETURN_IF_ERROR(OpCalendar::Construct(&new_object));
			opwidget = new_object;

			m_listener = OP_NEW(WidgetListener, (vd->GetDocumentManager(), he));

			if (!m_listener)
			{
				opwidget->Delete();
				opwidget = NULL;
				break;
			}

			if (type == INPUT_WEEK)
			{
				new_object->SetToWeekMode();
			}
			else if (type == INPUT_MONTH)
			{
				new_object->SetToMonthMode();
			}

			m_memsize = sizeof(OpCalendar) + sizeof(WidgetListener);
		}
		break;
		case INPUT_TIME:
		{
			// Create the widget
			OpTime* new_object;
			RETURN_IF_ERROR(OpTime::Construct(&new_object));
			opwidget = new_object;

			m_listener = OP_NEW(WidgetListener, (vd->GetDocumentManager(), he));

			if (!m_listener)
			{
				opwidget->Delete();
				opwidget = NULL;
				break;
			}

			m_memsize = sizeof(OpTime) + sizeof(WidgetListener);
		}
		break;
		case INPUT_DATETIME:
		case INPUT_DATETIME_LOCAL:
		{
			// Create the widget
			OpDateTime* new_object;
			RETURN_IF_ERROR(OpDateTime::Construct(&new_object));
			opwidget = new_object;

			m_listener = OP_NEW(WidgetListener, (vd->GetDocumentManager(), he));

			if (!m_listener)
			{
				opwidget->Delete();
				opwidget = NULL;
				break;
			}

			new_object->SetIncludeTimezone(type == INPUT_DATETIME);

			m_memsize = sizeof(OpDateTime) + sizeof(WidgetListener);
		}
		break;
		case INPUT_COLOR:
		{
			// Create the widget
			OpColorBox* new_object;
			RETURN_IF_ERROR(OpColorBox::Construct(&new_object));
			opwidget = new_object;

			m_listener = OP_NEW(WidgetListener, (vd->GetDocumentManager(), he));

			if (!m_listener)
			{
				opwidget->Delete();
				opwidget = NULL;
				break;
			}

			m_memsize = sizeof(OpColorBox) + sizeof(WidgetListener);
			break;
		}
		case INPUT_BUTTON: // This covers all kinds of buttons, see IsButton above
		{
			// Create the widget
			OpButton* new_object;
			RETURN_IF_ERROR(OpButton::Construct(&new_object));
			opwidget = new_object;

			m_listener = OP_NEW(WidgetListener, (vd->GetDocumentManager(), he));

			if (!m_listener)
			{
				opwidget->Delete();
				opwidget = NULL;
				break;
			}

			m_memsize = sizeof(OpButton) + sizeof(WidgetListener);
			break;
		}
		default:
			OP_ASSERT(!"If we end up here we have missed an input type which will then have no widget");
			break;
	}

	if (m_memsize > 0)
		// We don't care if OOM occurred. The important part is to use the same number as in the destructor.

		REPORT_MEMMAN_INC(m_memsize);

	if (!opwidget)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	OP_ASSERT(m_listener);

	// Initialize it
	InitialiseWidget(props);

	RETURN_IF_ERROR(ConfigureWidgetFromHTML(TRUE));

	return OpStatus::OK;
}

InputObject* InputObject::Create(VisualDevice* vd,
								 const HTMLayoutProperties& props,
								 FramesDocument* doc,
								 InputType type,
								 HTML_Element* he,
								 BOOL read_only)
{
	InputObject* io = OP_NEW(InputObject, (vd, doc, type, he, read_only));
	if (!io || OpStatus::IsError(io->Construct(vd, props, type, he)))
	{
		OP_DELETE(io);
		return NULL;
	}
	return io;
}

InputObject* InputObject::Create(VisualDevice* vd,
								 const HTMLayoutProperties& props,
								 FramesDocument* doc,
								 InputType type,
								 HTML_Element* he,
								 int maxlength,
								 BOOL read_only,
								 const uni_char* label)
{
	InputObject* io = OP_NEW(InputObject, (vd, doc, type, he, read_only /*, label */));
	if (!io || OpStatus::IsError(io->Construct(vd, props, type, maxlength, he, label)))
	{
		OP_DELETE(io);
		return NULL;
	}
	return io;
}

OP_STATUS InputObject::ConfigureWidgetFromHTML(BOOL set_start_value)
{
	HTML_Element* he = GetHTML_Element();
	const uni_char *min = he->GetStringAttr(ATTR_MIN);
	const uni_char *max = he->GetStringAttr(ATTR_MAX);
	const uni_char *step = he->GetStringAttr(ATTR_STEP);

	const uni_char *placeholder = m_helm->GetStringAttr(ATTR_PLACEHOLDER);
	OpString placeholder_tmp; // Tempbuffer that must live for the rest of this function
	if (placeholder)
	{
		// If we have placeholder text, remove newlines characters from it
		// before we set it on the widget
		if (OpStatus::IsSuccess(placeholder_tmp.Set(placeholder)))
		{
			FormValueText::FilterChars(placeholder_tmp);
			placeholder = placeholder_tmp.CStr();
		}
	}
	else
	{
		placeholder = UNI_L("");
	}

	InputType type = he->GetInputType();
	if (IsEditFieldType(type))
	{
		ConfigureEditWidgetFromHTML(he, static_cast<OpEdit *>(opwidget), placeholder);
	}
	else if (type == INPUT_CHECKBOX)
	{
		OpCheckBox* checkbox = static_cast<OpCheckBox*>(opwidget);
		checkbox->SetIndeterminate(he->GetIndeterminate());
	}
	else if (type == INPUT_RANGE)
	{
		OpSlider* slider = static_cast<OpSlider*>(opwidget);
		double min_value;
		double max_value;
		double step_base;
		double step_value;
		WebForms2Number::GetNumberRestrictions(he, min_value, max_value,
											   step_base, step_value);
		slider->SetMin(min_value);
		slider->SetMax(max_value);
		slider->SetStep(step_value);
		slider->SetStepBase(step_base);

		if (set_start_value)
		{
			double value = min_value;
			if (step_value > 0.0)
			{
				// Must have a legal value
				int whole_steps = (int)((value - step_base)/ step_value);
				value = step_base + whole_steps * step_value;
			}

			slider->SetValue(value); // Good start value if nothing else is set
		}
		slider->SetReadOnly(he->GetReadOnly());

		// A bit slow way to update the slider with the datalist values.
		// We need to invalidate the widget when something in a datalist change.
		// And ideally, we should update the slider with the values only after that happen.
		if (he->HasAttr(ATTR_LIST))
		{
			FormSuggestion form_suggestion(GetDocument(), GetHTML_Element(), FormSuggestion::AUTOMATIC);
			uni_char **items;
			int num_items = 0;
			int num_columns = 0;
			OP_STATUS ret_val = form_suggestion.GetItems(NULL, &items, &num_items, &num_columns);
			if (OpStatus::IsSuccess(ret_val))
			{
				OpSlider::TICK_VALUE *val = OP_NEWA(OpSlider::TICK_VALUE, num_items);
				if (val)
				{
					for(int i = 0; i < num_items; i++)
					{
						double v = uni_atoi(items[i * num_columns]);
						val[i].snap = FALSE;
						val[i].value = v;
					}
					slider->SetTickValues(num_items, val, 0);
					OP_DELETEA(val);
				}
				// Clean up
				for(int i = 0; i < num_items * num_columns; i++)
				{
					OP_DELETEA(items[i]);
				}
				OP_DELETEA(items);
			}
		}
	}
	else if (type == INPUT_NUMBER)
	{
		OpNumberEdit* number_edit = static_cast<OpNumberEdit*>(opwidget);
		double min_value;
		double max_value;
		double step_base;
		double step_value;
		WebForms2Number::GetNumberRestrictions(he, min_value, max_value,
											   step_base, step_value);
		number_edit->SetMaxValue(max_value);
		number_edit->SetMinValue(min_value);
		if (step_value > 0.0)
		{
			number_edit->SetStepBase(step_base);
			number_edit->SetStep(step_value);
		}
		number_edit->SetGhostText(placeholder);

		number_edit->SetReadOnly(he->GetReadOnly());
	}
	else if (type == INPUT_WEEK || type == INPUT_MONTH || type == INPUT_DATE)
	{
		OpCalendar* calendar = static_cast<OpCalendar*>(opwidget);
		double step_base = 0.0; // Default value per spec for weeks, months as well as days
		if (type == INPUT_WEEK)
		{
			if (max)
			{
				WeekSpec max_value;
				if (max_value.SetFromISO8601String(max))
				{
					calendar->SetMaxValue(max_value);
					step_base = max_value.AsDouble();
				}
			}

			if (min)
			{
				WeekSpec min_value;
				if (min_value.SetFromISO8601String(min))
				{
					calendar->SetMinValue(min_value);
					step_base = min_value.AsDouble();
				}
			}
		}
		else if (type == INPUT_MONTH)
		{
			if (max)
			{
				MonthSpec max_value;
				if (max_value.SetFromISO8601String(max))
				{
					calendar->SetMaxValue(max_value);
					step_base = max_value.AsDouble();
				}
			}

			if (min)
			{
				MonthSpec min_value;
				if (min_value.SetFromISO8601String(min))
				{
					calendar->SetMinValue(min_value);
					step_base = min_value.AsDouble();
				}
			}
		}
		else if (type == INPUT_DATE)
		{
			if (max)
			{
				DaySpec max_value;
				if (max_value.SetFromISO8601String(max))
				{
					calendar->SetMaxValue(max_value);
					step_base = max_value.AsDouble();
				}
			}

			if (min)
			{
				DaySpec min_value;
				if (min_value.SetFromISO8601String(min))
				{
					calendar->SetMinValue(min_value);
					step_base = min_value.AsDouble();
				}
			}
		}

		BOOL legal_step = FALSE;
		if (step)
		{
			// in weeks
			double step_value;
			legal_step = WebForms2Number::StringToDouble(step, step_value) &&
				step_value > 0 &&
				step_value == static_cast<int>(step_value);
			if (legal_step)
			{
				calendar->SetStep(step_base, step_value);
			}
		}
		if (!legal_step)
		{
			calendar->SetStep(0.0, 0.0); // Disables step
		}

		calendar->SetReadOnly(he->GetReadOnly());
	}
	else if (type == INPUT_DATETIME	|| type == INPUT_DATETIME_LOCAL)
	{
		OpDateTime* datetime = static_cast<OpDateTime*>(opwidget);
		double step_base = 0.0; // default per spec
		if (max)
		{
			DateTimeSpec max_value;
			if (max_value.SetFromISO8601String(max, type == INPUT_DATETIME))
			{
				datetime->SetMaxValue(max_value);
				step_base = max_value.AsDouble();
			}
		}

		if (min)
		{
			DateTimeSpec min_value;
			if (min_value.SetFromISO8601String(min, type == INPUT_DATETIME))
			{
				datetime->SetMinValue(min_value);
				step_base = min_value.AsDouble();
			}
		}

		if (step)
		{
			// in seconds
			double step_value;
			BOOL legal_step = WebForms2Number::StringToDouble(step, step_value) && step_value > 0;
			if (legal_step)
			{
				// We have step in seconds, and need both values in milliseconds
				datetime->SetStep(step_base, step_value*1000.0);
			}
		}

		datetime->SetReadOnly(he->GetReadOnly());
	}
	else if (type == INPUT_COLOR)
	{
		opwidget->SetReadOnly(he->GetReadOnly());
	}
	else if (type == INPUT_TIME)
	{
		OpTime* time_widget = static_cast<OpTime*>(opwidget);
		TimeSpec step_base;
		step_base.Clear();

		if (max)
		{
			TimeSpec max_value;
			if (max_value.SetFromISO8601String(max))
			{
				time_widget->SetMaxValue(max_value);
				step_base = max_value;
			}
		}

		if (min)
		{
			TimeSpec min_value;
			if (min_value.SetFromISO8601String(min))
			{
				time_widget->SetMinValue(min_value);
				step_base = min_value;
			}
		}

		if (step)
		{
			// in seconds
			double step_value;
			BOOL legal_step = WebForms2Number::StringToDouble(step, step_value) && step_value > 0;
			OpTime::TimeFieldPrecision precision = OpTime::TIME_PRECISION_MINUTES;
			if (legal_step)
			{
				time_widget->SetStep(step_base, step_value);
				if (step_value - static_cast<int>(step_value) > 0.001)
				{
					// we have decimals, need hundreths
					precision = OpTime::TIME_PRECISION_SUBSECOND;
				}
				else if (static_cast<long>(step_value + 0.5) % 60 != 0)
				{
					// not whole minutes, need seconds
					precision = OpTime::TIME_PRECISION_SECONDS;
				}
			}
			else
			{
				time_widget->ClearStep();
			}
			time_widget->SetTimePrecision(precision);
		}
		else
		{
			time_widget->ClearStep();
		}
		time_widget->SetReadOnly(he->GetReadOnly());
	}

	return FormObject::ConfigureWidgetFromHTML(set_start_value);
}

OP_STATUS InputObject::GetFormWidgetValue(OpString& out_value, BOOL allow_wrap /* = TRUE */)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.

	OP_ASSERT(out_value.IsEmpty());

	switch (object_type)
	{
		case INPUT_TEXT:
		case INPUT_PASSWORD:
		case INPUT_URI:
		case INPUT_EMAIL:
		case INPUT_TEL:
		case INPUT_SEARCH:
			{
				OpEdit* edit = static_cast<OpEdit*>(opwidget);
				INT32 maxlen = edit->GetTextLength();
				uni_char* buf = out_value.Reserve(maxlen+1);
				if (!buf)
				{
					return OpStatus::ERR_NO_MEMORY;
				}
				edit->GetText(buf, maxlen, 0);
				return OpStatus::OK;
			}
		case INPUT_NUMBER:
		case INPUT_RANGE:
			{
				if (object_type == INPUT_RANGE)
				{
					return static_cast<OpSlider*>(opwidget)->GetText(out_value);
				}
				else
				{
					return static_cast<OpNumberEdit*>(opwidget)->GetText(out_value);
				}
			}
		case INPUT_WEEK:
			{
				OpCalendar* date_widget = static_cast<OpCalendar*>(opwidget);
				if (date_widget->HasValue())
				{
					WeekSpec date = date_widget->GetWeekSpec();
					int max_len = date.GetISO8601StringLength() + 1; // Add room for null byte
					uni_char* buf = out_value.Reserve(max_len);
					if (!buf)
					{
						return OpStatus::ERR_NO_MEMORY;
					}
					date.ToISO8601String(buf);
				}
				return OpStatus::OK;
			}
		case INPUT_MONTH:
			{
				OpCalendar* date_widget = static_cast<OpCalendar*>(opwidget);
				if (date_widget->HasValue())
				{
					MonthSpec date = date_widget->GetMonthSpec();
					int max_len = date.GetISO8601StringLength() + 1; // Add room for null byte
					uni_char* buf = out_value.Reserve(max_len);
					if (!buf)
					{
						return OpStatus::ERR_NO_MEMORY;
					}
					date.ToISO8601String(buf);
				}
				return OpStatus::OK;
			}
		case INPUT_DATE:
			{
				OpCalendar* date_widget = static_cast<OpCalendar*>(opwidget);
				if (date_widget->HasValue())
				{
					DaySpec date = date_widget->GetDaySpec();
					int max_len = date.GetISO8601StringLength() + 1; // Add room for null byte
					uni_char* buf = out_value.Reserve(max_len);
					if (!buf)
					{
						return OpStatus::ERR_NO_MEMORY;
					}
					date.ToISO8601String(buf);
				}
				return OpStatus::OK;
			}
		case INPUT_DATETIME:
		case INPUT_DATETIME_LOCAL:
			{
				OpDateTime* date_time_widget = static_cast<OpDateTime*>(opwidget);
				DateTimeSpec date_time;
				if (date_time_widget->HasValue() &&
					date_time_widget->GetDateTime(date_time))
				{
					int max_len = date_time.GetISO8601StringLength(object_type == INPUT_DATETIME) + 1; // Add room for null byte
					uni_char* buf = out_value.Reserve(max_len);
					if (!buf)
					{
						return OpStatus::ERR_NO_MEMORY;
					}
					date_time.ToISO8601String(buf, object_type == INPUT_DATETIME);
				}
				return OpStatus::OK;
			}
		case INPUT_TIME:
			{
				OpTime* time_widget = static_cast<OpTime*>(opwidget);
				TimeSpec time;
				if (time_widget->HasValue() &&
					time_widget->GetTime(time))
				{
					int max_len = time.GetISO8601StringLength() + 1; // Add room for null byte
					uni_char* buf = out_value.Reserve(max_len);
					if (!buf)
					{
						return OpStatus::ERR_NO_MEMORY;
					}
					time.ToISO8601String(buf);
				}
				return OpStatus::OK;
			}
		case INPUT_COLOR:
			{
				OpColorBox* color_field = static_cast<OpColorBox*>(opwidget);
				return color_field->GetText(out_value);
			}

		case INPUT_CHECKBOX:
		case INPUT_RADIO:
			{
				BOOL checked;
				if(object_type == INPUT_CHECKBOX)
				{
					checked = !!static_cast<OpCheckBox*>(opwidget)->GetValue();
				}
				else
				{
					checked = !!static_cast<OpRadioButton*>(opwidget)->GetValue();
				}

				if (checked)
				{
					const uni_char *temp = GetHTML_Element()->GetValue();
					if (!temp)
					{
						temp = UNI_L("on");
					}
					// Important that the empty string is stored as "" and not NULL
					uni_char* buf = out_value.Reserve(uni_strlen(temp)+1);
					if (!buf)
					{
						return OpStatus::ERR_NO_MEMORY;
					}
					uni_strcpy(buf, temp);
					OP_ASSERT(out_value.CStr());
				}
				return OpStatus::OK;
			}

		default:
			if (FormManager::IsButton(object_type)) // INPUT_SUBMIT, INPUT_RESET, ...
			{
				return opwidget->GetText(out_value);
			}
			break; // no matching text for hidden, image, button, file, textarea, ... etc.
	}
	return OpStatus::OK;
}

/* virtual */
void InputObject::Click(ES_Thread* thread)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	if (m_helm->IsDisabled(m_doc))
	{
		return;
	}
	if (object_type == INPUT_CHECKBOX)
	{
		OpCheckBox* checkbox = (OpCheckBox*) opwidget;
		checkbox->Toggle(); // Will toggle and send ONCHANGE
	}
	else if (object_type == INPUT_RADIO)
	{
		m_listener->OnClick(opwidget); // Will call CheckRadio
	}

	m_listener->HandleGeneratedClick(opwidget, thread); // Will trigger ONCLICK
}
OP_STATUS InputObject::SetFormWidgetValue(const uni_char* txt)
{
	return SetFormWidgetValue(txt, FALSE);
}
OP_STATUS InputObject::SetFormWidgetValue(const uni_char* txt, BOOL use_undo_stack)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	UpdatePosition();
	switch (object_type)
	{
		case INPUT_TEXT:
		case INPUT_PASSWORD:
		case INPUT_URI:
		case INPUT_EMAIL:
		case INPUT_TEL:
		case INPUT_SEARCH:
		{
#ifdef WIDGETS_UNDO_REDO_SUPPORT
			if(use_undo_stack)
			{
				RETURN_IF_MEMORY_ERROR( ((OpEdit*)opwidget)->SetTextWithUndo(txt) );
			}
			else
#endif
			RETURN_IF_MEMORY_ERROR( ((OpEdit*)opwidget)->SetText(txt) );
		}
		break;

		case INPUT_RANGE:
		{
			RETURN_IF_MEMORY_ERROR( ((OpSlider*)opwidget)->SetText(txt) );
		}
		break;

		case INPUT_NUMBER:
		{
			RETURN_IF_MEMORY_ERROR( ((OpNumberEdit*)opwidget)->SetText(txt) );
		}
		break;

		case INPUT_WEEK:
		case INPUT_DATE:
		case INPUT_MONTH:
		{
			RETURN_IF_MEMORY_ERROR( ((OpCalendar*)opwidget)->SetText(txt) );
		}
		break;

		case INPUT_DATETIME:
		case INPUT_DATETIME_LOCAL:
		{
			RETURN_IF_MEMORY_ERROR( ((OpDateTime*)opwidget)->SetText(txt) );
		}
		break;

		case INPUT_TIME:
		{
			RETURN_IF_MEMORY_ERROR( ((OpTime*)opwidget)->SetText(txt) );
		}
		break;

		case INPUT_COLOR:
		{
			RETURN_IF_MEMORY_ERROR( (static_cast<OpColorBox*>(opwidget))->SetText(txt) );
		}
		break;

		case INPUT_CHECKBOX:
		{
			// Do case-insensitive compare.
			if (txt && !uni_strni_eq( txt, "FALSE", 5 ))
				((OpCheckBox*)opwidget)->SetValue(TRUE);
			else
				((OpCheckBox*)opwidget)->SetValue(FALSE);
		}
		break;

		case INPUT_RADIO:
		{
			BOOL checked = (txt && !uni_strni_eq( txt, "FALSE", 5 ));
			((OpRadioButton*)opwidget)->SetValue(checked);
		}
		break;

		default:
			if (FormManager::IsButton(object_type))
			{
				RETURN_IF_MEMORY_ERROR(opwidget->SetText(txt));
			}
			break;
	}

	opwidget->Invalidate(opwidget->GetBounds());
	return OpStatus::OK;
}

SelectionObject::SelectionObject(VisualDevice* vd,
								 FramesDocument* doc,
                                 BOOL multi,
                                 int size,
								 int min_width,
								 int max_width,
                                 HTML_Element* he)
	: FormObject(vd, he, doc)
	, m_page_requested_size(size)
	, m_multi_selection(multi)
	, m_min_width(min_width)
	, m_max_width(max_width)
	, m_last_set_need_intrinsic_width(TRUE)
	, m_last_set_need_intrinsic_height(TRUE)
	, m_form_value_rebuild_timer_active(FALSE)
{
	m_form_value_rebuild_timer.SetTimerListener(this);
}

/* static */
OP_STATUS SelectionObject::ConstructSelectionObject(VisualDevice* vd,
								 const HTMLayoutProperties& props,
								 FramesDocument* doc,
                                 BOOL multi,
                                 int size,
								 int form_width,
								 int form_height,
								 int min_width,
								 int max_width,
                                 HTML_Element* he,
                                 BOOL use_default_font,
								 FormObject*& created_selection_object)
{
	SelectionObject* obj = OP_NEW(SelectionObject, (vd, doc, multi, size, min_width, max_width, he));
	if (!obj || OpStatus::IsError(obj->ConstructInternals(props, form_width, form_height, use_default_font)))
	{
		OP_DELETE(obj);
		return OpStatus::ERR_NO_MEMORY;
	}

	created_selection_object = obj;
	return OpStatus::OK;
}

OP_STATUS SelectionObject::ConstructInternals(
								 const HTMLayoutProperties& props,
								 int form_width,
								 int form_height,
                                 BOOL use_default_font)
{
	if (IsListbox()) // Listbox
	{
		// Create the widget
		OpListBox* new_object;
		RETURN_IF_ERROR(OpListBox::Construct(&new_object, m_multi_selection));
		opwidget = new_object;
	}
	else // Dropdown
	{
		// Create the widget
		OpDropDown* new_object;
		RETURN_IF_ERROR(OpDropDown::Construct(&new_object));
		opwidget = new_object;

#ifdef _WML_SUPPORT_
		// If it is a wml document then to invoke onpick the element must be selected
		// even if the index of highlighted item hasn't changed
		if (m_vis_dev->GetDocumentManager()->WMLHasWML())
		{
			new_object->SetAlwaysInvoke(TRUE);
		}
#endif //_WML_SUPPORT_

#ifdef PLATFORM_FONTSWITCHING
		if (HLDocProfile *hldprof = m_doc->GetHLDocProfile())
			new_object->SetPreferredCodePage(hldprof->GetPreferredCodePage());
#endif // PLATFORM_FONTSWITCHING
	}

	m_listener = OP_NEW(SelectionObjectListener, (m_vis_dev->GetDocumentManager(), m_helm));

	if (!m_listener && opwidget)
	{
		opwidget->Delete();
		opwidget = NULL;
	}

	if (opwidget == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	OP_ASSERT(m_listener);

	Init(props, m_doc);
	return OpStatus::OK;
}

void SelectionObject::Init(const HTMLayoutProperties& props, FramesDocument* doc)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	// Initialize it
	InitialiseWidget(props);

#ifdef CSS_SCROLLBARS_SUPPORT
	URL sn;
	if (doc)
	{
		sn = doc->GetURL();
	}
	if (IsListbox() &&
	    g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::EnableScrollbarColors, sn))
	{
		ScrollbarColors colors;
		if (m_doc && m_doc->GetHLDocProfile())
			colors = *m_doc->GetHLDocProfile()->GetScrollbarColors();
		m_helm->GetScrollbarColor(&colors);

		((OpListBox*)opwidget)->SetScrollbarColors(colors);
	}
#endif // CSS_SCROLLBARS_SUPPORT
	// The widget can have an external window that needs to
	// move. Tell it to request such information.
	if (!IsListbox())
		static_cast<OpDropDown*>(opwidget)->SetOnMoveWanted(TRUE);
}

OP_STATUS SelectionObject::ConfigureWidgetFromHTML(BOOL set_start_value)
{
	if (IsListbox())
	{
		HTML_Element* he = GetHTML_Element();
		static_cast<OpListBox*>(opwidget)->SetMultiple(he->GetMultiple());
	}

	return FormObject::ConfigureWidgetFromHTML(set_start_value);
}


INT32 SelectionObject::EndAddElement(BOOL need_intrinsic_width, BOOL need_intrinsic_height)
{
	m_last_set_need_intrinsic_width = need_intrinsic_width;
	m_last_set_need_intrinsic_height = need_intrinsic_height;
	return EndAddElement();
}

INT32 SelectionObject::EndAddElement()
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.

	// Change the width to match the widest item
	INT32 preferred_w = m_width;
	INT32 preferred_h = m_height;
	INT32 cols =  m_helm->GetCols();
	INT32 rows = m_helm->GetSize();
	cols = MAX(1, cols);
	rows = MAX(1, rows);
	opwidget->GetPreferedSize(&preferred_w, &preferred_h, cols, rows);

	if (m_last_set_need_intrinsic_width)
	{
		int new_width = preferred_w;
		if (m_min_width != 0 && new_width < m_min_width)
			new_width = m_min_width;
		if (m_max_width >= 0 && new_width > m_max_width)
			new_width = m_max_width;
		m_width = new_width;
	}
	if (m_last_set_need_intrinsic_height)
	{
		m_height = preferred_h;
	}

	opwidget->SetSize(m_width, m_height);

	return preferred_w;
}

void SelectionObject::ChangeSizeIfNeeded()
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	INT32 old_width = m_width, old_height = m_height;
	RecalculateWidestItem();
	EndAddElement(/*props->content_width == CONTENT_WIDTH_AUTO*/ TRUE, // FIX: need properties
				  /*props->content_height == CONTENT_HEIGHT_AUTO*/ TRUE);

	if (old_width != m_width || old_height != m_height)
		m_helm->MarkDirty(m_doc);
}

void SelectionObject::RecalculateWidestItem()
{
	if (IsListbox())
	{
		static_cast<OpListBox*>(opwidget)->RecalculateWidestItem();
	}
	else
	{
		static_cast<OpDropDown*>(opwidget)->RecalculateWidestItem();
	}
}

void SelectionObject::GetRelevantOptionRange(const OpRect& area, UINT32& start, UINT32& stop)
{
	// Start is first option visible, stop is first after last visible, so that 0,0 means no elements
	if (IsListbox())
	{
		static_cast<OpListBox*>(opwidget)->GetRelevantOptionRange(area, start, stop);
	}
	else
	{
		static_cast<OpDropDown*>(opwidget)->GetRelevantOptionRange(area, start, stop);
	}
}

OP_STATUS SelectionObject::AddElement(const uni_char* intext, BOOL selected, BOOL disabled, int idx)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	if (!intext)
		return OpStatus::ERR;

	if (IsListbox()) //Listbox
	{
		OpListBox* listbox = (OpListBox*) opwidget;
		int i = (idx == -1 ? listbox->CountItems() : idx);
		RETURN_IF_ERROR(listbox->AddItem(intext, idx));
		listbox->SelectItem(i, selected);
		listbox->EnableItem(i, !disabled);
	}
	else			//Dropdown
	{
		OpDropDown* dropdown = (OpDropDown*) opwidget;
		int i = (idx == -1 ? dropdown->CountItems() : idx);
		RETURN_IF_ERROR(dropdown->AddItem(intext, idx));
		dropdown->SelectItem(i, selected);
		dropdown->EnableItem(i, !disabled);
	}

	if (m_displayed)
	{
		UpdatePosition();
		opwidget->Invalidate(opwidget->GetBounds());
	}

	return OpStatus::OK;
}

OP_STATUS SelectionObject::ChangeElement(const uni_char* txt, BOOL selected, BOOL disabled, int idx)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.

	if (IsListbox())
	{
		OpListBox* listbox = (OpListBox*) opwidget;
		INT32 num_items = listbox->CountItems();
		if (num_items == 0 || idx > num_items - 1)
			return AddElement(txt, selected, disabled, idx);

		RETURN_IF_ERROR(listbox->ChangeItem(txt, idx));
		if (selected)
			listbox->SelectItem(idx, TRUE);
		listbox->EnableItem(idx, !disabled);
	}
	else
	{
		OpDropDown* dropdown = (OpDropDown*) opwidget;
		INT32 num_items = dropdown->CountItems();
		if (num_items == 0 || idx > num_items - 1)
			return AddElement(txt, selected, disabled, idx);

		RETURN_IF_ERROR(dropdown->ChangeItem(txt, idx));
		if (selected)
			dropdown->SelectItem(idx, TRUE);
		dropdown->EnableItem(idx, !disabled);
	}
	return OpStatus::OK;
}

void SelectionObject::ApplyProps(int idx, LayoutProperties* layout_props)
{
	OP_ASSERT(layout_props);
	const HTMLayoutProperties* props = layout_props->GetProps();
	OP_ASSERT(props);

	// If this triggers, ApplyProps is called when it shouldn't be (eg before element is created)
	OP_ASSERT(idx < (IsListbox() ? static_cast<OpListBox*>(opwidget)->CountItems() : static_cast<OpDropDown*>(opwidget)->CountItems()));
	// FIXME: Don't use the ItemHandler directly
	ItemHandler& ih = IsListbox() ? static_cast<OpListBox*>(opwidget)->ih : static_cast<OpDropDown*>(opwidget)->ih;

	if (opwidget->GetAllowStyling())
	{
		UINT32 fg_color = props->font_color;
		UINT32 bg_color = props->bg_color;

		// HACK: fix inheritance here by bubbling upwards until
		// something other than USE_DEFAULT_COLOR/transparent is found
		LayoutProperties* tmp = layout_props;
		while ((bg_color == USE_DEFAULT_COLOR || tmp->GetProps()->GetBgIsTransparent()))
		{
			tmp = tmp->Pred();
			if (!tmp)
				break;
			bg_color = tmp->GetProps()->bg_color;
			// don't stroll to far into the woods my friend, or you'll get lost
			if (tmp->html_element->Type() == HE_SELECT)
				break;
		}

		ih.SetFgColor(idx, fg_color);
		ih.SetBgColor(idx, bg_color);
	}

	ih.SetScript(idx, props->current_script);
}

BOOL SelectionObject::IsDisabled(int idx)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	if (idx >= GetElementCount())
		return FALSE;
	if (IsListbox()) //Listbox
		return !((OpListBox*) opwidget)->IsEnabled(idx);

	//Dropdown
	return !((OpDropDown*) opwidget)->IsEnabled(idx);
}


BOOL SelectionObject::IsSelected(int idx)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	if (idx >= GetElementCount())
		return FALSE;
	if (IsListbox()) //Listbox
		return ((OpListBox*) opwidget)->IsSelected(idx);

	//Dropdown
	return ((OpDropDown*) opwidget)->IsSelected(idx);
}

void SelectionObject::RemoveElement(int idx)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	if (idx >= GetElementCount())
		return;
	if (IsListbox()) //Listbox
		((OpListBox*) opwidget)->RemoveItem(idx);
	else			//Dropdown
		((OpDropDown*) opwidget)->RemoveItem(idx);

	if (m_displayed)
	{
		UpdatePosition();
		opwidget->Invalidate(opwidget->GetBounds());
	}
}

void SelectionObject::RemoveAll()
{
	if (GetElementCount() == 0)
		return;
	if (IsListbox()) //Listbox
		static_cast<OpListBox*>(opwidget)->RemoveAll();
	else			//Dropdown
		static_cast<OpDropDown*>(opwidget)->RemoveAll();

	if (m_displayed)
	{
		UpdatePosition();
		opwidget->Invalidate(opwidget->GetBounds());
	}
}

OP_STATUS SelectionObject::BeginGroup(HTML_Element* elm, const HTMLayoutProperties* props /* = NULL */)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	const uni_char* label = elm->GetStringAttr(ATTR_LABEL);

#ifdef _WML_SUPPORT_
	// In WML 1 the attribute "title" was used. In html and in WML 2 it is "label"
	if (!label && m_doc->GetDocManager() && m_doc->GetDocManager()->WMLHasWML())
		label = elm->GetElementTitle();
#endif //_WML_SUPPORT_

	if (label == NULL)
		label = UNI_L("");
	OP_STATUS status;
	if (IsListbox()) //Listbox
		status = ((OpListBox*) opwidget)->BeginGroup(label);
	else			//Dropdown
		status = ((OpDropDown*) opwidget)->BeginGroup(label);

	if (OpStatus::IsSuccess(status))
	{
		UINT32 fg_color;
		UINT32 bg_color;
		if (props)
		{
			fg_color = props->font_color;
			bg_color = props->bg_color;
		}
		else
		{
			// FIXME: This is not the correct way to get the colors, or the correct place to set them.
			//        GetCssColor/GetCssBackgroundColor are incorrect/deprecated and will eventually be removed.
			fg_color = elm->GetCssColorFromStyleAttr(m_doc);
			bg_color = elm->GetCssBackgroundColorFromStyleAttr(m_doc);
		}
		ItemHandler* ih;
		if (IsListbox())
			ih = &((OpListBox*) opwidget)->ih;
		else
			ih = &((OpDropDown*) opwidget)->ih;

		int index = ih->CountItemsAndGroups() - 1;
		if (opwidget->GetAllowStyling())
		{
			ih->GetItemAtIndex(index)->SetBgColor(bg_color);
			ih->GetItemAtIndex(index)->SetFgColor(fg_color);
		}
	}

	return status;
}

void SelectionObject::EndGroup(HTML_Element* elm)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	if (IsListbox()) //Listbox
		((OpListBox*) opwidget)->EndGroup();
	else			//Dropdown
		((OpDropDown*) opwidget)->EndGroup();
}

void SelectionObject::RemoveGroup(int idx)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	if (IsListbox()) //Listbox
		((OpListBox*) opwidget)->RemoveGroup(idx);
	else			//Dropdown
		((OpDropDown*) opwidget)->RemoveGroup(idx);
}

void SelectionObject::RemoveAllGroups()
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	if (IsListbox()) //Listbox
		((OpListBox*) opwidget)->RemoveAllGroups();
	else			//Dropdown
		((OpDropDown*) opwidget)->RemoveAllGroups();
}


OP_STATUS SelectionObject::GetFormWidgetValue(OpString& out_value, BOOL allow_wrap /* = TRUE */)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	if (IsListbox())
	{
		INT32 elm_count = GetElementCount();
		unsigned len = elm_count*5 + 1; // This breaks for > 110000 options

		uni_char* buf = out_value.Reserve(len+1); // room for trailing NULL
		if (!buf)
		{
			return OpStatus::ERR_NO_MEMORY;
		}

		uni_char *tmp = buf;

		for (int i=0; i<elm_count; i++)
		{
			BOOL selected = static_cast<OpListBox*>(opwidget)->IsSelected(i);
			if (selected)
			{
				uni_snprintf(tmp, 10, UNI_L("%d"), i);
				tmp += uni_strlen(tmp);
				*tmp++ = ',';
			}
		}
		if (tmp != buf)
		{
			tmp--; // trim trailing comma
		}
		*tmp = '\0';
		return OpStatus::OK;
	}
	else
	{
		int item = static_cast<OpDropDown*>(opwidget)->GetSelectedItem();
		uni_char* buf = out_value.Reserve(10+1); // room for trailing NULL
		if (!buf)
		{
			return OpStatus::ERR_NO_MEMORY;
		}

		uni_snprintf(buf, 10, UNI_L("%d"), (unsigned int) item);
		return OpStatus::OK;
	}
}

void SelectionObject::SetElement(int i, BOOL selected)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	if (IsListbox()) //Listbox
		((OpListBox*) opwidget)->SelectItem(i, selected);
	else			//Dropdown
		((OpDropDown*) opwidget)->SelectItem(i, selected);

	if (m_displayed)
	{
		UpdatePosition();
		opwidget->Invalidate(opwidget->GetBounds());
	}
}

int SelectionObject::GetElementCount()
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	if (IsListbox()) //Listbox
		return ((OpListBox*) opwidget)->CountItems();
	else			//Dropdown
		return ((OpDropDown*) opwidget)->CountItems();
}

unsigned int SelectionObject::GetFullElementCount()
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	if (IsListbox()) //Listbox
		return ((OpListBox*) opwidget)->CountItemsIncludingGroups();
	else			//Dropdown
		return ((OpDropDown*) opwidget)->CountItemsIncludingGroups();
}

BOOL SelectionObject::ElementAtRealIndexIsOptGroupStop(unsigned int idx)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	if (IsListbox()) //Listbox
		return ((OpListBox*) opwidget)->IsElementAtRealIndexGroupStop(idx);
	else			//Dropdown
		return ((OpDropDown*) opwidget)->IsElementAtRealIndexGroupStop(idx);
}

BOOL SelectionObject::ElementAtRealIndexIsOptGroupStart(unsigned int idx)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	if (IsListbox()) //Listbox
		return ((OpListBox*) opwidget)->IsElementAtRealIndexGroupStart(idx);
	else			//Dropdown
		return ((OpDropDown*) opwidget)->IsElementAtRealIndexGroupStart(idx);
}


void SelectionObject::ScrollIfNeeded()
{
	if (IsListbox())
	{
		opwidget->ScrollIfNeeded();
	}
}

int SelectionObject::GetSelectedIndex()
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	if (IsListbox())
	{
		OpListBox *listbox = (OpListBox *) opwidget;

		for (int i = 0, l = listbox->CountItems(); i < l; ++i)
			if (listbox->IsSelected(i))
				return i;

		return -1;
	}
	else
		return ((OpDropDown *) opwidget)->GetSelectedItem();
}

void SelectionObject::SetSelectedIndex(int index)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	if (IsListbox())
	{
		OpListBox *listbox = (OpListBox *) opwidget;

		if (index == -1)
			listbox->SelectItem(-1, TRUE);

		for (int i = 0, l = listbox->CountItems(); i < l; ++i)
			listbox->SelectItem(i, i == index);
	}
	else
		((OpDropDown *) opwidget)->SelectItem(index, TRUE);

	opwidget->Invalidate(opwidget->GetBounds());
}

OP_STATUS SelectionObject::SetFormWidgetValue(const uni_char* txt)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	UpdatePosition();
	if (txt)
	{
		const uni_char *tmp = txt;
		int selIndex = uni_atoi(tmp);
		BOOL select_none = FALSE;
		if(*tmp == 0)
		{
			select_none = TRUE;
		}
		if (IsListbox()) // Listbox
		{
			OpListBox* listBox = (OpListBox*)opwidget;
			if (m_multi_selection)
			{
				int num = listBox->CountItems();
				for (int i=0; i<num; i++)
				{
					BOOL selected = (i == selIndex);
					if(select_none)
						selected = FALSE;
					listBox->SelectItem(i, selected);
					if (selected && tmp)
					{
						tmp = uni_strchr(tmp, uni_char(','));
						if (tmp)
						{
							tmp++;
							selIndex = uni_atoi(tmp);
						}
						else
						{
							selIndex = -1;
						}
					}
				}
			}
			else
			{
				if(!select_none)
					listBox->SelectItem(selIndex, TRUE);
			}
		}
		else // Combobox
		{
			OpDropDown* dropdown = (OpDropDown*)opwidget;
			dropdown->SelectItem(selIndex, TRUE);
		}
	}
	opwidget->Invalidate(opwidget->GetBounds());
	return OpStatus::OK;
}

void SelectionObject::Click(ES_Thread* thread)
{
	OP_ASSERT(opwidget);
	if (!m_helm->IsDisabled(m_doc))
	{
		m_listener->HandleGeneratedClick(opwidget, thread); // Will trigger ONCLICK
	}
}

int SelectionObject::GetWidgetScrollPosition()
{
	if (IsListbox())
	{
		OpWidget* child_widget = opwidget->GetFirstChild();
		while (child_widget)
		{
			if (child_widget->GetType() == OpTypedObject::WIDGET_TYPE_SCROLLBAR)
			{
				OpScrollbar* scrollbar = static_cast<OpScrollbar*>(child_widget);
				OP_ASSERT(!scrollbar->IsHorizontal());
				return scrollbar->GetValue();
			}

			child_widget = child_widget->GetNextSibling();
		}
	}
	// Fallback
	return 0;
}

void SelectionObject::SetWidgetScrollPosition(int scroll_pos)
{
	OpWidget* child_widget = opwidget->GetFirstChild();
	while (child_widget)
	{
		if (child_widget->GetType() == OpTypedObject::WIDGET_TYPE_SCROLLBAR)
		{
			OpScrollbar* scrollbar = static_cast<OpScrollbar*>(child_widget);
			OP_ASSERT(!scrollbar->IsHorizontal());
			scrollbar->SetValue(scroll_pos);
		}

		child_widget = child_widget->GetNextSibling();
	}
}

static HTML_Element* GetOptionWithIndex(HTML_Element* select, int idx)
{
	OP_ASSERT(select);
	if (idx < 0)
		return NULL;

	HTML_Element *he = select->FirstChildActual();

	while (he)
	{
		if (he->IsMatchingType(HE_OPTION, NS_HTML))
		{
			if (idx == 0)
			{
				return he;
			}
			idx--;
		}
		else if (he->IsMatchingType(HE_OPTGROUP, NS_HTML))
		{
			he = he->FirstChildActual();
			continue;
		}

		if (he->SucActual())
			he = he->SucActual();
		else if (he->ParentActual()->IsMatchingType(HE_OPTGROUP, NS_HTML))
			he = he->ParentActual()->SucActual();
		else
			he = NULL;
	}

	return NULL;

}

HTML_Element* SelectionObject::GetOptionAtPos(int document_x, int document_y)
{
	OP_ASSERT(IsListbox());

	// Transform to widget space
	OpPoint widget_pt = ToWidgetCoords(opwidget, OpPoint(document_x, document_y));

	OpListBox* listbox = static_cast<OpListBox*>(opwidget);

	int nr = listbox->FindItemAtPosition(widget_pt.y);
	return GetOptionWithIndex(GetHTML_Element(), nr);
}


void SelectionObject::LockWidgetUpdate(BOOL lock)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	if (IsListbox())
		static_cast<OpListBox*>(opwidget)->LockUpdate(lock);
	else
		static_cast<OpDropDown*>(opwidget)->LockUpdate(lock);
}

void SelectionObject::UpdateWidget()
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	if (IsListbox())
		static_cast<OpListBox*>(opwidget)->Update();
	else
		static_cast<OpDropDown*>(opwidget)->Update();
}

void SelectionObject::ScheduleFormValueRebuild()
{
	m_form_value_rebuild_timer_active = TRUE;
	m_form_value_rebuild_timer.Start(FORM_VALUE_REBUILD_DELAY);
}

void SelectionObject::OnTimeOut(OpTimer*)
{
	m_form_value_rebuild_timer_active = FALSE;
	FormValueList* list = FormValueList::GetAs(m_helm->GetFormValue());
	list->Rebuild(m_helm);
}

TextAreaObject::TextAreaObject(VisualDevice* vd,
							   FramesDocument* doc,
                               int xsize,
                               int ysize,
							   BOOL read_only,
                               HTML_Element* he)
  : FormObject(vd, he, doc),
  	m_rows(ysize),
	m_cols(xsize),
	m_is_user_modified(FALSE)
{
}

/* static */
OP_STATUS TextAreaObject::ConstructTextAreaObject(VisualDevice* vd,
							   const HTMLayoutProperties& props,
							   FramesDocument* doc,
                               int xsize,
                               int ysize,
							   BOOL read_only,
                               const uni_char* value,
                               int form_width,
                               int form_height,
                               HTML_Element* he,
                               BOOL use_default_font,
							   FormObject*& constructed_text_area_object)
{
	TextAreaObject* obj = OP_NEW(TextAreaObject, (vd, doc, xsize, ysize, read_only, he));
	if (!obj || OpStatus::IsError(obj->ConstructInternals(props, value, form_width, form_height, use_default_font)))
	{
		OP_DELETE(obj);
		return OpStatus::ERR_NO_MEMORY;
	}
	constructed_text_area_object = obj;
	return OpStatus::OK;
}

OP_STATUS TextAreaObject::ConfigureWidgetFromHTML(BOOL set_start_value)
{
	RETURN_IF_ERROR(FormObject::ConfigureWidgetFromHTML(set_start_value));

	OpMultilineEdit* multiline = static_cast<OpMultilineEdit*>(opwidget);
	const uni_char *placeholder = m_helm->GetStringAttr(ATTR_PLACEHOLDER);

	if (placeholder)
	{
		// If we have placeholder text, remove newlines characters from it
		// before we set it on the widget
		OpString placeholder_tmp;

		RETURN_IF_ERROR(placeholder_tmp.Set(placeholder));
		FormValueText::FilterChars(placeholder_tmp);
		RETURN_IF_ERROR(multiline->SetGhostText(placeholder_tmp.CStr()));
	}

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	HTML_Element* he = GetHTML_Element();

	if (!multiline->SpellcheckByUser())
	{
		switch (he->SpellcheckEnabledByAttr())
		{
			case HTML_Element::SPC_DISABLE:
				multiline->DisableSpellcheck(FALSE /*force*/);
				break;
			case HTML_Element::SPC_ENABLE:
				multiline->EnableSpellcheck();
				break;
			case HTML_Element::SPC_ENABLE_DEFAULT:
				/* Let the widget decide. Turns on spellcheck when the textarea is focused. */
				break;
			default:
				OP_ASSERT(!"TextArea spellcheck attribute default is incorrect!");
				break;
		}
	}
#endif

	return OpStatus::OK;
}


OP_STATUS TextAreaObject::ConstructInternals(
							   const HTMLayoutProperties& props,
                               const uni_char* value,
                               int form_width,
                               int form_height,
                               BOOL use_default_font)
{
	m_width = form_width; //temp
	m_height = form_height;

	// Create the widget
	OpMultilineEdit* new_object;
	RETURN_IF_ERROR(OpMultilineEdit::Construct(&new_object));
	opwidget = new_object;

	m_listener = OP_NEW(WidgetListener, (m_vis_dev->GetDocumentManager(), m_helm));
	if (!m_listener)
	{
		opwidget->Delete();
		opwidget = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}

	// Initialize it
	new_object->SetReadOnly(m_helm->GetReadOnly());

	ConfigureScrollbars(new_object, props);

	InitialiseWidget(props);

#ifdef CSS_SCROLLBARS_SUPPORT
	if (g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::EnableScrollbarColors, m_doc->GetURL()))
	{
		ScrollbarColors colors;
		if (m_doc->GetHLDocProfile())
			colors = *m_doc->GetHLDocProfile()->GetScrollbarColors();
		m_helm->GetScrollbarColor(&colors);

		new_object->SetScrollbarColors(colors);
	}
#endif // CSS_SCROLLBARS_SUPPORT


	// Fix for when <INPUT TYPE="TEXT"...> specifies rows and cols so we get a textarea. (It has no text content so we must set it here).
	if (m_helm->Type() == HE_INPUT)
		return new_object->SetText(NULL);

	return OpStatus::OK;
}

int TextAreaObject::GetPreferredHeight() const
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	OpMultilineEdit* multiline = (OpMultilineEdit*) opwidget;
	return multiline->GetPreferedHeight();
}

OP_STATUS TextAreaObject::GetFormWidgetValue(OpString& out_value, BOOL allow_wrap /* = TRUE */)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	OpMultilineEdit* multiline = (OpMultilineEdit*) opwidget;
	const uni_char* str = m_helm->GetAttrValue(UNI_L("WRAP"));
	BOOL insert_linebreak = FALSE;
	if (str && allow_wrap)
		insert_linebreak = uni_stri_eq(str, "HARD");

	INT32 len = multiline->GetTextLength(insert_linebreak);
	uni_char* buf = out_value.Reserve(len+1); // Room for trailing NULL
	if (!buf)
	{
		return OpStatus::OK;
	}
	multiline->GetText(buf, len, 0, insert_linebreak);
	buf[len] = '\0';
	return OpStatus::OK;
}

OP_STATUS TextAreaObject::SetFormWidgetValue(const uni_char* txt)
{
	return SetFormWidgetValue(txt, FALSE);
}
OP_STATUS TextAreaObject::SetFormWidgetValue(const uni_char* txt, BOOL use_undo_stack)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	UpdatePosition();
	OP_STATUS status = SetText(txt, use_undo_stack);
	opwidget->Invalidate(opwidget->GetBounds());
	m_is_user_modified = FALSE; // This isn't the user. It's a script or the document.
	return status;
}
OP_STATUS TextAreaObject::SetText(const uni_char* value, BOOL use_undo_stack)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	// NULL is ok in OpMultilineEdit::SetText
	m_is_user_modified = FALSE; // This is a script or the document.
#if defined(WIDGETS_UNDO_REDO_SUPPORT)
	if(use_undo_stack)
	{
		return ((OpMultilineEdit*)opwidget)->SetTextWithUndo(const_cast<uni_char*>(value));
	}
	else
#endif
	return ((OpMultilineEdit*)opwidget)->SetText(const_cast<uni_char*>(value));
}

void TextAreaObject::GetWidgetScrollPosition(int& out_scroll_x, int& out_scroll_y)
{
	// fallback values
	out_scroll_x = 0;
	out_scroll_y = 0;

	OpWidget* child_widget = opwidget->GetFirstChild();
	while (child_widget)
	{
		if (child_widget->GetType() == OpTypedObject::WIDGET_TYPE_SCROLLBAR)
		{
			OpScrollbar* scrollbar = static_cast<OpScrollbar*>(child_widget);
			if (scrollbar->IsHorizontal()) // x
			{
				out_scroll_x = scrollbar->GetValue();
			}
			else
			{
				out_scroll_y = scrollbar->GetValue();
			}
		}

		child_widget = child_widget->GetNextSibling();
	}
}

void TextAreaObject::SetWidgetScrollPosition(int scroll_x, int scroll_y)
{
	OpWidget* child_widget = opwidget->GetFirstChild();
	while (child_widget)
	{
		if (child_widget->GetType() == OpTypedObject::WIDGET_TYPE_SCROLLBAR)
		{
			OpScrollbar* scrollbar = static_cast<OpScrollbar*>(child_widget);
			scrollbar->SetValue(scrollbar->IsHorizontal() ? scroll_x : scroll_y);
		}

		child_widget = child_widget->GetNextSibling();
	}
}

void TextAreaObject::GetScrollableSize(int& out_width, int& out_height)
{
	OpMultilineEdit* multiedit = static_cast<OpMultilineEdit*>(opwidget);

	OpRect area = multiedit->GetVisibleRect();

	INT32 height = MAX(multiedit->GetLineHeight(), multiedit->GetContentHeight());

	out_width = MAX(area.width, area.width - multiedit->GetVisibleWidth() + multiedit->GetContentWidth());
	out_height = MAX(area.height, area.height - multiedit->GetVisibleHeight() + height);
}

/* virtual */
void TextAreaObject::Click(ES_Thread* thread)
{
	m_listener->HandleGeneratedClick(opwidget, thread); // Will trigger ONCLICK
}

/* virtual */
void TextAreaObject::GetSelection(INT32& start_ofs, INT32& stop_ofs, SELECTION_DIRECTION& direction)
{
	OP_ASSERT(opwidget);
	static_cast<OpMultilineEdit *>(opwidget)->GetSelection(start_ofs, stop_ofs, direction, TRUE);
}

/* virtual */
void TextAreaObject::SetSelection(INT32 start_ofs, INT32 stop_ofs, SELECTION_DIRECTION direction)
{
	OP_ASSERT(opwidget);
	static_cast<OpMultilineEdit *>(opwidget)->SetSelection(start_ofs, stop_ofs, direction, TRUE);
	opwidget->ScrollIfNeeded();
}

/* virtual */
INT32 TextAreaObject::GetCaretOffset()
{
    return static_cast<OpMultilineEdit *>(opwidget)->GetCaretOffsetNormalized();
}

/* virtual */
void TextAreaObject::SetCaretOffset(INT32 caret_ofs)
{
    static_cast<OpMultilineEdit *>(opwidget)->SetCaretOffsetNormalized(caret_ofs);
}

FileUploadObject::FileUploadObject(VisualDevice* vd,
								   FramesDocument* doc,
								   BOOL read_only,
                                   HTML_Element* he)
	: FormObject(vd, he, doc)
{
}

/* static */
OP_STATUS FileUploadObject::ConstructFileUploadObject(VisualDevice* vd,
								   const HTMLayoutProperties& props,
								   FramesDocument* doc,
								   BOOL read_only,
                                   const uni_char* szValue,
                                   HTML_Element* he,
                                   BOOL use_default_font,
								   FormObject*& created_file_upload_object)
{
	FileUploadObject* obj = OP_NEW(FileUploadObject, (vd, doc, read_only, he));
	if (!obj || OpStatus::IsError(obj->ConstructInternals(props, szValue, use_default_font)))
	{
		OP_DELETE(obj);
		return OpStatus::ERR_NO_MEMORY;
	}
	created_file_upload_object = obj;
	return OpStatus::OK;
}

OP_STATUS FileUploadObject::ConstructInternals(
								   const HTMLayoutProperties& props,
                                   const uni_char* szValue,
                                   BOOL use_default_font)
{
	// OP_ASSERT(szValue);
	OP_ASSERT(m_helm);
	OP_ASSERT(m_doc);
	OP_ASSERT(m_vis_dev);

	// Create the widget
	OpFileChooserEdit* new_object;
	RETURN_IF_ERROR(OpFileChooserEdit::Construct(&new_object));
	opwidget = new_object;
#ifdef _FILE_UPLOAD_SUPPORT_
	if (m_helm->GetBoolAttr(ATTR_MULTIPLE))
	{
		new_object->SetMaxNumberOfFiles(MaxNumberOfUploadFiles);
	}
#endif // _FILE_UPLOAD_SUPPORT_

	m_listener = OP_NEW(WidgetListener, (m_vis_dev->GetDocumentManager(), m_helm));

	if (!m_listener)
	{
		opwidget->Delete();
		opwidget = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}

#ifdef _FILE_UPLOAD_SUPPORT_
	// Tell widget it is an upload object. The widget will then be
	// disabled if in kiosk mode.
	new_object->SetIsFileUpload(TRUE);
#endif // _FILE_UPLOAD_SUPPORT_

	// Initialize it
	InitialiseWidget(props);

#ifdef _FILE_UPLOAD_SUPPORT_
	// Set buttontext
	RETURN_IF_ERROR(new_object->SetText(UNI_L("")));
	return SetFormWidgetValue(szValue);
#else
	// Just a dummy widget
	opwidget->SetEnabled(FALSE);
	return OpStatus::OK;
#endif // _FILE_UPLOAD_SUPPORT_
}

OP_STATUS FileUploadObject::SetFormWidgetValue(const uni_char * szValue)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
	UpdatePosition();
#ifdef _FILE_UPLOAD_SUPPORT_
	const uni_char* uni_val = szValue ? szValue : UNI_L("");
	const uni_char* tmp = uni_val;
	RETURN_IF_ERROR( ((OpFileChooserEdit*)opwidget)->SetText((uni_char*)tmp) );
	opwidget->Invalidate(opwidget->GetBounds());
#endif // _FILE_UPLOAD_SUPPORT_
	return OpStatus::OK;
}

OP_STATUS FileUploadObject::GetFormWidgetValue(OpString& out_value, BOOL allow_wrap /* = TRUE */)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.
#ifdef _FILE_UPLOAD_SUPPORT_
	OpFileChooserEdit* obj = static_cast<OpFileChooserEdit*>(opwidget);
	int len = obj->GetTextLength();
#else
	int len = 0;
#endif // // _FILE_UPLOAD_SUPPORT_
	uni_char* buf = out_value.Reserve(len+1); // Room for trailing NULL
	if (!buf)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

#ifdef _FILE_UPLOAD_SUPPORT_
	obj->GetText(buf);
#else
	buf[0] = '\0';
#endif // _FILE_UPLOAD_SUPPORT_
	return OpStatus::OK;
}

void FileUploadObject::Click(ES_Thread* thread)
{
	OP_ASSERT(opwidget); // Someone is using a FormObject that wasn't constructed successfully. Check construction code.

#ifdef _FILE_UPLOAD_SUPPORT_
	if (!m_helm->IsDisabled(m_doc))
	{
		static_cast<OpFileChooserEdit*>(opwidget)->OnClick();
	}
#endif
}
