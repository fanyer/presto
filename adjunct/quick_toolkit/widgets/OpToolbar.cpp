/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/OpToolbar.h"

#include "modules/formats/uri_escape.h"
#include "modules/display/vis_dev.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/pi/OpDragManager.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/prefs/prefsmanager/prefstypes.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/prefsfile/prefsentry.h"
#include "modules/prefsfile/prefssection.h"
#include "modules/prefs/prefsmanager/prefstypes.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/skin/OpSkinElement.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/util/OpLineParser.h"
#include "modules/widgets/OpEdit.h"
#include "modules/dragdrop/dragdrop_manager.h"

#include "adjunct/quick/controller/factories/ButtonFactory.h"
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
#include "adjunct/desktop_util/search/searchenginemanager.h"
#endif // DESKTOP_UTIL_SEARCH_ENGINES
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/m2_ui/widgets/MailSearchField.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick/controller/AddActionButtonController.h"
#include "adjunct/quick/dialogs/CustomizeToolbarDialog.h"
#include "adjunct/quick/managers/KioskManager.h"
#include "adjunct/quick/managers/FeatureManager.h"
#include "adjunct/quick/managers/AnimationManager.h"
#include "adjunct/quick/managers/DesktopExtensionsManager.h"
#include "adjunct/quick/managers/FeatureManager.h"
#include "adjunct/quick/managers/KioskManager.h"
#include "adjunct/quick/managers/opsetupmanager.h"
#include "adjunct/quick/widgets/OpAccountDropdown.h"
#include "adjunct/quick/widgets/OpAddressDropDown.h"
#include "adjunct/quick_toolkit/widgets/OpGroup.h"
#include "adjunct/quick/widgets/OpIdentifyField.h"
#include "adjunct/quick/widgets/OpMinimizedUpdateDialogButton.h"
#include "adjunct/quick/widgets/OpProgressField.h"
#include "adjunct/quick_toolkit/widgets/OpRichTextLabel.h"
#include "adjunct/quick_toolkit/widgets/OpQuickFind.h"
#include "adjunct/quick/widgets/OpSearchDropDown.h"
#include "adjunct/quick/widgets/OpResizeSearchDropDown.h"
#include "adjunct/quick/widgets/OpSearchEdit.h"
#include "adjunct/quick_toolkit/widgets/OpStateButton.h"
#include "adjunct/quick/widgets/OpStatusField.h"
#include "adjunct/quick/widgets/OpZoomDropDown.h"
#include "adjunct/quick/widgets/OpToolbarMenuButton.h"
#include "adjunct/quick_toolkit/widgets/OpIcon.h"
#include "adjunct/quick/widgets/OpNewPageButton.h"
#include "adjunct/quick/widgets/PagebarButton.h"
#include "adjunct/quick/widgets/OpExtensionSet.h"
#include "adjunct/quick/widgets/OpExtensionButton.h"
#include "adjunct/quick_toolkit/widgets/OpBrowserView.h"
#include "adjunct/quick/widgets/OpTrashBinButton.h"
#include "adjunct/quick_toolkit/widgets/OpBrowserView.h"
#include "adjunct/quick/usagereport/UsageReport.h"
#include "adjunct/quick_toolkit/widgets/OpProgressbar.h"

#if defined(MSWIN)
# include "platforms/windows/win_handy.h"
#endif

#ifndef MAX_ACTION_LENGTH
#define MAX_ACTION_LENGTH (100 * 1024)
#endif // MAX_ACTION_LENGTH

#ifndef EXTENDER_HEIGHT
#define EXTENDER_HEIGHT 15
#endif //EXTENDER_HEIGHT

#ifndef EXTENDER_WIDTH
#define EXTENDER_WIDTH 15
#endif //EXTENDER_WIDTH

// == OpShrinkAnimationWidget - Used in toolbar for shrinking space (when widgets are removed) ===========

#ifdef VEGA_OPPAINTER_SUPPORT
class OpShrinkAnimationWidget : public OpButton, public QuickAnimationWidget
{
public:
	OpShrinkAnimationWidget(INT32 start_width, INT32 start_height, bool horizontal, OpToolbar *toolbar)
		: QuickAnimationWidget(this, ANIM_MOVE_SHRINK, horizontal)
		, m_start_width(start_width)
		, m_start_height(start_height)
		, m_toolbar(toolbar)
	{
		SetDead(TRUE);
		g_animation_manager->startAnimation(this, ANIM_CURVE_SLOW_DOWN, -1, TRUE);
	}
	virtual void OnRemoving()
	{
		m_toolbar = NULL;
	}
	virtual ~OpShrinkAnimationWidget()
	{
		// This widget should only be deleted when its own animation is complete
		// or as a result of the toolbar deleting it (f.ex on toolbar destruction).
		OP_ASSERT(!m_toolbar);
	}
	void GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
	{
		*w = m_start_width;
		*h = m_start_height;
	}
	void OnAnimationComplete()
	{
		if (!GetParent() || !m_toolbar)
			return;
		QuickDisableAnimationsHere dummy; ///< Avoid recursion.
		int index = m_toolbar->FindWidget(this);
		m_toolbar->RemoveWidget(index);
	}
	virtual BOOL IsOfType(Type type) { return type == WIDGET_TYPE_SHRINK_ANIMATION || OpButton::IsOfType(type); }
	INT32 m_start_width, m_start_height;
	OpToolbar *m_toolbar;
};
#endif // VEGA_OPPAINTER_SUPPORT

void OpToolbar::OnAnimationStart()
{
	OnAnimationUpdate(0);
}

void OpToolbar::OnAnimationUpdate(double position)
{
	m_animation_progress = position;

	if (GetParent())
	{
		// Make sure we don't call Relayout if we really don't change width/height.
		// OnAnimationUpdate will probably be called a lot more often than the integer changes.

		// We allow OnLayout() to lay out the widgets on the frist animation step. This is important
		// should one or more of the widgets be a multiline edit widget. The widget's width must be
		// set so that it can report the required height with the current string. OpStatusField can be
		// configured to multiline mode (DSK-347641)
		INT32 used_width, used_height;
		OnLayout(position > 0.0, GetParent()->GetWidth(), GetParent()->GetHeight(), used_width, used_height);

		if ((GetAlignment() == OpBar::ALIGNMENT_TOP || GetAlignment() == OpBar::ALIGNMENT_BOTTOM) && used_height != GetBounds().height)
			GetParent()->Relayout();
		else if ((GetAlignment() == OpBar::ALIGNMENT_LEFT || GetAlignment() == OpBar::ALIGNMENT_RIGHT) && used_width != GetBounds().width)
			GetParent()->Relayout();
	}
}

void OpToolbar::OnAnimationComplete()
{
	if (!m_animation_in)
		SetAlignment(ALIGNMENT_OFF);
	m_animation_size = 0;
}

BOOL OpToolbar::SetAlignmentAnimated(Alignment alignment, double animation_duration)
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if (!g_animation_manager->GetEnabled())
		return SetAlignment(alignment);
	if (alignment == ALIGNMENT_OFF)
	{
		if (GetAlignment() == ALIGNMENT_OFF)
			return FALSE;
		if (IsAnimating())
		{
			m_animation_in = FALSE;
			g_animation_manager->rescheduleAnimationFlip(this, animation_duration);
			return TRUE;
		}
		m_animation_in = FALSE;
		g_animation_manager->startAnimation(this, ANIM_CURVE_LINEAR, animation_duration, TRUE);
	}
	else
	{
		if (!SetAlignment(alignment))
		{
			if (IsAnimating())
			{
				m_animation_in = TRUE;
				g_animation_manager->rescheduleAnimationFlip(this, animation_duration);
				return TRUE;
			}
			return FALSE;
		}
		m_animation_in = TRUE;
		g_animation_manager->startAnimation(this, ANIM_CURVE_LINEAR, animation_duration, TRUE);
	}
	return TRUE;
#else
	return SetAlignment(alignment);
#endif
}

/***********************************************************************************
**
**	OpToolbar
**
***********************************************************************************/

