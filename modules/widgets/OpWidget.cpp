/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/widgets/OpWidget.h"
#include "modules/widgets/OpListBox.h"

#ifdef WIDGETS_IMS_SUPPORT
#include "modules/widgets/OpIMSObject.h"
#endif // WIDGETS_IMS_SUPPORT

#include "modules/doc/html_doc.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/OpWindow.h"
#include "modules/hardcore/timer/optimer.h"

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
# include "modules/accessibility/AccessibleOpWidgetLabel.h"
# include "modules/accessibility/AccessibleDocument.h"
# ifdef QUICK
#  include "adjunct/quick_toolkit/widgets/OpWorkspace.h"
#  include "adjunct/quick_toolkit/widgets/OpLabel.h"
# endif // QUICK
#endif // ACCESSIBILITY_EXTENSION_SUPPORT

#include "modules/widgets/WidgetContainer.h"
#include "modules/inputmanager/inputmanager.h"

#ifdef SKIN_SUPPORT
#include "modules/skin/OpSkinManager.h"
#endif

#include "modules/prefs/prefsmanager/collections/pc_display.h"

#ifdef QUICK
# include "adjunct/quick_toolkit/windows/DesktopWindow.h"
# include "adjunct/quick_toolkit/widgets/OpGroup.h"
# include "adjunct/quick/Application.h"
#endif

// Do we really have to include Qt specific code in this file?
#if defined(LIBOPERA) 
# ifdef SDK
#  include "product/sdk/include/operacursorset.h"
# else // SDK
#  include "product/embedded/include/operacursorset.h"
# endif // SDK
#endif

#if defined(USE_SKINSTATE_PARAMETER) && defined(HAS_TAB_BUTTON_POSITION)
# include "modules/widgets/OpButton.h"
#endif

#ifdef _PHOTON_
# include "modules/widgets/OpButton.h"
# include "modules/widgets/OpEdit.h"
# include "platforms/photon/pi_impl/photon_opwindow.h"
#endif // _PHOTON_

#include "modules/hardcore/mh/messages.h"

#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/fdelm.h"
#include "modules/forms/piforms.h"
#include "modules/layout/cssprops.h"
#include "modules/layout/layoutprops.h"
#include "modules/style/css.h"
#include "modules/display/fontdb.h"
#include "modules/display/styl_man.h"
#include "modules/display/vis_dev.h"
#include "modules/display/FontAtt.h"

#include "modules/prefs/prefsmanager/collections/pc_display.h"

#ifdef NEARBY_ELEMENT_DETECTION
# include "modules/widgets/finger_touch/element_expander.h"
#endif // NEARBY_ELEMENT_DETECTION

#ifdef DRAG_SUPPORT
#include "modules/dragdrop/dragdrop_manager.h"
#include "modules/pi/OpDragObject.h"
#endif // DRAG_SUPPORT

#define GET_PREF_INT(x) g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::x)

// == OpWidget ==========================================================

OpWidget*& OpWidget::over_widget
{
	return g_widget_globals->hover_widget;
}

#ifndef MOUSELESS
void OpWidget::ZeroCapturedWidget()
{
	if (g_widget_globals->captured_widget)
	{
		g_widget_globals->captured_widget->OnCaptureLost();
		g_widget_globals->captured_widget = 0;
	}
}
void OpWidget::ZeroHoveredWidget()
{
	if (g_widget_globals->hover_widget)
	{
		if (g_widget_globals->hover_widget->listener)
			g_widget_globals->hover_widget->listener->OnMouseLeave(g_widget_globals->hover_widget);

		g_widget_globals->hover_widget->OnMouseLeave();
		g_widget_globals->hover_widget = 0;
	}
}

OpWidget*& OpWidget::hooked_widget
{
	return g_widget_globals->captured_widget;
}
#endif

void OpWidget::InitializeL()
{
	g_widget_globals = OP_NEW_L(WIDGET_GLOBALS, ());
	g_widget_globals->dummy_str = UNI_L("");
	g_widget_globals->hover_widget = NULL;
#ifdef DRAG_SUPPORT
	g_widget_globals->drag_widget = NULL;
#endif // DRAG_SUPPORT
#ifndef MOUSELESS
	g_widget_globals->captured_widget = NULL;
#endif // !MOUSELESS
#ifdef SKIN_SUPPORT
	g_widget_globals->skin_painted_widget = NULL;
	g_widget_globals->painted_widget_extra_state = 0;
#endif

	OpString password_placeholder;
	g_pcdisplay->GetStringPrefL(PrefsCollectionDisplay::PasswordCharacterPlaceholder, password_placeholder);
	OP_ASSERT((password_placeholder.Length() > 0) && "Empty password placeholder?");
	g_widget_globals->passwd_char = uni_strdup(password_placeholder.CStr());
	LEAVE_IF_NULL(g_widget_globals->passwd_char);

	g_widget_globals->had_selected_items = TRUE;
	g_widget_globals->is_down = FALSE;
	g_widget_globals->itemsearch = OP_NEW(OpItemSearch, ());

	// X11 sends two mouse events when a popup window is closed. We have to
	// block the second when the mouse clicked on the dropdown arrow when
	// the dropdown was already visible.
	// The pointer itself is never used, we just compare it with the 'this'
	// pointer
	g_widget_globals->m_blocking_popup_dropdown_widget = 0;

	// Op(Multi)Edit-related
	// char_recognizers points to an array containing instances
	// of all CharRecognizer subclasses
	g_widget_globals->char_recognizers[0] = &g_widget_globals->word_char_recognizer;
	g_widget_globals->char_recognizers[1] = &g_widget_globals->whitespace_char_recognizer;
	g_widget_globals->char_recognizers[2] = &g_widget_globals->word_delimiter_char_recognizer;

	g_widget_globals->m_blocking_popup_calender_widget = NULL;

	g_widget_globals->curr_autocompletion = NULL;
	g_widget_globals->autocomp_popups_visible = FALSE;
}

void OpWidget::Free()
{
	if (g_widget_globals)
	{
		OP_DELETE(g_widget_globals->itemsearch);
		op_free(g_widget_globals->passwd_char);
	}

	OP_DELETE(g_widget_globals);
	g_widget_globals = NULL;
}

OpWidget::OpWidget()
	: m_id(0)
	// Initialize widget faraway<tm> so we won't get unnecessary repaints in document on 0,0 when the widget Invalidate
	// itself before it has been displayed. (Layout sets size, but position is not known until it is displayed first time)
	, rect(-10000, -10000, 0, 0)
	, painter_manager(g_widgetpaintermanager)
	, listener(NULL)
	, vis_dev(NULL)
	, opwindow(NULL)
	, m_script(WritingSystem::Unknown)
	, m_overflow_wrap(0)
	, m_border_left(0), m_border_top(0), m_border_right(0), m_border_bottom(0)
	, m_margin_left(0), m_margin_top(0), m_margin_right(0), m_margin_bottom(0)
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	, m_acc_label(NULL)
	, m_accessible_parent(NULL)
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
	, m_vd_scale(100)
#ifdef WIDGETS_IME_SUPPORT
	, m_suppress_ime(FALSE)
	, m_suppress_ime_mouse_down(FALSE)
#endif // WIDGETS_IME_SUPPORT
	, m_action(NULL)
	, m_widget_container(NULL)
	, form_object(NULL)
	, m_timer(NULL)
	, m_timer_delay(0)
	, m_padding_left(0)
	, m_padding_top(0)
	, m_padding_right(0)
	, m_padding_bottom(0)
#ifdef SKIN_SUPPORT
	, m_border_skin(FALSE)
	, m_foreground_skin(TRUE)
	, m_skin_manager(g_skin_manager)
#endif
#ifndef MOUSELESS
	, m_button_mask(0)
#endif // !MOUSELESS
	, parent(NULL)
	, init_status(OpStatus::OK)
#ifdef NAMED_WIDGET
#ifdef DEBUG_ENABLE_OPASSERT
	, m_has_duplicate_named_widget(FALSE)
#endif // DEBUG_ENABLE_OPASSERT	
#endif // NAMED_WIDGET
{
	op_memset(&packed, 0, sizeof(packed));
	packed.is_enabled = TRUE;
	packed.is_visible = TRUE;
	packed.has_css_border = FALSE;
	packed.has_css_backgroundimage = FALSE;
	packed.has_focus_rect = FALSE;
	packed.is_special_form = FALSE;
	packed.wants_onmove = FALSE;
	packed.ellipsis = (unsigned int)ELLIPSIS_NONE;
	packed.custom_font = FALSE;
	packed.justify_changed_by_user = FALSE;
	packed.relayout_needed = FALSE;
	packed.layout_needed = FALSE;
	packed.is_painting_pending = TRUE;
	packed.is_dead = FALSE;
	packed.is_added = FALSE;
	packed.ignore_mouse = FALSE;
	packed.is_mini = FALSE;
#ifdef WIDGET_IS_HIDDEN_ATTRIB
	packed.is_hidden = FALSE;
#endif // WIDGET_IS_HIDDEN_ATTRIB
#ifdef NAMED_WIDGET
	packed.added_to_name_collection = FALSE;
#endif // NAMED_WIDGET
	packed.locked = FALSE;
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	packed.accessibility_skip = FALSE;
	packed.accessibility_is_ready = TRUE;
	packed.checked_for_label = FALSE;
	packed.accessibility_prune = ACCESSIBILITY_DONT_PRUNE;
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
	packed.is_always_hoverable = FALSE;
	packed.resizability = WIDGET_RESIZABILITY_NONE;

	font_info.font_info = NULL;
	font_info.justify = JUSTIFY_LEFT;

	text_transform = CSS_TEXT_TRANSFORM_none;
	text_indent = 0;

#ifdef SKIN_SUPPORT
	m_border_skin.SetListener(this);
	m_foreground_skin.SetListener(this);
#endif
	m_color.background_color = OP_RGB(0xAA, 0xAA, 0xAA);
	m_color.foreground_color = OP_RGB(0, 0, 0);
	m_color.use_default_background_color = TRUE;
	m_color.use_default_foreground_color = TRUE;

	UpdateSystemFont(TRUE);
}

OpWidget::~OpWidget()
{
	OP_ASSERT(m_widget_container == NULL);

	childs.Clear();
	if (g_main_message_handler)
	{
		g_main_message_handler->RemoveDelayedMessage(MSG_DELETE_WIDGETS, (MH_PARAM_1)this, 0);
		g_main_message_handler->UnsetCallBacks(this);
	}
	OpInputAction::DeleteInputActions(m_action);
	OP_DELETE(m_timer);
#ifdef DRAG_SUPPORT
	if (this == g_widget_globals->drag_widget)
		g_widget_globals->drag_widget = NULL;
#endif // DRAG_SUPPORT
#ifndef MOUSELESS
	if (this == g_widget_globals->captured_widget)
		g_widget_globals->captured_widget = NULL;
#endif // !MOUSELESS
	if (this == g_widget_globals->hover_widget)
		g_widget_globals->hover_widget = NULL;

#ifdef DRAG_SUPPORT
	OpDragObject *drag_data = g_drag_manager->GetDragObject();
	if (g_drag_manager->IsDragging() && this == drag_data->GetSource())
		drag_data->SetSource(NULL);
#endif // DRAG_SUPPORT
#ifdef USE_OP_CLIPBOARD
	g_clipboard_manager->UnregisterListener(this);
#endif // USE_OP_CLIPBOARD
}

// OpInputContext
BOOL OpWidget::OnInputAction(OpInputAction* action)
{
	BOOL handled = FALSE;
	switch (action->GetAction())
	{
#ifdef PAN_SUPPORT
#ifdef ACTION_COMPOSITE_PAN_ENABLED
	case OpInputAction::ACTION_COMPOSITE_PAN:
	{
		int16* delta = (int16*)action->GetActionData();
		if (delta[0] && GenerateOnScrollAction(delta[0], FALSE, FALSE))
			delta[0] = 0;
		if (delta[1] && GenerateOnScrollAction(delta[1], TRUE, FALSE))
			delta[1] = 0;
		handled = !delta[0] && !delta[1];
		break;
	}
#endif // ACTION_COMPOSITE_PAN_ENABLED

	case OpInputAction::ACTION_PAN_X:
	case OpInputAction::ACTION_PAN_Y:
		{
			BOOL vertical = (action->GetAction() == OpInputAction::ACTION_PAN_Y);
			if (IsScrollable(vertical))
				handled = GenerateOnScrollAction(action->GetActionData(), vertical, FALSE);
			break;
		}
#endif // PAN_SUPPORT
	default:
		break;
	};
	return handled;
}

void OpWidget::GetBorders(short& left, short& top, short& right, short& bottom) const
{
	left = m_border_left;
	top = m_border_top;
	right = m_border_right;
	bottom = m_border_bottom;
}

void OpWidget::SetMargins(short margin_left, short margin_top, short margin_right, short margin_bottom)
{
	m_margin_left = margin_left;
	m_margin_top = margin_top;
	m_margin_right = margin_right;
	m_margin_bottom = margin_bottom;
}

void OpWidget::GetMargins(short& left, short& top, short& right, short& bottom) const
{
	left = m_margin_left;
	top = m_margin_top;
	right = m_margin_right;
	bottom = m_margin_bottom;
}

