/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "modules/widgets/OpButton.h"
#include "modules/inputmanager/inputaction.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/widgets/WidgetContainer.h"
#ifdef BUTTON_GROUP_SUPPORT
#include "modules/widgets/OpButtonGroup.h"
#endif // BUTTON_GROUP_SUPPORT
#include "modules/widgets/OpWidgetStringDrawer.h"

#ifdef SKIN_SUPPORT
#include "modules/skin/OpSkinManager.h"
#endif

#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/display/vis_dev.h"

#ifdef QUICK
#include "adjunct/quick/managers/FavIconManager.h"
#include "adjunct/quick_toolkit/widgets/OpGroup.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/desktop_util/prefs/PrefsCollectionDesktopUI.h" 
#endif

/***********************************************************************************
**
**	OpButton
**
***********************************************************************************/

#ifdef WIDGETS_AUTOSCROLL_HIDDEN_CONTENT
# define BUTTON_MARQUEE_DELAY (m_x_scroll == 0 ? 1000 : GetInfo()->GetScrollDelay(FALSE, FALSE))
#endif // WIDGETS_AUTOSCROLL_HIDDEN_CONTENT

#ifdef SKIN_SUPPORT
OP_STATUS OpButton::Construct(OpButton** obj, ButtonType button_type, ButtonStyle button_style)
{
	*obj = OP_NEW(OpButton, (button_type, button_style));
#else
OP_STATUS OpButton::Construct(OpButton** obj, ButtonType button_type)
{
	*obj = OP_NEW(OpButton, (button_type));
#endif
	if (!*obj)
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsError((*obj)->init_status))
	{
		OP_DELETE(*obj);
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

DEFINE_CONSTRUCT(OpRadioButton)
DEFINE_CONSTRUCT(OpCheckBox)

#ifdef SKIN_SUPPORT
OpButton::OpButton(ButtonType button_type, ButtonStyle button_style)
	: m_button_type(button_type)
	, m_button_style(button_style)
	, m_button_extra(EXTRA_NONE)
	, m_hover_counter(0)
	, m_hover_start_time(0.0)
	, m_click_start_time(0.0)
	, m_action_to_use(NULL)
#else
OpButton::OpButton(ButtonType button_type) : m_button_type(button_type)
#endif
#ifdef HAS_TAB_BUTTON_POSITION
	, m_tab_state(0)
#endif
#ifdef WIDGETS_AUTOSCROLL_HIDDEN_CONTENT
	, m_x_scroll(0)
#endif
#ifdef BUTTON_GROUP_SUPPORT
	, m_button_group(NULL)
#endif
#ifdef QUICK
	, m_property_page(NULL)
#endif
{
	packed2.has_defaultlook = FALSE;
	packed2.has_forced_focuslook = FALSE;
	packed2.is_down = FALSE;
	packed2.is_outside = TRUE;
	packed2.is_pushed = FALSE;
	packed2.has_attention = FALSE;
	packed2.is_double_action = FALSE;
	packed2.is_click_animation_on = FALSE;
	packed2.is_bold = FALSE;
	packed2.has_fixed_type_and_style = FALSE;
	packed2.has_fixed_image = TRUE;
	packed2.show_shortcut = FALSE;
#ifdef WIDGETS_AUTOSCROLL_HIDDEN_CONTENT
	packed2.scroll_content = FALSE;
#endif // WIDGETS_AUTOSCROLL_HIDDEN_CONTENT

	string.Reset(this);
#ifdef SKIN_SUPPORT
	SetButtonTypeAndStyle(m_button_type, m_button_style);
	SetSkinned(TRUE);
#ifdef QUICK
	SetUpdateNeededWhen(UPDATE_NEEDED_WHEN_VISIBLE);
#endif
#else
	font_info.justify = JUSTIFY_CENTER;
	SetTabStop(TRUE);
#endif

}

#ifdef WIDGETS_CLONE_SUPPORT

OP_STATUS OpButton::CreateClone(OpWidget **obj, OpWidget *parent, INT32 font_size, BOOL expanded)
{
	*obj = NULL;
	OpButton *widget;

	RETURN_IF_ERROR(OpButton::Construct(&widget));
	parent->AddChild(widget);

	if (OpStatus::IsError(widget->CloneProperties(this, font_size)))
	{
		widget->Remove();
		OP_DELETE(widget);
		return OpStatus::ERR;
	}
	widget->SetDefaultLook(HasDefaultLook());
	widget->SetForcedFocusedLook(HasForcedFocusedLook());
#ifdef SKIN_SUPPORT
	widget->SetButtonTypeAndStyle(GetButtonType(), m_button_style, TRUE);
#endif
	*obj = widget;
	return OpStatus::OK;
}

#endif // WIDGETS_CLONE_SUPPORT

/***********************************************************************************
**
**	SetDefaultLook
**
***********************************************************************************/

void OpButton::SetDefaultLook(BOOL state)
{
	if ((BOOL)packed2.has_defaultlook != state)
	{
		packed2.has_defaultlook = state;
#ifdef SKIN_SUPPORT
		if (GetWidgetContainer())
		{
			GetWidgetContainer()->UpdateDefaultPushButton();
		}
		else
		{
			SetButtonType(packed2.has_defaultlook ? TYPE_PUSH_DEFAULT : TYPE_PUSH);
		}
#endif
	}
}

#ifdef SKIN_SUPPORT

/***********************************************************************************
**
**	SetButtonTypeAndStyle
**
***********************************************************************************/

void OpButton::SetButtonTypeAndStyle(ButtonType type, ButtonStyle style, BOOL force_style)
{
	m_button_type = type;
	m_button_style = style;

	const char* skin	= NULL;
	const char* fallback_skin	= NULL;
	BOOL tab_stop			= FALSE;
#ifdef QUICK
	OP_SYSTEM_FONT font		= OP_SYSTEM_FONT_UI_TOOLBAR;
#endif
	BOOL button_can_be_tab_stop = TRUE;
	button_can_be_tab_stop = g_op_ui_info->IsFullKeyboardAccessActive();

	switch (m_button_type)
	{
		case TYPE_PUSH:
			skin = "Push Button Skin";

			tab_stop = button_can_be_tab_stop;
#ifdef QUICK
			font = OP_SYSTEM_FONT_UI_DIALOG;
#endif
			m_button_style = STYLE_TEXT;
			break;

		case TYPE_PUSH_DEFAULT:
			skin = "Push Default Button Skin";
			tab_stop = button_can_be_tab_stop;
#ifdef QUICK
			font = OP_SYSTEM_FONT_UI_DIALOG;
#endif
			m_button_style = STYLE_TEXT;
			break;

		case TYPE_TOOLBAR:
			skin = "Toolbar Button Skin";
			SetFixedImage(FALSE);
#ifdef WIDGETS_TABS_AND_TOOLBAR_BUTTONS_ARE_TAB_STOP
			tab_stop = button_can_be_tab_stop;
#endif
			break;

		case TYPE_SELECTOR:
			skin = "Selector Button Skin";
			break;

		case TYPE_LINK:
			skin = "Link Button Skin";
			break;

		case TYPE_TAB:
			skin = "Tab Button Skin";
#ifdef QUICK
			font = OP_SYSTEM_FONT_UI_DIALOG;
#endif
#ifdef WIDGETS_TABS_AND_TOOLBAR_BUTTONS_ARE_TAB_STOP
			tab_stop = button_can_be_tab_stop;
#endif
			break;

		case TYPE_PAGEBAR:
			skin = "Pagebar Button Skin";
#ifdef WIDGETS_TABS_AND_TOOLBAR_BUTTONS_ARE_TAB_STOP
			tab_stop = button_can_be_tab_stop;
#endif
			break;

		case TYPE_HEADER:
			skin = "Header Button Skin";
#ifdef QUICK
			font = OP_SYSTEM_FONT_UI_DIALOG;
#endif
			break;

		case TYPE_MENU:
			skin = "Menu Button Skin";
#ifdef QUICK
			font = OP_SYSTEM_FONT_UI_MENU;
#endif
			break;

		case TYPE_OMENU:
			skin = "Opera Menu Button Skin";
#ifdef QUICK
			font = OP_SYSTEM_FONT_UI_MENU;
#endif
			break;

		case TYPE_START:
			skin = "Start Button Skin";
			fallback_skin = "Push Button Skin";
#ifdef QUICK
			font = OP_SYSTEM_FONT_UI_DIALOG;
#endif
			tab_stop = button_can_be_tab_stop;
			break;

		case TYPE_RADIO:
			tab_stop = button_can_be_tab_stop;
#ifdef QUICK
			font = OP_SYSTEM_FONT_UI_DIALOG;
#endif
			m_button_style = STYLE_TEXT;
			break;

		case TYPE_CHECKBOX:
			tab_stop = button_can_be_tab_stop;
#ifdef QUICK
			font = OP_SYSTEM_FONT_UI_DIALOG;
#endif
			m_button_style = STYLE_TEXT;
			break;
	}

	if (force_style)
		m_button_style = style;

	font_info.justify = (m_button_style == STYLE_IMAGE_AND_TEXT_ON_RIGHT || m_button_style == STYLE_IMAGE_AND_TEXT_ON_LEFT) ? JUSTIFY_LEFT : JUSTIFY_CENTER;
	if (GetRTL() && font_info.justify == JUSTIFY_LEFT)
		font_info.justify = JUSTIFY_RIGHT;

	SetTabStop(tab_stop);

#ifdef QUICK
	if (!IsForm())
		SetSystemFont(font);
#endif
	// only reset the skin if this is one of the known types and not TYPE_CUSTOM
	if(m_button_type != TYPE_CUSTOM)
		GetBorderSkin()->SetImage(skin, fallback_skin);
}

OpInputAction* OpButton::GetNextClickAction(BOOL plus_action)
{
	OpInputAction* action = GetAction();

	if (plus_action)
	{
		action = action->GetNextInputAction(OpInputAction::OPERATOR_PLUS);
	}
	else if (packed2.is_double_action && packed2.is_pushed)
	{
		action = action->GetNextInputAction(OpInputAction::OPERATOR_OR);
	}
	else
	{
		while (action && (action->GetActionState() & OpInputAction::STATE_DISABLED) == OpInputAction::STATE_DISABLED)
			action = action->GetNextInputAction();
	}

	return action;
}

void OpButton::Click(BOOL plus_action /* = FALSE */)
{
	if (!IsEnabled())
		return;

	OpInputAction* action = GetNextClickAction(plus_action);

	if (action)
	{
		OpRect rect = GetRect(TRUE);
		OpPoint point = vis_dev->GetView()->ConvertToScreen(OpPoint(rect.x, rect.y));
		rect.x = point.x;
		rect.y = point.y;
		action->SetActionPosition(rect);

#if defined(_UNIX_DESKTOP_)
		// We will not receive any events until the popup menu is closed
		// and we need to indicate that the button is no longer active
		// There is no global pointer telling what button is pressed down.
		if( HasDropdown())
		{
			OnMouseLeave();
		}
#endif
		g_input_manager->InvokeAction(action, GetParentInputContext());
		// Hmm, what if the called action kills the button? (ie: something that
		// results in a "delete this")
		// That will crash below.
	}
	else if (m_button_type == TYPE_CHECKBOX)
	{
		// if is has no action, just autotoggle
		Toggle();
		return;
	}
#ifdef BUTTON_GROUP_SUPPORT
	if (m_button_group)
	{
		// AutoRadioButton when part of a group, but still call onclick later too
		SetValue(TRUE);
	}
#endif // BUTTON_GROUP_SUPPORT

	if (listener)
	{
		listener->OnClick(this, GetID()); // Invoke!
	}
}

void OpButton::SetAction(OpInputAction* action)
{
	OpWidget::SetAction(action);

	m_action_to_use = action;

	m_button_extra = EXTRA_NONE;
	BOOL plus = FALSE;

	while (action)
	{
#ifdef _POPUP_MENU_SUPPORT_
		if (action->GetAction() == OpInputAction::ACTION_SHOW_POPUP_MENU)
		{
			m_button_extra = plus ? EXTRA_PLUS_DROPDOWN : EXTRA_DROPDOWN;
		}

		if (action->GetAction() == OpInputAction::ACTION_SHOW_HIDDEN_POPUP_MENU)
		{
			m_button_extra = plus ? EXTRA_PLUS_HIDDEN_DROPDOWN : EXTRA_HIDDEN_DROPDOWN;
		}
#endif

#ifdef ACTION_SHOW_MENU_SECTION_ENABLED
		if (action->GetAction() == OpInputAction::ACTION_SHOW_MENU_SECTION)
			m_button_extra = EXTRA_SUB_MENU;
#endif

		if (!plus && action->GetActionOperator() == OpInputAction::OPERATOR_PLUS)
		{
			m_button_extra = EXTRA_PLUS_ACTION;
			plus = TRUE;
		}

		action = action->GetNextInputAction();
	}

	if (m_button_type != TYPE_LINK)
	{
		m_dropdown_image.SetImage(m_button_extra == EXTRA_PLUS_DROPDOWN || m_button_extra == EXTRA_DROPDOWN ? "Dropdown" : NULL);
	}
}
#ifdef QUICK
BOOL OpButton::HasToolTipText(OpToolTip* tooltip)
{
	if (m_button_type == TYPE_TOOLBAR || m_button_style == STYLE_IMAGE)
		return TRUE;

	if (m_action_to_use && (m_action_to_use->GetActionInfo().HasTooltipText() || m_action_to_use->GetActionInfo().HasStatusText()))
		return TRUE;

	if (IsDebugToolTipActive())
		return TRUE;

	return FALSE;
}

void OpButton::GetToolTipText(OpToolTip* tooltip, OpInfoText& text)
{
	// Check for the debug tooltip first
	if (IsDebugToolTipActive())
	{
		// Implemented in QuickOpWidgetBase
		OpWidget::GetToolTipText(tooltip, text);
		return;
	}

	OpString tooltip_string;

	if (m_action_to_use && m_action_to_use->GetActionInfo().HasTooltipText())
	{
		//Use tooltip text for action, if it has been set
		//(Ignore OOM errors, in worst case the tooltip is empty)
		OpStatus::Ignore(tooltip_string.Set(m_action_to_use->GetActionInfo().GetTooltipText()));
	}
	else if (m_action_to_use && m_action_to_use->GetActionInfo().HasStatusText())
	{
		//Use text meant for status field as fallback
		OpStatus::Ignore(tooltip_string.Set(m_action_to_use->GetActionInfo().GetStatusText()));
	}
	else
	{
		//No informative tooltip available :( Use button text to at least show something
		OpStatus::Ignore(tooltip_string.Set(string.Get()));
	}

	//Find the keyboard shortcut for this button, so it can be shown in tooltip
	OpString shortcut_string;
	GetShortcutString(shortcut_string);

	if (shortcut_string.HasContent())
	{
		//A keyboard shortcut has been found, add it at the end of the tooltip
		tooltip_string.AppendFormat(UNI_L(" (%s)"), shortcut_string.CStr());
	}
	//Finally, set the composed string as the tooltip that should be shown
	text.SetTooltipText(tooltip_string.CStr());
}

INT32 OpButton::GetToolTipDelay(OpToolTip* tooltip)
{
	if (m_button_type == TYPE_SELECTOR && m_button_style == STYLE_IMAGE)
	{
		return 200;
	}
	return OpToolTipListener::GetToolTipDelay(tooltip);
}
#endif

#ifdef BUTTON_GROUP_SUPPORT
BOOL OpButton::RegisterToButtonGroup(OpButtonGroup* button_group)
{
	if (!m_button_group)
	{
		m_button_group = button_group;
		m_button_group->RegisterButton(this);
		return TRUE;
	}
		
	return FALSE;
}

void OpButton::OnRemoved()
{
	if (m_button_group)
	{
		m_button_group->RemoveButton(this);
		m_button_group = 0;
	}
}
#endif // BUTTON_GROUP_SUPPORT


void OpButton::GetShortcutString(OpString& string)
{
#ifdef QUICK
	OpInputAction* action = OpInputAction::CopyInputActions(m_action_to_use, OpInputAction::OPERATOR_PLUS | OpInputAction::OPERATOR_NEXT | OpInputAction::OPERATOR_OR);

	g_input_manager->GetShortcutStringFromAction(action, string, this);

	OpInputAction::DeleteInputActions(action);
#endif
}

void OpButton::UpdateInfoText()
{
#ifdef QUICK
	if (!packed2.is_outside && GetParentDesktopWindow())
	{
		if (m_action_to_use && m_action_to_use->GetActionInfo().HasStatusText())
		{
			GetParentDesktopWindow()->SetStatusText(m_action_to_use->GetActionInfo().GetStatusText());
		}
		else
		{
			OpString full_string;
			OpString shortcut_string;

			OpStatus::Ignore(full_string.Set(string.Get()));
			GetShortcutString(shortcut_string);

			if (shortcut_string.HasContent())
			{
				full_string.Append(UNI_L(" ("));
				full_string.Append(shortcut_string.CStr());
				full_string.Append(UNI_L(")"));
			}

			GetParentDesktopWindow()->SetStatusText(full_string);
		}
	}
#endif //QUICK
}

OpInputAction* OpButton::GetAction()
{
	if (OpWidget::GetAction() && vis_dev && vis_dev->GetView())
	{
		OpRect rect = GetRect(TRUE);
		OpPoint point = vis_dev->GetView()->ConvertToScreen(OpPoint(rect.x, rect.y));
		rect.x = point.x;
		rect.y = point.y;
		OpWidget::GetAction()->SetActionPosition(rect);
	}

	return OpWidget::GetAction();
}

#endif // SKIN_SUPPORT

OP_STATUS OpButton::SetText(const uni_char* text)
{
	// only if text changes

	const uni_char* old_text = string.Get();

	if (text && old_text && uni_strcmp(text, old_text) == 0)
		return OpStatus::OK;

	OP_STATUS status = string.Set(text, this);

#ifdef QUICK
	OpStatus::Ignore(m_text.Set(text));
	Relayout();
#else
	InvalidateAll();
#endif // QUICK

#ifdef WIDGETS_AUTOSCROLL_HIDDEN_CONTENT
	m_x_scroll = 0;
	if (!IsTimerRunning() && ShowHiddenContent())
		StartTimer(BUTTON_MARQUEE_DELAY);
#endif // WIDGETS_AUTOSCROLL_HIDDEN_CONTENT

	return status;
}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

OpAccessibleItem* OpButton::GetAccessibleParent()
{
#ifdef BUTTON_GROUP_SUPPORT
	if (m_button_group)
		return m_button_group;
	else
#endif // BUTTON_GROUP_SUPPORT
		return OpWidget::GetAccessibleParent();
}

OP_STATUS OpButton::AccessibilityClicked()
{
	Click();
	return OpStatus::OK;
}

OP_STATUS OpButton::AccessibilityGetText(OpString& text)
{
	if (m_accessibility_text.Length())
		return text.Set(m_accessibility_text.CStr());
	return text.Set(string.Get());
}

Accessibility::ElementKind OpButton::AccessibilityGetRole() const
{
	switch (m_button_type)
	{
		case TYPE_PUSH:
		case TYPE_PUSH_DEFAULT:
			return Accessibility::kElementKindButton;

		case TYPE_TOOLBAR:
		case TYPE_SELECTOR:
		case TYPE_LINK:
			return Accessibility::kElementKindButton;

		case TYPE_TAB:
		case TYPE_PAGEBAR:
			return Accessibility::kElementKindTab;

		case TYPE_HEADER:
			return Accessibility::kElementKindListHeader;
		case TYPE_MENU:
		case TYPE_OMENU:
		case TYPE_START:
			return Accessibility::kElementKindButton;

		case TYPE_RADIO:
			return Accessibility::kElementKindRadioButton;

		case TYPE_CHECKBOX:
			return Accessibility::kElementKindCheckbox;
	}
	return Accessibility::kElementKindButton;
}

Accessibility::State OpButton::AccessibilityGetState()
{
	Accessibility::State state = OpWidget::AccessibilityGetState();
	if (packed2.has_defaultlook)
		state |= Accessibility::kAccessibilityStateDefaultButton;
	if ((m_button_type == TYPE_CHECKBOX || m_button_type == TYPE_RADIO) && GetValue())
		state |= Accessibility::kAccessibilityStateCheckedOrSelected;
	return state;
}

#ifdef PROPERTY_PAGE_CHILDREN_OF_TABS

int	OpButton::GetAccessibleChildrenCount()
{
#ifdef PROPERTY_PAGE_CHILDREN_OF_TABS
	if (m_button_type == TYPE_TAB && m_property_page)
		return 1;
	else
#endif //PROPERTY_PAGE_CHILDREN_OF_TABS
		return OpWidget::GetAccessibleChildrenCount();
}

OpAccessibleItem* OpButton::GetAccessibleChild(int childNr)
{
#ifdef PROPERTY_PAGE_CHILDREN_OF_TABS
	if (m_button_type == TYPE_TAB && m_property_page)
		return (childNr == 0? m_property_page: NULL);
	else
#endif //PROPERTY_PAGE_CHILDREN_OF_TABS
		return OpWidget::GetAccessibleChild(childNr);
}

int OpButton::GetAccessibleChildIndex(OpAccessibleItem* child)
{
#ifdef PROPERTY_PAGE_CHILDREN_OF_TABS
	if (m_button_type == TYPE_TAB && m_property_page)
		return (child == m_property_page? 0: Accessibility::NoSuchChild);
	else
#endif //PROPERTY_PAGE_CHILDREN_OF_TABS
		return OpWidget::GetAccessibleChildIndex(child);
}


OpAccessibleItem* OpButton::GetAccessibleChildOrSelfAt(int x, int y)
{
	OpAccessibleItem* is_self = OpWidget::GetAccessibleChildOrSelfAt(x,y);
	if (is_self)
		return is_self;
#ifdef PROPERTY_PAGE_CHILDREN_OF_TABS
	if (m_button_type == TYPE_TAB && m_property_page)
		if (m_property_page->GetAccessibleChildOrSelfAt(x, y))
			return m_property_page;
#endif //PROPERTY_PAGE_CHILDREN_OF_TABS
	return NULL;
}

OpAccessibleItem* OpButton::GetAccessibleFocusedChildOrSelf()
{
	OpAccessibleItem* is_self = OpWidget::GetAccessibleFocusedChildOrSelf();
	if (is_self)
		return is_self;

#ifdef PROPERTY_PAGE_CHILDREN_OF_TABS
	if (m_button_type == TYPE_TAB && m_property_page)
		if (m_property_page->GetAccessibleFocusedChildOrSelf())
			return m_property_page;
#endif //PROPERTY_PAGE_CHILDREN_OF_TABS

	return NULL;
}


#endif // PROPERTY_PAGE_CHILDREN_OF_TABS

OP_STATUS OpButton::SetAccessibilityText(const uni_char* text)
{
	return m_accessibility_text.Set(text);
}

#endif

OP_STATUS OpButton::GetText(OpString& text)
{
#ifdef QUICK
	return text.Set(m_text.CStr());
#else
	return text.Set(string.Get());
#endif
}

void OpButton::SetValue(INT32 value)
{
	if (static_cast<bool>(packed2.is_pushed) != !!value)
	{
		packed2.is_pushed = !!value;
		Update();

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
		if (m_button_type == TYPE_CHECKBOX || m_button_type == TYPE_RADIO)
			AccessibilitySendEvent(Accessibility::Event(Accessibility::kAccessibilityStateCheckedOrSelected));
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

	}
#ifdef BUTTON_GROUP_SUPPORT
	if(value && m_button_group)
		m_button_group->ButtonSet(this);
#endif // BUTTON_GROUP_SUPPORT

	if (listener)
	{
		listener->OnChange(this);
	}
}

void OpButton::SetAttention(BOOL attention)
{
	if (packed2.has_attention != (UINT32) attention)
	{
		packed2.has_attention = attention;
		Update();
	}
}

#ifdef QUICK
void OpButton::GetActionState(OpInputAction*& action_to_use,
		INT32& state_to_use, BOOL& next_operator_used)
{
	action_to_use = NULL;
	state_to_use = 0;
	next_operator_used = FALSE;

	OpInputAction* action = OpWidget::GetAction();
	if (!action)
		return;

	// Action to base state on chosen by priority:
	// 1. it is selected
	// 2. it is enabled
	// 3. first one.

	while (action)
	{
		if (action->GetActionOperator() == OpInputAction::OPERATOR_NEXT)
			next_operator_used = TRUE;

		INT32 state = g_input_manager->GetActionState(action, GetParentInputContext());

		if (!action_to_use
			|| (!(state & OpInputAction::STATE_DISABLED) && (state_to_use & OpInputAction::STATE_DISABLED) && !(state_to_use & OpInputAction::STATE_ENABLED))
			|| (state & OpInputAction::STATE_SELECTED))
		{
			action_to_use = action;
			state_to_use = state;
		}

		if (state & OpInputAction::STATE_SELECTED || (state & OpInputAction::STATE_UNSELECTED && !next_operator_used))
		{
			if (state & OpInputAction::STATE_DISABLED)
				state_to_use &= ~OpInputAction::STATE_SELECTED;

			break;
		}

		action = action->GetActionOperator() != OpInputAction::OPERATOR_PLUS ? action->GetNextInputAction() : NULL;
	}
}

/***********************************************************************************
**
**	OnUpdateActionState
**
***********************************************************************************/

void OpButton::OnUpdateActionState()
{
	INT32 state_to_use = 0;
	BOOL next_operator_used = FALSE;
	GetActionState(m_action_to_use, state_to_use, next_operator_used);
	if (!m_action_to_use)
		return;

	packed2.is_pushed = state_to_use & OpInputAction::STATE_SELECTED && !next_operator_used;
	packed2.has_attention = (state_to_use & OpInputAction::STATE_ATTENTION) ? 1 : 0;

	SetEnabled(!(state_to_use & OpInputAction::STATE_DISABLED) || state_to_use & OpInputAction::STATE_ENABLED || IsCustomizing());

	if (!HasFixedImage())
	{
		if (m_action_to_use->GetAction() == OpInputAction::ACTION_GO_TO_PAGE && !m_action_to_use->HasActionImage())
		{
			Image img = g_favicon_manager->Get(m_action_to_use->GetActionDataString());
			GetForegroundSkin()->SetBitmapImage(img, FALSE);
			GetForegroundSkin()->SetRestrictImageSize(TRUE);

			if (!GetForegroundSkin()->HasBitmapImage())
			{
				GetForegroundSkin()->SetImage(m_action_to_use->GetActionImage());
				GetForegroundSkin()->SetRestrictImageSize(FALSE);
			}
		}
		else 
#ifdef EXTENSION_SUPPORT
		if (m_action_to_use->GetAction() == OpInputAction::ACTION_EXTENSION_ACTION
			? !GetForegroundSkin()->HasContent() : TRUE)
#endif // EXTENSION_SUPPORT
		{
			GetForegroundSkin()->SetImage(NULL, m_action_to_use->GetActionImage());
		}
	}

	OpStringC new_text = m_action_to_use->GetActionText();

	if (!new_text || !*new_text)
	{
		new_text = m_text.CStr();
	}
	if (!new_text.CStr())
		// Avoid Relayout below from being called when new_text has no buffer and string is empty.
		new_text = UNI_L("");

	if (new_text.Compare(string.Get()))
	{
		OpStatus::Ignore(string.Set(new_text, this));
		Relayout();
	}

	Update();
	UpdateInfoText();
}
#endif

void OpButton::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	string.NeedUpdate();
#ifdef QUICK
	UpdateActionStateIfNeeded();
#endif
	GetInfo()->GetPreferedSize(this, GetType(), w, h, cols, rows);
}

void OpButton::GetMinimumSize(INT32* minw, INT32* minh)
{
	GetInfo()->GetMinimumSize(this, OpTypedObject::WIDGET_TYPE_BUTTON, minw, minh);
}

void OpButton::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	if (m_button_type == TYPE_RADIO)
		widget_painter->DrawRadiobutton(GetBounds(), !!GetValue());
	else if (m_button_type == TYPE_CHECKBOX)
		widget_painter->DrawCheckbox(GetBounds(), !!GetValue());
	else
		widget_painter->DrawButton(GetBounds(), packed2.is_click_animation_on || ((packed2.is_down && !packed2.is_outside) || packed2.is_pushed), packed2.has_defaultlook);
}