OP_STATUS OpToolbar::Construct(OpToolbar** obj, PrefsCollectionUI::integerpref prefs_setting, PrefsCollectionUI::integerpref autoalignment_prefs_setting)
{
	*obj = OP_NEW(OpToolbar, (prefs_setting, autoalignment_prefs_setting));
	if (!*obj)
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsError((*obj)->init_status))
	{
		OP_DELETE(*obj);
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

OpToolbar::OpToolbar(PrefsCollectionUI::integerpref prefs_setting, PrefsCollectionUI::integerpref autoalignment_prefs_setting)
  : OpBar(prefs_setting, autoalignment_prefs_setting),
	m_selected(-1),
	m_focused(-1),
	m_selector(FALSE),
	m_deselectable(FALSE),
	m_wrapping(WRAPPING_NEWLINE),
	m_centered(FALSE),
	m_fixed_max_width(0),
	m_fixed_min_width(0),
	m_fixed_height(FIXED_HEIGHT_NONE),
	m_shrink_to_fit(FALSE),
#ifdef QUICK_FISHEYE_SUPPORT
	m_fisheye_shrink(FALSE),
#endif // QUICK_FISHEYE_SUPPORT
	m_grow_to_fit(FALSE),
	m_mini_buttons(FALSE),
	m_mini_edit(TRUE),
	m_fill_to_fit(FALSE),
	m_locked(FALSE),
	m_auto_update_auto_alignment(TRUE),
	m_report_click_usage(FALSE),
	m_button_type(OpButton::TYPE_TOOLBAR),
	m_button_style(OpButton::STYLE_IMAGE_AND_TEXT_ON_RIGHT),
	m_skin_type(SKINTYPE_DEFAULT),
	m_skin_size(SKINSIZE_DEFAULT),
	m_drop_pos(-1),
	m_standard(TRUE),
	m_dead(FALSE),
	m_computed_column_count(1),
	m_extender_button(0),
	m_drag_tooltip(Str::NOT_A_STRING),
	m_dummy_button(NULL),
#ifdef WINDOW_MOVE_FROM_TOOLBAR
	m_left_mouse_down(FALSE),
	m_desktop_window(NULL),
#endif // WINDOW_MOVE_FROM_TOOLBAR
	m_auto_hide_timer(NULL),
	m_auto_hide_delay(0),
	m_content_listener(NULL),
	m_animation_progress(0),
	m_animation_in(FALSE),
	m_animation_size(0)
{
	GetBorderSkin()->SetImage("Toolbar Skin");
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	AccessibilityPrunesMe(ACCESSIBILITY_PRUNE_WHEN_INVISIBLE);
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
}

void OpToolbar::OnDeleted()
{
	UpdateTargetToolbar(FALSE);

	if(m_dummy_button)
	{
		m_dummy_button->Delete();
	}

	OpWidget::OnDeleted();
}
/***********************************************************************************
**
**  CreateButton - Create a toolbar button
**
***********************************************************************************/
OP_STATUS OpToolbar::CreateButton(Type type, OpButton **button)
{
	switch(type)
	{
		case WIDGET_TYPE_BUTTON:
			 return OpButton::Construct(button);

		case WIDGET_TYPE_RADIOBUTTON:
			{
				OpRadioButton* radiobutton;
				RETURN_IF_ERROR(OpRadioButton::Construct(&radiobutton));
				*button = radiobutton;
				return OpStatus::OK;
			}

		case WIDGET_TYPE_CHECKBOX:
			{
				OpCheckBox* checkbox;
				RETURN_IF_ERROR(OpCheckBox::Construct(&checkbox));
				*button = checkbox;
				return OpStatus::OK;
			}

		default:
			OP_ASSERT(FALSE);
	}

	return OpStatus::ERR;
}

BOOL OpToolbar::IsFocusedOrHovered()
{
	if (OpWidget::GetFocused() && (this == OpWidget::GetFocused() || IsParentInputContextOf(OpWidget::GetFocused())))
		return TRUE;
	if (OpWidget::over_widget && (this == OpWidget::over_widget || IsParentInputContextOf(OpWidget::over_widget)))
		return TRUE;
	return FALSE;
}

/***********************************************************************************
**
**	AddButton
**
***********************************************************************************/

OpButton* OpToolbar::AddButton(const uni_char* label, const char* image, OpInputAction* action, void* user_data, INT32 pos, BOOL double_action)
{
	return AddButton(label, image, action, user_data, pos, double_action, Str::NOT_A_STRING);
}

OpButton* OpToolbar::AddButton(const uni_char* label, const char* image, OpInputAction* action, void* user_data, INT32 pos, BOOL double_action, Str::LocaleString string_id)
{
	OpButton* button;
	if (OpStatus::IsError(CreateButton(WIDGET_TYPE_BUTTON, &button)))
	{
		OP_DELETE(action);
		return NULL;
	}

	AddButton(button, label, image, action, user_data, pos, double_action, string_id);

	return button;
}

void OpToolbar::AddButton(OpButton* button, const uni_char* label, const char* image, OpInputAction* action, void* user_data, INT32 pos, BOOL double_action)
{
	AddButton(button, label, image, action, user_data, pos, double_action, Str::NOT_A_STRING);
}

void OpToolbar::AddButton(OpButton* button, const uni_char* label, const char* image, OpInputAction* action, void* user_data, INT32 pos, BOOL double_action, Str::LocaleString string_id)
{
	button->SetUserData(user_data);
	button->SetText(label);
	button->SetStringID(Str::LocaleString(string_id));

	if (image)
	{
		button->GetForegroundSkin()->SetImage(image);
	}
	button->SetAction(action);
	button->SetListener(this);

	m_widgets.Insert(pos, button);

	AddChild(button, TRUE);

	if (m_selected != -1 && pos != -1 && m_selected >= pos)
	{
		m_selected++;
	}

	if (m_focused != -1 && pos != -1 && m_focused >= pos)
	{
		m_focused++;
	}

	if (m_dead)
	{
		button->SetDead(TRUE);
	}

	if (double_action)
	{
		button->SetDoubleAction();
	}

	SetWidgetMiniSize(button);

	UpdateWidget(button);
	UpdateAutoAlignment();

	Relayout();

#ifdef VEGA_OPPAINTER_SUPPORT
	if (g_animation_manager->GetEnabled() && !button->IsHidden() && GetType() == WIDGET_TYPE_PAGEBAR)
	{
		QuickAnimationWidget *animation = OP_NEW(QuickAnimationWidget, (button, ANIM_MOVE_GROW, !!IsHorizontal()));
		if (animation)
			g_animation_manager->startAnimation(animation, ANIM_CURVE_SLOW_DOWN, -1, TRUE);
	}
#endif
}

/***********************************************************************************
**
**	AddWidget
**
***********************************************************************************/

void OpToolbar::AddWidget(OpWidget* widget, INT32 pos)
{
	m_widgets.Insert(pos, widget);

	if (!widget->GetListener())
		widget->SetListener(this);

	AddChild(widget, TRUE);

	widget->SetSystemFont(OP_SYSTEM_FONT_UI_TOOLBAR);

	if (m_selected != -1 && pos != -1 && m_selected >= pos)
	{
		m_selected++;
	}

	if (m_focused != -1 && pos != -1 && m_focused >= pos)
	{
		m_focused++;
	}

	SetWidgetMiniSize(widget);

	UpdateWidget(widget);
	UpdateAutoAlignment();
	Relayout();
}

/***********************************************************************************
**
**	RemoveButton
**
***********************************************************************************/

OP_STATUS OpToolbar::RemoveWidget(INT32 index)
{
	BOOL send_change = FALSE;

	if (m_selected > index)
	{
		m_selected--;
	}
	else if (m_selected == index)
	{
		m_selected = -1;
		send_change = TRUE;
	}

	if (m_focused > index)
	{
		m_focused--;
	}
	else if (m_focused == index)
	{
		m_focused = -1;
	}

	OpWidget* widget = m_widgets.Remove(index);

	if (send_change && listener)
	{
		listener->OnChange(this, TRUE);
	}

#ifdef VEGA_OPPAINTER_SUPPORT
	if (g_animation_manager->GetEnabled() && !widget->IsHidden() && GetType() == WIDGET_TYPE_PAGEBAR)
	{
		// Create animated shrink widget
		OpShrinkAnimationWidget *s = OP_NEW(OpShrinkAnimationWidget, (widget->GetRect(FALSE).width, widget->GetRect(FALSE).height, !!IsHorizontal(), this));
		if (s)
		{
			// Copy styles from the button we are removing.
			OpString text;
			widget->GetText(text);
			s->SetText(text.CStr());
			s->SetEllipsis(widget->GetEllipsis());
			if (widget->GetType() == OpTypedObject::WIDGET_TYPE_BUTTON)
			{
				OpButton *btn = (OpButton *) widget;
				s->SetButtonTypeAndStyle(btn->GetButtonType(), btn->m_button_style);
				s->SetFixedTypeAndStyle(TRUE);
			}
			if (widget->IsOfType(WIDGET_TYPE_PAGEBAR_BUTTON))
			{
				// Fix for not getting the icon trom thumbnail tabs
				s->GetForegroundSkin()->SetWidgetImage(((PagebarButton*)widget)->GetIconSkin());
			}
			else
			{
				s->GetForegroundSkin()->SetWidgetImage(widget->GetForegroundSkin());
			}
			s->GetBorderSkin()->SetWidgetImage(widget->GetBorderSkin());
			AddWidget(s, index);
		}
	}
#endif // VEGA_OPPAINTER_SUPPORT

	UpdateAutoAlignment();
	Relayout();

	widget->Delete();

	return OpStatus::OK;
}

/***********************************************************************************
**
**	SetSelected
**
***********************************************************************************/

BOOL OpToolbar::SetSelected(INT32 index, BOOL invoke_listeners, BOOL changed_by_mouse)
{
	if (!m_selector)
		return FALSE;

	if (index >= GetWidgetCount())
	{
		index = GetWidgetCount() - 1;
	}
	else if (index < -1)
	{
		index = -1;
	}

	if (m_selected == index)
		return FALSE;

	if (m_selected >= 0 && m_selected < GetWidgetCount())
	{
		OpButton* last_button = (OpButton*) m_widgets.Get(m_selected);

		if (last_button)
		{
			last_button->SetValue(FALSE);
			last_button->SetDead(m_dead);
		}
	}

	if (index >= 0 && index < GetWidgetCount())
	{
		OpButton* button = (OpButton*) m_widgets.Get(index);

		if (button)
		{
			button->SetValue(TRUE);
		}
	}

	m_selected = index;
	m_focused = index;

	if (invoke_listeners && listener)
	{
		listener->OnChange(this, changed_by_mouse);
		if (m_selected != -1)
			listener->OnClick(this, m_widgets.Get(m_selected)->GetID());
	}

	Relayout();
	return TRUE;
}

/***********************************************************************************
**
**	FindWidgetByUserData
**
***********************************************************************************/

INT32 OpToolbar::FindWidgetByUserData(void* user_data)
{
	for (UINT32 i = 0; i < m_widgets.GetCount(); i++)
	{
		if (m_widgets.Get(i)->GetUserData() == user_data)
			return i;
	}

	return -1;
}

/***********************************************************************************
**
**	UpdateAllWidgets
**
***********************************************************************************/

void OpToolbar::UpdateAllWidgets()
{
	for (INT32 i = 0; i < GetWidgetCount(); i++)
	{
		UpdateWidget(m_widgets.Get(i));
	}

	Relayout();
}

/***********************************************************************************
**
**	UpdateWidget
**
***********************************************************************************/

void OpToolbar::UpdateWidget(OpWidget* widget)
{
	UpdateButton(widget);
	UpdateSkin(widget);

	// has to be after UpdateWidget() because setting the type changes the skin
	if(m_button_skin.HasContent() && 
	   (widget->GetType() == OpTypedObject::WIDGET_TYPE_BUTTON ||
	    widget->GetType() == OpTypedObject::WIDGET_TYPE_STATE_BUTTON ||
	    widget->GetType() == OpTypedObject::WIDGET_TYPE_ZOOM_MENU_BUTTON ||
	    widget->GetType() == OpTypedObject::WIDGET_TYPE_NEW_PAGE_BUTTON ||
	    widget->GetType() == OpTypedObject::WIDGET_TYPE_EXTENSION_BUTTON))
	{
		widget->GetBorderSkin()->SetImage(m_button_skin.CStr(), "Toolbar Button Skin");

		// buttons can have also icons with restricted size; we want different
		// skin element for them to match size of normal buttons
		// see documentation of OpWidgetImage::SetRestrictImageSize
		if (widget->GetForegroundSkin()->GetRestrictImageSize()
			// widgets with action GO_TO_PAGE are also restricted in size
			// but it's not visible here :(
			|| (widget->GetAction() && widget->GetAction()->GetAction() == OpInputAction::ACTION_GO_TO_PAGE))
		{
			OpString8 skin_restricted;
			skin_restricted.Set(m_button_skin);
			skin_restricted.Append(" Restricted");
			widget->GetBorderSkin()->SetImage(skin_restricted.CStr(), m_button_skin.CStr());
		}
	}
}

/***********************************************************************************
**
**	UpdateButton
**
***********************************************************************************/

void OpToolbar::UpdateButton(OpWidget* widget)
{
	if (!widget || 
		(widget->GetType() != WIDGET_TYPE_BUTTON && 
		widget->GetType() != WIDGET_TYPE_STATE_BUTTON &&
		widget->GetType() != WIDGET_TYPE_ZOOM_MENU_BUTTON &&
		widget->GetType() != WIDGET_TYPE_NEW_PAGE_BUTTON &&
		widget->GetType() != WIDGET_TYPE_TOOLBAR_MENU_BUTTON &&
		widget->GetType() != WIDGET_TYPE_EXTENSION_BUTTON && 
		widget->GetType() != WIDGET_TYPE_TRASH_BIN_BUTTON))
		return;

	OpButton* button = (OpButton*) widget;

	// The menu button needs to have fixed type, but can change its style
	if (button->GetType() == WIDGET_TYPE_TOOLBAR_MENU_BUTTON)
	{
		// Regular calls to SetButtonStyle changes the skin, so we need to call our own method so we can
		// reset the skin back afterwards
		static_cast<OpToolbarMenuButton*>(button)->UpdateButton(m_button_style);
	}
	else if (!button->HasFixedTypeAndStyle())
	{
		button->SetButtonType(m_button_type);
		button->SetButtonStyle(m_button_style);
	}

	if (!button->HasFixedMinMaxWidth())
	{
		button->SetMaxWidth(m_fixed_max_width);
		button->SetMinWidth(m_fixed_min_width);
	}
	button->SetUpdateNeededWhen(GetAutoAlignment() ? UPDATE_NEEDED_ALWAYS : UPDATE_NEEDED_WHEN_VISIBLE);
}

/***********************************************************************************
**
**	UpdateSkin
**
***********************************************************************************/

void OpToolbar::UpdateSkin(OpWidget* widget)
{
	if (!widget)
		return;

	widget->GetBorderSkin()->SetType(m_skin_type);
	widget->GetForegroundSkin()->SetType(m_skin_type);
	widget->GetBorderSkin()->SetSize(m_skin_size);
	widget->GetForegroundSkin()->SetSize(m_skin_size);
}

/***********************************************************************************
**
**	UpdateTargetToolbar
**
***********************************************************************************/

void OpToolbar::UpdateTargetToolbar(BOOL is_target_toolbar)
{
	CustomizeToolbarDialog* dialog = g_application->GetCustomizeToolbarsDialog();

	if (!dialog)
		return;

	if (is_target_toolbar)
	{
		dialog->SetTargetToolbar(this);
	}
	else if (dialog->GetTargetToolbar() == this)
	{
		dialog->SetTargetToolbar(NULL);
	}
}

/***********************************************************************************
**
**	IsTargetToolbar
**
***********************************************************************************/

BOOL OpToolbar::IsTargetToolbar()
{
	CustomizeToolbarDialog* dialog = g_application->GetCustomizeToolbarsDialog();

	if (!dialog)
		return FALSE;

	return dialog->GetTargetToolbar() == this;
}

/***********************************************************************************
**
**	SelectNext
**
***********************************************************************************/

void OpToolbar::SelectNext()
{
	INT32 index = m_selected + 1;

	if (index >= GetWidgetCount())
	{
		index = 0;
	}

	SetSelected(index, TRUE);
}

/***********************************************************************************
**
**	SelectPrevious
**
***********************************************************************************/

void OpToolbar::SelectPrevious()
{
	INT32 index = m_selected - 1;

	if (index < 0)
	{
		index = m_widgets.GetCount() - 1;
	}

	SetSelected(index, TRUE);
}

/***********************************************************************************
**
**	HasGrowable
**
***********************************************************************************/

BOOL OpToolbar::HasGrowable()
{
	INT32 widget_count = GetWidgetCount();

	// Loop all widgets currently in the toolbar
	for (int i = 0; i < widget_count; i++)
	{
		OpWidget* widget = m_widgets.Get(i);

#ifdef WIDGET_IS_HIDDEN_ATTRIB
		// Don't include any hidden widgets as you can't see them :P
		if (widget->IsHidden())
			continue;
#endif // WIDGET_IS_HIDDEN_ATTRIB

		//widget->Out();

		if (widget->GetGrowValue())
			return TRUE;
	}

	return FALSE;
}


/***********************************************************************************
**
**	OnLayout
**
***********************************************************************************/
void OpToolbar::OnLayout(BOOL compute_size_only, INT32 available_width, INT32 available_height, INT32& used_width, INT32& used_height)
{
	INT32 widget_count = GetWidgetCount();
	INT32 i;

	INT32 offset_x = 0;
	INT32 offset_y = 0;
	if (IsAnimating() && !compute_size_only && m_animation_size)
	{
		// If we don't want the animation to squeeze children, we will use the size unaffected by the animation as available size.
		// We will also offset the child widgets with the diff caused by the animation so they appear to slide in with the toolbar.
		if (GetAlignment() == OpBar::ALIGNMENT_TOP || GetAlignment() == OpBar::ALIGNMENT_BOTTOM)
		{
			offset_y = available_height - m_animation_size;
			available_height = m_animation_size;
		}
		else if (GetAlignment() == OpBar::ALIGNMENT_LEFT || GetAlignment() == OpBar::ALIGNMENT_RIGHT)
		{
			offset_x = available_width - m_animation_size;
			available_width = m_animation_size;
		}
	}

	BOOL available_height_is_infinite = (available_height < 0x40000000); /* happens when called from GetRequiredSize() */
	
	// get padding and spacing for this toolbar

	INT32 left_padding, top_padding, right_padding, bottom_padding, spacing;

	GetPadding(&left_padding, &top_padding, &right_padding, &bottom_padding);
	GetSpacing(&spacing);

	// remove right / bottom padding from available size

	available_width -= right_padding;
	available_height -= bottom_padding;

	// Happens on startup (UNIX).
	if( available_width <= 0 )
	{
		used_width = 0;
		used_height = 0;
		return;
	}

	INT32 row_height = 0;
	INT32 column_width = 0;
	INT32 available_width_modified = OnBeforeAvailableWidth(available_width);

	// compute column_width and row height

	if (HasFixedHeight() && m_fixed_height != FIXED_HEIGHT_STRETCH)
	{
		// if it's a fixed height toolbar, just use a dummy fixed size

		INT32 left_margin, top_margin, right_margin, bottom_margin;
		OP_STATUS status = OpStatus::OK;

		if(!m_dummy_button)
		{
			status = CreateButton(WIDGET_TYPE_BUTTON, &m_dummy_button);
			if (OpStatus::IsSuccess(status))
			{
				m_dummy_button->SetVisualDevice(vis_dev);
				m_dummy_button->SetText(UNI_L("Size to this dummy string"));
				m_dummy_button->GetForegroundSkin()->SetImage("Window Document Icon");
			}
		}
		if (OpStatus::IsSuccess(status))
		{
			UpdateWidget(m_dummy_button);

			m_dummy_button->GetRequiredSize(column_width, row_height);
			m_dummy_button->GetSkinMargins(&left_margin, &top_margin, &right_margin, &bottom_margin);

			column_width += left_margin + right_margin;
			row_height += top_margin + bottom_margin;
		}
	}
	else
	{
		for (i = 0; i < widget_count; i++)
		{
			OpWidget* widget = m_widgets.Get(i);
#ifdef WIDGET_IS_HIDDEN_ATTRIB
			// A hidden widget has no valid size
			if (widget->IsHidden())
				continue;
#endif // WIDGET_IS_HIDDEN_ATTRIB

			INT32 button_width = 0, button_height = 0;

			widget->Update();
			widget->SetAvailableParentSize(available_width, available_height);
			widget->GetRequiredSize(button_width, button_height);

			INT32 left_margin, top_margin, right_margin, bottom_margin;
			widget->GetSkinMargins(&left_margin, &top_margin, &right_margin, &bottom_margin);

			button_width += left_margin + right_margin;
			button_height += top_margin + bottom_margin;

			if (button_width > column_width)
			{
				column_width = button_width;
			}

			if (button_height > row_height)
			{
				row_height = button_height;
			}
		}
	}

	if (HasFixedHeight() && IsHorizontal())
	{
		if (m_wrapping != WRAPPING_NEWLINE && (!compute_size_only || (m_fixed_height == FIXED_HEIGHT_STRETCH && available_height_is_infinite)))
			row_height = available_height - top_padding;
		else if (m_fixed_height == FIXED_HEIGHT_STRETCH && available_height_is_infinite)
		{
			// count how many rows we'll need
			INT32 row_count = 1;
			INT32 width = left_padding;
			for (i = 0; i < widget_count; i++)
			{
				OpWidget* widget = m_widgets.Get(i);
				
				INT32 button_width = 0, button_height = 0;

				widget->SetAvailableParentSize(available_width, available_height);
				widget->GetRequiredSize(button_width, button_height);
				INT32 left_margin, top_margin, right_margin, bottom_margin;
				widget->GetSkinMargins(&left_margin, &top_margin, &right_margin, &bottom_margin);

				if (widget->GetType() == WIDGET_TYPE_SPACER && ((OpSpacer*)widget)->GetSpacerType() == OpSpacer::SPACER_WRAPPER)
				{
					row_count++;
					width = left_padding;
					continue;
				}
				else if (width + left_margin + button_width > available_width)
				{
					row_count++;
					width = left_padding;
				}
				width += left_margin + button_width + right_margin + spacing;
			}
			row_height = (available_height - top_padding) / row_count;
		}
	}

	// layout

	INT32 x = left_padding;
	INT32 y = top_padding;

	used_width = left_padding + (IsVertical() ? column_width : 0);
	used_height = top_padding + (IsHorizontal() ? row_height : 0);

	OpRect* positions;

	if (widget_count)
	{
		positions = OP_NEWA(OpRect, widget_count);
		if (!positions)
			return;
	}
	else
	{
		positions = NULL;
	}

	OpINT32Vector last_on_each_row;

	BOOL has_growable = FALSE;
	BOOL was_previous_wrapper = FALSE;

	m_computed_column_count = 1;
	BOOL toolbar_is_wrapping = FALSE;

	for (i = 0; i < widget_count; i++)
	{
		OpWidget* widget = m_widgets.Get(i);

		// Test for wrapping widget. If all widgets after it are invisible we trucate the list we want to layout
		BOOL is_wrapper = widget->GetType() == WIDGET_TYPE_SPACER && ((OpSpacer*)widget)->GetSpacerType() == OpSpacer::SPACER_WRAPPER;
		if (is_wrapper)
		{
			BOOL all_hidden = TRUE;
			for (INT32 j = i+1; j < widget_count && all_hidden; j++)
				all_hidden = m_widgets.Get(j)->IsHidden();
			if (all_hidden)
			{
				widget_count = i;
				break;
			}
		}

#ifdef WIDGET_IS_HIDDEN_ATTRIB
		// Don't include any hidden widgets in the size calulation. These will not be shown anyway
		if (widget->IsHidden())
			continue;
#endif // WIDGET_IS_HIDDEN_ATTRIB

		widget->Out();

		if (i == m_selected)
		{
			widget->IntoStart(&childs);
		}
		else
		{
			widget->Into(&childs);
		}

		if (widget->GetGrowValue())
		{
			has_growable = TRUE;
		}

		INT32 button_width = 0, button_height = 0;

		widget->SetAvailableParentSize(available_width, available_height);
		widget->GetRequiredSize(button_width, button_height);
		
		INT32 left_margin, top_margin, right_margin, bottom_margin;

		widget->GetSkinMargins(&left_margin, &top_margin, &right_margin, &bottom_margin);
#if 0 // Experimental test of creating a gaph on drag'n'drop
		if (i == m_drop_pos - 1)
			right_margin += 10;
		if (i == m_drop_pos)
			left_margin += 10;
#endif

		INT32 available_column_width = column_width - left_margin - right_margin;
		INT32 available_row_height = row_height - top_margin - bottom_margin;


		if (IsHorizontal())
		{
			if (HasFixedHeight())
			{
				button_height = available_row_height;
			}

			if (x != left_padding && ((m_wrapping == WRAPPING_NEWLINE && (x + button_width + left_margin /*+ right_margin*/ > available_width)) || was_previous_wrapper) )
			{
				last_on_each_row.Add(i - 1);
				x = left_padding;
				y = used_height;
				used_height += row_height;
				toolbar_is_wrapping = TRUE;
			}

			positions[i].Set(x + left_margin, y + top_margin + (available_row_height - button_height) / 2, button_width, button_height);

			x += button_width + left_margin;

			if (x > used_width)
			{
				used_width = x;
			}

			x += right_margin + spacing;
		}
		else if (IsVertical())
		{
			last_on_each_row.Add(i);

			if (m_fixed_max_width || button_width > available_column_width || m_grow_to_fit)
			{
				button_width = available_column_width;
			}

			if (HasFixedHeight())
			{
				button_height = available_row_height;
				if (widget->GetAnimation())
				{
					INT32 left_margin, top_margin, right_margin, bottom_margin;
					widget->GetSkinMargins(&left_margin, &top_margin, &right_margin, &bottom_margin);

					button_height += top_margin + bottom_margin;

					INT32 dummy_width = 0;
					widget->GetAnimation()->GetCurrentValue(dummy_width, button_height);

					button_height -= top_margin + bottom_margin;
				}
			}

			if (y != top_padding && ((m_wrapping == WRAPPING_NEWLINE && y + button_height + top_margin /*+ bottom_margin*/ > available_height) || was_previous_wrapper) )
			{
				y = top_padding;
				x = used_width;
				used_width += column_width;
				m_computed_column_count ++;
				toolbar_is_wrapping = TRUE;
			}

			positions[i].Set(x + left_margin, y + top_margin, button_width, button_height);

			y += button_height + top_margin;

			if (y > used_height)
			{
				used_height = y;
			}
			
			y += bottom_margin + spacing;
		}

		was_previous_wrapper = is_wrapper;
	}

	if (positions)
	{
		if (last_on_each_row.GetCount() == 0 || last_on_each_row.Get(last_on_each_row.GetCount() - 1) != widget_count - 1)
		{
			last_on_each_row.Add(widget_count - 1);
		}

		if (IsHorizontal() && m_shrink_to_fit && m_wrapping != WRAPPING_EXTENDER)
		{
			INT32 first_on_row = 0;

			for (UINT32 row = 0; row < last_on_each_row.GetCount(); row++)
			{
				INT32 last_on_row = last_on_each_row.Get(row);
				// Skip any hidden widgets on the end of the row
				while (last_on_row > first_on_row && positions[last_on_row].x == 0 && positions[last_on_row].y == 0 && positions[last_on_row].width == 0 && positions[last_on_row].height == 0)
					last_on_row--;
				INT32 real_available_width = available_width - positions[first_on_row].x;
				INT32 needed_width = positions[last_on_row].x + positions[last_on_row].width - available_width;

				if(last_on_row >= first_on_row && needed_width > 0)
				{
#ifdef QUICK_FISHEYE_SUPPORT
					if (m_fisheye_shrink && m_selected >= 0)
					{
						INT32 highcount = 0;
						INT32 fish_slots = 0;
						INT32 fish_base_width = 0;
						for (i = first_on_row; i <= last_on_row; i++)
						{
							highcount++;
							int selected_delta = op_abs(m_selected-i);
							if (selected_delta < 3)
							{
								if (!selected_delta)
									fish_base_width = positions[i].width;
								fish_slots += 3-selected_delta;
							}
						}
						if (fish_base_width > real_available_width/4)
							fish_base_width = real_available_width/4;
						// The base width which is used on non fisheye tabs is fish_slots/3 * (prefered_width-base_width) + base_width * num_items = total_width
						// or, base_width = (total_width-(prefered_width*fish_slots)/3) / (num_items-fish_slots/3)
						int base_width = 3*real_available_width-(fish_base_width*fish_slots);
						base_width = base_width / (3*highcount-fish_slots);

						// The width of the second is base_width+(prefered-base)*2/3 to make sure they are not smaller than the non-fisheye tabs
						fish_base_width -= base_width;
						int fish_widths[3];
						fish_widths[0] = base_width+fish_base_width;
						fish_widths[1] = base_width+(fish_base_width*2)/3;
						fish_widths[2] = base_width+fish_base_width/3;

						// Spread out the unused amount on the slots
						int unused_width = real_available_width-highcount*base_width;
						for (i = max(m_selected-2, first_on_row); i <= m_selected+2 && i <= last_on_row; ++i)
						{
							unused_width -= fish_widths[op_abs(i-m_selected)]-base_width;
						}

						int diff = 0;
						for (i = first_on_row; i <= last_on_row; ++i)
						{
							positions[i].x -= diff;

							int new_width;
							if (i >= m_selected-2 && i <= m_selected+2)
								new_width = fish_widths[op_abs(i-m_selected)];
							else
								new_width = base_width;
							new_width += (unused_width-->0)?1:0;
							diff += positions[i].width-new_width;
							positions[i].width = new_width;
						}
					}
					else
#endif // QUICK_FISHEYE_SUPPORT
					{
						INT32 vis_count = 0;
						for (i = first_on_row; i <= last_on_row; i++)
						{
							if (positions[i].width > 0)
							{
								vis_count++;
							}
						}
						INT32 avg = real_available_width / (vis_count ? vis_count : 1);
						INT32 highcount = 0;
						INT32 highsum = 0;
						for (i = first_on_row; i <= last_on_row; i++)
						{
							OpWidget* widget = m_widgets.Get(i);
							if (positions[i].width >= avg && (widget->GetMaxWidth() > avg || !widget->HasFixedMinMaxWidth()))
							{
								highcount++;
								highsum += positions[i].width;
							}
						}
						if(highcount > 0)
						{
							INT32 bigavg = (highsum - needed_width) / highcount;
							INT32 diff = 0;

							for (i = first_on_row; i <= last_on_row; ++i)
							{
								OpWidget* widget = m_widgets.Get(i);
								positions[i].x -= diff;
								if (widget->HasFixedMinMaxWidth() && bigavg < widget->GetMinWidth())
								{
									INT32 new_width = widget->GetMinWidth();
									needed_width -= positions[i].width - new_width;
									diff += positions[i].width - new_width;
									highsum -= positions[i].width;
									positions[i].width = new_width;
									--highcount;
								}
							}

							// sometimes, highcount can go back to zero in the above loop and crash here;
							// see DSK-365087
							if (highcount > 0)
							{
								bigavg = (highsum - needed_width) / highcount;
								INT32 remains = (highsum - needed_width) % highcount;
								diff = 0;

								for (i = first_on_row; i <= last_on_row; i++)
								{
									OpWidget* widget = m_widgets.Get(i);
									positions[i].x -= diff;
									if (positions[i].width >= avg &&
											(!widget->HasFixedMinMaxWidth() || widget->GetMaxWidth() > avg && positions[i].width > widget->GetMinWidth())
										)
									{
										INT32 new_width = bigavg + (remains-- > 0 ? 1 : 0);
										diff += positions[i].width - new_width;
										positions[i].width = new_width;
									}
								}
							}
						}
					}
				}

				OP_ASSERT(positions[last_on_row].x + positions[last_on_row].width <= available_width); // FIXME: fails on 64-bit !

				first_on_row = last_on_row + 1;
			}
		}
		if (IsHorizontal() && (m_grow_to_fit || has_growable) && !compute_size_only)
		{
			INT32 first_on_row = 0;
			INT32* grow_values = OP_NEWA(INT32, m_widgets.GetCount());
			if (grow_values)
			{
				INT32* max_widths = OP_NEWA(INT32, m_widgets.GetCount());
				if (max_widths)
				{
					for(i = 0; i < (INT32)m_widgets.GetCount(); i++)
					{
						OpWidget* current_widget = m_widgets.Get(i);
						grow_values[i] = current_widget->GetGrowValue();
						max_widths[i] = current_widget->GetMaxWidth();
					}

					for (UINT32 row = 0; row < last_on_each_row.GetCount(); row++)
					{
						INT32 last_on_row = last_on_each_row.Get(row);

						while (positions[last_on_row].x + positions[last_on_row].width < available_width)
						{
							INT32 slimmest_button = first_on_row;

							for (i = first_on_row + 1; i <= last_on_row; i++)
							{
								if (!m_grow_to_fit)
								{
									if (grow_values[i] < 2 || grow_values[i] < grow_values[slimmest_button])
										continue;

									if (grow_values[i] > grow_values[slimmest_button] || positions[i].width < positions[slimmest_button].width)
									{
										slimmest_button = i;
									}
								}
								else if (positions[i].width < positions[slimmest_button].width)
								{
									slimmest_button = i;
								}
							}

							if (!m_grow_to_fit && (!grow_values[slimmest_button] || (grow_values[slimmest_button] == 1 && (slimmest_button != first_on_row || slimmest_button != last_on_row || available_width > 250)) || (max_widths[slimmest_button] && positions[slimmest_button].width == max_widths[slimmest_button])))
							{
								break;
							}

							positions[slimmest_button].width++;

							for (i = slimmest_button + 1; i <= last_on_row; i++)
							{
								positions[i].x++;
							}
						}

						first_on_row = last_on_row + 1;
					}
					OP_DELETEA(max_widths);
				}
				OP_DELETEA(grow_values);
			}
		}
		else if (IsVertical() && m_grow_to_fit && !compute_size_only && m_computed_column_count == 1)
		{
			for (i = 0; i < widget_count; i++)
			{
				positions[i].width = available_width - positions[i].x;
			}
		}

		if (IsHorizontal() && !m_shrink_to_fit && m_wrapping == WRAPPING_OFF && !compute_size_only)
		{
			// If we shouldn't wrap and not shrink_to_fit but still overflow, we will still try to squeeze the last widget
			// a bit. That must be better than not showing it at all. (Would ideally shrink other widgets as well)
			int last = widget_count - 1;
			if (positions[last].x + positions[last].width > available_width_modified)
			{
				positions[last].width = available_width_modified - positions[last].x;
				positions[last].width = MAX(m_widgets.Get(last)->GetMinWidth(), positions[last].width);
			}
		}

		if (m_centered)
		{
			INT32 first_on_row = 0;

			for (UINT32 row = 0; row < last_on_each_row.GetCount(); row++)
			{
				INT32 last_on_row = last_on_each_row.Get(row);

				INT32 offset = (available_width - (positions[last_on_row].x + positions[last_on_row].width)) / 2;

				if (offset > 0)
				{
					for (i = first_on_row; i <= last_on_row; i++)
					{
						positions[i].x += offset;
					}
				}

				first_on_row = last_on_row + 1;
			}
		}
		if (offset_x || offset_y)
		{
			for (i = 0; i < widget_count; i++)
			{
				positions[i].x += offset_x;
				positions[i].y += offset_y;
			}
		}
	}

	if (compute_size_only == FALSE)
	{
		OpRect extender_rect(-1,-1,0,0);

		BOOL can_show_extender = FALSE;

		for (i = 0; i < widget_count; i++)
		{
			BOOL show = (IsHorizontal() && positions[i].x + positions[i].width <= available_width_modified) 
				|| toolbar_is_wrapping 
				|| (IsVertical() && (m_wrapping != WRAPPING_EXTENDER || positions[i].y + positions[i].height <= available_height - top_padding)) 
				|| available_width_modified <= 0;

#ifdef WIDGET_IS_HIDDEN_ATTRIB
			// Hidden widgets are ones that have "Feature" defines in from of them
			if (m_widgets.Get(i)->IsHidden())
				show = FALSE;
#endif // WIDGET_IS_HIDDEN_ATTRIB

			if (m_wrapping == WRAPPING_EXTENDER  && !show && !m_widgets.Get(i)->IsHidden() && !can_show_extender)
			{
				// this widget will be clipped.. and we want an extender.. maybe that means clipping previous one too

				if (IsVertical())
				{
					INT32 extender_height = EXTENDER_HEIGHT;

					INT32 left_margin=0, top_margin=0, right_margin=0, bottom_margin=0;
					OpWidget *widget = i > 0 ? m_widgets.Get(i-1) : m_widgets.Get(0);
					widget->GetSkinMargins(&left_margin, &top_margin, &right_margin, &bottom_margin);

					extender_rect.x = left_padding + left_margin;
					extender_rect.y = available_height - top_padding - extender_height;
					extender_rect.height = extender_height;
					extender_rect.width = available_width - left_margin - right_margin;

					// give derived classes the chance to move it somewhere else if needed
					OnBeforeExtenderLayout(extender_rect);

					// check if we really need to hide it now
					show = positions[i].y + positions[i].height < available_height - extender_rect.height;

					if (!show)
						can_show_extender = TRUE;

					// check if previous one must be clipped too

					INT32 previous = i - 1;

					INT32 previous_level = GetType() == WIDGET_TYPE_PANEL_SELECTOR ? extender_rect.y : available_height - extender_rect.height;

					if (previous >= 0 && positions[previous].y + positions[previous].height > previous_level)
					{
						m_widgets.Get(previous)->SetVisibility(FALSE);
					}

				}
				else
				{
					can_show_extender = TRUE;

					INT32 extender_width = EXTENDER_WIDTH;

					INT32 left_margin=0, top_margin=0, right_margin=0, bottom_margin=0;
					OpWidget *widget = i > 0 ? m_widgets.Get(i-1) : m_widgets.Get(0);
					widget->GetSkinMargins(&left_margin, &top_margin, &right_margin, &bottom_margin);
					
					extender_rect.x = available_width_modified - extender_width;
					extender_rect.y = top_padding + top_margin;
					extender_rect.height = available_height - top_margin - bottom_margin - top_padding;
					extender_rect.width = extender_width;

					// give derived classes the chance to move it somewhere else if needed
					OnBeforeExtenderLayout(extender_rect);

					// check if previous one must be clipped too

					INT32 previous = i - 1;

					if (previous >= 0 && positions[previous].x + positions[previous].width > (available_width_modified - extender_width))
					{
						m_widgets.Get(previous)->SetVisibility(FALSE);
					}
				}
			}

			OpWidget *widget = m_widgets.Get(i);
			OnBeforeWidgetLayout(widget, positions[i]);
			if (widget->IsFloating())
				SetChildOriginalRect(widget, positions[i]);
			else
				SetChildRect(widget, positions[i]);

			widget->SetVisibility(show);
		}

		if( m_wrapping == WRAPPING_EXTENDER )
		{
			if( can_show_extender )
			{
				if( !m_extender_button )
				{
					if (OpStatus::IsSuccess(OpButton::Construct(&m_extender_button)))
					{
						m_extender_button->SetButtonTypeAndStyle(OpButton::TYPE_LINK,OpButton::STYLE_IMAGE);
						OpInputAction *action = OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_POPUP_MENU));
						if (action)
						{
							action->SetActionDataString(UNI_L("Internal Toolbar Extender List"));
							m_extender_button->SetAction(action);
						}
						AddChild(m_extender_button, TRUE);
					}
				}
				if( m_extender_button )
				{
					m_extender_button->GetBorderSkin()->SetImage("Extender Button Skin");
					if(IsVertical())
					{
						m_extender_button->GetForegroundSkin()->SetImage("Extender Down Arrow", "Down Arrow");
					}
					else
					{
						m_extender_button->GetForegroundSkin()->SetImage("Extender Right Arrow", "Right Arrow");
					}
					SetChildRect(m_extender_button, extender_rect);
					m_extender_button->SetVisibility(TRUE);
				}
			}
			else if( m_extender_button )
			{
				m_extender_button->SetVisibility(FALSE);
			}
		}
		else if( m_extender_button )
		{
			m_extender_button->SetVisibility(FALSE);
		}
	}


	OP_DELETEA(positions);

	if (IsHorizontal())
	{
		if (m_grow_to_fit || m_fill_to_fit || (m_shrink_to_fit && used_width > available_width))
		{
			used_width = available_width;
		}
	}
	else
	{
		if (m_grow_to_fit || m_fill_to_fit || (m_shrink_to_fit && used_height > available_height))
		{
			used_height = available_height;
		}
	}

	if (g_application->IsCustomizingToolbars())
	{
		if (used_width < 10)
			used_width = 16;

		if (used_height < 10)
			used_height = 16;
	}

	used_width += right_padding;
	used_height += bottom_padding;

	if (IsAnimating())
	{
		// Affect the used size with the animation progress.

		double p = m_animation_in ? m_animation_progress : 1.0 - m_animation_progress;
		if (GetAlignment() == OpBar::ALIGNMENT_TOP || GetAlignment() == OpBar::ALIGNMENT_BOTTOM)
		{
			m_animation_size = used_height;
			used_height *= p;
		}
		else if (GetAlignment() == OpBar::ALIGNMENT_LEFT || GetAlignment() == OpBar::ALIGNMENT_RIGHT)
		{
			m_animation_size = used_width;
			used_width *= p;
		}
	}
}