#ifdef SKIN_SUPPORT
BOOL OpWidget::UseMargins(short margin_left, short margin_top, short margin_right, short margin_bottom,
						  UINT8& left, UINT8& top, UINT8& right, UINT8& bottom)
{
	return g_widgetpaintermanager->UseMargins(this,
											  margin_left, margin_top, margin_right, margin_bottom,
											  left, top, right, bottom);
}

void OpWidget::SetSkinManager(OpSkinManager* skin_manager)
{
	m_skin_manager = skin_manager;

	m_border_skin.SetSkinManager(skin_manager);
	m_foreground_skin.SetSkinManager(skin_manager);

	OpWidget* child = (OpWidget*) childs.First();

	while(child)
	{
		child->SetSkinManager(skin_manager);
		child = (OpWidget*) child->Suc();
	}
}

void OpWidget::EnsureSkin(const OpRect& cpaint_rect, BOOL clip, BOOL force)
{
	OpRect paint_rect = cpaint_rect;
	OpWidget* widget_with_skin = this;
	OpRect rect = GetBounds();

	while (widget_with_skin)
	{
		if (!force && widget_with_skin->GetSkinned() && widget_with_skin != this && GetWidgetContainer() && widget_with_skin != GetWidgetContainer()->GetRoot())
		{
			break;
		}

		if (widget_with_skin->packed.skin_is_background)
		{
			if (!widget_with_skin->GetSkinned() || force || widget_with_skin == this || (GetWidgetContainer() && widget_with_skin == GetWidgetContainer()->GetRoot()))
			{
				rect.width = widget_with_skin->GetBounds().width;
				rect.height = widget_with_skin->GetBounds().height;

				if (clip)
				{
					SetClipRect(paint_rect);
				}

				if (GetWidgetContainer() && widget_with_skin == GetWidgetContainer()->GetRoot())
				{
					INT32 left = 0;
					INT32 top = 0;
					INT32 right = 0;
					INT32 bottom = 0;

					GetParentOpWindow()->GetMargin(&left, &top, &right, &bottom);

					rect.x -= left;
					rect.y -= top;
					rect.width += left + right;
					rect.height += top + bottom;
				}

				INT32 extra_state = this == OpWidget::GetFocused() ? SKINSTATE_FOCUSED : 0;

				g_widget_globals->skin_painted_widget = widget_with_skin;
				// The extra state is intended for native skins on some platforms. Not default skin
				g_widget_globals->painted_widget_extra_state = extra_state;

				widget_with_skin->GetBorderSkin()->Draw(vis_dev, rect, &paint_rect, extra_state); //FIXME

				g_widget_globals->skin_painted_widget = 0;
				g_widget_globals->painted_widget_extra_state = 0;

				if (clip)
				{
					RemoveClipRect();
				}
			}

			break;
		}
	
		rect.x -= widget_with_skin->GetRect().x;
		rect.y -= widget_with_skin->GetRect().y;

		widget_with_skin = widget_with_skin->parent;
	}

	if (!packed.skin_is_background && GetBorderSkin()->HasContent())
	{
		OpRect dest = GetBounds();

#ifdef SKIN_SCALESIZE_NOTPIXELS
		int scale = vis_dev->GetScale();
		VDStateNoScale state_noscale;
		if (scale != 100 && GetType() == OpTypedObject::WIDGET_TYPE_BUTTON)
		{
			state_noscale = vis_dev->BeginNoScalePainting(dest);
			dest = paint_rect = state_noscale.dst_rect;
		}
#endif

		g_widget_globals->skin_painted_widget = this;

		INT32 extra_state = this == OpWidget::GetFocused() ? SKINSTATE_FOCUSED : 0;

#ifdef USE_SKINSTATE_PARAMETER
		// Introduced because of DSK-293790. Can't use OpWidgetImage::SetState() here
		if (GetType() == OpTypedObject::WIDGET_TYPE_DROPDOWN)
		{
			if (this == g_widget_globals->hover_widget)
				extra_state |= SKINSTATE_HOVER;
			if (this == g_widget_globals->captured_widget)
				extra_state |= SKINSTATE_PRESSED;
		}

#ifdef HAS_TAB_BUTTON_POSITION
		if (IsOfType(OpTypedObject::WIDGET_TYPE_BUTTON) &&
		    (((OpButton*)this)->GetButtonType() == OpButton::TYPE_TAB))

			extra_state |= (((OpButton*)this)->GetTabState() << OpButton::TAB_STATE_OFFSET);
#endif // HAS_TAB_BUTTON_POSITION

#endif // USE_SKINSTATE_PARAMETER

		GetBorderSkin()->Draw(vis_dev, dest, &paint_rect, extra_state);		//fix

		g_widget_globals->skin_painted_widget = 0;

#ifdef SKIN_SCALESIZE_NOTPIXELS
		if (scale != 100 && GetType() == OpTypedObject::WIDGET_TYPE_BUTTON)
		{
			vis_dev->EndNoScalePainting(state_noscale);
		}
#endif
	}
}

void OpWidget::EnsureSkinOverlay(const OpRect& cpaint_rect)
{
	OpRect paint_rect = cpaint_rect;

	if (!packed.skin_is_background && GetBorderSkin()->HasContent())
	{
		OpRect dest = GetBounds();

#ifdef SKIN_SCALESIZE_NOTPIXELS
		int scale = vis_dev->GetScale();
		VDStateNoScale state_noscale;
		if (scale != 100 && GetType() == OpTypedObject::WIDGET_TYPE_BUTTON)
		{
			state_noscale = vis_dev->BeginNoScalePainting(dest);
			dest = paint_rect = state_noscale.dst_rect;
		}
#endif

		g_widget_globals->skin_painted_widget = this;

		INT32 extra_state = this == OpWidget::GetFocused() ? SKINSTATE_FOCUSED : 0;

#if defined(USE_SKINSTATE_PARAMETER) && defined(HAS_TAB_BUTTON_POSITION)
		if (IsOfType(OpTypedObject::WIDGET_TYPE_BUTTON) &&
		    (((OpButton*)this)->GetButtonType() == OpButton::TYPE_TAB))

			extra_state |= (((OpButton*)this)->GetTabState() << OpButton::TAB_STATE_OFFSET);
#endif

		GetBorderSkin()->Draw(vis_dev, dest, &paint_rect, extra_state, TRUE);

		g_widget_globals->skin_painted_widget = 0;

#ifdef SKIN_SCALESIZE_NOTPIXELS
		if (scale != 100 && GetType() == OpTypedObject::WIDGET_TYPE_BUTTON)
		{
			vis_dev->EndNoScalePainting(state_noscale);
		}
#endif
	}
}

void OpWidget::OnImageChanged(OpWidgetImage* widget_image)
{
#ifdef QUICK
	Relayout(TRUE, FALSE);
#endif
	Invalidate(GetBounds());
}

#ifdef ANIMATED_SKIN_SUPPORT
void OpWidget::OnAnimatedImageChanged(OpWidgetImage* widget_image)
{
	if(IsAllVisible())
	{
		Invalidate(GetBounds());
	}
}
BOOL OpWidget::IsAnimationVisible()
{
	return IsAllVisible();
}
#endif

void OpWidget::SetMiniSize(BOOL is_mini) 
{ 
	packed.is_mini = is_mini;
	
	// Only change the MINI state
	INT32 state = 0, mask = SKINSTATE_MINI;

	if (is_mini)
		state |= SKINSTATE_MINI;
	
	GetBorderSkin()->SetState(state, mask);
	GetForegroundSkin()->SetState(state, mask);
}

#endif // SKIN_SUPPORT

void OpWidget::Delete()
{
	Remove();
	GenerateOnDeleted();

	OpWidgetExternalListener *el = (OpWidgetExternalListener *) g_opera->widgets_module.external_listeners.First();
	while (el)
	{
		el->OnDeleted(this);
		el = (OpWidgetExternalListener *) el->Suc();
	}

	g_opera->widgets_module.PostDeleteWidget(this);
}

void OpWidget::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch (msg)
	{
#ifdef QUICK
	case MSG_LAYOUT:
	{
		DesktopWindow* parent_window = GetParentDesktopWindow();

		if (parent_window && !parent_window->IsVisible())
			return;

		SyncLayout();
		break;
	}
#endif
	}
}

void OpWidget::AddChild(OpWidget* child, BOOL is_internal_child, BOOL first)
{
	InsertChild(child, NULL, FALSE, is_internal_child, first);
}

void OpWidget::InsertChild(OpWidget* child, OpWidget* ref_widget, BOOL after_ref_widget, BOOL is_internal_child, BOOL first)
{
	if (ref_widget && !ref_widget->InList())
		return;

	child->packed.is_internal_child = is_internal_child;
	child->parent = this;

	if (child->packed.wants_onmove)
		SetOnMoveWanted(TRUE);

	if (first)
		child->IntoStart(&childs);
	else if (!ref_widget)
		child->Into(&childs);
	else
	{
		if (after_ref_widget)
			child->Follow(ref_widget);
		else
			child->Precede(ref_widget);
	}

	child->SetListener(listener, FALSE);

	if (vis_dev)
	{
		child->GenerateOnAdded(vis_dev, m_widget_container, opwindow);
		if (child->IsAllVisible())
			child->GenerateOnShow(TRUE);
	}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	//removes children that are only there for shadowing and the like
	if (child->GetType() == WIDGET_TYPE)
		child->AccessibilitySkipsMe();
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
}

void OpWidget::Remove()
{
	if (vis_dev)
	{
		if (!IsForm())
			InvalidateAll();
		GenerateOnRemoving();
	}

	Out();
	parent = NULL;

	if (vis_dev)
	{
		GenerateOnRemoved();
	}
}

void OpWidget::SetZ(Z z)
{
	if (!parent)
		return;
	Out();
	if (z == Z_TOP)
		IntoStart(&parent->childs);
	else if (z == Z_BOTTOM)
		Into(&parent->childs);

	OP_ASSERT(InList());
}

void OpWidget::GetBorderBox(INT32& width, INT32& height)
{
	width = GetWidth();
	height = GetHeight();

	if (HasCssBorder())
	{
		short left, top, right, bottom;
		GetBorders(left, top, right, bottom);

		width += left + right;
		height += top + bottom;
	}
}

BOOL OpWidget::IsForm()
{
	return form_object || packed.is_special_form || (GetParent() && GetParent()->IsForm());
}

FormObject* OpWidget::GetFormObject(BOOL iterate_parents) const
{
	if (iterate_parents && GetParent())
		return form_object ? form_object : GetParent()->GetFormObject(TRUE);
	return form_object;
}

void OpWidget::SetParentInputContext(OpInputContext* parent_context, BOOL keep_keyboard_focus)
{
	OpInputContext::SetParentInputContext(parent_context, keep_keyboard_focus);

	if (IsForm())
	{
		OpWidget* child = (OpWidget*) childs.First();
		while(child)
		{
			child->SetParentInputContext(this, keep_keyboard_focus);
			child = (OpWidget*) child->Suc();
		}
	}
}

void OpWidget::GenerateHighlightRectChanged(const OpRect &rect, BOOL report_caret)
{
	if (!vis_dev)
		return;

	AffinePos doc_pos = GetPosInDocument();

	if (GetFormObject() && !GetFormObject()->IsDisplayed()) ///< Tempfix for not yet displayed forms. The widget doesn't really know the pos yet.
	{
		HTML_Element* helm = GetFormObject()->GetHTML_Element();
		// Make sure the element isn't dirty since that may lead to this form being deleted while its used in GetPosInDocument.
		if (helm && !helm->IsDirty() && !helm->IsPropsDirty())
		{
			// Make sure we're not already reflowing as that may cause a crash.
			DocumentManager* doc_manager = vis_dev->GetDocumentManager();
			if (doc_manager)
			{
				FramesDocument* frames_doc = doc_manager->GetCurrentDoc();
				if (frames_doc && !frames_doc->IsReflowing())
					GetFormObject()->GetPosInDocument(&doc_pos);
			}
		}
	}

	OpRect dstrect = rect;
	doc_pos.Apply(dstrect);

	dstrect.OffsetBy(-vis_dev->GetRenderingViewX(), -vis_dev->GetRenderingViewY());
	dstrect = vis_dev->ScaleToScreen(dstrect);
	dstrect = vis_dev->OffsetToContainer(dstrect);

	vis_dev->GetOpView()->OnHighlightRectChanged(dstrect);
	if (report_caret)
		vis_dev->GetOpView()->OnCaretPosChanged(dstrect);
}

void OpWidget::GenerateOnAdded(VisualDevice* vd, WidgetContainer* container, OpWindow* window)
{
	m_widget_container = container;
	vis_dev = vd;
	opwindow = window;
	SetParentInputContext(parent);
	packed.is_added = TRUE;
#ifdef QUICK
	SetUpdateNeeded(TRUE);
#endif

#ifdef NAMED_WIDGET
	// GenerateOnAdded may be called multiple times for a widgets if it's added to its parent before the parent is added.
	// So we must make sure we only add it if it's not already added.
	if(!packed.added_to_name_collection)
	{
		if (OpNamedWidgetsCollection *nwc = parent->GetNamedWidgetsCollection())
		{
			nwc->WidgetAdded(this);
			packed.added_to_name_collection = TRUE;
		}
	}
#endif //NAMED_WIDGET

	OpWidget* child = (OpWidget*) childs.First();
	while(child)
	{
		child->GenerateOnAdded(vd, container, window);
		child = (OpWidget*) child->Suc();
	}
	OnAdded();
}