void OpButton::PaintContent(OpRect text_rect, UINT32 fcol)
{
	VisualDevice * vd = GetVisualDevice();

	int padding_left = GetPaddingLeft();
	int padding_right = GetPaddingRight();
	int padding_top = GetPaddingTop();
	int padding_bottom = GetPaddingBottom();

	int overflow = string.GetWidth() + padding_left + padding_right - text_rect.width;
	if (overflow > 0)
	{
		// stringwidth + padding is to wide for the button. Cut away as much as possible from padding.
		padding_left = MAX(0, padding_left - overflow / 2);
		padding_right = MAX(0, padding_right - overflow / 2);
	}

	overflow = string.GetHeight() + padding_top + padding_bottom - text_rect.height;
	if (overflow > 0)
	{
		// stringheight + padding is to high for the button. Cut away as much as possible from padding.
		if (padding_top + padding_bottom > 0)
		{
			int ratio_top = 1000 * padding_top / (padding_top + padding_bottom);
			int ratio_bottom = 1000 * padding_bottom / (padding_top + padding_bottom);
			padding_top = MAX(0, padding_top - (overflow * ratio_top) / 1000);
			padding_bottom = MAX(0, padding_bottom - (overflow * ratio_bottom) / 1000);
		}
	}

	text_rect.x += padding_left;
	text_rect.width -= padding_left + padding_right;

	text_rect.y += padding_top;
	text_rect.height -= padding_top + padding_bottom;

	INT32 x_scroll = 0;

#ifdef WIDGETS_AUTOSCROLL_HIDDEN_CONTENT
	x_scroll = m_x_scroll;
	if (x_scroll > string.GetWidth() - text_rect.width)
		x_scroll = string.GetWidth() - text_rect.width;
	if (string.GetWidth() < text_rect.width)
		x_scroll = 0;
#endif

	ELLIPSIS_POSITION ellipsis_position = IsForm() ? ELLIPSIS_NONE : GetEllipsis();
	OpWidgetStringDrawer string_drawer;
	string_drawer.Draw(&string, text_rect, vd, fcol, ellipsis_position, x_scroll);
}