/***********************************************************************************
**
**	UpdateAutoAlignment
**
***********************************************************************************/

void OpToolbar::UpdateAutoAlignment()
{
	if (!m_auto_update_auto_alignment)
		return;

	INT32 enabled_count = 0;
	INT32 count = GetWidgetCount();

	for (INT32 i = 0; i < count; i++)
	{
		OpWidget* widget = GetWidget(i);

		if (widget->IsEnabled())
			enabled_count++;
	}

	BOOL show;

	if (m_selector)
	{
		show = count > 1;
	}
	else
	{
		show = enabled_count > 0;
	}

	SetAutoVisible(show);
}

/***********************************************************************************
**
**	FindWidget
**
***********************************************************************************/

INT32 OpToolbar::FindWidget(OpWidget* widget, BOOL recursively)
{
	while (widget)
	{
		INT32 pos = m_widgets.Find(widget);

		if (pos != -1)
			return pos;

		if (!recursively)
			break;

		widget = widget->GetParent();
	}
	return -1;
}

/***********************************************************************************
**
**	OnEnabled
**
***********************************************************************************/

void OpToolbar::OnEnabled(OpWidget *widget, BOOL enabled)
{
	if (!GetAutoAlignment())
		return;

	UpdateAutoAlignment();
}

/***********************************************************************************
**
**	OnClick
**
***********************************************************************************/