void OpWidget::GenerateOnRemoving()
{
#ifdef NAMED_WIDGET
	if(packed.added_to_name_collection)
		if (OpNamedWidgetsCollection *nwc = parent->GetNamedWidgetsCollection())
		{
			nwc->WidgetRemoved(this);
			packed.added_to_name_collection = FALSE;
		}
#endif //NAMED_WIDGET

	OpWidget* child = (OpWidget*) childs.First();
	while(child)
	{
		child->GenerateOnRemoving();
		child = (OpWidget*) child->Suc();
	}
	OnRemoving();
}

void OpWidget::GenerateOnRemoved()
{
	OpWidget* child = (OpWidget*) childs.First();
	while(child)
	{
		child->GenerateOnRemoved();
		child = (OpWidget*) child->Suc();
	}

	packed.is_added = FALSE;

	SetParentInputContext(NULL);
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	// Unlink this node from the accessibility tree of the document
	if (vis_dev && vis_dev->GetAccessibleDocument())
	{
		vis_dev->GetAccessibleDocument()->WidgetRemoved(this);
	}
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

#ifdef DRAG_SUPPORT
	if (this == g_widget_globals->drag_widget)
		g_widget_globals->drag_widget = NULL;
#endif // DRAG_SUPPORT
#ifndef MOUSELESS
	if (this == g_widget_globals->captured_widget)
		g_widget_globals->captured_widget = NULL;
#endif // !MOUSELESS
	if (this == g_widget_globals->hover_widget)
		g_widget_globals->hover_widget = NULL;
	m_widget_container = NULL;
	vis_dev = NULL;
	opwindow = NULL;
	OnRemoved();
}

void OpWidget::GenerateOnDeleted()
{
	OpWidget* child = (OpWidget*) childs.First();
	while(child)
	{
		child->GenerateOnDeleted();
		child = (OpWidget*) child->Suc();
	}

#if defined (QUICK)
	MarkDeleted();
#endif

	StopTimer();
	listener = NULL;
	OnDeleted();
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	if (m_acc_label)
		m_acc_label->DissociateLabelFromOpWidget(this);
	m_acc_label = NULL;
	packed.accessibility_is_ready = FALSE;
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

	// We might live longer than the form_object so make sure it's NULL here.
	form_object = NULL;
}

BOOL OpWidget::IsAllVisible() const
{
	if (!vis_dev)
		return FALSE;

	for (OpWidget* widget = (OpWidget*) this; widget; widget = widget->GetParent())
	{
		if (!widget->IsVisible())
		{
			return FALSE;
		}
	}

	return TRUE;
}

void OpWidget::SetWidgetContainer(WidgetContainer* container)
{
	m_widget_container = container;

	OpWidget* child = (OpWidget*) childs.First();
	while(child)
	{
		child->SetWidgetContainer(container);
		child = (OpWidget*) child->Suc();
	}
}

void OpWidget::SetListener(OpWidgetListener* listener, BOOL force)
{
	OpWidget* child = (OpWidget*) childs.First();
	while(child)
	{
		if (!child->packed.is_internal_child)
			child->SetListener(listener, force);
		child = (OpWidget*) child->Suc();
	}
	if (this->listener == NULL || force)
		this->listener = listener;
}

void OpWidget::SetVisualDevice(VisualDevice* vis_dev)
{
	OpWidget* child = (OpWidget*) childs.First();
	while(child)
	{
		child->SetVisualDevice(vis_dev);
		child = (OpWidget*) child->Suc();
	}
	if (vis_dev)
	{
		packed.is_added = TRUE;
		// SetVisualDevice is called every time the widget is painted,
		// so m_vd_scale should only be updated when visual device
		// changes
		if (vis_dev != this->vis_dev)
			m_vd_scale = vis_dev->GetScale();
	}
	else
	{
		packed.is_added = FALSE;
		m_vd_scale = 100;
	}

	this->vis_dev = vis_dev;
}

void OpWidget::SetHasCssBorder(BOOL has_css_border)
{
	if (GetAllowStyling() || !has_css_border)
		packed.has_css_border = has_css_border;
}

void OpWidget::SetHasCssBackground(BOOL has_css_background)
{
	if (GetAllowStyling() || !has_css_background)
		packed.has_css_backgroundimage = has_css_background;
}

void OpWidget::SetHasFocusRect(BOOL has_focus_rect)
{
	packed.has_focus_rect = has_focus_rect;

	OpWidget* child = (OpWidget*) childs.First();

	while(child)
	{
		child->SetHasFocusRect(has_focus_rect);
		child = (OpWidget*) child->Suc();
	}
}

BOOL OpWidget::GetAllowStyling()
{
	const uni_char* host = NULL;
	if (vis_dev && vis_dev->GetDocumentManager()
		&& vis_dev->GetDocumentManager()->GetCurrentDoc()
		)
	{
		host = vis_dev->GetDocumentManager()->GetCurrentDoc()->GetHostName();
	}

	if (!IsForm() || g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::EnableStylingOnForms, host))
		return TRUE;
	return FALSE;
}

void OpWidget::SetResizability(WIDGET_RESIZABILITY resizability)
{
	if (GetResizability() != resizability)
	{
		packed.resizability = resizability;
		OnResizabilityChanged();
	}
}

void OpWidget::SetCanHaveFocusInPopup(BOOL new_val)
{
	packed.can_have_focus_in_popup = new_val; 

	OpWidget* child = (OpWidget*) childs.First();

	while(child)
	{
		child->SetCanHaveFocusInPopup(new_val);
		child = (OpWidget*) child->Suc();
	}
}

BOOL OpWidget::IsInWebForm() const
{
	const OpWidget* widget = this;
	do
	{
		if (widget->form_object)
		{
			return TRUE;
		}
		widget = widget->parent;
	} 
	while (widget);

	return FALSE;
}

void OpWidget::StartTimer(UINT32 millisec)
{
	OP_ASSERT(millisec > 0); // Don't try to start a timer with 0 millisec
	StopTimer();
	m_timer_delay = millisec;
	if (!m_timer)
	{
		OP_STATUS status = OpStatus::OK;
		m_timer = OP_NEW(OpTimer, ());
		if (!m_timer)
			status = OpStatus::ERR_NO_MEMORY;
		if (OpStatus::IsError(status))
		{
			ReportOOM();
			return;
		}
		m_timer->SetTimerListener(this);
	}
	m_timer->Start(millisec);
}

void OpWidget::StopTimer()
{
	if (m_timer)
	{
		m_timer->Stop();
		m_timer_delay = 0;
	}
}

void OpWidget::OnTimeOut(OpTimer* timer)
{
	if (GetFormObject())
		GetFormObject()->UpdatePosition();
	timer->Start(m_timer_delay);
	OnTimer();
}

UINT32 OpWidget::GetTimerDelay() const { return m_timer_delay; }
BOOL OpWidget::IsTimerRunning() const { return m_timer_delay > 0; }

void OpWidget::SetAction(OpInputAction* action)
{
	if (action != m_action)
	{
#ifdef QUICK
		SetUpdateNeeded(TRUE);
		Relayout();
#endif
		OpInputAction::DeleteInputActions(m_action);
		m_action = action; 
	}
}

void OpWidget::SetRect(const OpRect &crect, BOOL invalidate, BOOL resize_children)
{
	OpRect rect = crect;

	int dx = rect.width - this->rect.width;
	int dy = rect.height - this->rect.height;

	BOOL resized = dx || dy;
	BOOL moved = this->rect.x != rect.x || this->rect.y != rect.y;

	if (invalidate && (moved || resized) && !IsForm())
		InvalidateAll();

	this->rect = rect;

	if(resized)
	{
		// iterate through children and change them according to resize effect

#if defined (QUICK)
		if (resize_children)
		{
			for (OpWidget* child = GetFirstChild(); child != NULL; child = child->GetNextSibling())
			{
				RESIZE_EFFECT x_effect = child->GetXResizeEffect();
				RESIZE_EFFECT y_effect = child->GetYResizeEffect();

				if (x_effect != RESIZE_FIXED || y_effect != RESIZE_FIXED || GetRTL())
				{
					OpRect child_rect = child->GetRect();

					switch (x_effect)
					{
						case RESIZE_FIXED:
							if (GetRTL())
								child_rect.x += dx;
							break;

						case RESIZE_MOVE:
							if (!GetRTL())
								child_rect.x += dx;
							break;

						case RESIZE_SIZE:
							child_rect.width += dx;
							break;

						case RESIZE_CENTER:
							child_rect.x = (rect.width - child_rect.width) / 2;
							break;
					}

					switch (y_effect)
					{
						case RESIZE_MOVE:
							child_rect.y += dy;
							break;

						case RESIZE_SIZE:
							child_rect.height += dy;
							break;

						case RESIZE_CENTER:
							child_rect.y = (rect.height - child_rect.height) / 2;
							break;
					}

					child->SetRect(child_rect);
				}
			}
		}
#endif

		OnResize(&rect.width, &rect.height);
	}

	if (moved)
	{
		GenerateOnMove();
	}

	if (invalidate && (moved || resized) && !IsForm())
		InvalidateAll();

#ifdef QUICK
	if (resized)
	{
		Relayout(FALSE, FALSE);
	}
#endif
}

void OpWidget::GenerateOnMove()
{
	if(packed.wants_onmove)
	{
		OnMove();

		OpWidget* child = (OpWidget*) childs.First();

		while(child)
		{
			child->GenerateOnMove();
			child = (OpWidget*) child->Suc();
		}
	}
}

#ifdef DRAG_SUPPORT

void OpWidget::OnDragStart(const OpPoint& point)
{
	if (listener)
		// Delegate to a listener so it decides.
		listener->OnDragStart(this, 0, point.x, point.y);
	// Nothing to drag so cancel.
	else
		g_drag_manager->StopDrag(TRUE);
}

void OpWidgetListener::OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y)
{
	// Nothing to drag so cancel.
	g_drag_manager->StopDrag(TRUE);
}

void OpWidget::GenerateOnDragStart(const OpPoint& point)
{
	g_widget_globals->drag_widget = GetWidget(point, TRUE);
	if (g_widget_globals->drag_widget)
	{
		OpRect rect = g_widget_globals->drag_widget->GetRect(TRUE);
		OpPoint translated_point = OpPoint(point.x - rect.x, point.y - rect.y);
		g_widget_globals->drag_widget->OnDragStart(translated_point);
	}
}

void OpWidget::GenerateOnDragLeave(OpDragObject* drag_object)
{
	if (g_widget_globals->drag_widget)
	{
		g_widget_globals->drag_widget->OnDragLeave(drag_object);
		g_widget_globals->drag_widget = NULL;
	}
}

void OpWidget::GenerateOnDragCancel(OpDragObject* drag_object)
{
	if (g_widget_globals->drag_widget)
	{
		g_widget_globals->drag_widget->OnDragCancel(drag_object);
		g_widget_globals->drag_widget = NULL;
	}
}

static void EnsureCorrectDropType(OpWidget* widget, OpDragObject* object)
{
	OP_ASSERT(object);
	DropType drop_type = object->GetSuggestedDropType();
	if (drop_type == DROP_UNINITIALIZED)
		drop_type = object->GetDropType();

	unsigned int effects_allowed = object->GetEffectsAllowed();
	if (((drop_type & effects_allowed) != 0) || effects_allowed == DROP_UNINITIALIZED)
	{
		object->SetDropType(drop_type);
		object->SetVisualDropType(drop_type);
		return;
	}

	object->SetDropType(DROP_NONE);
	object->SetVisualDropType(DROP_NONE);
	if (widget)
		widget->OnDragLeave(object);
}


void OpWidget::GenerateOnDragMove(OpDragObject* drag_object, const OpPoint& point)
{
	OpWidget* new_drag_widget = GetWidget(point, TRUE);

	if (new_drag_widget)
	{
		// Make sure a widget will not use disallowed effect.
		EnsureCorrectDropType(new_drag_widget, drag_object);
		if (g_widget_globals->drag_widget != new_drag_widget)
		{
			if (g_widget_globals->drag_widget)
				g_widget_globals->drag_widget->GenerateOnDragLeave(drag_object);
			g_widget_globals->drag_widget = new_drag_widget;
		}

		OpRect rect = new_drag_widget->GetRect(TRUE);
		OpPoint translated_point = OpPoint(point.x - rect.x, point.y - rect.y);
		new_drag_widget->OnDragMove(drag_object, translated_point);
		if (drag_object->GetDropType() != DROP_NONE)
			// A widget might have changed it to something illegal. Ensure it's correct again.
			EnsureCorrectDropType(new_drag_widget, drag_object);
		OpWidget::OnSetCursor(translated_point);
	}
}

void OpWidget::OnDragMove(OpDragObject* drag_object, const OpPoint& point)
{
	drag_object->SetDropType(DROP_NOT_AVAILABLE);
	drag_object->SetVisualDropType(DROP_NOT_AVAILABLE);
	if (listener)
		listener->OnDragMove(this, drag_object, 0, point.x, point.y);
}