void OpButton::OnMouseMove(const OpPoint &point)
{
	BOOL old_is_outside = packed2.is_outside;
	if (GetBounds().Contains(point))
		packed2.is_outside = FALSE;
	else if (GetInfo()->CanDropButton())
		packed2.is_outside = TRUE;

	if ((BOOL)packed2.is_outside != old_is_outside)
		InvalidateAll();

#ifdef SKIN_SUPPORT
	if (IsDead())
		packed2.is_outside = TRUE;

	if (packed2.is_outside != (UINT32) old_is_outside && !packed2.is_click_animation_on)
	{
		Update();
		UpdateInfoText();

		if (!packed2.is_outside && m_hover_start_time == 0 && g_pccore->GetIntegerPref(PrefsCollectionCore::SpecialEffects) || (m_button_extra == EXTRA_PLUS_HIDDEN_DROPDOWN) )
		{
			m_hover_start_time = g_op_time_info->GetWallClockMS();
			StartTimer(10);
		}

		if (packed2.is_outside && packed2.is_down)
		{
			if (HasDropdown())
			{
				if (point.y >= GetHeight())
				{
					m_click_start_time = 0;
					packed2.is_pushed = TRUE;
					Update();
					Sync();
					Click(m_button_extra != EXTRA_DROPDOWN && m_button_extra != EXTRA_HIDDEN_DROPDOWN);
					packed2.is_pushed = FALSE;
					packed2.is_down = FALSE;
					Update();
					return;
				}
			}
			else if (packed2.is_double_action)
			{
				packed2.is_pushed = TRUE;
				Click();
			}
		}
	}
#endif
}