void OpToolbar::OnClick(OpWidget *widget, UINT32 id)
{
	INT32 index = FindWidget(widget);

	if (index == -1)
		return;

	if (m_selector)
	{
		SetSelected(index == m_selected && m_deselectable ? -1 : index, TRUE, TRUE);
	}
	else
	{
		m_selected = index;

		if (listener)
		{
			listener->OnClick(this, widget->GetID());
		}
	}
	if(m_report_click_usage && 
		g_usage_report_manager && g_usage_report_manager->GetButtonClickReport() && 
		widget->IsOfType(WIDGET_TYPE_BUTTON))	// we only want buttons
	{
		// we want to report clicks on this one
		g_usage_report_manager->GetButtonClickReport()->AddClick(widget);
	}
}

/***********************************************************************************
**
**	OnPaintAfterChildren
**
***********************************************************************************/

void OpToolbar::OnPaintAfterChildren(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	if (IsTargetToolbar())
	{
		OpRect rect = GetBounds();

		vis_dev->SetColor(255, 205, 0);
		vis_dev->DrawRect(rect);
		vis_dev->DrawRect(rect.InsetBy(1, 1));
	}

	// If inside a group only the active tab has a valid geometry
	int drop_pos = GetActiveTabInGroup(m_drop_pos);

	if (drop_pos != -1 && m_widgets.GetCount() > 0)
	{
		const char* element = IsHorizontal() ?  "Vertical Drop Insert" : "Horizontal Drop Insert";

		INT32 width, height;
		GetSkinManager()->GetSize(element, &width, &height);

		const bool at_end = drop_pos >= INT32(m_widgets.GetCount());
		OpRect rect = m_widgets.Get(at_end ? m_widgets.GetCount() - 1 : drop_pos)->GetRect();

		if (IsHorizontal())
		{
			rect.x -= width / 2;

			if ((GetRTL() != FALSE) ^ at_end)
			{
				rect.x += rect.width;
			}

			rect.width = width;
			rect.y -= 2;
			rect.height += 4;
		}
		else
		{
			rect.y -= height / 2;

			if (at_end)
			{
				rect.y += rect.height;
			}

			rect.height = height;
			rect.x -= 2;
			rect.width += 4;
		}

		GetSkinManager()->DrawElement(vis_dev, element, rect);
	}
}

/***********************************************************************************
**
**	OnDragLeave
**
***********************************************************************************/

void OpToolbar::OnDragLeave(OpDragObject* drag_object)
{
	// Don't show a drag tooltip when the mouse pointer is leaving this toolbar
	if (g_application->GetToolTipListener() == this)
	{
		g_application->SetToolTipListener(NULL);
		m_drag_tooltip = Str::NOT_A_STRING;
	}

	UpdateDropPosition(-1);
}


/***********************************************************************************
**
**	OnDragCancel
**
***********************************************************************************/

void OpToolbar::OnDragCancel(OpDragObject* drag_object)
{
	UpdateDropPosition(-1);
}



/***********************************************************************************
**
**	OnDragMove
**
***********************************************************************************/

void OpToolbar::OnDragMove(OpDragObject* drag_object, const OpPoint& point)
{
	GenerateOnDragMove(static_cast<DesktopDragObject *>(drag_object), m_widgets.GetCount(), point.x, point.y);
}

/***********************************************************************************
**
**	OnDragDrop
**
***********************************************************************************/

void OpToolbar::OnDragDrop(OpDragObject* drag_object, const OpPoint& point)
{
	GenerateOnDragDrop(static_cast<DesktopDragObject *>(drag_object), m_widgets.GetCount(), point.x, point.y);
}

/***********************************************************************************
**
**	GetDragSourcePos
**
***********************************************************************************/

INT32 OpToolbar::GetDragSourcePos(DesktopDragObject* drag_object)
{
	return m_widgets.Find((OpWidget*)drag_object->GetObject());
}

/***********************************************************************************
**
**	OnDragStart
**
***********************************************************************************/

void OpToolbar::OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y)
{
	Type widget_type = widget->GetType();
	if (IsStandardToolbar()
	  && ( widget_type == WIDGET_TYPE_BUTTON
		|| widget_type == WIDGET_TYPE_RADIOBUTTON
		|| widget_type == WIDGET_TYPE_CHECKBOX
		|| widget_type == WIDGET_TYPE_EDIT
		|| widget_type == WIDGET_TYPE_DROPDOWN
		|| widget_type == WIDGET_TYPE_RESIZE_SEARCH_DROPDOWN
		|| widget_type == WIDGET_TYPE_ZOOM_SLIDER
		|| widget_type == WIDGET_TYPE_ZOOM_MENU_BUTTON
		|| widget_type == WIDGET_TYPE_TOOLBAR_MENU_BUTTON
		|| widget_type == WIDGET_TYPE_SPACER
		|| widget_type == WIDGET_TYPE_EXTENSION_BUTTON
		|| widget_type == WIDGET_TYPE_TRASH_BIN_BUTTON)
	 && g_application->IsDragCustomizingAllowed()
	 && !IsExtenderButton(widget))
	{
		OpTypedObject::Type drag_type;
		switch (widget_type)
		{
			case WIDGET_TYPE_EDIT					: drag_type = DRAG_TYPE_EDIT; break;
			case WIDGET_TYPE_SPACER					: drag_type = DRAG_TYPE_SPACER; break;
			case WIDGET_TYPE_DROPDOWN				: drag_type = DRAG_TYPE_DROPDOWN; break;
			case WIDGET_TYPE_RESIZE_SEARCH_DROPDOWN	: drag_type = DRAG_TYPE_RESIZE_SEARCH_DROPDOWN; break;
			case WIDGET_TYPE_ZOOM_SLIDER 			: drag_type = DRAG_TYPE_ZOOM_SLIDER; break;
			case WIDGET_TYPE_ZOOM_MENU_BUTTON 		: drag_type = DRAG_TYPE_ZOOM_MENU_BUTTON; break;
			case WIDGET_TYPE_TOOLBAR_MENU_BUTTON	: drag_type = DRAG_TYPE_TOOLBAR_MENU_BUTTON; break;
			case WIDGET_TYPE_EXTENSION_BUTTON		: drag_type = DRAG_TYPE_EXTENSION_BUTTON; break;
			case WIDGET_TYPE_TRASH_BIN_BUTTON : drag_type = DRAG_TYPE_TRASH_BIN_BUTTON; break;
			default									: drag_type = DRAG_TYPE_BUTTON;
		}
		DesktopDragObject* drag_object = widget->GetDragObject(drag_type, x, y);
		if (drag_object)
		{
			OpString text;

			widget->GetText(text);
			drag_object->SetAction(widget->GetAction());
			drag_object->SetTitle(text.CStr());
			drag_object->SetObject(widget);

			OpInputAction* action = widget->GetAction();

			if (action)
			{
				switch (action->GetAction())
				{
					case OpInputAction::ACTION_GO_TO_PAGE:
						drag_object->SetURL(action->GetActionDataString());
						break;

					case OpInputAction::ACTION_GO_TO_HOMEPAGE:
						drag_object->SetURL(g_pccore->GetStringPref(PrefsCollectionCore::HomeURL).CStr());
						break;
				}
			}

			g_drag_manager->StartDrag(drag_object, NULL, FALSE);
		}
	}
	else if (listener && listener != this)
	{
		listener->OnDragStart(this, m_widgets.Find(widget), x, y);
	}
}

void OpToolbar::OnDragMove(OpWidget* widget, OpDragObject* op_drag_object, INT32 pos, INT32 x, INT32 y)
{
	DesktopDragObject* drag_object = static_cast<DesktopDragObject *>(op_drag_object);

	if (IsStandardToolbar() && !g_application->IsDragCustomizingAllowed())
	{
		// Only buttons (from webpages) can be directly dropped on standard toolbars
		const uni_char* url = drag_object->GetURL();
		const uni_char* opera_url = UNI_L("opera:");
		if (!url || (!*url))
		{
			return;
		}
		else if(uni_strnicmp(url, opera_url, uni_strlen(opera_url)) != 0)
		{
			// This url could be dropped here if the shift key was pressed, show a tooltip to suggest this
			m_drag_tooltip = Str::S_DRAGGING_LINKS_ON_TOOLBARS_TOOLTIP;
			//make sure the this toolbar is the tooltip provider
			g_application->SetToolTipListener(this);			

			drag_object->SetDesktopDropType(DROP_NOT_AVAILABLE);
			return;
		}
	}
	if (KioskManager::GetInstance()->GetNoChangeButtons() || GetMaster())
	{
		drag_object->SetDesktopDropType(DROP_NOT_AVAILABLE);
		return;
	}
	if (widget == this)
	{
		if (IsStandardToolbar())
		{
			const INT32 src_pos = GetDragSourcePos(drag_object);
			if (src_pos == -1 || src_pos != pos)
			{
				BOOL copy = src_pos == -1;
				drag_object->SetDesktopDropType(copy ? DROP_COPY : DROP_MOVE);
			}
		}
		else if (listener && listener != this)
		{
			listener->OnDragMove(this, drag_object, pos, x, y);
		}
	}
	else
	{
		OpRect rect = widget->GetRect();
		pos = m_widgets.Find(widget);
		GenerateOnDragMove(drag_object, pos, x + rect.x, y + rect.y);
	}
}

/***********************************************************************************
**
**	OnDragDrop
**
***********************************************************************************/

void OpToolbar::OnDragDrop(OpWidget* widget, OpDragObject* op_drag_object, INT32 pos, INT32 x, INT32 y)
{
	DesktopDragObject* drag_object = static_cast<DesktopDragObject *>(op_drag_object);

	if (KioskManager::GetInstance()->GetNoChangeButtons() || GetMaster())
		return;

	if (widget == this)
	{
		if (IsStandardToolbar())
		{
			if (!m_locked)
			{
				// dropping something on a normal toolbar

				switch (drag_object->GetType())
				{
					case DRAG_TYPE_BUTTON:
					case DRAG_TYPE_DROPDOWN:
					case DRAG_TYPE_EDIT:
					case DRAG_TYPE_SPACER:
					case DRAG_TYPE_QUICK_FIND:
					case WIDGET_TYPE_MAIL_SEARCH_FIELD:
					case DRAG_TYPE_ACCOUNT_DROPDOWN:
					case DRAG_TYPE_ADDRESS_DROPDOWN:
					case DRAG_TYPE_SEARCH_DROPDOWN:
					case DRAG_TYPE_RESIZE_SEARCH_DROPDOWN:
					case DRAG_TYPE_SEARCH_EDIT:
					case DRAG_TYPE_ZOOM_DROPDOWN:
					case WIDGET_TYPE_STATUS_FIELD:
					case WIDGET_TYPE_STATE_BUTTON:
					case WIDGET_TYPE_IDENTIFY_FIELD:
					case WIDGET_TYPE_PROGRESS_FIELD:
					case WIDGET_TYPE_NEW_PAGE_BUTTON:
					case DRAG_TYPE_ZOOM_SLIDER:
					case WIDGET_TYPE_ZOOM_SLIDER:
					case DRAG_TYPE_ZOOM_MENU_BUTTON:
					case WIDGET_TYPE_ZOOM_MENU_BUTTON:
					case WIDGET_TYPE_TOOLBAR_MENU_BUTTON:
					case DRAG_TYPE_TOOLBAR_MENU_BUTTON:
					case WIDGET_TYPE_EXTENSION_BUTTON:
					case DRAG_TYPE_EXTENSION_BUTTON:
#ifdef AUTO_UPDATE_SUPPORT
					case WIDGET_TYPE_MINIMIZED_AUTOUPDATE_STATUS_BUTTON:
#endif
					case DRAG_TYPE_TRASH_BIN_BUTTON:
					case WIDGET_TYPE_TRASH_BIN_BUTTON:
					{
						INT32 src_pos = m_widgets.Find((OpWidget*)drag_object->GetObject());

						if (src_pos == -1 || src_pos != pos)
						{
							BOOL copy = src_pos == -1;

							if (copy)
							{
								switch (drag_object->GetType())
								{
									case DRAG_TYPE_BUTTON:
									case DRAG_TYPE_TRASH_BIN_BUTTON:
									case WIDGET_TYPE_TRASH_BIN_BUTTON:
									{
										OpButton* src_button = static_cast<OpButton*>(drag_object->GetObject());

										OpButton* new_button; OpTrashBinButton* trash_button;
										OP_STATUS status = OpStatus::OK;
										if (drag_object->GetType() == DRAG_TYPE_BUTTON) 
											status = CreateButton(src_button->GetType(), &new_button);
										else 
										{
											status = OpTrashBinButton::Construct(&trash_button);
											new_button = trash_button;
										}


										if (OpStatus::IsSuccess(status))
										{
											new_button->SetButtonType(src_button->GetButtonType());
											new_button->SetFixedTypeAndStyle(src_button->HasFixedTypeAndStyle());
											new_button->SetRelativeSystemFontSize(src_button->GetRelativeSystemFontSize());
											new_button->SetName(src_button->GetName());
											AddButton(new_button, drag_object->GetTitle(), NULL, OpInputAction::CopyInputActions(drag_object->GetAction()), NULL, pos, src_button->packed2.is_double_action, src_button->GetStringID());
										}
										break;
									}
									case DRAG_TYPE_DROPDOWN:
									{
										OpDropDown* dropdown;
										if (OpStatus::IsSuccess(OpDropDown::Construct(&dropdown)))
										{
											dropdown->SetAction(OpInputAction::CopyInputActions(drag_object->GetAction()));

											AddWidget(dropdown, pos);
#ifdef _MACINTOSH_					
											dropdown->SetSystemFont(OP_SYSTEM_FONT_UI_DIALOG);
#endif // _MACINTOSH_					
										}
										break;
									}
									case DRAG_TYPE_EDIT:
									{
										OpEdit* edit;
										if (OpStatus::IsSuccess(OpEdit::Construct(&edit)))
										{
											edit->SetAction(OpInputAction::CopyInputActions(drag_object->GetAction()));

											AddWidget(edit, pos);
										}
										break;
									}
									case DRAG_TYPE_SPACER:
									{
										OpSpacer* src_spacer = static_cast<OpSpacer*>(drag_object->GetObject());

										OpSpacer* spacer = OP_NEW(OpSpacer, (src_spacer->GetSpacerType()));
										if (spacer)
											AddWidget(spacer, pos);
										break;
									}
									case DRAG_TYPE_QUICK_FIND:
									{
										OpQuickFind* src_quickfind = static_cast<OpQuickFind*>(drag_object->GetObject());
										OpQuickFind* new_quickfind;
											
										if (OpStatus::IsSuccess(OpQuickFind::Construct(&new_quickfind)))
										{
											new_quickfind->SetEditOnChangeDelay(src_quickfind->GetEditOnChangeDelay());

											AddWidget(new_quickfind, pos);
										}
										break;
									}
									case WIDGET_TYPE_MAIL_SEARCH_FIELD:
									{
										MailSearchField* src_quickfind = static_cast<MailSearchField*>(drag_object->GetObject());
										MailSearchField* new_quickfind;
											
										if (OpStatus::IsSuccess(MailSearchField::Construct(&new_quickfind)))
										{
											new_quickfind->SetEditOnChangeDelay(src_quickfind->GetEditOnChangeDelay());

											AddWidget(new_quickfind, pos);
										}
										break;
									}
									case DRAG_TYPE_ACCOUNT_DROPDOWN:
									{
#ifdef M2_SUPPORT
										OpAccountDropDown *dd;
										if (OpStatus::IsSuccess(OpAccountDropDown::Construct(&dd)))
										{
											AddWidget(dd, pos);
#ifdef _MACINTOSH_					
											dd->SetSystemFont(OP_SYSTEM_FONT_UI_DIALOG);
#endif // _MACINTOSH_					
										}
#endif
										break;
									}
									case DRAG_TYPE_ADDRESS_DROPDOWN:
									{
										OpAddressDropDown *dd;
										if (OpStatus::IsSuccess(OpAddressDropDown::Construct(&dd)))
											AddWidget(dd, pos);
										break;
									}
									case DRAG_TYPE_RESIZE_SEARCH_DROPDOWN:
									{
										OpResizeSearchDropDown *dd;
										if (OpStatus::IsSuccess(OpResizeSearchDropDown::Construct(&dd)))
											AddWidget(dd, pos);
										break;
									}
									case DRAG_TYPE_SEARCH_DROPDOWN:
									{
										OpSearchDropDown *dd;
										if (OpStatus::IsSuccess(OpSearchDropDown::Construct(&dd)))
											AddWidget(dd, pos);
										break;
									}
									case DRAG_TYPE_SEARCH_EDIT:
									{
										OpSearchEdit *edit = OP_NEW(OpSearchEdit, (drag_object->GetID()));
										if (edit)
											AddWidget(edit, pos);
										break;
									}
									case DRAG_TYPE_ZOOM_DROPDOWN:
									{
										OpZoomDropDown *dd;
										if (OpStatus::IsSuccess(OpZoomDropDown::Construct(&dd)))
											AddWidget(dd, pos);
										break;
									}

									case WIDGET_TYPE_STATUS_FIELD:
									{
										OpStatusField* src_status_field = (OpStatusField*) drag_object->GetObject();
										OpStatusField* status_field = OP_NEW(OpStatusField, ((DesktopWindowStatusType) drag_object->GetID()));

										if (status_field)
										{
											status_field->SetGrowValue(src_status_field->GetGrowValue());
	
											status_field->SetRelativeSystemFontSize(src_status_field->GetRelativeSystemFontSize());
											AddWidget(status_field, pos);
										}
										break;
									}

									case WIDGET_TYPE_STATE_BUTTON:
									{
										OpStateButton * src_state_button = static_cast<OpStateButton *>(drag_object->GetObject());
										OpStateButton * state_button = OP_NEW(OpStateButton, (*src_state_button));

										if (state_button)
										{
											AddWidget(state_button, pos);
												
											/*By calling SetModifier on the state_button adds the widget into the WidgetStateListener.*/
											state_button->SetModifier(state_button->GetModifier()); 
										}
										break;
									}
									case WIDGET_TYPE_IDENTIFY_FIELD:
									{
										OpIdentifyField *ifield = OP_NEW(OpIdentifyField, (this));
										if (ifield)
											AddWidget(ifield, pos);
										break;
									}
									case WIDGET_TYPE_PROGRESS_FIELD:
									{
										OpProgressField *pfield;
										if (OpStatus::IsSuccess(OpProgressField::Construct(&pfield, (OpProgressField::ProgressType) drag_object->GetID())))
											AddWidget(pfield, pos);
										break;
									}
									case WIDGET_TYPE_NEW_PAGE_BUTTON:
									{
										OpNewPageButton *button;
										if (OpStatus::IsSuccess(OpNewPageButton::Construct(&button)))
											AddWidget(button, pos);
										break;
									}
									case DRAG_TYPE_ZOOM_SLIDER:
									case WIDGET_TYPE_ZOOM_SLIDER:
									{
										OpZoomSlider* slider;
										if (OpStatus::IsSuccess(OpZoomSlider::Construct(&slider)))
											AddWidget(slider, pos);
										break;
									}
									case DRAG_TYPE_ZOOM_MENU_BUTTON:
									case WIDGET_TYPE_ZOOM_MENU_BUTTON:
									{
										OpZoomMenuButton* button;
										if (OpStatus::IsSuccess(OpZoomMenuButton::Construct(&button)))
											AddWidget(button, pos);
										break;
									}
									case WIDGET_TYPE_TOOLBAR_MENU_BUTTON:
									case DRAG_TYPE_TOOLBAR_MENU_BUTTON:
									{
										OpToolbarMenuButton* button;
										if (OpStatus::IsSuccess(OpToolbarMenuButton::Construct(&button, m_button_style)))
										{
											AddWidget(button, pos);
										}
										break;
									}

#ifdef AUTO_UPDATE_SUPPORT
									case WIDGET_TYPE_MINIMIZED_AUTOUPDATE_STATUS_BUTTON:
									{
										OpMinimizedUpdateDialogButton* button;
										if (OpStatus::IsSuccess(OpMinimizedUpdateDialogButton::Construct(&button)))
										{
											AddWidget(button, pos);
										}
										break;
									}
#endif // AUTO_UPDATE_SUPPORT
									case WIDGET_TYPE_EXTENSION_BUTTON:
									case DRAG_TYPE_EXTENSION_BUTTON:
									{
										OpExtensionButton *src = static_cast<OpExtensionButton*>(drag_object->GetObject());

										OpExtensionButton *button;
										if (OpStatus::IsSuccess(OpExtensionButton::Construct(&button, src->GetId())))
										{
											AddWidget(button, pos);
										}
										break;
									}
								}

								g_input_manager->UpdateAllInputStates(TRUE);
							}
							else
							{
								MoveWidget(src_pos, pos);
							}
							WriteContent();
						}
						break;
					}

					default:
					{
						if (drag_object->GetURL() && *drag_object->GetURL() && !KioskManager::GetInstance()->GetNoDownload() )
						{
							OpString suggested_name;

							TRAPD(err, urlManager->GetURL(drag_object->GetURL()).GetAttributeL(URL::KSuggestedFileName_L, suggested_name, TRUE));
							if (OpStatus::IsError(err))
								return;

							if (!AddButtonFromURL(drag_object->GetURL(), drag_object->GetTitle(), pos))
							{
								OpInputAction* button_action = OP_NEW(OpInputAction, (OpInputAction::ACTION_GO_TO_PAGE));
								if (!button_action)
									return;
								button_action->SetActionData(1);
								button_action->SetActionDataString(drag_object->GetURL());
								AddButton(drag_object->GetTitle() ? drag_object->GetTitle() : suggested_name.HasContent() ? suggested_name.CStr() : drag_object->GetURL(), NULL, button_action, NULL, pos);
								WriteContent();
							}
						}
						break;
					}
				}
			}
		}
		else if (listener && listener != this)
		{
			listener->OnDragDrop(this, drag_object, pos, x, y);
		}
	}
	else
	{
		OpRect rect = widget->GetRect();
		pos = m_widgets.Find(widget);
		GenerateOnDragDrop(drag_object, pos, x + rect.x, y + rect.y);
	}
}