void OpWidget::GenerateOnDragDrop(OpDragObject* drag_object, const OpPoint& point)
{
	GenerateOnDragMove(drag_object, point);

	if (g_widget_globals->drag_widget)
	{
		OpRect rect = g_widget_globals->drag_widget->GetRect(TRUE);
		OpPoint translated_point = OpPoint(point.x - rect.x, point.y - rect.y);
		g_widget_globals->drag_widget->OnDragDrop(drag_object, translated_point);
		g_widget_globals->drag_widget = NULL;
	}
}

#endif // DRAG_SUPPORT

void OpWidget::GenerateOnWindowMoved()
{
	OnWindowMoved();

	OpWidget* child = (OpWidget*) childs.First();

	while(child)
	{
		child->GenerateOnWindowMoved();
		child = (OpWidget*) child->Suc();
	}
}


void OpWidget::GenerateOnWindowActivated(BOOL activate)
 {
	OnWindowActivated(activate);

	OpWidget* child = (OpWidget*) childs.First();

	while(child)
	{
		child->GenerateOnWindowActivated(activate);
		child = (OpWidget*) child->Suc();
	}
}

void OpWidget::SetSize(INT32 w, INT32 h)
{
	SetRect(OpRect(rect.x, rect.y, w, h));
}

void OpWidget::SetPosInDocument(const AffinePos& ctm)
{
	BOOL moved = document_ctm != ctm;
	document_ctm = ctm;
	if (moved)
		GenerateOnMove();
}

void OpWidget::Sync()
{
#ifdef QUICK
	if (GetWidgetContainer())
	{
		OpWidget* root = GetWidgetContainer()->GetRoot();

		if (g_main_message_handler->HasCallBack(root, MSG_LAYOUT, (MH_PARAM_1)root))
			return;
	}
#endif
	if (vis_dev && vis_dev->view)
	{
		vis_dev->view->Sync();
	}
}

void OpWidget::ReportOOM()
{
	if (vis_dev && vis_dev->GetDocumentManager()
		&& vis_dev->GetDocumentManager()->GetCurrentDoc()
		&& vis_dev->GetDocumentManager()->GetCurrentDoc()->GetWindow()
		)
		vis_dev->GetDocumentManager()->GetCurrentDoc()->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
	else
		g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
}

AffinePos OpWidget::GetPosInDocument()
{
	if (parent)
	{
		AffinePos pos = parent->GetPosInDocument();
		pos.AppendTranslation(rect.x, rect.y);
		return pos;
	}
	return document_ctm;
}

AffinePos OpWidget::GetPosInRootDocument()
{
	AffinePos pos = GetPosInDocument();
	if (vis_dev && vis_dev->GetDocumentManager() &&
		vis_dev->GetDocumentManager()->GetFrame())
	{
		FramesDocElm *fde = vis_dev->GetDocumentManager()->GetFrame();
		pos.PrependTranslation(fde->GetAbsX(), fde->GetAbsY());
	}
	return pos;
}

OpPoint OpWidget::GetMousePos()
{
	OpPoint point;

#ifndef MOUSELESS
	if (vis_dev)
	{
		vis_dev->view->GetMousePos(&point.x, &point.y);

		point.x = vis_dev->ScaleToDoc(point.x);
		point.y = vis_dev->ScaleToDoc(point.y);

		point.x += vis_dev->GetRenderingViewX();
		point.y += vis_dev->GetRenderingViewY();

		AffinePos doc_pos = GetPosInDocument();
		doc_pos.ApplyInverse(point);
	}
#endif // MOUSELESS

	return point;
}

OpRect OpWidget::GetRect(BOOL relative_to_root)
{
	if (relative_to_root)
	{
		OpRect root_rect = rect;

		OpWidget* next_parent = parent;

		while (next_parent)
		{
			root_rect.x += next_parent->rect.x;
			root_rect.y += next_parent->rect.y;

			next_parent = next_parent->parent;
		}

		return root_rect;
	}
	else
	{
		return rect;
	}
}

OpRect OpWidget::GetDocumentRect(BOOL relative_to_root)
{
	OpRect doc_rect = GetRect(relative_to_root);
	if (relative_to_root)
	{
		OpWidget* ctm_widget = this;

		while (ctm_widget->parent)
			ctm_widget = ctm_widget->parent;

		ctm_widget->document_ctm.Apply(doc_rect);
	}
	else
		document_ctm.Apply(doc_rect);

	return doc_rect;
}

OpRect OpWidget::GetScreenRect()
{
	OpRect rect = GetDocumentRect(TRUE);
	if (vis_dev)
	{
		rect.OffsetBy(-vis_dev->GetRenderingViewX(), -vis_dev->GetRenderingViewY());
		rect = vis_dev->ScaleToScreen(rect);
		OpPoint point(0,0);
		point = vis_dev->view->ConvertToScreen(point);
		rect.OffsetBy(point);
	}
	return rect;
}

OpRect OpWidget::GetBounds(BOOL include_margin/* = FALSE*/)
{
	OpRect r = OpRect(0, 0, rect.width, rect.height);
	if (include_margin)
	{
		if (m_margin_left > 0)
		{
			r.x -= m_margin_left;
			r.width += m_margin_left;
		}
		if (m_margin_right > 0)
		{
			r.width += m_margin_right;
		}
		if (m_margin_top > 0)
		{
			r.y -= m_margin_top;
			r.height += m_margin_top;
		}
		if (m_margin_bottom > 0)
		{
			r.height += m_margin_bottom;
		}
	}
	return r;
}

OpRect OpWidget::AddNonclickableMargins(const OpRect& rect)
{
	OpRect r(rect);
#ifdef SKIN_SUPPORT
	INT32 m_left = 0, m_top = 0, m_right = 0, m_bottom = 0;
	GetBorderSkin()->GetMargin(&m_left, &m_top, &m_right, &m_bottom);
	if (m_left < 0)
	{
		r.x += -m_left;
		r.width -= -m_left;
	}
	if (m_right < 0)
		r.width -= -m_right;
	if (m_top < 0)
	{
		r.y += -m_top;
		r.height -= -m_top;
	}
	if (m_bottom < 0)
		r.height -= -m_bottom;
#endif
	return r;
}

void OpWidget::SetClipRect(const OpRect &cliprect)
{
	vis_dev->BeginClipping(cliprect);
}

void OpWidget::RemoveClipRect()
{
	vis_dev->EndClipping();
}

OpRect OpWidget::GetClipRect()
{
	OpRect cliprect = vis_dev->GetClipping();

	for (OpWidget* run = this; run->parent; run = run->parent)
	{
		cliprect.x -= run->rect.x;
		cliprect.y -= run->rect.y;
	}

	return cliprect;
}

BOOL OpWidget::GetIntersectingCliprect(OpRect &rect)
{
	if (form_object == NULL || !form_object->IsDisplayed())
	{
		rect = GetRect();
		return TRUE;
	}

	FramesDocument* doc = vis_dev->GetDocumentManager()->GetCurrentDoc();

	if (doc)
	{
		RECT clip_rect;

		HTML_Element* helm = form_object->GetHTML_Element();
		// Make sure the element isn't dirty which could cause GetCliprect reflow the document
		// which might lead to this widget being deleted.
		if (helm == NULL || helm->IsDirty() || helm->IsPropsDirty())
		{
			rect = GetRect();
			return FALSE;
		}

		// GetCliprect doesn't deal with scrollable
		// containers properly, so the quick-path in OpMultilineEdit
		// and ListboxContainer cannot be taken for these cases - see
		// eg CORE-36703
		if (GetScrollableParent(helm))
			return FALSE;

		BOOL status = doc->GetLogicalDocument()->GetCliprect(helm, clip_rect);

		rect.x = clip_rect.left;
		rect.y = clip_rect.top;
		rect.width = clip_rect.right - clip_rect.left;
		rect.height = clip_rect.bottom - clip_rect.top;
		rect.x -= vis_dev->GetRenderingViewX();
		rect.y -= vis_dev->GetRenderingViewY();

		return status;
	}

	return FALSE;
}

OpWidget* OpWidget::GetWidget(OpPoint point, BOOL include_internal_children, BOOL include_ignored_mouse)
{
	if (!packed.is_visible || !AddNonclickableMargins(GetBounds()).Contains(point) ||
		(packed.is_internal_child && !include_internal_children) ||
		(IgnoresMouse() && !include_ignored_mouse))
	{
		return NULL;
	}

	OpWidget* child = (OpWidget*) childs.First();

	while(child)
	{
		OpWidget* hit = child->GetWidget(OpPoint(point.x - child->rect.x, point.y - child->rect.y), include_internal_children, include_ignored_mouse);

		if (hit)
		{
			return hit;
		}

		child = (OpWidget*) child->Suc();
	}

	return this;
}

void OpWidget::Invalidate(const OpRect &invalidate_rect, BOOL intersect, BOOL force, BOOL timed)
{
	if (vis_dev && (packed.is_visible || force))
	{
		if (GetFormObject() && !GetFormObject()->IsDisplayed())
			return;

		if (!timed && this->GetType() == OpTypedObject::WIDGET_TYPE_SCROLLBAR)
		{
			// Use timed updating for scrollbar so we don't repaint it too much when page is loading.
			// Since the scrollbar call Sync on hover etc. this won't affect its responsiveness even during loading.
			timed = TRUE;
		}

		OpRect irect(invalidate_rect);

		// If childobject, translate and update the parent instead.
		if(parent)
		{
			irect.OffsetBy(GetRect().TopLeft());

#ifdef _MACINTOSH_
			parent->Invalidate(irect, intersect, force, timed);
#else
			parent->Invalidate(irect, TRUE, FALSE, timed);
#endif
			return;
		}

		if (intersect)
			irect.IntersectWith(GetBounds()); // Don't allow a bigger update than the widget itself.

		// Offset with document position
		document_ctm.Apply(irect);

		{
			// This seems a little strange to me. Update expects nonscaled koordinates but with the scaled value for scrolling added.
			vis_dev->Update(irect.x, irect.y, irect.width, irect.height, timed);
		}
	}
}

void OpWidget::InvalidateAll()
{
#ifdef SKIN_SUPPORT
	FocusRectInMargins()?
		Invalidate(GetBounds(TRUE), FALSE) :
#endif // SKIN_SUPPORT
		Invalidate(GetBounds());
	packed.is_painting_pending = TRUE;
}

void OpWidget::SetEnabled(BOOL enabled)
{
	OpWidget* child = (OpWidget*) childs.First();
	while(child)
	{
		child->SetEnabled(enabled);
		child = (OpWidget*) child->Suc();
	}

	if (packed.is_enabled != (UINT32) enabled)
	{
		packed.is_enabled = enabled;
		if (!enabled)
		{
			ReleaseFocus();
		}
		Update();
		InvalidateAll();
#ifdef SKIN_SUPPORT
		INT32 state = enabled ? 0 : SKINSTATE_DISABLED;

		if (GetBorderSkin()->HasContent())
		{
			GetBorderSkin()->SetState(state, SKINSTATE_DISABLED, GetBorderSkin()->GetHoverValue());
		}
#endif
		if (listener)
			listener->OnEnabled(this, enabled);
	}
}

void OpWidget::SetVisibility(BOOL visible)
{
	if (packed.is_visible != (UINT32) visible)
	{
		packed.is_visible = visible;

		if (!visible)
		{
			ReleaseFocus();
		}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
		AccessibilitySendEvent(Accessibility::Event(Accessibility::kAccessibilityStateInvisible));
#endif

		// GenerateOnShow events if this widget really changed
		// view state due to this call (ie. if all parents are visible)

		if (!vis_dev)
			return;

		OpWidget* next_parent = parent;

		while (next_parent)
		{
			if (!next_parent->packed.is_visible)
			{
				return;
			}

			next_parent = next_parent->parent;
		}

		// all parents are visible, making this item really change Show state

		GenerateOnShow(visible);

		// temp kludge here.. if parent is a splitter, relayout it.
		// Proper thing is to let splitter be a listener, but then
		// we need to support the notion of multiple listeners on a widget first

#ifdef QUICK
		if (parent && parent->GetType() == WIDGET_TYPE_SPLITTER)
		{
			parent->Relayout();
		}
#endif
	}
}

void OpWidget::SetOnMoveWanted(BOOL onmove_wanted)
{
	packed.wants_onmove = onmove_wanted;
	
	if (onmove_wanted && GetParent())
	{
		GetParent()->SetOnMoveWanted(TRUE);
	}
}

UINT32 OpWidget::GetForegroundColor(UINT32 def_color, INT32 state)
{
	if (!m_color.use_default_foreground_color)
		return m_color.foreground_color;

	// foreground color is text color and check if widget or any
	// parent has a specific text color

#ifdef SKIN_SUPPORT
	UINT32 color;

	if (GetSkinManager()->GetTextColor(GetBorderSkin()->GetImage(), &color, state, GetBorderSkin()->GetType(), GetBorderSkin()->GetSize(), GetBorderSkin()->IsForeground()) == OpStatus::OK)
	{
		return color;
	}
#endif

	if (parent)
	{
		return parent->GetForegroundColor(def_color, state);
	}

	return def_color;
}