void OpButton::OnMouseLeave()
{
	packed2.is_outside = TRUE;
	packed2.is_down = FALSE;
#ifdef SKIN_SUPPORT
	m_click_start_time = 0;

	// start timer to blend out hover effect
	if (m_hover_counter)
	{
		m_hover_start_time = g_op_time_info->GetWallClockMS();
		StartTimer(10);
	}
#endif
#ifdef QUICK
	if(GetParentDesktopWindow())
	{
		OpString empty;
		GetParentDesktopWindow()->SetStatusText(empty);
	}
#endif

	Update();
}

void OpButton::OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	if (listener)
	{
#ifndef MOUSELESS
		listener->OnMouseEvent(this, -1, point.x, point.y, button, TRUE, nclicks);
#endif // MOUSELESS
	}

#ifdef SKIN_SUPPORT
	// Quick-n-dirty fix. UNIX behavior. I guess everyone wants it.
	if( button == MOUSE_BUTTON_3 )
	{
		OpInputAction* action = GetAction();
		if (action)
		{
			switch (action->GetAction())
			{
# ifdef ACTION_OPEN_LINK_ENABLED
			case OpInputAction::ACTION_OPEN_LINK:
				g_input_manager->InvokeAction(OpInputAction::ACTION_OPEN_LINK_IN_NEW_PAGE,action->GetActionData());
				return;
# endif // ACTION_OPEN_LINK_ENABLED

# ifdef ACTION_NEW_PAGE_ENABLED
			case OpInputAction::ACTION_NEW_PAGE:
				g_input_manager->InvokeAction(OpInputAction::ACTION_NEW_PAGE,action->GetActionData());
				return;
# endif // ACTION_NEW_PAGE_ENABLED
			}
		}
	}