/***********************************************************************************
**
**	GenerateOnDragMove
**
***********************************************************************************/

void OpToolbar::GenerateOnDragMove(DesktopDragObject* drag_object, INT32 pos, INT32 x, INT32 y)
{
	const INT32 src_pos = GetDragSourcePos(drag_object);

	if( pos >= GetWidgetCount() )
	{
		// This fixes the jumping cursor problem when there is space between buttons. For
		// a group of tabs we need the active tag since that tab has a valid geometry
		pos = GetActiveTabInGroup(GetWidgetPosition(x, y));
	}

	bool show_drop_marker = true;
	bool drop_center = false;
	if( pos != -1 && pos < GetWidgetCount() )
	{
		OpWidget* widget = GetWidget(pos);
		if( widget )
		{
			OpRect rect = widget->GetRect(FALSE);
			int wx = x - rect.x;
			int wy = y - rect.y;
			if (IsHorizontal())
				wx = MAX(wx, 0);
			else
				wy = MAX(wy, 0);

			INT32 drop_area = IsHorizontal() ? widget->GetDropArea(wx,-1) : widget->GetDropArea(-1,wy);
			if( drop_area & DROP_CENTER )
			{
				show_drop_marker = false;
				drop_center = true;
			}
			else if (drop_area & ((GetRTL() ? DROP_LEFT : DROP_RIGHT) | DROP_BOTTOM))
			{
				IncDropPosition(pos, true);
			}
		}
	}
	if (src_pos == -1 || (src_pos != pos && (src_pos + 1) != pos))
	{
		OnDragMove(this, drag_object, pos, x, y);
	}

	bool drop_available = IsDropAvailable(drag_object, x, y);
	for (INT32 i = drag_object->GetIDCount() - 1; i >= 0; i--)
	{
		DesktopWindowCollectionItem* item = g_application->GetDesktopWindowCollection().GetItemByID(drag_object->GetID(i));
		if (drop_center && GetType() == WIDGET_TYPE_PAGEBAR && item && item->GetType() == OpTypedObject::WINDOW_TYPE_GROUP)
		 	drop_available = false;
	}

	if (DisableDropPositionIndicator() || !show_drop_marker || !drop_available)
	{
		pos = -1;
	}

	UpdateDropPosition(pos);
	UpdateDropAction(drag_object, !!drop_available);
}


/***********************************************************************************
**
**	GenerateOnDragDrop
**
***********************************************************************************/

void OpToolbar::GenerateOnDragDrop(DesktopDragObject* drag_object, INT32 pos, INT32 x, INT32 y)
{
	const INT32 src_pos = GetDragSourcePos(drag_object);

	if (pos >= GetWidgetCount())
	{
		// This fixes the jumping cursor problem when there is space between buttons
		pos = GetWidgetPosition( x, y );
	}

	if (pos != -1 && pos < GetWidgetCount())
	{
		OpWidget* widget = GetWidget(pos);
		if (widget)
		{
			OpRect rect = widget->GetRect(FALSE);
			int wx = x - rect.x;
			int wy = y - rect.y;
			if (IsHorizontal())
				wx = MAX(wx, 0);
			else
				wy = MAX(wy, 0);

			INT32 drop_area = IsHorizontal() ? widget->GetDropArea(wx,-1) : widget->GetDropArea(-1,wy);
			if (drop_area & DROP_CENTER)
			{
				// Do nothing
			}
			else
			{
				drag_object->SetInsertType(DesktopDragObject::INSERT_BEFORE);
				if (drop_area & ((GetRTL() ? DROP_LEFT : DROP_RIGHT) | DROP_BOTTOM))
					IncDropPosition(pos, true);
			}
		}
	}

	if (src_pos == -1 || (src_pos != pos && (src_pos + 1) != pos))
	{
		OnDragDrop(this, drag_object, pos, x, y);
	}

	if (!drag_object->IsDropAvailable() || DisableDropPositionIndicator())
	{
		pos = -1;
	}

	UpdateDropPosition(pos);
	OnDragLeave(drag_object);
}

void OpToolbar::UpdateDropPosition(int new_drop_pos)
{
	if (m_drop_pos != new_drop_pos)
	{
		m_drop_pos = new_drop_pos;
		InvalidateAll();
		Relayout();
	}
}

void OpToolbar::SetWidgetMiniSize(OpWidget *widget)
{
	switch(widget->GetType())
	{
		case WIDGET_TYPE_SEARCH_EDIT:
		case WIDGET_TYPE_EDIT:
			widget->SetMiniSize(m_mini_edit);
			break;

		default:
			widget->SetMiniSize(m_mini_buttons);
	}
}

/***********************************************************************************
**
**	GetWidgetPosition
**
***********************************************************************************/

INT32 OpToolbar::GetWidgetPosition(INT32 x, INT32 y)
{
	if (GetRTL())
		x = GetRect().Right() - (x - GetRect().x);

	for (unsigned i = 0; i < m_widgets.GetCount(); i++)
	{
		const OpRect r = AdjustForDirection(m_widgets.Get(i)->GetRect());
		if (IsHorizontal())
		{
			if (x < r.Right() && y <= r.Bottom() && y >= r.Top())
				return i;
		}
		else
		{
			if (y < r.Bottom() && x >= r.Left() && x <= r.Right())
				return i;
		}
	}
	return m_widgets.GetCount();
}

/***********************************************************************************
**
**	GetUserData
**
***********************************************************************************/

void* OpToolbar::GetUserData(INT32 index)
{
	OpWidget* widget = m_widgets.Get(index);

	if (!widget)
		return NULL;

	return widget->GetUserData();
}

/***********************************************************************************
**
**	OnContextMenu
**
***********************************************************************************/

BOOL OpToolbar::OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked)
{
	if (widget == this)
	{
		if (listener && listener != this && listener->OnContextMenu(widget, child_index, menu_point, avoid_rect, keyboard_invoked))
		{
			return TRUE;
		}
		else if (IsStandardToolbar() && !m_locked)
		{
			const PopupPlacement at_cursor = PopupPlacement::AnchorAtCursor();
			if (child_index == -1)
			{
				g_application->GetMenuHandler()->ShowPopupMenu("Toolbar Popup Menu", at_cursor, 0, keyboard_invoked);
				return TRUE;
			}
			switch (m_widgets.Get(child_index)->GetType())
			{
				case WIDGET_TYPE_IDENTIFY_FIELD:
					g_application->GetMenuHandler()->ShowPopupMenu("Toolbar Identify Popup Menu", at_cursor, 0, keyboard_invoked);
					return TRUE;

				case WIDGET_TYPE_RESIZE_SEARCH_DROPDOWN:
					g_application->GetMenuHandler()->ShowPopupMenu("Toolbar Edit Item Popup Menu", at_cursor, 0, keyboard_invoked);
					return TRUE;

				case WIDGET_TYPE_ADDRESS_DROPDOWN:
				case WIDGET_TYPE_SEARCH_DROPDOWN:
				case WIDGET_TYPE_ZOOM_DROPDOWN:
					if( (static_cast<OpDropDown*>(m_widgets.Get(child_index))->m_dropdown_window ))
					{
						return TRUE; // Eat and do not show popup menu when dropdown is visible
					}
					// falltrough

				case WIDGET_TYPE_SEARCH_EDIT:
				case WIDGET_TYPE_QUICK_FIND:
				case WIDGET_TYPE_MAIL_SEARCH_FIELD:
				case WIDGET_TYPE_COMPOSE_EDIT:
				case WIDGET_TYPE_EDIT:
					g_application->GetMenuHandler()->ShowPopupMenu("Toolbar Edit Item Popup Menu", at_cursor, 0, keyboard_invoked);
					return TRUE;

				default:
					g_application->GetMenuHandler()->ShowPopupMenu("Toolbar Item Popup Menu", at_cursor, 0, keyboard_invoked);
					return TRUE;
			}
		}
		return FALSE;
	}
	else
	{
		return OnContextMenu(this, m_widgets.Find(widget), menu_point, avoid_rect, keyboard_invoked);
	}
}

/***********************************************************************************
**
**	OnMouseDown
**
***********************************************************************************/

void OpToolbar::OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	OpBar::OnMouseDown(point, button, nclicks);

#ifdef WINDOW_MOVE_FROM_TOOLBAR
	if (nclicks == 1 && button == MOUSE_BUTTON_1 &&
		!g_application->IsCustomizingToolbars())
	{
		m_left_mouse_down = TRUE;
		m_down_point = point;
		m_desktop_window = GetParentDesktopWindow();
		if (m_desktop_window)
			m_desktop_window = m_desktop_window->GetToplevelDesktopWindow();
	}
#endif // WINDOW_MOVE_FROM_TOOLBAR
	
	UpdateTargetToolbar(TRUE);
}

////////////////////////////////////////////////////////////////////////////////////

void OpToolbar::OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	OpBar::OnMouseUp(point, button, nclicks);

#ifdef WINDOW_MOVE_FROM_TOOLBAR
	m_left_mouse_down = FALSE;
#endif // WINDOW_MOVE_FROM_TOOLBAR
}

////////////////////////////////////////////////////////////////////////////////////

void OpToolbar::OnMouseMove(const OpPoint &point)
{
	OpBar::OnMouseMove(point);
	
#ifdef WINDOW_MOVE_FROM_TOOLBAR
	if (m_left_mouse_down && m_desktop_window)
	{
		OpRect rect;
		OpWindow::State state;

		m_desktop_window->GetOpWindow()->GetDesktopPlacement(rect, state);
		
		rect.x += point.x - m_down_point.x;
		rect.y += point.y - m_down_point.y;
	
		m_desktop_window->GetOpWindow()->SetDesktopPlacement(rect, state);
	}
#endif // WINDOW_MOVE_FROM_TOOLBAR
}

/***********************************************************************************
**
**	OnMouseEvent
**
***********************************************************************************/

void OpToolbar::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	if (widget == this)
	{
		if (listener && listener != this)
		{
			listener->OnMouseEvent(widget, pos, x, y, button, down, nclicks);
		}
	}
	else
	{
		if (down && (!widget->IsDead() || widget->IsCustomizing()))
		{
			m_focused = FindWidget(widget, TRUE);
		}

		OnMouseEvent(this, m_focused, x, y, button, down, nclicks);
	}

	UpdateTargetToolbar(TRUE);
}

/***********************************************************************************
**
**	HasToolTipText
**
***********************************************************************************/

BOOL OpToolbar::HasToolTipText(OpToolTip* tooltip) 
{
	if (IsDebugToolTipActive())
		return TRUE;

	return m_drag_tooltip != Str::NOT_A_STRING;
}

/***********************************************************************************
**
**	GetToolTipText
**
***********************************************************************************/

void OpToolbar::GetToolTipText(OpToolTip* tooltip, OpInfoText& text)
{
	// Check for the debug tooltip first
	if (IsDebugToolTipActive())
	{
		// Implemented in QuickOpWidgetBase
		OpWidget::GetToolTipText(tooltip, text);
		return;
	}

	OP_ASSERT(m_drag_tooltip != Str::NOT_A_STRING); // why is a tooltip text requested while there is no tooltip?
	// get tooltip string, the string that should be shown is defined earlier in m_drag_tooltip
	OpString str;
	RETURN_VOID_IF_ERROR(g_languageManager->GetString(m_drag_tooltip, str));
	// use the string in the tooltip
	text.SetTooltipText(str.CStr());
}

/***********************************************************************************
**
**	OnFocus
**
***********************************************************************************/

void OpToolbar::OnFocus(BOOL focus,FOCUS_REASON reason)
{
	if (!focus)
		return;

	if (GetSelected() != -1)
	{
		GetWidget(GetSelected())->SetFocus(reason);
	}
	else
	{
		// cannot use widget->RestoreFocus since it searches for tab stop and toolbars button
		// don't have that.. using input manager directly instead

		g_input_manager->RestoreKeyboardInputContext(this, GetWidget(0), reason);
	}
}

/***********************************************************************************
**
**	OnAdded
**
***********************************************************************************/