UINT32 OpWidget::GetBackgroundColor(UINT32 def_color)
{
	return m_color.use_default_background_color ? def_color : m_color.background_color;
}

void OpWidget::SetForegroundColor(UINT32 color)
{
	if (GetAllowStyling())
	{
		// Set this on all children also. Needed for composite form
		// widgets like the file upload widget and WebForms2 widgets.
		OpWidget* child = (OpWidget*) childs.First();
		while(child)
		{
			child->SetForegroundColor(color);
			child = (OpWidget*) child->Suc();
		}

		m_color.foreground_color = color;
		m_color.use_default_foreground_color = FALSE;
	}
}

void OpWidget::SetBackgroundColor(UINT32 color)
{
	if (GetAllowStyling())
	{
		m_color.background_color = color;
		m_color.use_default_background_color = FALSE;
	}
}

void OpWidget::UnsetForegroundColor()
{
	OpWidget* child = (OpWidget*) childs.First();
	while(child)
	{
		child->UnsetForegroundColor();
		child = (OpWidget*) child->Suc();
	}

	m_color.foreground_color = OP_RGB(0, 0, 0);
	m_color.use_default_foreground_color = TRUE;
}

void OpWidget::UnsetBackgroundColor()
{
	OpWidget* child = (OpWidget*) childs.First();
	while(child)
	{
		child->UnsetBackgroundColor();
		child = (OpWidget*) child->Suc();
	}

	m_color.background_color = OP_RGB(0xAA, 0xAA, 0xAA);
	m_color.use_default_background_color = TRUE;
}

void OpWidget::SetPainterManager(OpWidgetPainterManager* painter_manager)
{
	OpWidget* child = (OpWidget*) childs.First();
	while(child)
	{
		child->SetPainterManager(painter_manager);
		child = (OpWidget*) child->Suc();
	}
	this->painter_manager = painter_manager;
}

BOOL OpWidget::SetJustify(JUSTIFY justify, BOOL changed_by_user)
{
	if (packed.justify_changed_by_user && !changed_by_user)
		return FALSE;

	packed.justify_changed_by_user = changed_by_user;

	BOOL justify_changed = this->font_info.justify != justify;
	if (justify_changed)
	{
		this->font_info.justify = justify;
		OnJustifyChanged();
		InvalidateAll();
	}
	return justify_changed;
}

void OpWidget::SetVerticalAlign(WIDGET_V_ALIGN align)
{
	if ((WIDGET_V_ALIGN) packed.v_align != align)
	{
		packed.v_align = align;
		InvalidateAll();
	}
}

void OpWidget::UpdateSystemFont(BOOL force_system_font)
{
	if (packed.custom_font && !force_system_font)
		return;

	FontAtt font;
#if defined(_QUICK_UI_FONT_SUPPORT_)
	g_op_ui_info->GetFont(GetSystemFont(),font,FALSE);
	this->font_info.font_info = styleManager->GetFontInfo(font.GetFontNumber());

	if (this->font_info.font_info == NULL)
#endif
	{
		g_op_ui_info->GetUICSSFont(CSS_VALUE_message_box, font);
		this->font_info.font_info = styleManager->GetFontInfo(font.GetFontNumber());

		if (this->font_info.font_info == NULL)
		{
			this->font_info.font_info = styleManager->GetFontInfo(0);
			
			OP_ASSERT(this->font_info.font_info); //What????? Should not happen!
		}
	}


	this->font_info.size = font.GetSize();
	this->font_info.italic = font.GetItalic();
	this->font_info.weight = font.GetWeight();
	this->font_info.char_spacing_extra = 0;
	packed.custom_font = FALSE;

#ifdef QUICK
	this->font_info.size = this->font_info.size * GetRelativeSystemFontSize() / 100;
	if (GetSystemFontWeight())
		this->font_info.weight = GetSystemFontWeight();
	OnFontChanged();
	Relayout();
#endif
}

BOOL OpWidget::SetFontInfo(const OpFontInfo* font_info, INT32 size, BOOL italic, INT32 weight,
						   JUSTIFY justify, int char_spacing_extra)
{
	BOOL child_changed = FALSE;
	OpWidget* child = (OpWidget*) childs.First();
	while(child)
	{
		child_changed |= child->SetFontInfo(font_info, size, italic, weight, justify, char_spacing_extra);
		child = (OpWidget*) child->Suc();
	}
	BOOL changed = FALSE;

	if (font_info)
	{
		changed |= this->font_info.font_info != font_info;
		this->font_info.font_info = font_info;
	}

	changed |= this->font_info.size != size;
	this->font_info.size = size;

	changed |= this->font_info.italic != italic;
	this->font_info.italic = italic;

	changed |= this->font_info.weight != weight;
	this->font_info.weight = weight;

	changed |= SetJustify(justify, FALSE);

	changed |= this->font_info.char_spacing_extra != char_spacing_extra;
	this->font_info.char_spacing_extra = char_spacing_extra;

	packed.custom_font = TRUE;

	if (changed)
	{
		OnFontChanged();
#ifdef QUICK
		Relayout();
#endif
	}
	return changed || child_changed;
}

BOOL OpWidget::SetFontInfo(const WIDGET_FONT_INFO& wfi)
{
	return SetFontInfo(wfi.font_info, wfi.size, wfi.italic, wfi.weight, wfi.justify, wfi.char_spacing_extra);
}

void OpWidget::SetProps(const HTMLayoutProperties& props)
{
	BOOL update = FALSE;

	if (props.text_indent != text_indent)
		update = TRUE;
	text_indent = props.text_indent;

	SetTextTransform(props.text_transform);

	SetPadding(props.padding_left, props.padding_top, props.padding_right, props.padding_bottom);

	m_script = props.current_script;

	m_overflow_wrap = props.overflow_wrap;

	m_border_left   = props.border.left.width;
	m_border_top    = props.border.top.width;
	m_border_right  = props.border.right.width;
	m_border_bottom = props.border.bottom.width;

	m_margin_left   = props.margin_left;
	m_margin_top    = props.margin_top;
	m_margin_right  = props.margin_right;
	m_margin_bottom = props.margin_bottom;

	packed.border_box = (props.box_sizing == CSS_VALUE_border_box);

	if (update)
		InvalidateAll();
}

void OpWidget::SetCursor(CursorType cursor)
{
	if (packed.is_special_form || (GetFormObject() && GetFormObject()->GetHTML_Element()->HasCursorSettings()))
		return;
#ifndef MOUSELESS
#ifndef LIBOPERA
	if (vis_dev && vis_dev->GetDocumentManager())
		vis_dev->GetDocumentManager()->GetWindow()->SetCursor(cursor);
	else if (m_widget_container)
		m_widget_container->GetWindow()->SetCursor(cursor);
#endif // LIBOPERA
#endif // MOUSELESS
}

#ifdef WIDGETS_CLONE_SUPPORT

BOOL IsClonableWidgetWithText(OpWidget *widget)
{
	switch (widget->GetType())
	{
	case OpWidget::WIDGET_TYPE_MULTILINE_EDIT:
	case OpWidget::WIDGET_TYPE_EDIT:
	case OpWidget::WIDGET_TYPE_RADIOBUTTON:
	case OpWidget::WIDGET_TYPE_CHECKBOX:
	case OpWidget::WIDGET_TYPE_BUTTON:
	case OpWidget::WIDGET_TYPE_FILECHOOSER_EDIT:
		return TRUE;
	};
	return FALSE;
}

OP_STATUS OpWidget::CloneProperties(OpWidget *source, INT32 font_size)
{
	SetRTL(source->GetRTL());
	// It's probably better to not clone CSS colors.
//	if (source->GetColor().use_default_foreground_color == FALSE)
//		SetForegroundColor(source->GetColor().foreground_color);
//	if (source->GetColor().use_default_background_color == FALSE)
//		SetBackgroundColor(source->GetColor().background_color);
	SetFontInfo(source->font_info.font_info,
				font_size ? font_size : source->font_info.size,
				source->font_info.italic,
				source->font_info.weight,
				source->font_info.justify,
				source->font_info.char_spacing_extra);
	SetVerticalAlign(source->GetVerticalAlign());

	SetValue(source->GetValue());
	SetColor(source->GetColor());

	// All widgets doesn't have text and those return failed always.
	if (IsClonableWidgetWithText(this))
	{
		OpString text;
		RETURN_IF_ERROR(source->GetText(text));
		RETURN_IF_ERROR(SetText(text));
	}

	INT32 start_ofs, stop_ofs;
	SELECTION_DIRECTION direction;
	source->GetSelection(start_ofs, stop_ofs, direction);
	SetSelection(start_ofs, stop_ofs, direction);
	INT32 caret_ofs = source->GetCaretOffset();
	SetCaretOffset(caret_ofs);

	return OpStatus::OK;
}

#endif // WIDGETS_CLONE_SUPPORT

void OpWidget::SetRTL(BOOL is_rtl)
{
	if (packed.is_rtl != (unsigned int)(is_rtl ? 1 : 0))
	{
		packed.is_rtl = is_rtl ? 1 : 0;
#ifdef SKIN_SUPPORT
		const INT32 skin_state = is_rtl ? SKINSTATE_RTL : 0;
		GetBorderSkin()->SetState(skin_state, SKINSTATE_RTL);
		GetForegroundSkin()->SetState(skin_state, SKINSTATE_RTL);
#endif // SKIN_SUPPORT
		OnDirectionChanged();
	}
}

BOOL OpWidget::LeftHandedUI()
{
	BOOL pos = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::LeftHandedUI);
#ifdef SUPPORT_TEXT_DIRECTION
	if (GetRTL() && g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::RTLFlipsUI))
		pos = !pos;
#endif
	return pos;
}

OpWindow* OpWidget::GetParentOpWindow()
{
	OpWindow* parentwindow = NULL;

	if (GetWidgetContainer())
	{
		parentwindow = GetWidgetContainer()->GetWindow();
	}
	else if ( GetVisualDevice() &&
		 GetVisualDevice()->GetDocumentManager() && 
		 GetVisualDevice()->GetDocumentManager()->GetWindow())
	{
		parentwindow = (OpWindow*) GetVisualDevice()->GetDocumentManager()->GetWindow()->GetOpWindow();
	}

	return parentwindow;
}

/******************************************************************************
**
**  Accessibility methods
**
******************************************************************************/

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

BOOL OpWidget::AccessibilityIsReady() const
{
	if (!packed.accessibility_is_ready || packed.accessibility_skip)
		return false;

	const OpWidget* w = this;
	while (w)
	{
		if (w->AccessibilityPrunes())
			return false;
		w = w->parent;
	}

	return true;
}

void OpWidget::DisableAccessibleItem()
{
	OpWidget* child = (OpWidget*) childs.First();
	while(child)
	{
		child->DisableAccessibleItem();
		child = (OpWidget*) child->Suc();
	}
	packed.accessibility_is_ready = FALSE;
}

OP_STATUS OpWidget::AccessibilityClicked()
{
	if (listener)
		listener->OnClick(this);
	return OpStatus::OK;
}

OP_STATUS OpWidget::AccessibilitySetValue(int value)
{
	SetValue(value);
	return OpStatus::OK;
}

OP_STATUS OpWidget::AccessibilitySetText(const uni_char* text)
{
	return SetText(text);
}

BOOL OpWidget::AccessibilityChangeSelection(Accessibility::SelectionSet flags, OpAccessibleItem* child)
{
	return FALSE;
}

BOOL OpWidget::AccessibilitySetFocus()
{
	SetFocus(FOCUS_REASON_OTHER);
	return FALSE;
}

Accessibility::State OpWidget::AccessibilityGetState()
{
	Accessibility::State state=Accessibility::kAccessibilityStateNone;
	if (!IsVisible())
		state |= Accessibility::kAccessibilityStateInvisible;
	if (!IsEnabled())
		state |= Accessibility::kAccessibilityStateDisabled;
	if (this == g_input_manager->GetKeyboardInputContext())
		state |= Accessibility::kAccessibilityStateFocused;
	if (m_acc_label)
		state |= Accessibility::kAccessibilityStateReliesOnLabel;
	return state;
}

OpAccessibleItem* OpWidget::GetAccessibleLabelForControl()
{
	if (!packed.checked_for_label)
	{
#ifdef NAMED_WIDGET
		if (GetParent() && !m_acc_label && GetType() != WIDGET_TYPE_LABEL)
		{
			OpString8 ctrl_name;
			ctrl_name.Set("label_for_");
			ctrl_name.Append(GetName());
			OpWidget* widget = GetWidgetByName(ctrl_name.CStr());
			if (widget && widget->GetType() == WIDGET_TYPE_LABEL)
			{
				((OpLabel*)widget)->AssociateLabelWithOpWidget(this);
			}
		}
#endif // NAMED_WIDGET
		packed.checked_for_label = TRUE;
	}
	if (m_acc_label)
		return m_acc_label->GetLabelExtension();
	return NULL;
}

int	OpWidget::GetAccessibleChildrenCount()
{
	int children=0;
	OpWidget* child = (OpWidget*) childs.First();

	OpAccessibleItem* child_parent;
	if (AccessibilitySkips())
		child_parent = GetAccessibleParent();
	else
		child_parent = this;

	while (child)
	{
		if (child->AccessibilityPrunes() || child->GetAccessibleParent() != child_parent)
			;
		else if (child->AccessibilitySkips())
			children += child->GetAccessibleChildrenCount();
		else
			children++;

		child = (OpWidget*) child->Suc();
	}

	return children;

}