#endif // SKIN_SUPPORT

	if (button != MOUSE_BUTTON_1 || IsDead())
		return;

#ifdef SKIN_SUPPORT
	if (!IsForm() && (m_button_type == TYPE_PUSH || m_button_type == TYPE_PUSH_DEFAULT || m_button_type == TYPE_RADIO || m_button_type == TYPE_CHECKBOX || m_button_type == TYPE_CUSTOM))
		SetFocus(FOCUS_REASON_MOUSE);

	BOOL in_split_dropdown = FALSE;

	if (m_button_extra == EXTRA_PLUS_DROPDOWN)
	{
		INT32 left, top, right, bottom;
		GetPadding(&left, &top, &right, &bottom);

		INT32 image_width;
		INT32 image_height;
		m_dropdown_image.GetSize(&image_width, &image_height);

		INT32 dropdown_left, dropdown_top, dropdown_right, dropdown_bottom;
		m_dropdown_image.GetMargin(&dropdown_left, &dropdown_top, &dropdown_right, &dropdown_bottom);

		INT32 dropdown_width = dropdown_left + dropdown_right + left + right + image_width;
		in_split_dropdown = GetRTL() ? point.x < dropdown_width : point.x > GetBounds().width - dropdown_width;
	}

	if (in_split_dropdown)
	{
		m_click_start_time = 0;
		packed2.is_pushed = TRUE;
		Update();
		Sync();
		Click(in_split_dropdown);
		packed2.is_pushed = FALSE;
		packed2.is_down = FALSE;
		Update();
	}
	else