void OpToolbar::OnAdded()
{
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	if (GetParent()->GetType() == WIDGET_TYPE_TOOLBAR ||
		GetParent()->GetType() == WIDGET_TYPE_INFOBAR ||
		GetParent()->GetType() == WIDGET_TYPE_PAGEBAR ||
		GetParent()->GetType() == WIDGET_TYPE_PANEL_SELECTOR)
		
		AccessibilitySkipsMe(TRUE);
#endif
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL OpToolbar::OnInputAction(OpInputAction* action)
{
	if (OpBar::OnInputAction(action))
		return TRUE;

	// first actions that work on any toolbar

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_TYPED_OBJECT:
		{
			if (action->GetActionData() == WIDGET_TYPE_TOOLBAR)
			{
				action->SetActionObject(this);
				return TRUE;
			}
			return FALSE;
		}

		// We have no "ACTION_FOCUS_NEXT_TOOBARBUTTONWIDGET" but this will do.
		case OpInputAction::ACTION_FOCUS_NEXT_RADIO_WIDGET:
			SelectNext();
			if (GetSelected() != -1)
			{
				GetWidget(GetSelected())->SetFocus(FOCUS_REASON_KEYBOARD);
				return TRUE;
			}
			return FALSE;
		case OpInputAction::ACTION_FOCUS_PREVIOUS_RADIO_WIDGET:
			SelectPrevious();
			if (GetSelected() != -1)
			{
				GetWidget(GetSelected())->SetFocus(FOCUS_REASON_KEYBOARD);
				return TRUE;
			}
			return FALSE;

		case OpInputAction::ACTION_GO:
		case OpInputAction::ACTION_SEARCH:
		case OpInputAction::ACTION_CLEAR:
		{
			INT32 i;

			// To the left
			for (i = m_focused; i >= 0; i--)
			{
				if (GetWidget(i)->OnInputAction(action))
				{
					return TRUE;
				}
			}
			// To the right
			for (i = m_focused + 1; i < (INT32) m_widgets.GetCount(); i++)
			{
				if (GetWidget(i)->OnInputAction(action))
				{
					return TRUE;
				}
			}
			return FALSE;
		}
	}

	if (GetMaster() || !IsActionForMe(action))
		return FALSE;

	// then only unamed actions for named toolbars

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_EXTENSION_ACTION:
				{
					return g_desktop_extensions_manager->HandleAction(action);
				}
				case OpInputAction::ACTION_SET_WRAPPING:
				{
					child_action->SetSelected(m_wrapping == child_action->GetActionData());
					return TRUE;
				}
				case OpInputAction::ACTION_SET_BUTTON_STYLE:
				{
					child_action->SetSelected(m_button_style == child_action->GetActionData());
					return TRUE;
				}
				case OpInputAction::ACTION_ENABLE_LARGE_IMAGES:
				case OpInputAction::ACTION_DISABLE_LARGE_IMAGES:
				{
					child_action->SetSelectedByToggleAction(OpInputAction::ACTION_ENABLE_LARGE_IMAGES, m_skin_size == SKINSIZE_LARGE);
					return TRUE;
				}
				case OpInputAction::ACTION_RESTORE_TO_DEFAULTS:
				{
					PrefsFile* pf = g_setup_manager->GetDefaultToolbarSetupFile();
					child_action->SetEnabled(pf && pf->IsSection("Version") );
					return TRUE;
				}
				case OpInputAction::ACTION_REMOVE:
				{
					child_action->SetEnabled(m_focused != -1);
					return TRUE;
				}
			}
			break;
		}

		case OpInputAction::ACTION_EXTENSION_ACTION:
		{
			action->SetActionObject(m_extender_button);
			return g_desktop_extensions_manager->HandleAction(action);
		}

		case OpInputAction::ACTION_CUSTOMIZE_TOOLBARS:
		{
			if (!GetSupportsReadAndWrite() || action->GetActionData() < 0)
				return FALSE;

			return g_application->CustomizeToolbars(this, action->GetActionData() == 1 ? CUSTOMIZE_PAGE_PANELS : CUSTOMIZE_PAGE_TOOLBARS);
		}

		case OpInputAction::ACTION_REMOVE:
		{
			if (IsStandardToolbar() && m_focused != -1)
			{
				RemoveWidget(m_focused);
				WriteContent();
				return TRUE;
			}
			return FALSE;
		}

		case OpInputAction::ACTION_RESTORE_TO_DEFAULTS:
		{
			if (IsStandardToolbar())
			{
				RestoreToDefaults();
				return TRUE;
			}
			return FALSE;
		}

		case OpInputAction::ACTION_SET_BUTTON_STYLE:
		{
			SetButtonStyle((OpButton::ButtonStyle) action->GetActionData());
			WriteStyle();
			return TRUE;
		}

		case OpInputAction::ACTION_SET_WRAPPING:
		{
			SetWrapping((OpBar::Wrapping) action->GetActionData());
			WriteStyle();
			return TRUE;
		}

		case OpInputAction::ACTION_ENABLE_LARGE_IMAGES:
		case OpInputAction::ACTION_DISABLE_LARGE_IMAGES:
		{
			if ((action->GetAction() == OpInputAction::ACTION_ENABLE_LARGE_IMAGES) != (m_skin_size == SKINSIZE_LARGE))
			{
				SetButtonSkinSize(m_skin_size == SKINSIZE_LARGE ? SKINSIZE_DEFAULT : SKINSIZE_LARGE);
				WriteStyle();
				return TRUE;
			}
			return FALSE;
		}
	}

	return FALSE;
}

void OpToolbar::AnimateWidgets(int start_index, int stop_index, int ignore_index, double duration)
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if (g_animation_manager->GetEnabled() && GetType() == WIDGET_TYPE_PAGEBAR)
	{
		OP_ASSERT(start_index <= stop_index);
		OP_ASSERT(start_index >= 0 && stop_index < (int)m_widgets.GetCount());
		for(int i = start_index; i <= stop_index; i++)
		{
			if (i == ignore_index)
				continue;
			OpWidget* dst_widget = m_widgets.Get(i);

			// Shrink animation window can never change animation type since that will make it kill itself and crash.
			if (!dst_widget->IsOfType(WIDGET_TYPE_SHRINK_ANIMATION))
			{
				if (dst_widget->IsFloating() && !dst_widget->GetAnimation())
					// Floating but no animation! We must be dragging this widget and don't want any animation to take charge of it
					continue;

				QuickAnimationParams params(dst_widget);

				params.curve     = ANIM_CURVE_SLOW_DOWN;
				params.duration  = duration / 1000.0;
				params.move_type = ANIM_MOVE_RECT_TO_ORIGINAL;

				g_animation_manager->startAnimation(params);
			}
		}
	}
#endif
}

/***********************************************************************************
**
**	MoveWidget
**
***********************************************************************************/

void OpToolbar::MoveWidget(INT32 src, INT32 dst)
{
	INT32 count = m_widgets.GetCount();

	if (dst >= count || dst == -1)
		dst = count;

	if (src == dst || src == dst - 1)
		return;

#ifdef VEGA_OPPAINTER_SUPPORT
	if (g_animation_manager->GetEnabled() && GetType() == WIDGET_TYPE_PAGEBAR)
	{
		// Animate all tabs that is moved between the old and new position.
		int dst_index = dst > src ? dst - 1 : dst;
		if (dst_index >= 0 && dst_index < count)
		{
			int i1 = MIN(src, dst_index);
			int i2 = MAX(src, dst_index);
			AnimateWidgets(i1, i2, -1);
		}
	}
#endif

	OpWidget* widget = m_widgets.Remove(src);

	if (dst > src)
	{
		m_widgets.Insert(dst - 1, widget);

		if (m_selected == src)
		{
			m_selected = dst - 1;
		}
		else if (m_selected > src && m_selected < dst)
		{
			m_selected--;
		}

		if (m_focused == src)
		{
			m_focused = dst - 1;
		}
		else if (m_focused > src && m_focused < dst)
		{
			m_focused--;
		}
	}
	else
	{
		m_widgets.Insert(dst, widget);

		if (m_selected == src)
		{
			m_selected = dst;
		}
		else if (m_selected < src && m_selected >= dst)
		{
			m_selected++;
		}

		if (m_focused == src)
		{
			m_focused = dst;
		}
		else if (m_focused < src && m_focused >= dst)
		{
			m_focused++;
		}
	}

	Relayout();
}

/***********************************************************************************
**
**	OnReadStyle
**
***********************************************************************************/

void OpToolbar::OnReadStyle(PrefsSection *section)
{
	OpStringC button_skin;

	// don't use setters here, as it will result in lots of redundant calls to UpdateAllWidgets()
	TRAPD(err,
		if (IsStandardToolbar()) 
			m_button_type = (OpButton::ButtonType)g_setup_manager->GetIntL(section, "Button type", m_button_type);	
		m_button_style = (OpButton::ButtonStyle)g_setup_manager->GetIntL(section, "Button style", m_button_style); 
		m_skin_size = (SkinSize)g_setup_manager->GetIntL(section, "Large images", m_skin_size); 
		m_wrapping = (Wrapping)g_setup_manager->GetIntL(section, "Wrapping", m_wrapping); 
		m_fixed_max_width = g_setup_manager->GetIntL(section, "Maximum Button Width", m_fixed_max_width); 
		m_grow_to_fit = g_setup_manager->GetIntL(section, "Grow to fit", m_grow_to_fit); 
		m_mini_buttons = g_setup_manager->GetIntL(section, "Mini Buttons", FALSE);
		m_mini_edit = g_setup_manager->GetIntL(section, "Mini Edit", TRUE);
		button_skin = g_setup_manager->GetStringL(section, "Button Skin");
);

	OpStatus::Ignore(SetButtonSkin(button_skin));

	UpdateAllWidgets();
}

/***********************************************************************************
**
**OnReadContent
**
** !!!!! NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE !!!!!!
**
** If you add a new button type to be read from the toolbar.ini here, you MUST
** add support for it in OnWriteContent again or it will no longer work after
** customization of the toolbar.
**
** !!!!! NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE !!!!!!
**
***********************************************************************************/