OpAccessibleItem* OpWidget::GetAccessibleParent()
{
	if (m_accessible_parent)
		return m_accessible_parent;

	if (parent)
	{
		if (parent->AccessibilitySkips())
			return parent->GetAccessibleParent();
		else
			return parent;
	}

	OpInputContext* parent_input_context = GetParentInputContext();
	if (parent_input_context && parent_input_context->GetType() == OpTypedObject::VISUALDEVICE_TYPE)
		return ((VisualDevice*)parent_input_context)->GetAccessibleDocument();

	return NULL;
}

OpAccessibleItem* OpWidget::GetAccessibleChild(int childNr)
{
	int children=0;
	OpWidget* child = (OpWidget*) childs.First();

	OpAccessibleItem* child_parent;
	if (AccessibilitySkips())
		child_parent = GetAccessibleParent();
	else
		child_parent = this;

	while (child)
	{
		if (child->AccessibilityPrunes() || child->GetAccessibleParent() != child_parent)
			;
		else if (child->AccessibilitySkips())
		{
			int count = child->GetAccessibleChildrenCount();
			if (count <= childNr-children)
				children +=count;
			else
				return child->GetAccessibleChild(childNr-children);
		}
		else if (children == childNr)
			return child;
		else
			children++;

		child = (OpWidget*) child->Suc();
	}

	return NULL;
}

int OpWidget::GetAccessibleChildIndex(OpAccessibleItem* item)
{
	int children=0;
	OpWidget* child = (OpWidget*) childs.First();

	OpAccessibleItem* child_parent;
	if (AccessibilitySkips())
		child_parent = GetAccessibleParent();
	else
		child_parent = this;

	while (child)
	{
		if (child->AccessibilityPrunes() || child->GetAccessibleParent() != child_parent)
			;
		else if (child->AccessibilitySkips())
		{
			int index = child->GetAccessibleChildIndex(item);
			if (index != Accessibility::NoSuchChild)
				return children +index;
			children += child->GetAccessibleChildrenCount();
		}
		else if (child == item)
			return children;
		else
			children++;

		child = (OpWidget*) child->Suc();
	}

	return Accessibility::NoSuchChild;
}

OpAccessibleItem* OpWidget::GetAccessibleChildOrSelfAt(int x, int y)
{
	OpRect widget_rect = GetScreenRect();

	OpWidget* child = (OpWidget*) childs.First();

	OpAccessibleItem* child_parent;
	if (AccessibilitySkips())
		child_parent = GetAccessibleParent();
	else
		child_parent = this;

	while (child)
	{
		if (child->AccessibilityPrunes() || child->GetAccessibleParent() != child_parent)
			;
		else if (child->AccessibilitySkips())
		{
			OpAccessibleItem* child_at = child->GetAccessibleChildOrSelfAt(x, y);
			if (child_at && child_at != child)
				return child_at;
		}
		else
		{
			if (child->packed.is_visible && child->GetClickableRect().Contains(OpPoint(x-widget_rect.x,y-widget_rect.y)))
				return child;
		}

		child = (OpWidget*) child->Suc();
	}

	return AddNonclickableMargins(widget_rect).Contains(OpPoint(x,y)) && IsVisible() ? this : NULL;
}

OpAccessibleItem* OpWidget::GetNextAccessibleSibling()
{
	OpWidget* sibling = GetNextSibling();

	while (sibling)
	{
		if (sibling->AccessibilitySkips() && sibling->GetAccessibleChildrenCount())
		{
			OpAccessibleItem* sibling_child = sibling->GetAccessibleChild(0);
			if (sibling_child)
				return sibling_child;
		}
		if(sibling->AccessibilitySkips() || sibling->AccessibilityPrunes() || sibling->GetAccessibleParent() != GetAccessibleParent() )
		{
			sibling = (OpWidget*) sibling->Suc();
		}
		else
			break;
	}

	if (sibling == NULL)
	{
		OpAccessibleItem* parent = GetAccessibleParent();
		int index = parent->GetAccessibleChildIndex(this);
		if (index < parent->GetAccessibleChildrenCount() - 1)
			return parent->GetAccessibleChild(index + 1);
	}

	return sibling;
}

OpAccessibleItem* OpWidget::GetPreviousAccessibleSibling()
{
	OpWidget* sibling = GetPreviousSibling();

	while (sibling)
	{
		if (sibling->AccessibilitySkips() && sibling->GetAccessibleChildrenCount())
		{
			OpAccessibleItem* sibling_child = sibling->GetAccessibleChild(sibling->GetAccessibleChildrenCount()-1);
			if (sibling_child)
				return sibling_child;
		}
		if(sibling->AccessibilitySkips() || sibling->AccessibilityPrunes() || sibling->GetAccessibleParent() != GetAccessibleParent())
		{
			sibling = (OpWidget*) sibling->Pred();
		}
		else
			break;
	}

	if (sibling == NULL)
	{
		OpAccessibleItem* parent = GetAccessibleParent();
		int index = parent->GetAccessibleChildIndex(this);
		if (index > 0)
			return parent->GetAccessibleChild(index - 1);
	}

	return sibling;
}

OpAccessibleItem* OpWidget::GetAccessibleFocusedChildOrSelf()
{
	//Step 0: We get the currently focused input context
	OpInputContext* focused;
	focused = g_input_manager->GetKeyboardInputContext();
	OpAccessibleItem* acc_focused_child = NULL;

	//Step 1: We navigate up the tree from that context to find something that is an accessible item
	while (focused)
	{
		if (focused && focused->GetType() >= WIDGET_TYPE && focused->GetType() < WIDGET_TYPE_LAST)
		{
			acc_focused_child = (OpWidget*) focused;
			break;
		}
		if (focused && focused->GetType() == VISUALDEVICE_TYPE)
		{
			acc_focused_child = (VisualDevice*) focused;
			break;
		}
		focused = focused->GetParentInputContext();
	}

	if (!acc_focused_child)
		return NULL;
	if (acc_focused_child == (OpAccessibleItem*)this)
		return this;

	OpAccessibleItem* acc_focused_parent;

	OpAccessibleItem* child_parent;
	if (AccessibilitySkips())
		child_parent = GetAccessibleParent();
	else
		child_parent = this;

	//step 2: Now that we have an accessible item, we navigate up
	//        the accessibility tree (to respect reparented widgets, etc)
	//        until we find out if we are a parent of the focused item or not
	while ((acc_focused_parent = acc_focused_child->GetAccessibleParent()) != NULL && acc_focused_parent != child_parent)
		acc_focused_child = acc_focused_parent;

	if (!acc_focused_parent)
		return NULL;

	//Step 3: Now that we know we are the parent and we know which
	//        is the accessible child that reported us as a parent,
	//        check if it is one of our children.
	OpWidget* child = (OpWidget*) childs.First();

	while (child)
	{
		if (acc_focused_child == (OpAccessibleItem*)child)
			break;
		
		if (child->AccessibilitySkips())
		{
			OpAccessibleItem* child_at = child->GetAccessibleFocusedChildOrSelf();
			if (child_at && child_at != child)
				return child_at;
		}
		child = (OpWidget*) child->Suc();
	}

	//The subclass should know how to deal with it
	if (!child)
		return NULL;

	if (child->AccessibilityPrunes())
		return this;
	else
		return child;
}

OP_STATUS OpWidget::AccessibilityGetAbsolutePosition(OpRect &rect)
{
	rect = GetScreenRect();
	return OpStatus::OK;
}

/*these four methods need to be implemented by core with whatever they
plan for this kind of navigation*/

OpAccessibleItem* OpWidget::GetLeftAccessibleObject()
{
	return NULL;
}

OpAccessibleItem* OpWidget::GetRightAccessibleObject()
{
	return NULL;
}

OpAccessibleItem* OpWidget::GetUpAccessibleObject()
{
	return NULL;
}

OpAccessibleItem* OpWidget::GetDownAccessibleObject()
{
	return NULL;
}

OP_STATUS OpWidget::AccessibilityGetKeyboardShortcut(ShiftKeyState* shifts, uni_char* kbdShortcut)
{
	return OpStatus::ERR_NOT_SUPPORTED;
}

int OpWidget::AccessibilityGetValue(int &value)
{
	value = GetValue();
	return OpStatus::OK;
}

OP_STATUS OpWidget::AccessibilityGetText(OpString& str)
{
	return GetText(str);
}

OP_STATUS OpWidget::AccessibilityGetDescription(OpString& str)
{
	return str.Set(UNI_L(""));
}

OP_STATUS OpWidget::AccessibilityGetURL(OpString& str)
{
	return str.Set(UNI_L(""));
}


#endif //ACCESSIBILITY_EXTENSION_SUPPORT


// == Functions that generates hooks for the apropriate childobjects in this widget ==================

void OpWidget::SetPaintingPending()
{
	if (packed.is_painting_pending)
		return;

	packed.is_painting_pending = TRUE;

	OpWidget* child = (OpWidget*) childs.First();
	while(child)
	{
		child->SetPaintingPending();
		child = (OpWidget*) child->Suc();
	}
}

void OpWidget::GenerateOnBeforePaint()
{
	if (!IsVisible())
		return;

	packed.is_painting_pending = FALSE;

#if defined (QUICK)
	UpdateActionStateIfNeeded();
#endif

	OnBeforePaint();

	if (listener)
		listener->OnBeforePaint(this);

	OpWidget* child = (OpWidget*) childs.First();
	while(child)
	{
		child->GenerateOnBeforePaint();
		child = (OpWidget*) child->Suc();
	}
}

void OpWidget::CheckScale()
{
	if (vis_dev && vis_dev->GetScale() != m_vd_scale)
	{
		m_vd_scale = vis_dev->GetScale();
		OnScaleChanged();
	}
}

void OpWidget::GenerateOnPaint(const OpRect &paint_rect, BOOL force_ensure_skin)
{
	if (paint_rect.width == 0 || paint_rect.height == 0)
		return;

	// Fixes for PHOTON that gets draw events while window height and width are 1 pixel RFZ
	BOOL noclip =!(GetRect(FALSE).width && GetRect(FALSE).height);

	if (!packed.is_visible)
		return;

	if (IsForm() || (!GetParent() && GetType() == WIDGET_TYPE_SCROLLBAR))
	{
		// No OnBeforePaint are called on formobjects since they are not children of the widget tree.
		GenerateOnBeforePaint();
	}

	UINT8 opacity = GetOpacity();

	if (opacity == 0)
		return;

	if (parent)
	{
		vis_dev->Translate(GetRect().x, GetRect().y);
	}

#if defined(SKIN_SUPPORT) && defined(VEGA_OPPAINTER_SUPPORT)
	// This variable must be declared before any "goto nofont" to compile on some compilers.
	BOOL has_stencil = FALSE;
#endif

	OpWidget* child;

	painter_manager->InitPaint(vis_dev, this);

	if (font_info.font_info && !form_object)
	{
		// Update font for non-forms widgets
		vis_dev->SetFont(font_info.font_info->GetFontNumber());
		vis_dev->SetFontSize(font_info.size);
		vis_dev->SetFontWeight(font_info.weight);
		vis_dev->SetFontStyle(font_info.italic ? FONT_STYLE_ITALIC : FONT_STYLE_NORMAL);
		if (vis_dev->painter)
		{
			OpFont* font = (OpFont*)vis_dev->GetFont();
			if (!font)
				goto nofont;
			vis_dev->painter->SetFont(font);
		}
	}

	if (!noclip)
	{
#ifdef SKIN_SUPPORT
		FocusRectInMargins()?
			SetClipRect(GetBounds(TRUE)) :
#endif // SKIN_SUPPORT
			SetClipRect(GetBounds());
	}

	if (opacity != 255)
	{
		if (OpStatus::IsError(vis_dev->BeginOpacity(GetBounds(), opacity)))
			opacity = 255; ///< It failed, so we should not call EndOpacity
	}

#ifdef SKIN_SUPPORT
	if (packed.is_skinned && !g_widgetpaintermanager->NeedCssPainter(this))
	{
		EnsureSkin(paint_rect, FALSE, force_ensure_skin);
	}
#endif

#if defined(SKIN_SUPPORT) && defined(VEGA_OPPAINTER_SUPPORT)
	if (GetSkinned() && !paint_rect.IsEmpty())
	{
		// Set stencil with radius if the skin has it.

		OpSkinElement* elm = GetBorderSkin()->GetSkinElement();

		INT32 state = this == OpWidget::GetFocused() ? SKINSTATE_FOCUSED : 0;
		if (GetRTL())
			state |= SKINSTATE_RTL;

		OpRect radius_rect = GetBounds();
		// We can't use AddPadding here. AddPadding is overridden by Quick UI to do weird things with padding.
		// We want the radius to use the actual skin padding.
		// AddPadding(radius_rect);
		radius_rect.x += m_padding_left;
		radius_rect.y += m_padding_top;
		radius_rect.width -= m_padding_left + m_padding_right;
		radius_rect.height -= m_padding_top + m_padding_bottom;

		UINT8 radius[4];
		if (!radius_rect.IsEmpty() &&
			elm && OpStatus::IsSuccess(elm->GetRadius(radius, state)) &&
			(radius[0] || radius[1] || radius[2] || radius[3]))
			has_stencil = OpStatus::IsSuccess(vis_dev->BeginStencil(paint_rect));

		if (has_stencil)
		{
			Border border;
			border.Reset();
			border.top.radius_start = border.left.radius_start = radius[0];
			border.top.radius_end = border.right.radius_start = radius[1];
			border.right.radius_end = border.bottom.radius_end = radius[2];
			border.left.radius_end = border.bottom.radius_start = radius[3];

			vis_dev->BeginModifyingStencil();
			vis_dev->SetBgColor(OP_RGB(0, 0, 0));
			vis_dev->DrawBgColorWithRadius(radius_rect, &border);
			vis_dev->EndModifyingStencil();
		}
	}
#endif // SKIN_SUPPORT && VEGA_OPPAINTER_SUPPORT

		CheckScale();

	OnPaint(painter_manager, paint_rect);

	if (listener)
		listener->OnPaint(this, paint_rect);

	//debug
	//vis_dev->SetColor(255, 0, 255, 100);
	//vis_dev->DrawRect(GetBounds());

	child = (OpWidget*) childs.Last();

	while (child)
	{
		OpRect child_rect = child->GetRect();

		if (child_rect.Intersecting(paint_rect)) // Check if we need to paint the child at all
		{
			OpPoint offset = OpPoint(-child_rect.x, -child_rect.y);

			child_rect.IntersectWith(paint_rect);
			child_rect.OffsetBy(offset);

			child->vis_dev = vis_dev;
			if (child_rect.width > 0 && child_rect.height > 0)
				child->GenerateOnPaint(child_rect);
		}

		child = (OpWidget*) child->Pred();
	}

	OnPaintAfterChildren(painter_manager, paint_rect);

#if defined(SKIN_SUPPORT) && defined(VEGA_OPPAINTER_SUPPORT)
	if (has_stencil)
		vis_dev->EndStencil();
#endif // SKIN_SUPPORT && VEGA_OPPAINTER_SUPPORT

#ifdef SKIN_SUPPORT
	if (packed.is_skinned && !g_widgetpaintermanager->NeedCssPainter(this))
		EnsureSkinOverlay(paint_rect);
#endif

	if (opacity != 255)
	{
		vis_dev->EndOpacity();
	}

	if (!noclip)
	{
		RemoveClipRect();
	}

	//debug clickable rect
	//if (this == g_widget_globals->hover_widget)
	//{
	//	vis_dev->SetColor(255, 0, 255, 100);
	//	vis_dev->DrawRect(GetBounds());
	//	vis_dev->SetColor(255, 205, 0, 100);
	//	vis_dev->DrawRect(AddNonclickableMargins(GetBounds()));
	//}

nofont:

	if (parent)
	{
		vis_dev->Translate(-GetRect().x, -GetRect().y);
	}
}