#endif
	{
#ifdef SKIN_SUPPORT
		if (packed2.is_double_action  || m_button_type == TYPE_TAB)
		{
			Click();
		}
#endif
		packed2.is_outside = FALSE;
		packed2.is_down = TRUE;
#ifdef SKIN_SUPPORT
		if (m_button_extra == EXTRA_PLUS_ACTION || m_button_extra == EXTRA_PLUS_HIDDEN_DROPDOWN)
		{
			m_click_start_time = g_op_time_info->GetWallClockMS();
			// start timer for long-click functionality
			if (!IsTimerRunning())
				StartTimer(500);
		}
#endif
		Update();
	}
}

void OpButton::OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	if (listener)
	{
#ifndef MOUSELESS
		listener->OnMouseEvent(this, -1, point.x, point.y, button, FALSE, nclicks);
#endif // MOUSELESS
	}

	if (button != MOUSE_BUTTON_1 || packed2.is_down == FALSE  || IsDead())
		return;

#ifdef SKIN_SUPPORT
	if (!packed2.is_outside && (m_button_extra == EXTRA_DROPDOWN || m_button_extra == EXTRA_HIDDEN_DROPDOWN))
	{
		m_click_start_time = 0;
		packed2.is_pushed = TRUE;
		Update();
		Sync();
		Click();
		packed2.is_pushed = FALSE;
		packed2.is_down = FALSE;
		Update();
		return;
	}