void OpToolbar::OnReadContent(PrefsSection *section)
{
	// Get the name of the current widget that has the keyboard input
	// context so we can restore the focus after it has been recreated.
	OpString8 focused_item_name;
	OpInputContext* keycontext = g_input_manager->GetKeyboardInputContext();
	OpInputContext* parcontext = keycontext;
	while (parcontext && parcontext->GetParentInputContext() != this)
	{
		parcontext = parcontext->GetParentInputContext();
	}
	if (parcontext && parcontext->GetType() >= OpTypedObject::WIDGET_TYPE && parcontext->GetType() < OpTypedObject::WIDGET_TYPE_LAST)
	{
		focused_item_name.Set(static_cast<OpWidget*>(parcontext)->GetName());
	}

	m_widgets.DeleteAll();
	m_focused = -1;
	m_selected = -1;
	BOOL menu_enabled = g_pcui->GetIntegerPref(PrefsCollectionUI::ShowMenu);
	BOOL extensionset_added = FALSE;
#ifdef VEGA_OPPAINTER_SUPPORT
	QuickDisableAnimationsHere dummy;
#endif

	if (section)
	{
		for (const PrefsEntry *OP_MEMORY_VAR entry = section->Entries(); entry; entry = (const PrefsEntry *) entry->Suc())
		{
			OpLineParser line(entry->Key());

			OpString8 item_type;
			OpString item_name;
			Str::LocaleString string_id(Str::NOT_A_STRING);

			RETURN_VOID_IF_ERROR(line.GetNextTokenWithoutTrailingDigits(item_type));


			if( op_stristr(item_type.CStr(), "Platform") != NULL )
			{
#if defined(_UNIX_DESKTOP_)
				if( op_stristr(&item_type.CStr()[8], "Unix") == NULL )
				{
					continue;
				}
#elif defined(MSWIN)
				if(( op_stristr(&item_type.CStr()[8], "Windows") == NULL ) &&
					((op_stristr(&item_type.CStr()[8], "Win2000") == NULL) || !IsSystemWin2000orXP()))
				{
					continue;
				}
#elif defined(_MACINTOSH_)
				if( op_stristr(&item_type.CStr()[8], "Mac") == NULL )
				{
					continue;
				}
#endif
				RETURN_VOID_IF_ERROR(line.GetNextTokenWithoutTrailingDigits(item_type));
			}
			if( op_stristr(item_type.CStr(), "Feature") != NULL )
			{
				BOOL skip = TRUE;

				if( op_stristr(&item_type.CStr()[8], "OperaAccount") != NULL )
				{
					skip = FALSE;
				}

				if( op_stristr(&item_type.CStr()[8], "ButtonMenu") != NULL )
				{
					// Hide this button if the main menu is enabled
					if (!menu_enabled)
						skip = FALSE;
				}
				// Skip anything not handled above
				if (skip)
					continue;

				RETURN_VOID_IF_ERROR(line.GetNextTokenWithoutTrailingDigits(item_type));
			}

			if (m_content_listener && !m_content_listener->OnReadWidgetType(item_type))
				continue;

			if (item_type.CompareI("Status") == 0 || item_type.CompareI("NamedStatus") == 0)
			{

				OpString8 widget_name;
				if(item_type.CompareI("NamedStatus") == 0)
					line.GetNextToken8(widget_name);

				INT32 type = 0;
				line.GetNextValue(type);

				OpStatusField* widget = OP_NEW(OpStatusField, ((DesktopWindowStatusType) type));

				if (widget)
				{
					if(item_type.CompareI("NamedStatus") == 0)
					{
						widget->SetName(widget_name.CStr());
					}

					INT32 value = 0;

					line.GetNextValue(value);

					if (value)
					{
						widget->SetRelativeSystemFontSize(value);
					}
					INT32 grow_value = 0;
					line.GetNextValue(grow_value);

					if (grow_value)
					{
						widget->SetGrowValue(grow_value);
					}

					INT32 weight = 0;

					line.GetNextValue(weight);

					if (weight != 0)
					{
						widget->SetSystemFontWeight(WEIGHT_BOLD);
					}
					AddWidget(widget);
				}
			}
			else if (item_type.CompareI("StateButton") == 0 || item_type.CompareI("NamedStateButton") == 0 ||
			         item_type.CompareI("CompressionRate") == 0 || item_type.CompareI("NamedCompressionRate") == 0)
			{
				OpStateButton *button;

				OpString8 button_kind;
				if (item_type.FindI("CompressionRate") != KNotFound)
					button_kind.Set("Turbo");
				else
					line.GetNextToken8(button_kind);

				if (OpStatus::IsSuccess(ButtonFactory::ConstructStateButtonByType(button_kind.CStr(), &button)))
				{
					if(item_type.FindI("Named") == 0)
					{
						OpString8 widget_name;
						line.GetNextToken8(widget_name);
						button->SetName(widget_name.CStr());
					}
					AddWidget(button);
				}
			}

			else if (item_type.CompareI("Identify") == 0)
			{
				OpIdentifyField *ifield = OP_NEW(OpIdentifyField, (this));
				if (ifield)
					AddWidget(ifield);
			}
			else if (item_type.CompareI("Address") == 0 || item_type.CompareI("NamedAddress") == 0)
			{
				OpAddressDropDown *addrdd;
				if (OpStatus::IsSuccess(OpAddressDropDown::Construct(&addrdd)))
				{
					if(item_type.CompareI("NamedAddress") == 0)
					{
						OpString8 widget_name;
						line.GetNextToken8(widget_name);
						addrdd->SetName(widget_name.CStr());
					}

					AddWidget(addrdd);
				}
			}
			else if (item_type.CompareI("Account") == 0)
			{
#ifdef M2_SUPPORT
				OpAccountDropDown *accdd;
				if (OpStatus::IsSuccess(OpAccountDropDown::Construct(&accdd)))
				{
					AddWidget(accdd);
#ifdef _MACINTOSH_					
					accdd->SetSystemFont(OP_SYSTEM_FONT_UI_DIALOG);
#endif // _MACINTOSH_					
				}
#endif
			}
            else if (item_type.CompareI("Multiresizesearch") == 0 || item_type.CompareI("NamedMultiresizesearch") == 0)
            {
				OpResizeSearchDropDown *searchdd;
				if (OpStatus::IsSuccess(OpResizeSearchDropDown::Construct(&searchdd)))
				{
					if(item_type.CompareI("NamedMultiresizesearch") == 0)
					{
						OpString8 widget_name;
						line.GetNextToken8(widget_name);
						searchdd->SetName(widget_name.CStr());
					}
					
					INT32 width = -1;
					line.GetNextValue(width);

					if(width > 100)
					{
						width = DEFAULT_ADDRESS_SEARCH_DROP_DOWN_WIDTH;
					}
					searchdd->SetCurrentWeightedWidth(width);
					AddWidget(searchdd);
				}
			}			
			else if (item_type.CompareI("Multisearch") == 0)
			{
				OpSearchDropDown *searchdd;
				if (OpStatus::IsSuccess(OpSearchDropDown::Construct(&searchdd)))
					AddWidget(searchdd);
			}
			else if (item_type.CompareI("Spacer") == 0 || item_type.CompareI("NamedSpacer") == 0)
			{

				OpString8 widget_name;
				if (item_type.CompareI("NamedSpacer") == 0)
				{
					line.GetNextToken8(widget_name);
				}

				INT32 type = 0;
				if(OpStatus::IsSuccess(line.GetNextValue(type)))
				{
					OpSpacer *spacer = OP_NEW(OpSpacer, ((OpSpacer::SpacerType) type));
					
					if (spacer && widget_name.HasContent())
					{
						spacer->SetName(widget_name);
					}

					if(spacer && type == OpSpacer::SPACER_FIXED)
					{
						INT32 size = -1;
						if(OpStatus::IsSuccess(line.GetNextValue(size)) && size > -1)
						{
							spacer->SetFixedSize(size);
						}
					}

					if (spacer)
						AddWidget(spacer);
				}
			}
			else if (item_type.CompareI("SearchButton") == 0)
			{
				INT32 type = 0;
				line.GetNextValue(type);

				OpButton* button;
				if (OpStatus::IsSuccess(CreateButton(WIDGET_TYPE_BUTTON, &button)))
				{
					OpString buf;
					buf.Reserve(64);
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
					g_searchEngineManager->MakeSearchWithString(type, buf);
#endif // DESKTOP_UTIL_SEARCH_ENGINES

					OpInputAction* input_action = OP_NEW(OpInputAction, (OpInputAction::ACTION_SEARCH));
					if (input_action)
						input_action->SetActionData(type);

					AddButton(button, buf.CStr(), NULL, input_action, NULL, -1);
				}
			}
			else if (item_type.CompareI("Search") == 0 || item_type.CompareI("NamedSearch") == 0)
			{
				OpString8 widget_name;
				if (item_type.CompareI("NamedSearch") == 0)
				{
					line.GetNextToken8(widget_name);
				}

				INT32 index_or_type = 0;
				OpString guid;
				OpSearchEdit* search_edit = NULL;
				
				if(OpStatus::IsSuccess(line.GetNextStringOrValue(guid, index_or_type)))
					search_edit = (guid.HasContent() ? OP_NEW(OpSearchEdit, (guid)) : OP_NEW(OpSearchEdit, (index_or_type)));

				if (search_edit)
				{
					if(item_type.CompareI("NamedSearch") == 0 && widget_name.CStr())
					{
						search_edit->SetName(widget_name.CStr());
					}
					INT32 index = search_edit->GetSearchIndex();

#ifdef DESKTOP_UTIL_SEARCH_ENGINES
					// make GetWidgetByTypeAndId work in more cases
					INT id = 0;

					if (g_searchEngineManager->GetItemByPosition(index))
						id = g_searchEngineManager->GetItemByPosition(index)->GetID(); // this is the OpTypedObject id of the search

					search_edit->SetID(id); // hm, which is set to be the id of the searchedit too
#endif // DESKTOP_UTIL_SEARCH_ENGINES

					AddWidget(search_edit);
				}
			}
			else if (item_type.CompareI("AllUISearches") == 0)
			{
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
				// Special entry that adds all the searches that appear under the dropdown, menu and pref dialog
				for (unsigned index = 0; index < g_searchEngineManager->GetSearchEnginesCount(); index++)
				{
					SearchTemplate *search = g_searchEngineManager->GetSearchEngine(index);

					if (search->GetKey().Length() >= 1)
					{
						OpSearchEdit* search_edit = OP_NEW(OpSearchEdit, (index));
						if (search_edit)
						{
							search_edit->SetID(search->GetID());
	
							AddWidget(search_edit);
						}
					}
				}
#endif // DESKTOP_UTIL_SEARCH_ENGINES
			}
			else if (item_type.CompareI("Progress") == 0)
			{
				INT32 type = 0;

				line.GetNextValue(type);

				OpProgressField* pfield;
				if (OpStatus::IsSuccess(OpProgressField::Construct(&pfield, (OpProgressField::ProgressType) type)))
					AddWidget(pfield);
			}
			else if (item_type.CompareI("NamedProgress") == 0)
			{
				INT32 type = 0;

				line.GetNextValue(type);

				OpString8 widget_name;
				line.GetNextToken8(widget_name);

				OpProgressField* pfield;
				if (OpStatus::IsSuccess(OpProgressField::Construct(&pfield, (OpProgressField::ProgressType) type)))
				{
					pfield->SetName(widget_name.CStr());
					AddWidget(pfield);
				}
			}
			else if (item_type.CompareI("NamedProgressBar") == 0)
			{
				OpString8 widget_name;
				line.GetNextToken8(widget_name);

				OpProgressBar* pfield;
				if (OpStatus::IsSuccess(OpProgressBar::Construct(&pfield)))
				{
					pfield->SetName(widget_name.CStr());
					AddWidget(pfield);
				}
			}
			else if (item_type.CompareI("Zoom") == 0)
			{
				OpZoomDropDown *zoomdd;
				if (OpStatus::IsSuccess(OpZoomDropDown::Construct(&zoomdd)))
					AddWidget(zoomdd);
			}
			else if (item_type.CompareI("Dropdown") == 0 || item_type.CompareI("NamedDropdown") == 0)
			{
				OpDropDown* dropdown;
				if (OpStatus::IsSuccess(OpDropDown::Construct(&dropdown)))
				{
					OpInputAction* input_action;
					if (OpStatus::IsSuccess(OpInputAction::CreateInputActionsFromString(entry->Value(), input_action)))
						dropdown->SetAction(input_action);

					if(item_type.CompareI("NamedDropdown") == 0)
					{
						OpString8 widget_name;
						line.GetNextToken8(widget_name);
						dropdown->SetName(widget_name.CStr());
					}

					AddWidget(dropdown);
#ifdef _MACINTOSH_					
					dropdown->SetSystemFont(OP_SYSTEM_FONT_UI_DIALOG);
#endif // _MACINTOSH_					
				}
			}
			else if (item_type.CompareI("Edit") == 0)
			{
				OpEdit* edit;
				if (OpStatus::IsSuccess(OpEdit::Construct(&edit)))
				{
					OpInputAction* input_action;
					if (OpStatus::IsSuccess(OpInputAction::CreateInputActionsFromString(entry->Value(), input_action)))
						edit->SetAction(input_action);
					AddWidget(edit);
				}
			}
			else if (item_type.CompareI("Quickfind") == 0 || item_type.CompareI("NamedQuickfind") == 0) 
			{
				INT32 delay = 0;

				OpString8 widget_name;
				if (item_type.CompareI("NamedQuickfind") == 0)
					line.GetNextToken8(widget_name);

				line.GetNextValue(delay);

				OpQuickFind* quickfind;
				if (OpStatus::IsSuccess(OpQuickFind::Construct(&quickfind)))
				{
					quickfind->SetEditOnChangeDelay(delay);

					if (item_type.CompareI("NamedQuickfind") == 0)
						quickfind->SetName(widget_name.CStr());

					AddWidget(quickfind);
				}
			}
			else if (item_type.CompareI("MailSearchField") == 0 || item_type.CompareI("NamedMailSearchField") == 0)
			{
				INT32 delay = 0;

				MailSearchField* quickfind;
				if (OpStatus::IsSuccess(MailSearchField::Construct(&quickfind)))
				{
					if(item_type.CompareI("NamedMailSearchField") == 0)
					{
						OpString8 widget_name;
						line.GetNextToken8(widget_name);
						quickfind->SetName(widget_name.CStr());
					}

					line.GetNextValue(delay);
					quickfind->SetEditOnChangeDelay(delay);

					AddWidget(quickfind);
				}
			}
			else if (item_type.CompareI("QuickButton") == 0)	// double action button
			{
				OpInputAction* input_action;
				if (OpStatus::IsError(OpInputAction::CreateInputActionsFromString(entry->Value(), input_action)))
					input_action = NULL;

				line.GetNextLanguageString(item_name, &string_id);
				AddButton(item_name.CStr(), NULL, input_action, NULL, -1, TRUE, string_id);
			}
			else if (item_type.CompareI("Button") == 0 || item_type.CompareI("NamedButton") == 0 
				|| item_type.CompareI("TrashBinButton") == 0 || item_type.CompareI("NamedTrashBinButton") == 0)
			{
				BOOL is_trash_bin_button = item_type.CompareI("TrashBinButton") == 0 || item_type.CompareI("NamedTrashBinButton") == 0 ; 

				OpButton* button;
				OpTrashBinButton* trash_button;
				OP_STATUS status = OpStatus::OK;

				if (!is_trash_bin_button)
					status = CreateButton(WIDGET_TYPE_BUTTON, &button);
				else 
				{
					status = OpTrashBinButton::Construct(&trash_button);
					button=trash_button;
				}

				if (OpStatus::IsSuccess(status))
				{
					OpInputAction* input_action;
					if (OpStatus::IsError(OpInputAction::CreateInputActionsFromString(entry->Value(), input_action)))
						input_action = NULL;

					if(item_type.CompareI("NamedButton") == 0 || item_type.CompareI("NamedTrashBinButton") == 0)
					{
						OpString8 widget_name;
						line.GetNextToken8(widget_name);
						button->SetName(widget_name.CStr());
					}
	
					line.GetNextLanguageString(item_name, &string_id);

					INT32 value = 0;

					line.GetNextValue(value);

					if (value)
					{
						button->SetRelativeSystemFontSize(value);
					}
					// to be able to use type and style = 0, we need a small hack here
					value = 99;
					line.GetNextValue(value);
					if (value < 99)
					{
						button->SetButtonType((OpButton::ButtonType)value);
						button->SetFixedTypeAndStyle(TRUE);
					}
					value = 99;
					line.GetNextValue(value);
					if (value < 99)
					{
						button->SetButtonStyle((OpButton::ButtonStyle)value);
						button->SetFixedTypeAndStyle(TRUE);
					}
					value = 0;
					line.GetNextValue(value);
					if (value != 0)
					{
						button->SetBold(TRUE);
						button->SetSystemFontWeight(WEIGHT_BOLD);
					}
					AddButton(button, item_name.CStr(), NULL, input_action, NULL, -1, FALSE, string_id);
				}
			}
			else if (item_type.CompareI("Checkbox") == 0 || item_type.CompareI("NamedCheckbox") == 0)
			{
				OpCheckBox* button;
				if (OpStatus::IsSuccess(OpCheckBox::Construct(&button)))
				{
					OpInputAction* input_action;
					if (OpStatus::IsError(OpInputAction::CreateInputActionsFromString(entry->Value(), input_action)))
						input_action = NULL;

					if(item_type.CompareI("NamedCheckbox") == 0)
					{
						OpString8 widget_name;
						line.GetNextToken8(widget_name);
						button->SetName(widget_name.CStr());
					}
	

					button->SetFixedTypeAndStyle(TRUE);
					line.GetNextLanguageString(item_name, &string_id);
					AddButton(button, item_name.CStr(), NULL, input_action, NULL, -1, FALSE, string_id);
				}
			}
			else if (item_type.CompareI("Radiobutton") == 0 || item_type.CompareI("NamedRadiobutton") == 0)
			{
				OpButton* button;
				if (OpStatus::IsSuccess(CreateButton(WIDGET_TYPE_RADIOBUTTON, &button)))
				{
					button->SetButtonType(OpButton::TYPE_RADIO);

					OpInputAction* input_action;
					if (OpStatus::IsError(OpInputAction::CreateInputActionsFromString(entry->Value(), input_action)))
						input_action = NULL;

					if(item_type.CompareI("NamedRadiobutton") == 0)
					{
						OpString8 widget_name;
						line.GetNextToken8(widget_name);
						button->SetName(widget_name.CStr());
					}

					button->SetFixedTypeAndStyle(TRUE);
					button->SetSystemFont(OP_SYSTEM_FONT_UI_TOOLBAR);
					line.GetNextLanguageString(item_name, &string_id);
					AddButton(button, item_name.CStr(), NULL, input_action, NULL, -1, FALSE, string_id);
				}
			}
#ifdef AUTO_UPDATE_SUPPORT
			else if (item_type.CompareI("MinimizedUpdate") == 0)
			{
				 OpMinimizedUpdateDialogButton* button;
				 if (OpStatus::IsSuccess(OpMinimizedUpdateDialogButton::Construct(&button)))
				 {
						 AddWidget(button);
				 }
			}
#endif // AUTO_UPDATE_SUPPORT
			else if (item_type.CompareI("MenuButton") == 0)
			{
				OpToolbarMenuButton *button;

				BOOL show_menu = g_pcui->GetIntegerPref(PrefsCollectionUI::ShowMenu);

				if (!show_menu && OpStatus::IsSuccess(OpToolbarMenuButton::Construct(&button, m_button_style)))
				{
					AddWidget(button);
				}
			}
			else if (item_type.CompareI("RichTextLabel") == 0)
			{
				OpRichTextLabel *label;
				
				if (OpStatus::IsSuccess(OpRichTextLabel::Construct(&label)))
				{
                    // Set not to wrap in toolbar
                    label->SetWrapping(FALSE);
					AddWidget(label);
					line.GetNextLanguageString(item_name, &string_id);
					label->SetStringID(Str::LocaleString(string_id));
					if (item_name.HasContent())
						label->SetText(item_name.CStr());
				}
			}
			
			else if (item_type.CompareI("Label") == 0 || item_type.CompareI("NamedLabel") == 0)
			{
				OpLabel *label;

				if (OpStatus::IsSuccess(OpLabel::Construct(&label)))
				{
					if(item_type.CompareI("NamedLabel") == 0)
					{
						OpString8 widget_name;
						line.GetNextToken8(widget_name);
						label->SetName(widget_name.CStr());
					}
					AddWidget(label);
					line.GetNextLanguageString(item_name, &string_id);
					label->SetStringID(Str::LocaleString(string_id));
					if (item_name.HasContent())
						label->SetText(item_name.CStr());
				}
			}
			else if (item_type.CompareI("Icon") == 0)
			{
				OpIcon *icon;

				if (OpStatus::IsSuccess(OpIcon::Construct(&icon)))
				{
					AddWidget(icon);

					line.GetNextLanguageString(item_name, &string_id);

					// The icon is special and the Language string is actually the Skin name to use
					if (item_name.HasContent())
					{
						OpString8 skin_name;

						skin_name.Set(item_name.CStr());
						icon->SetImage(skin_name.CStr(), TRUE);
					}
				}
			}
			else if(item_type.CompareI("NewPageButton") == 0 || item_type.CompareI("NamedNewPageButton") == 0)
			{
				OpNewPageButton* button;

				if (OpStatus::IsSuccess(OpNewPageButton::Construct(&button)))
				{
					if(item_type.CompareI("NamedNewPageButton") == 0)
					{
						OpString8 widget_name;
						line.GetNextToken8(widget_name);
						button->SetName(widget_name.CStr());
					}
					AddWidget(button);
				}
			}
			else if(item_type.CompareI("ZoomSlider") == 0)
			{
				OpZoomSlider* slider;

				if (OpStatus::IsSuccess(OpZoomSlider::Construct(&slider)))
				{
					INT32 show_tick_labels = 0;
					if(OpStatus::IsSuccess(line.GetNextValue(show_tick_labels)) && show_tick_labels > 0)
						slider->ShowTickLabels(TRUE);

					AddWidget(slider);
					OpInputAction *action = OP_NEW (OpInputAction, (OpInputAction::ACTION_ZOOM_TO));
					slider->SetAction(action);
				}
			}
			else if(item_type.CompareI("ZoomMenuButton") == 0 || item_type.CompareI("NamedZoomMenuButton") == 0)
			{
				OpZoomMenuButton* button;

				if (OpStatus::IsSuccess(OpZoomMenuButton::Construct(&button)))
				{
					if(item_type.CompareI("NamedZoomMenuButton") == 0)
					{
						OpString8 widget_name;
						line.GetNextToken8(widget_name);
						button->SetName(widget_name.CStr());
					}

					AddWidget(button);
				}
			}
			else if(item_type.CompareI("Browser") == 0)
			{
				OpBrowserView* br;
				
				if (OpStatus::IsSuccess(OpBrowserView::Construct(&br)))
				{
					br->SetEmbeddedInDialog(TRUE);
					br->SetPreferedSize(300, 200);
					br->SetOwnAuthDialog(TRUE);
					br->SetConsumeScrolls(TRUE);
					AddWidget(br);
				}
			}
			else if(item_type.CompareI("ExtensionSet") == 0)
			{
				OpExtensionSet* eset;

				if (OpStatus::IsSuccess(OpExtensionSet::Construct(&eset)))
				{
					AddWidget(eset);
					extensionset_added = TRUE;
				}
			}
			else if (item_type.CompareI("Progressbar") == 0)
			{
				// Different than the progressfield
				OpProgressBar* bar;
				if (OpStatus::IsSuccess(OpProgressBar::Construct(&bar)))
					AddWidget(bar);
			}
		} // for
	}

	//
	// Following code _forces_ certain widgets into toolbars
	// even if they are not part of user-modified toolbars.
	//
	// It's required to allow seamless update of toolbar content 
	// for users, who update their toolbar ini files by hand.
	//
	// See:	DSK-316682 [pobara 2010-11-30]
	//
	if (!extensionset_added)
	{
		OpString8 extensionset_parent;
		extensionset_parent.Set(g_pcui->GetStringPref(
					PrefsCollectionUI::ExtensionSetParent));
		if (IsNamed(extensionset_parent.CStr()))
		{
			OpExtensionSet* eset;

			if (OpStatus::IsSuccess(OpExtensionSet::Construct(&eset)))
			{
				AddWidget(eset);
			}
		}
	}

	// If the focused widget was deleted, try to set the focus back here if it still exists.
	if (g_input_manager->GetKeyboardInputContext() != keycontext && focused_item_name.HasContent())
	{
		OpWidget* tofocus = GetWidgetByName(focused_item_name.CStr());
		if (tofocus)
			tofocus->SetFocus(FOCUS_REASON_OTHER);
	}
}