BOOL OpWidget::GenerateOnScrollAction(INT32 delta, BOOL vertical, BOOL smooth)
{
	if (!packed.is_visible)
		return FALSE;
	return OnScrollAction(delta, vertical, smooth);
}

#ifndef MOUSELESS

void OpWidget::GenerateOnSetCursor(const OpPoint &cpoint)
{
# ifdef DRAG_SUPPORT
	if (g_drag_manager->IsDragging() && !g_drag_manager->IsBlocked())
	{
		OpWidget::OnSetCursor(cpoint);
		return;
	}
# endif // DRAG_SUPPORT

	if (g_widget_globals->hover_widget && packed.is_visible)
	{
		OpPoint point = cpoint;
		for (OpWidget* run = g_widget_globals->hover_widget; run->parent; run = run->parent)
		{
			point.x -= run->rect.x;
			point.y -= run->rect.y;
		}
		g_widget_globals->hover_widget->OnSetCursor(point);
	}
}

void OpWidget::OnSetCursor(const OpPoint &point)
{
#ifdef WIDGETS_DRAGNDROP_TEXT
	if (g_drag_manager->IsDragging() && !g_drag_manager->IsBlocked())
	{
		OpDragObject* drag_object = g_drag_manager->GetDragObject();
		DropType drop = drag_object->GetDropType();
		drag_object->SetVisualDropType(drop);
		switch (drop)
		{
			case DROP_NONE:
				SetCursor(CURSOR_NO_DROP);
				break;
			case DROP_LINK:
				SetCursor(CURSOR_DROP_LINK);
				break;
			case DROP_COPY:
				SetCursor(CURSOR_DROP_COPY);
				break;
			case DROP_MOVE:
				SetCursor(CURSOR_DROP_MOVE);
				break;
			default:
				SetCursor(CURSOR_DEFAULT_ARROW);
		}
	}
	else
#endif // WIDGETS_DRAGNDROP_TEXT
		SetCursor(CURSOR_DEFAULT_ARROW);
}

#if defined(WIDGETS_IME_SUPPORT) && defined(WIDGETS_IME_SUPPRESS_ON_MOUSE_DOWN)
void OpWidget::TrySuppressIMEOnMouseDown(const OpPoint &point)
{
	OpWidget* child = (OpWidget*) childs.First();
	while(child)
	{
		OpRect crect = child->GetRect();
		if (child->packed.is_visible && !child->packed.ignore_mouse && child->GetClickableRect().Contains(point))
		{
			OpPoint translated_point = OpPoint(point.x - crect.x, point.y - crect.y);
			child->TrySuppressIMEOnMouseDown(translated_point);
			return;
		}
		child = (OpWidget*) child->Suc();
	}

	if (IsEnabled())
	{
		SuppressIMEOnMouseDown();
	}
}
void OpWidget::SuppressIMEOnMouseDown()
{
	m_suppress_ime_mouse_down = TRUE;
	m_suppressed_ime_reason = FOCUS_REASON_MOUSE;
}
#endif

void OpWidget::GenerateOnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
#ifdef NEARBY_ELEMENT_DETECTION
	if (ElementExpander::IsEnabled() && IsForm())
	{
		OpWidget *form_obj_widget = GetFormObject() ? GetFormObject()->GetWidget() : NULL;
		if (form_obj_widget && form_obj_widget->IsCloneable())
			// Don't let clonable formswidgets get focus, mouse input etc. when nearby element detection will handle that.
			return;
	}
#endif
	OpWidget* child = (OpWidget*) childs.First();
	while(child)
	{
		OpRect crect = child->GetRect();
		if (child->packed.is_visible && !child->packed.ignore_mouse && child->GetClickableRect().Contains(point))
		{
			OpPoint translated_point = OpPoint(point.x - crect.x, point.y - crect.y);
			child->GenerateOnMouseDown(translated_point, button, nclicks);
			return;
		}
		child = (OpWidget*) child->Suc();
	}

	g_input_manager->SetMouseInputContext(this);
	g_widget_globals->captured_widget = this;
	g_widget_globals->start_point = point;

#ifdef PAN_SUPPORT
	// let the panning code decide if it wants to allow panning on this widget
	if (vis_dev)
		vis_dev->PanHookedWidget();
#endif // PAN_SUPPORT

	if (IsEnabled())
	{
		m_button_mask |= 1 << button;

		OnMouseDown(point, button, nclicks);
	}
	else if (listener)
	{
		listener->OnMouseEvent(this, -1, point.x, point.y, button, TRUE, nclicks);
	}
}

void OpWidget::GenerateOnMouseMove(const OpPoint &point)
{
	if (g_widget_globals->captured_widget)
	{
		g_input_manager->SetMouseInputContext(g_widget_globals->captured_widget);

		OpPoint translated_point = OpPoint(point.x, point.y);

		for (OpWidget* run = g_widget_globals->captured_widget; run->parent; run = run->parent)
		{
			translated_point.x -= run->rect.x;
			translated_point.y -= run->rect.y;
		}

		if (g_widget_globals->captured_widget->IsEnabled())
		{
			if (g_widget_globals->captured_widget->listener)
				g_widget_globals->captured_widget->listener->OnMouseMove(g_widget_globals->captured_widget, translated_point);

			g_widget_globals->captured_widget->OnMouseMove(translated_point);
			g_widget_globals->last_mousemove_point = translated_point;
		}
	}
	else
	{
		OpWidget* child = (OpWidget*) childs.First();
		while(child)
		{
			OpRect crect = child->GetRect();
			if (child->packed.is_visible && !child->packed.ignore_mouse && child->GetClickableRect().Contains(point))
			{
				OpPoint translated_point = OpPoint(point.x - crect.x, point.y - crect.y);
				child->GenerateOnMouseMove(translated_point);
				return;
			}
			child = (OpWidget*) child->Suc();
		}

		g_input_manager->SetMouseInputContext(this);

		if (packed.is_enabled || packed.is_always_hoverable)
		{
			if (g_widget_globals->hover_widget != this)
			{
				if (g_widget_globals->hover_widget)
				{
					if (g_widget_globals->hover_widget->listener)
						g_widget_globals->hover_widget->listener->OnMouseLeave(g_widget_globals->hover_widget);
					g_widget_globals->hover_widget->OnMouseLeave();
				}
				g_widget_globals->hover_widget = this;
#ifdef DRAG_SUPPORT
				if (g_drag_manager->IsDragging() && !g_drag_manager->IsBlocked())
					OpWidget::OnSetCursor(point);
				else
#endif // DRAG_SUPPORT
					OnSetCursor(point);
			}

			if (listener)
				listener->OnMouseMove(this, point);

			OnMouseMove(point);
			g_widget_globals->last_mousemove_point = point;
		}
		else
		{
			if (g_widget_globals->hover_widget)
			{
				if (g_widget_globals->hover_widget->listener)
					g_widget_globals->hover_widget->listener->OnMouseLeave(g_widget_globals->hover_widget);
				g_widget_globals->hover_widget->OnMouseLeave();
				g_widget_globals->hover_widget = NULL;
			}

			SetCursor(CURSOR_DEFAULT_ARROW);
		}
	}
}

void OpWidget::GenerateOnMouseLeave()
{
	if (g_widget_globals->hover_widget)
	{
		if (g_widget_globals->hover_widget->listener)
			g_widget_globals->hover_widget->listener->OnMouseLeave(g_widget_globals->hover_widget);

		g_widget_globals->hover_widget->OnMouseLeave();
		g_widget_globals->hover_widget = NULL;
	}

	if (g_widget_globals->captured_widget)
	{
		if (g_widget_globals->captured_widget->IsEnabled())
		{
			if (g_widget_globals->captured_widget->listener)
				g_widget_globals->captured_widget->listener->OnMouseLeave(g_widget_globals->captured_widget);

			g_widget_globals->captured_widget->OnMouseLeave();
		}
		if (g_widget_globals->captured_widget) // Just because i'm not sure this won't ever happen.
			g_widget_globals->captured_widget->m_button_mask = 0;
		g_widget_globals->captured_widget = NULL;
	}
}

void OpWidget::GenerateOnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks /* = 1 */)
{
	OpPoint translated_point = point;
	OpWidget* target = this;
	LockDeletedWidgetsCleanup lock;

	if (g_widget_globals->captured_widget)
	{
		target = g_widget_globals->captured_widget;

		target->m_button_mask &= ~(1 << button);

		for (OpWidget* run = g_widget_globals->captured_widget; run->parent; run = run->parent)
		{
			translated_point.x -= run->rect.x;
			translated_point.y -= run->rect.y;
		}

		if (!target->m_button_mask)
			g_widget_globals->captured_widget = NULL;
	}
	else
	{
		return;

/* Design issue: Should widgets be able to receive OnMouseUp, when they never got OnMouseDown?
   I think it makes more sense to not let them have it then.. 
		OpWidget* child = (OpWidget*) childs.First();
		while(child)
		{
			OpRect crect = child->GetRect();
			if (child->packed.is_visible && child->GetClickableRect().Contains(point))
			{
				translated_point = OpPoint(point.x - crect.x, point.y - crect.y);
				child->GenerateOnMouseUp(translated_point, button, nclicks);
				return;
			}
			child = (OpWidget*) child->Suc();
		}*/
	}

	if (target->IsEnabled())
	{
		target->OnMouseUp(translated_point, button, nclicks);
	}
	else if (target->listener)
	{
		target->listener->OnMouseEvent(target, -1, translated_point.x, translated_point.y, button, FALSE, nclicks);
	}

	if (button == MOUSE_BUTTON_2)
	{
		for ( ; target; target = target->GetParent())
		{
			if (target->OnContextMenu(translated_point, NULL, FALSE))
			{
				break;
			}
			if (target->listener && target->listener->OnContextMenu(target, -1, translated_point, NULL, FALSE))
				break;

			translated_point.x += target->rect.x;
			translated_point.y += target->rect.y;
		}
	}
}

BOOL OpWidget::GenerateOnMouseWheel(INT32 delta,BOOL vertical)
{
	if (!packed.is_visible)
		return FALSE;

	if (OnMouseWheel(delta,vertical))
		return TRUE;

	if (parent)
		return parent->GenerateOnMouseWheel(delta, vertical);

	return FALSE;
}