#endif

	packed2.is_down = FALSE;
#ifdef SKIN_SUPPORT
	m_click_start_time = 0;
#endif

	Update();

	if (!packed2.is_outside)
	{
#ifdef SKIN_SUPPORT
		if (packed2.is_double_action)
		{
			packed2.is_pushed = TRUE;
		}
		Click();
#else
		if (m_button_type == TYPE_CHECKBOX)
		{
			Toggle();
		}
		else if (listener)
		{
			listener->OnClick(this, GetID()); // Invoke!
		}
#endif
	}
}

/***********************************************************************************
**
**	Update()
**
***********************************************************************************/

void OpButton::Update()
{
#ifdef SKIN_SUPPORT
	INT32 state = 0;

	if (packed2.has_attention)
	{
		state |= SKINSTATE_ATTENTION;
	}

	if (packed2.is_pushed)
	{
		state |= SKINSTATE_SELECTED;
	}
	// other styles are handled in the painter
	if (m_button_style == OpButton::STYLE_IMAGE)
	{
		if(packed2.has_forced_focuslook)
		{
			state |= SKINSTATE_HOVER;
		}
	}
	if (packed2.is_click_animation_on || (packed2.is_down && !packed2.is_outside))
	{
		state |= SKINSTATE_PRESSED;
	}

	if (!IsEnabled())
	{
		state |= SKINSTATE_DISABLED;
	}

	INT32 hover_counter = m_hover_counter;

#ifdef QUICK
	if (!packed2.is_outside)
	{
		state |= SKINSTATE_HOVER;

		if (m_button_type == TYPE_MENU || m_button_type == TYPE_OMENU)
		{
			hover_counter = 100;
		}
	}
#endif

	if (IsMiniSize())
	{
		state |= SKINSTATE_MINI;
	}

	if (GetRTL())
	{
		state |= SKINSTATE_RTL;
	}

	if (state != GetBorderSkin()->GetState() && !IsForm())
	{
#if defined(_QUICK_UI_FONT_SUPPORT_)
		// If font says it is bold then we have to use that.
		FontAtt font;
		g_op_ui_info->GetFont(GetSystemFont(),font,FALSE);
		if( font.GetWeight() > 4 )
		{
			font_info.weight = font.GetWeight();
		}
		else
		{
			BOOL bold = FALSE;
			GetSkinManager()->GetTextBold(GetBorderSkin()->GetImage(), &bold, state, GetBorderSkin()->GetType());
			font_info.weight = bold || packed2.is_bold ? 7 : 4;
		}
#else
		BOOL bold = FALSE;

		GetSkinManager()->GetTextBold(GetBorderSkin()->GetImage(), &bold, state, GetBorderSkin()->GetType());

		font_info.weight = bold || packed2.is_bold ? 7 : 4;
#endif

		string.NeedUpdate();
	}

	GetBorderSkin()->SetState(state, -1, hover_counter);
	GetForegroundSkin()->SetState(state, -1, hover_counter);
	m_dropdown_image.SetState(state, -1, hover_counter);
#else
	InvalidateAll();
#endif
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL OpButton::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_CHECK_ITEM:
		{
			if (!GetAction() && !GetValue())
			{
				Toggle();
				if (listener)
					listener->OnGeneratedClick(this, GetID());
				return TRUE;
			}
			break;
		}

		case OpInputAction::ACTION_UNCHECK_ITEM:
		{
			if (!GetAction() && GetValue())
			{
				Toggle();
				if (listener)
					listener->OnGeneratedClick(this, GetID());
				return TRUE;
			}
			break;
		}

#ifdef _MACINTOSH_		
		// On Mac default buttons behave differently to other platforms in 2 ways:
		// 1. In web pages there is no default button so this will do nothing.
		// 2. In dialogs the default button is always the same so this action will
		//    seek out that default button and click it if any other button is pressed
		//    using the keyboard.
		case OpInputAction::ACTION_CLICK_DEFAULT_BUTTON:
		{
			WidgetContainer *widget_container = GetWidgetContainer();
			BOOL 			return_value = FALSE;
			
			// Check that there is a widget container
			if (widget_container != NULL) 
				return_value = widget_container->OnInputAction(action);
			
			return return_value;
		}
#endif
		
		case OpInputAction::ACTION_CLICK_BUTTON:
		{
#ifdef SKIN_SUPPORT
			AnimateClick();
#else
			if (listener)
			{
				listener->OnClick(this, GetID()); // Invoke!
			}
#endif // SKIN_SUPPORT
			if (listener)
			{
				listener->OnGeneratedClick(this, GetID());
			}
			return TRUE;
		}
	}
	return FALSE;
}

void OpButton::Toggle()
{
	SetValue(!GetValue());

	if (listener)
	{
		listener->OnClick(this);
		//OnChange is already called from SetValue!
		//listener->OnChange(this);
	}
}