/***********************************************************************************
**
**	AddButtonFromURL
**
***********************************************************************************/

BOOL OpToolbar::AddButtonFromURL(const uni_char* url_src, const uni_char* url_title, int pos, BOOL force)
{
	// This is supposed to return TRUE so that the system doesn't take you to a url error page
	// but instead nothing happens when you click on a button
	if (KioskManager::GetInstance()->GetNoChangeButtons())
		return TRUE;

	if (!url_src)
		return FALSE;

	OpString url;

	url.Set(url_src);

	const uni_char* button_prefix = UNI_L("Opera:/button/");
	INT32 len = uni_strlen(button_prefix);

	OP_MEMORY_VAR BOOL is_button = TRUE;

	if (uni_strnicmp(url.CStr(), button_prefix, len))
	{
		is_button = FALSE;

		const uni_char* edit_prefix = UNI_L("Opera:/edit/");
		len = uni_strlen(edit_prefix);

		if (uni_strnicmp(url.CStr(), edit_prefix, len))
			return FALSE;
	}

	OpInputAction* input_action;
	uni_char* action_text = (uni_char*) url.CStr() + len;
	len = uni_strlen(action_text);

	// Do not allow sick long actions

	if (len > MAX_ACTION_LENGTH)
		return FALSE;

	UriUnescape::ReplaceChars(action_text, UriUnescape::All);
	action_text[len] = 0;

	if (OpStatus::IsError(OpInputAction::CreateInputActionsFromString(action_text, input_action)))
		return FALSE;

	if (!force)
	{
		AddActionButtonController * controller = OP_NEW(AddActionButtonController, (this,is_button, input_action, pos, force));
		if (!controller)
			return TRUE;

		if(OpStatus::IsError(controller->SetData(action_text, url_title)))
		{
			OP_DELETE(controller);
			return TRUE;
		}
		
		OpStatus::Ignore(ShowDialog(controller,g_global_ui_context, GetParentDesktopWindow()));
		
		// KLUDGE until we change doc module.. if pos == -1, then this was called from doc module to add directly to mainbar because user
		// clicked a button link.. but we don't want them in  mainbar since it is off by default, we want them in the my buttons category in customize dialog
	}
	else
	{
		AddButtonFromURL2(is_button, input_action, action_text, url_title, pos, force);
	}
	return TRUE;
}

void OpToolbar::AddButtonFromURL2(BOOL is_button, OpInputAction * input_action, const uni_char* action_text, const uni_char* url_title, int pos, BOOL force)
{
	OpToolbar* target_toolbar = this;

	if (!force)
	{
		if (pos == -1)
		{
			g_application->CustomizeToolbars(NULL);

			CustomizeToolbarDialog* dialog = g_application->GetCustomizeToolbarsDialog();

			if (!dialog)
				return;

			target_toolbar = dialog->GoToCustomToolbar();

			if (!target_toolbar)
				return;
		}
	}

	if (!input_action->HasActionText() && url_title)
		input_action->SetActionText(url_title);

	if (is_button)
	{
		OpString title;
		if (!url_title)
		{
			title.Set(OpInputAction::GetStringFromAction(input_action->GetAction()));
			url_title = title.CStr();
		}
		target_toolbar->AddButton(url_title, NULL, input_action, NULL, pos);
	}
	else
	{
		OpEdit* edit;
		if (OpStatus::IsSuccess(OpEdit::Construct(&edit)))
		{
			edit->SetAction(input_action);
			target_toolbar->AddWidget(edit, pos);
		}
	}

	target_toolbar->WriteContent();
}

/***********************************************************************************
**
**	OnWriteStyle
**
***********************************************************************************/

void OpToolbar::OnWriteStyle(PrefsFile* prefs_file, const char* name)
{
	TRAPD(err,
		if (IsStandardToolbar()) prefs_file->WriteIntL(name, "Button type", GetButtonType()); 
		prefs_file->WriteIntL(name, "Button style", GetButtonStyle()); 
		prefs_file->WriteIntL(name, "Large Images", GetButtonSkinSize()); 
		prefs_file->WriteIntL(name, "Wrapping", m_wrapping); 
		prefs_file->WriteIntL(name, "Maximum Button Width", m_fixed_max_width); 
		prefs_file->WriteIntL(name, "Grow To Fit", m_grow_to_fit); 
		prefs_file->WriteIntL(name, "Mini Buttons", m_mini_buttons);
		prefs_file->WriteIntL(name, "Mini Edit", m_mini_edit);
		if(m_button_skin.HasContent()) { OpString tmp; tmp.Set(m_button_skin.CStr()); prefs_file->WriteStringL(name, "Button Skin", tmp); }
	);

	//notify other toolbars or interested components that the setup has changed
	g_application->SettingsChanged(SETTINGS_TOOLBAR_STYLE);
}

/***********************************************************************************
**
**	OnWriteContent
**
***********************************************************************************/

void OpToolbar::OnWriteContent(PrefsFile* prefs_file, const char* name)
{
	INT32 count = GetWidgetCount();
	OP_STATUS err;

	if (count == 0)
	{
		// force an empty section to be created
		TRAP(err,
			prefs_file->WriteIntL(name, "Dummy", 0);
			prefs_file->DeleteKeyL(name, "Dummy");
		);
	}

	for (INT32 i = 0; i < count; i++)
	{
		OpWidget* widget = GetWidget(i);

		OpLineString key;
		OpString value;

		switch (widget->GetType())
		{
		case WIDGET_TYPE_RADIOBUTTON:
		case WIDGET_TYPE_BUTTON:
		case WIDGET_TYPE_CHECKBOX:
		case WIDGET_TYPE_TRASH_BIN_BUTTON:
		{
			if (widget->GetType() == OpTypedObject::WIDGET_TYPE_RADIOBUTTON)
			{
				key.WriteTokenWithTrailingDigits("RadioButton", i);
			}
			else if (widget->GetType() == OpTypedObject::WIDGET_TYPE_CHECKBOX)
			{
				key.WriteTokenWithTrailingDigits("Checkbox", i);
			}
			else if (static_cast<OpButton*>(widget)->packed2.is_double_action)
			{
				key.WriteTokenWithTrailingDigits("QuickButton", i);
			}
			else if (widget->GetType() == WIDGET_TYPE_TRASH_BIN_BUTTON)
			{
				if (widget->IsNamed(WIDGET_NAME_TRASH_BIN_BUTTON))
				{
					key.WriteTokenWithTrailingDigits("NamedTrashBinButton", i);
					key.WriteString8(WIDGET_NAME_TRASH_BIN_BUTTON);
				}
				else
				{
					key.WriteTokenWithTrailingDigits("TrashBinButton", i);
				}
			}
			else if (widget->GetName().HasContent())
			{
				key.WriteTokenWithTrailingDigits("NamedButton", i);
				key.WriteToken8(widget->GetName().CStr());
			}
			else
			{
				key.WriteTokenWithTrailingDigits("Button", i);
			}

			OpString text;
			widget->GetText(text);

			if (widget->GetStringID() != Str::NOT_A_STRING)
			{
				key.WriteValue(widget->GetStringID());
			}
			else
			{
				// Replace '"' in string.
				// Fix for bug 232618
				StringUtils::Replace(text, OpStringC(UNI_L("\"")), OpStringC(UNI_L("''")));
				key.WriteString(text.CStr());
			}

			INT32 font_size = widget->GetRelativeSystemFontSize();

			if (font_size != 100)
			{
				key.WriteValue(font_size);
			}

			OpInputAction* action = widget->GetAction();

			if (action)
			{
				action->ConvertToString(value);
			}
			break;
		}
#ifdef AUTO_UPDATE_SUPPORT
		case WIDGET_TYPE_MINIMIZED_AUTOUPDATE_STATUS_BUTTON:
		{
			key.WriteTokenWithTrailingDigits("MinimizedUpdate", i);
			break;
		}
#endif // AUTO_UPDATE_SUPPORT
		case WIDGET_TYPE_QUICK_FIND:
		case WIDGET_TYPE_MAIL_SEARCH_FIELD:
		{
			key.WriteTokenWithTrailingDigits((widget->GetType() == WIDGET_TYPE_QUICK_FIND) ? "Quickfind" : "MailSearchField", i);
			key.WriteValue(((OpQuickFind*)widget)->GetEditOnChangeDelay());
			break;
		}
		case WIDGET_TYPE_DROPDOWN:
		{
			OpString name;
			name.Set(widget->GetName());
			if (name.HasContent())
			{
				name.Insert(0, "NamedDropdown, ");
				key.WriteToken(name.CStr());
				// NamedDropdowns should not store the actions, because the point of it is that it can be fetched by code
				// and actions can be added to the dropdown with code
				// if we don't do this this way we would eg save all the fonts in the font dropdown with their action!!
			}
			else
			{
				key.WriteTokenWithTrailingDigits("Dropdown", i);

				OpInputAction* action = widget->GetAction();

				if (action)
				{
					action->ConvertToString(value);
				}
			}
			break;
		}
		case WIDGET_TYPE_EDIT:
		{
			key.WriteTokenWithTrailingDigits("Edit", i);

			OpInputAction* action = widget->GetAction();

			if (action)
			{
				action->ConvertToString(value);
			}
			break;
		}
		case WIDGET_TYPE_ACCOUNT_DROPDOWN:
		{
			key.WriteTokenWithTrailingDigits("Account", i);
			break;
		}
		case WIDGET_TYPE_ADDRESS_DROPDOWN:
		{
			if (widget->GetName().HasContent())
			{
				OpStatus::Ignore(key.WriteTokenWithTrailingDigits("NamedAddress", i));
				OpStatus::Ignore(key.WriteToken8(widget->GetName().CStr()));
			}
			else
			{
				OpStatus::Ignore(key.WriteTokenWithTrailingDigits("Address", i));
			}
			break;
		}
        case WIDGET_TYPE_RESIZE_SEARCH_DROPDOWN:
        {
			if (widget->GetName().HasContent())
			{
				OpStatus::Ignore(key.WriteTokenWithTrailingDigits("NamedMultiresizesearch", i));
				OpStatus::Ignore(key.WriteToken8(widget->GetName().CStr()));
				OpStatus::Ignore(key.WriteValue(((OpResizeSearchDropDown *)widget)->GetCurrentWeightedWidth()));
			}
			else
			{
				OpStatus::Ignore(key.WriteTokenWithTrailingDigits("Multiresizesearch", i));
				OpStatus::Ignore(key.WriteValue(((OpResizeSearchDropDown *)widget)->GetCurrentWeightedWidth()));
			}
			break;
        }		
		case WIDGET_TYPE_SEARCH_DROPDOWN:
		{
			key.WriteTokenWithTrailingDigits("Multisearch", i);
			break;
		}
		case WIDGET_TYPE_SEARCH_EDIT:
		{
			key.WriteTokenWithTrailingDigits("Search", i);
			// Write the UniqueGUID of the search to the toolbar file
			key.WriteString(((OpSearchEdit*)widget)->GetSearchEditUniqueGUID().CStr());
			break;
		}
		case WIDGET_TYPE_SPACER:
		{
			key.WriteTokenWithTrailingDigits("Spacer", i);
			key.WriteValue(((OpSpacer*)widget)->GetSpacerType());
			if(((OpSpacer*)widget)->GetSpacerType() == OpSpacer::SPACER_FIXED)
			{
				key.WriteValue(((OpSpacer*)widget)->GetFixedSize());
			}
			break;
		}
		case WIDGET_TYPE_ZOOM_DROPDOWN:
		{
			key.WriteTokenWithTrailingDigits("Zoom", i);
			break;
		}
		case WIDGET_TYPE_STATUS_FIELD:
		{
			key.WriteTokenWithTrailingDigits("Status", i);
			key.WriteValue(((OpStatusField*)widget)->GetStatusType());

			INT32 font_size = widget->GetRelativeSystemFontSize();

			if (font_size != 100)
			{
				key.WriteValue(font_size);
			}
			break;
		}
		case WIDGET_TYPE_STATE_BUTTON:
		{
			OpStateButton *button = static_cast<OpStateButton *>(widget);

			key.WriteTokenWithTrailingDigits("StateButton", i);

			OP_ASSERT(button->GetModifier());

			if(button->GetModifier())
			{
				key.WriteString8(button->GetModifier()->GetDescriptionString());
			}
			break;
		}
		case WIDGET_TYPE_IDENTIFY_FIELD:
		{
			key.WriteTokenWithTrailingDigits("Identify", i);
			break;
		}
		case WIDGET_TYPE_PROGRESS_FIELD:
		{
			if (widget->GetName().HasContent())
			{
				key.WriteTokenWithTrailingDigits("NamedProgress", i);
				key.WriteValue(static_cast<OpProgressField*>(widget)->GetProgressType());
				key.WriteToken8(widget->GetName().CStr());
			}
			else
			{
				key.WriteTokenWithTrailingDigits("Progress", i);
				key.WriteValue(static_cast<OpProgressField*>(widget)->GetProgressType());
			}
			break;
		}
		case WIDGET_TYPE_PROGRESSBAR:
		{
			key.WriteTokenWithTrailingDigits("NamedProgressBar", i);
			key.WriteToken8(widget->GetName().CStr());
			break;
		}
		case WIDGET_TYPE_TOOLBAR_MENU_BUTTON:
		{
			key.WriteTokenWithTrailingDigits("MenuButton", i);
			key.WriteString8(widget->GetName());
			break;
		}
		case WIDGET_TYPE_LABEL:
		{
			key.WriteTokenWithTrailingDigits("Label", i);
			break;
		}
		case WIDGET_TYPE_ICON:
		{
			key.WriteTokenWithTrailingDigits("Icon", i);
			break;
		}
		case WIDGET_TYPE_NEW_PAGE_BUTTON:
		{
			key.WriteTokenWithTrailingDigits("NewPageButton", i);
			break;
		}
		case WIDGET_TYPE_ZOOM_SLIDER:
		{
			key.WriteTokenWithTrailingDigits("ZoomSlider", i);
			key.WriteValue(((OpZoomSlider*)widget)->GetShowTickLabels() ? 1 : 0);
			break;
		}
		case WIDGET_TYPE_ZOOM_MENU_BUTTON:
		{
			key.WriteTokenWithTrailingDigits("ZoomMenuButton", i);
			break;
		}
		case WIDGET_TYPE_EXTENSION_BUTTON:
		{
			key.WriteTokenWithTrailingDigits("ExtensionButton", i);
			key.WriteValue(static_cast<OpExtensionButton*>(widget)->GetId());
			break;
		}
		case WIDGET_TYPE_EXTENSION_SET:
		{
			key.WriteTokenWithTrailingDigits("ExtensionSet", i);
			break;
		}
		case WIDGET_TYPE_BROWSERVIEW:
		{
			key.WriteTokenWithTrailingDigits("Browser", i);
			break;
		}
		}

		if (key.HasContent())
		{
			OpString toolbar_name;
			toolbar_name.Set(name);
			TRAP(err, prefs_file->WriteStringL(toolbar_name, key.GetString(), value));
		}
	}
	//notify other toolbars or interested components that the setup has changed
	g_application->SettingsChanged(SETTINGS_TOOLBAR_CONTENTS);
}

void OpToolbar::EnableTransparentSkin(BOOL enable)
{
	if (enable)
		GetBorderSkin()->SetImage("Toolbar Transparent Skin", "Toolbar Skin");
	else
		GetBorderSkin()->SetImage("Toolbar Skin");
}

OpToolbar::~OpToolbar()
{
	OP_DELETE(m_auto_hide_timer);
}

void OpToolbar::SetAutoHide(BOOL auto_hide,UINT delay)
{
	if (auto_hide)
	{
		if (!m_auto_hide_timer)
		{
			m_auto_hide_timer = OP_NEW(OpTimer, ());
		}
	
		m_auto_hide_delay = delay;
		ResetAutoHideTimer();
	}
	else if (m_auto_hide_timer)
	{
		OP_DELETE(m_auto_hide_timer);
		m_auto_hide_timer = NULL;
		m_auto_hide_delay = 0;
	}
}

void OpToolbar::ResetAutoHideTimer()
{
	OP_ASSERT(m_auto_hide_timer && m_auto_hide_delay);
	if (m_auto_hide_timer && m_auto_hide_delay)
	{
		m_auto_hide_timer->SetTimerListener(this);
		m_auto_hide_timer->Start(m_auto_hide_delay);
	}
}

void OpToolbar::OnTimeOut(OpTimer* timer)
{
	if (timer == m_auto_hide_timer)
	{
		if (IsFocusedOrHovered())
		{
			// If focused or hovered we probably shouldn't hide it now. Restart the timer.
			ResetAutoHideTimer();
		}
		else
		{
			m_auto_hide_timer->Stop();
			CloseOnTimeOut();
		}
	}
	else
		OpBar::OnTimeOut(timer);
}

void OpToolbar::CloseOnTimeOut()
{
	SetAlignment(OpBar::ALIGNMENT_OFF);
}