void OpWidget::OnCaptureLost()
{
	// if widget looses focus, call OnMouseUp for all mouse buttons that has sent OnMouseDown but not OnMouseUp
	int b = MOUSE_BUTTON_1;
	if (listener)
		listener->OnMouseLeave(this);
	OnMouseLeave();
	while (m_button_mask)
	{
		if ((m_button_mask & 1) && IsEnabled())
			OnMouseUp(OpPoint(-1,-1), (MouseButton)b, 1);
		m_button_mask >>= 1;
		++b;
	}
}
#endif // !MOUSELESS

void OpWidget::SetPadding(short padding_left, short padding_top, short padding_right, short padding_bottom)
{
	m_padding_left = padding_left;
	m_padding_top = padding_top;
	m_padding_right = padding_right;
	m_padding_bottom = padding_bottom;
}

void OpWidget::AddPadding(OpRect& rect)
{
	INT32 left, top, right, bottom;
	GetPadding(&left, &top, &right, &bottom);
	rect.x += left;
	rect.y += top;
	rect.width -= left + right;
	rect.height -= top + bottom;
}

void OpWidget::GetPadding(INT32* left, INT32* top, INT32* right, INT32* bottom)
{
	*left = m_padding_left;
	*top = m_padding_top;
	*right = m_padding_right;
	*bottom = m_padding_bottom;
}

BOOL OpWidget::IsAtLastInternalTabStop(BOOL forward) const
{
	return GetNextInternalTabStop(forward) == NULL;
}

OpWidget* OpWidget::GetNextInternalTabStop(BOOL forward) const
{
	OpWidget* focused = GetFocused();

	OpWidget* child = const_cast<OpWidget*>(this);
	// Normally we don't automatically focus the next we see, but if nothing is yet focused we do.
	BOOL next_should_focus = (focused == NULL);
	while (child)
	{
		OpWidget* deeper_child = forward ? child->GetFirstChild() : child->GetLastChild();
		if (deeper_child)
		{
			child = deeper_child;
		}
		else
		{
			// A leaf
			if (child == focused)
			{
				next_should_focus = TRUE;
			}
			else if (next_should_focus)
			{
				if (child->IsTabStop() && child->IsEnabled())
				{
					return child;
				}
			}

			if (forward ? child->Suc() : child->Pred())
			{
				child = (OpWidget*)(forward ? child->Suc() : child->Pred());
			}
			else
			{
				while (child != parent && !(forward ? child->Suc() : child->Pred()))
				{
					child = child->GetParent();
					if (child == focused)
					{
						break;
					}
				}
				if (child == parent)
				{
					break;
				}

				child = forward ? child->GetNextSibling() : child->GetPreviousSibling();
			}
		}
	}

	OP_ASSERT(next_should_focus || focused == this); // Called when this hasn't focus!

	return NULL; 
}

OpWidget* OpWidget::GetFocusableInternalEdge(BOOL front) const
{
	// Iterate over the leaves in the widget tree. Would probably look better if it was recursive.
	OpWidget* child = const_cast<OpWidget*>(this);
	while (child)
	{
		OpWidget* deeper_child = front ? child->GetFirstChild() : child->GetLastChild();
		if (deeper_child)
		{
			child = deeper_child;
		}
		else
		{
			if (child->IsTabStop() && child->IsEnabled())
			{
				return child;
			}

			OpWidget* child_sibling = static_cast<OpWidget*>(front ? child->Suc() : child->Pred());
			if (child_sibling)
			{
				child = child_sibling;
			}
			else
			{
				while (child != parent && !child_sibling)
				{
					child = child->GetParent();
					if (child == this)
					{
						break;
					}
				}
				if (child == parent)
				{
					break;
				}

				child = front ? child->GetNextSibling() : child->GetPreviousSibling();
			}
		}
	}

	return NULL; 
}

void OpWidget::GenerateOnShow(BOOL show)
{
	if (!show && g_widget_globals->hover_widget == this)
	{
#ifndef MOUSELESS
		if (listener)
			listener->OnMouseLeave(this);

		OnMouseLeave();
#endif
		g_widget_globals->hover_widget = NULL;
	}

#ifdef ANIMATED_SKIN_SUPPORT
	if (GetBorderSkin())
		GetBorderSkin()->StartAnimation(show);
	if (GetForegroundSkin())
		GetForegroundSkin()->StartAnimation(show);
#endif // ANIMATED_SKIN_SUPPORT

	OnShow(show);

	packed.is_painting_pending = TRUE;

	Invalidate(GetBounds(), TRUE, TRUE);

	OpWidget* child = (OpWidget*) childs.First();

	while(child)
	{
		if (child->packed.is_visible)
		{
			child->GenerateOnShow(show);
		}
		child = (OpWidget*) child->Suc();
	}
}

/***********************************************************************************
**
**	GetFocused
**
***********************************************************************************/

/*static*/ OpWidget* OpWidget::GetFocused()
{
	OpInputContext* focused = g_input_manager->GetKeyboardInputContext();

	if (focused && focused->GetType() >= WIDGET_TYPE && focused->GetType() < WIDGET_TYPE_LAST)
	{
		return (OpWidget*) focused;
	}

	return NULL;
}

/***********************************************************************************
**
**	OnKeyboardInputGained
**
***********************************************************************************/

void OpWidget::OnKeyboardInputGained(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason)
{
	if (new_input_context == this)
	{
		if (reason == FOCUS_REASON_MOUSE ||
			reason == FOCUS_REASON_KEYBOARD ||
			reason == FOCUS_REASON_ACTION ||
			reason == FOCUS_REASON_SIMULATED)
			GenerateHighlightRectChanged(GetBounds());

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
		AccessibilitySendEvent(Accessibility::Event(Accessibility::kAccessibilityStateFocused));
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

#ifdef WIDGETS_IME_SUPPORT
		if(vis_dev && vis_dev->view)
		{
			vis_dev->view->GetOpView()->AbortInputMethodComposing();
		}
#endif // WIDGETS_IME_SUPPORT

		InvalidateAll();

#ifdef QUICK
		if (listener)
			listener->OnFocusChanged(this, reason);
#endif // QUICK
	}

	OpInputContext::OnKeyboardInputGained(new_input_context, old_input_context, reason);

	OpWidgetExternalListener *el = (OpWidgetExternalListener *) g_opera->widgets_module.external_listeners.First();
	while (el)
	{
		el->OnFocus(this, TRUE, reason);
		el = (OpWidgetExternalListener *) el->Suc();
	}
}

/***********************************************************************************
**
**	OnKeyboardInputLost
**
***********************************************************************************/

void OpWidget::OnKeyboardInputLost(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason)
{
	OpWidgetExternalListener *el = (OpWidgetExternalListener *) g_opera->widgets_module.external_listeners.First();
	while (el)
	{
		el->OnFocus(this, FALSE, reason);
		el = (OpWidgetExternalListener *) el->Suc();
	}

	if (old_input_context == this)
	{
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
		if (reason != FOCUS_REASON_RELEASE) {
			AccessibilitySendEvent(Accessibility::Event(Accessibility::kAccessibilityStateFocused));
		}
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
#ifdef WIDGETS_IME_SUPPORT
		if(vis_dev && vis_dev->view)
		{
			vis_dev->view->GetOpView()->AbortInputMethodComposing();
		}
#endif // WIDGETS_IME_SUPPORT

		InvalidateAll();

#ifdef QUICK
		if (listener)
			listener->OnFocusChanged(this, reason);
#endif // QUICK
	}

	OpInputContext::OnKeyboardInputLost(new_input_context, old_input_context, reason);
}

/***********************************************************************************
**
**	IsInputContextAvailable
**
***********************************************************************************/

BOOL OpWidget::IsInputContextAvailable(FOCUS_REASON reason)
{
	if (IsAllVisible() &&
			IsEnabled() &&
			vis_dev &&
			GetParentOpWindow() &&
#ifndef WIDGETS_TABS_AND_TOOLBAR_BUTTONS_ARE_TAB_STOP
			!IsDead() &&
#endif
			packed.is_added)
	{
		if (GetParentOpWindow()->GetStyle() != OpWindow::STYLE_POPUP)
		{
			return TRUE;
		}
		return CanHaveFocusInPopup();
	}
	return FALSE;
}

/***********************************************************************************
**
**	RestoreFocus
**
***********************************************************************************/

void OpWidget::RestoreFocus(FOCUS_REASON reason)
{
	g_input_manager->RestoreKeyboardInputContext(this, GetWidgetContainer() ? GetWidgetContainer()->GetNextFocusableWidget(this, TRUE) : NULL, reason);
}

/***********************************************************************************
**
**	GetOpacity
**
***********************************************************************************/

UINT8 OpWidget::GetOpacity() 
{
#ifdef QUICK
	return QuickOpWidgetBase::GetOpacity();
#else
	return 255; 
#endif
}

#ifdef QUICK

/***********************************************************************************
**
**	GetItemData
**
***********************************************************************************/

OP_STATUS OpWidget::GetItemData(ItemData* item_data)
{
	if (item_data->query_type != COLUMN_QUERY)
	{
		return OpStatus::OK;
	}

	item_data->column_query_data.column_text->Set(UNI_L("Widget"));

	return OpStatus::OK;
}
#endif // QUICK

OpWidgetInfo* OpWidget::GetInfo()
{
	return painter_manager->GetInfo(this);
}

#ifdef WIDGETS_IMS_SUPPORT
OP_STATUS OpWidget::StartIMS()
{
	OP_ASSERT(vis_dev != NULL);
	if (NULL == vis_dev)
		return OpStatus::ERR;

	if (!vis_dev->GetDocumentManager() || !vis_dev->GetDocumentManager()->GetWindow())
		return OpStatus::ERR_NOT_SUPPORTED;

	OP_ASSERT(!m_ims_object.IsIMSActive());
	if (m_ims_object.IsIMSActive())
		return OpStatus::ERR;

	// IMS is currently only supported for in-document widgets. 
	if (vis_dev->GetWindow() == NULL)
		return OpStatus::ERR_NOT_SUPPORTED;

	m_ims_object.SetListener(this);
	RETURN_IF_ERROR(SetIMSInfo(m_ims_object));

	return m_ims_object.StartIMS(vis_dev->GetDocumentManager()->GetWindow()->GetWindowCommander());
}

OP_STATUS OpWidget::UpdateIMS()
{
	if (vis_dev && m_ims_object.IsIMSActive())
	{
		RETURN_IF_ERROR(SetIMSInfo(m_ims_object));
		return m_ims_object.UpdateIMS(vis_dev->GetDocumentManager()->GetWindow()->GetWindowCommander());
	}
	else
	{
		return OpStatus::OK;
	}
}

void OpWidget::DestroyIMS()
{
	if (vis_dev && m_ims_object.IsIMSActive())
	{
		m_ims_object.DestroyIMS(vis_dev->GetDocumentManager()->GetWindow()->GetWindowCommander());
	}
}

OP_STATUS OpWidget::SetIMSInfo(OpIMSObject& ims_object)
{
	if (GetFormObject())
		GetFormObject()->UpdatePosition();

	AffinePos doc_pos = GetPosInRootDocument();
	OpRect rect = GetRect(TRUE);
	doc_pos.Apply(rect);

	ims_object.SetRect(rect);

	return OpStatus::OK;
}

void OpWidget::OnCommitIMS(OpIMSUpdateList* updates)
{
	DoCommitIMS(updates);
}

void OpWidget::OnCancelIMS()
{
	DoCancelIMS();
}
#endif // WIDGETS_IMS_SUPPORT

#ifdef NAMED_WIDGET

/***********************************************************************************
**
**  GetWidgetByName (NAMED_WIDGET API)
**
***********************************************************************************/
OpWidget* OpWidget::GetWidgetByName(const OpStringC8 & name)
{
	OpNamedWidgetsCollection *nwc = GetNamedWidgetsCollection();
	OP_ASSERT(nwc);
	return nwc ? nwc->GetWidgetByName(name) : 0;
}


/***********************************************************************************
**
**  GetWidgetByNameInSubtree (NAMED_WIDGET API)
**
***********************************************************************************/
OpWidget* OpWidget::GetWidgetByNameInSubtree(const OpStringC8 & name)
{
	if (name.IsEmpty())
		return NULL;

	if (IsNamed(name))
	{
		return this;
	}

	OpWidget* child = (OpWidget*) childs.First();

	while(child)
	{
		OpWidget* hit = child->GetWidgetByNameInSubtree(name);

		if (hit)
		{
			return hit;
		}

		child = (OpWidget*) child->Suc();
	}

	return NULL;
}

/***********************************************************************************
**
**  SetName (NAMED_WIDGET API)
**
***********************************************************************************/

void OpWidget::SetName(const OpStringC8 & name)
{
	// Note that if a widget is in a container the container
	// is responsible for changing the name.

	OpNamedWidgetsCollection *collection = parent ? parent->GetNamedWidgetsCollection() : NULL;

	if(collection)
		collection->ChangeName(this, name);
	else
		m_name.Set(name);

	OnNameSet();
}

#endif // NAMED_WIDGET


void OpWidget::SetAlwaysHoverable(BOOL val)
{
	packed.is_always_hoverable = val;
	OpWidget *child = GetFirstChild();
	while (child)
	{
		child->SetAlwaysHoverable(val);
		child = child->GetNextSibling();
	}
}