#ifdef SKIN_SUPPORT
void OpButton::AnimateClick()
{
	if( m_button_type == TYPE_PUSH_DEFAULT || m_button_type == TYPE_PUSH )
	{
		if( !packed2.is_click_animation_on )
		{
			packed2.is_click_animation_on = TRUE;
			Update();
			StartTimer(100);
		}
	}
	else
	{
		Click();
	}
}
#endif // SKIN_SUPPORT


void OpButton::SetEnabled(BOOL enabled)
{
	if (IsEnabled() != enabled)
	{
		OpWidget::SetEnabled(enabled);

#ifdef SKIN_SUPPORT
		if (GetWidgetContainer())
		{
			GetWidgetContainer()->UpdateDefaultPushButton();
		}
#endif
	}
}

void OpButton::OnFocus(BOOL focus,FOCUS_REASON reason)
{
#ifdef SKIN_SUPPORT
	if (GetWidgetContainer())
	{
		GetWidgetContainer()->UpdateDefaultPushButton();
	}

#ifdef _MACINTOSH_
	// Only need focus redrawing for push butons
	if (m_button_type == TYPE_PUSH_DEFAULT || m_button_type == TYPE_PUSH) {
		// this is to draw the focusborder
		Invalidate(GetBounds().InsetBy(-2,-2), FALSE, TRUE);
 	}
#endif
#endif
#ifdef WIDGETS_AUTOSCROLL_HIDDEN_CONTENT
	if (string.GetWidth() > GetBounds().width)
	{
		if (focus)
		{
			StartTimer(BUTTON_MARQUEE_DELAY);
			packed2.scroll_content = TRUE;
		}
		else
		{
#ifdef SKIN_SUPPORT
			m_hover_counter = 0;
			m_hover_start_time = 0;
#endif // SKIN_SUPPORT
			packed2.scroll_content = FALSE;
			if (!ShowHiddenContent())
			{
				m_x_scroll = 0;
				StopTimer();
			}
		}
	}
#endif // WIDGETS_AUTOSCROLL_HIDDEN_CONTENT

	InvalidateAll();
}

void OpButton::OnTimer()
{
	// marquee-like behavior of text overflowing button
	BOOL scroll_content = FALSE;

#ifdef WIDGETS_AUTOSCROLL_HIDDEN_CONTENT
	if (GetTimerDelay() != 10)
	{
		if (ShowHiddenContent())
		{
			scroll_content = TRUE;
			m_x_scroll += 2;

			if (m_x_scroll > string.GetWidth() - GetBounds().width + 30) // 30 to stop at end for a while.
				m_x_scroll = 0;

			Invalidate(GetBounds());

			StartTimer(BUTTON_MARQUEE_DELAY);
		}
		else
		{
			m_x_scroll = 0;
		}
	}
#endif

#ifndef MOUSELESS
	if (this != g_widget_globals->captured_widget)
	{
		packed2.is_outside = this != g_widget_globals->hover_widget;
	}
#endif

#ifdef SKIN_SUPPORT
	if( packed2.is_click_animation_on )
	{
		packed2.is_click_animation_on = FALSE;
		Update();
		Click();
	}
	else
	{
		double time_now = g_op_time_info->GetWallClockMS();

		// extra functionality when long-clicking
		if (m_click_start_time != 0 && time_now - m_click_start_time >= 500 &&
			(m_button_extra == EXTRA_PLUS_ACTION || m_button_extra == EXTRA_PLUS_HIDDEN_DROPDOWN))
		{
			m_click_start_time = 0;
			packed2.is_pushed = TRUE;
			Update();
			Sync();
			Click(TRUE);
			packed2.is_pushed = FALSE;
			packed2.is_down = FALSE;
			Update();
		}

		if (!g_pccore->GetIntegerPref(PrefsCollectionCore::SpecialEffects))
		{
			if (packed2.is_outside)
			{
				m_hover_counter = 0;
				m_hover_start_time = 0;
			}
		}

		// hover effect
		if (m_hover_start_time)
		{
			double hover_duration = 100;	// full duration should not be more than 100 milliseconds
			double elapsed = time_now - m_hover_start_time;
			INT32 step = (INT32)(elapsed * 100 / hover_duration);

			if (step <= 0)
				return;

			m_hover_start_time = time_now;

			// blend out
			if (packed2.is_outside)
			{
				m_hover_counter -= step;

				if (m_hover_counter <= 0)
				{
					m_hover_counter = 0;
					m_hover_start_time = 0;
				}

				Update();
			}
			// blend in
			else
			{
				m_hover_counter += step;

				if (m_hover_counter >= 100)
				{
					m_hover_counter = 100;
					m_hover_start_time = 0;
				}

				Update();
			}

			Sync();
		}
	}

	if (!m_hover_start_time && !m_click_start_time)
#endif //  SKIN_SUPPORT
	if (!scroll_content)
	{
#ifdef WIDGETS_AUTOSCROLL_HIDDEN_CONTENT
		// resume marquee scrolling
		if (ShowHiddenContent())
			StartTimer(BUTTON_MARQUEE_DELAY);
		else
#endif // WIDGETS_AUTOSCROLL_HIDDEN_CONTENT
		// timer no longer needed
		StopTimer();
	}
}

OpTypedObject::Type OpButton::GetType()
{
	switch (m_button_type)
	{
		case TYPE_RADIO:	return WIDGET_TYPE_RADIOBUTTON;
		case TYPE_CHECKBOX: return WIDGET_TYPE_CHECKBOX;
		default:			return WIDGET_TYPE_BUTTON;
	}
}
BOOL OpButton::IsOfType(Type type)
{
	Type this_type;
	switch (m_button_type)
	{
		case TYPE_RADIO:	this_type = WIDGET_TYPE_RADIOBUTTON; break;
		case TYPE_CHECKBOX: this_type = WIDGET_TYPE_CHECKBOX; break;
		default:			this_type = WIDGET_TYPE_BUTTON; break;
	}
	return type == this_type || OpWidget::IsOfType(type);
}

const char* OpButton::GetInputContextName()
{
	switch (m_button_type)
	{
		case TYPE_RADIO:	return "Radiobutton Widget";
		case TYPE_CHECKBOX: return "Checkbox Widget";
		default:			return "Button Widget";
	}
}
