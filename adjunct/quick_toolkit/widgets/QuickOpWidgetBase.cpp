/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/display/vis_dev.h"
#include "modules/hardcore/mh/constant.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/locale/locale-enum.h"
#include "modules/pi/OpTimeInfo.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/widgets/OpWidget.h"
#include "modules/widgets/WidgetContainer.h"

#include "adjunct/quick_toolkit/widgets/QuickOpWidgetBase.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/quick_toolkit/widgets/OpToolbar.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/dialogs/CustomizeToolbarDialog.h"
#include "adjunct/quick/managers/AnimationManager.h"

#include "modules/scope/src/scope_manager.h"
#include "adjunct/desktop_scope/src/scope_desktop_window_manager.h"

#define DEFAULT_SPACING_BETWEEN_IMAGE_AND_TEXT			2

QuickOpWidgetBase::QuickOpWidgetBase()
	: m_required_width(-1)
	, m_required_height(-1)
	, m_user_data(NULL)
	, m_max_width(INT_MAX)
	, m_min_width(0)
	, m_max_height(INT_MAX)
	, m_min_height(0)
	, m_grow_value(0)
	, m_right_offset(0)
	, m_relative_system_font_size(100)
	, m_string_id(Str::NOT_A_STRING)
#ifdef VEGA_OPPAINTER_SUPPORT
	, m_animation(NULL)
#endif
	, m_update_needed(false)
	, m_update_needed_when(UPDATE_NEEDED_NEVER)
	, m_syncing_layout(false)
	, m_deleted(false)
	, m_has_fixed_min_max_width(false)
	, m_x_resize_effect(RESIZE_FIXED)
	, m_y_resize_effect(RESIZE_FIXED)
	, m_standard_font(OP_SYSTEM_FONT_UI_DIALOG)
	, m_system_font_weight(WEIGHT_DEFAULT)
{
	m_input_state.SetInputStateListener(this);
}

QuickOpWidgetBase::~QuickOpWidgetBase()
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if (m_animation)
	{
		m_animation->RemoveListener();
		OP_DELETE(m_animation);
	}
#endif
}

OpWidget* QuickOpWidgetBase::GetOpWidget()
{
	return static_cast<OpWidget*>(this);
}

const OpWidget* QuickOpWidgetBase::GetOpWidget() const
{
	return static_cast<const OpWidget*>(this);
}

#ifdef VEGA_OPPAINTER_SUPPORT

void QuickOpWidgetBase::SetAnimation(QuickAnimationWidget *animation)
{
	// Set animation before calling the destructor for the current animation object in order 
	// to announce that a new animation object already is set.
	QuickAnimationWidget *tmp_animation = m_animation;
	m_animation = animation;
	if (tmp_animation)
		OP_DELETE(tmp_animation);
}

UINT8 QuickOpWidgetBase::GetOpacity()
{
	if (m_animation)
		return m_animation->GetOpacity();
	return 255;
}

#endif

/***********************************************************************************
**
**	SetUpdateNeededWhen
**
***********************************************************************************/

void QuickOpWidgetBase::SetUpdateNeededWhen(UpdateNeededWhen update_needed_when)
{
	m_update_needed_when = update_needed_when;

	if (m_update_needed_when == UPDATE_NEEDED_ALWAYS)
		ListenToActionStateUpdates(TRUE);
	else if (m_update_needed_when == UPDATE_NEEDED_NEVER)
		ListenToActionStateUpdates(FALSE);
}

/***********************************************************************************
**
**	OnUpdateInputState
**
***********************************************************************************/

void QuickOpWidgetBase::OnUpdateInputState(BOOL full_update)
{
	SetUpdateNeeded(TRUE);

	if (!GetOpWidget()->IsVisible() && GetUpdateNeededWhen() != UPDATE_NEEDED_ALWAYS)
	{
		ListenToActionStateUpdates(FALSE);
		return;
	}

	UpdateActionStateIfNeeded();
}

/***********************************************************************************
**
**	UpdateActionStateIfNeeded
**
***********************************************************************************/

void QuickOpWidgetBase::UpdateActionStateIfNeeded()
{
	if (m_update_needed_when != UPDATE_NEEDED_NEVER)
		ListenToActionStateUpdates(TRUE);

	if (!IsUpdateNeeded())
		return;

	OnUpdateActionState();

	SetUpdateNeeded(FALSE);
}

/***********************************************************************************
**
**	SetSkinManager
**
***********************************************************************************/

BOOL QuickOpWidgetBase::IsFullscreen()
{
	DesktopWindow* dw = GetParentDesktopWindow();

	if (dw)
		return dw->IsFullscreen();

	return FALSE;
}

BOOL QuickOpWidgetBase::IsCustomizing()
{
	if (g_application && !g_application->IsCustomizingToolbars())
		return FALSE;

	OpWidget* widget = GetOpWidget()->GetParent();

	while (widget)
	{
		if (widget->GetType() == OpTypedObject::WIDGET_TYPE_TOOLBAR || widget->GetType() == OpTypedObject::WIDGET_TYPE_PERSONALBAR)
		{
			OpToolbar* toolbar = (OpToolbar*) widget;

			return !toolbar->IsSelector();
		}

		widget = widget->GetParent();
	}

	return FALSE;
}

BOOL QuickOpWidgetBase::IsActionForMe(OpInputAction* action)
{
	if (!GetOpWidget()->HasName() || !action)
		return FALSE;

	if (action->GetAction() == OpInputAction::ACTION_GET_ACTION_STATE)
		action = action->GetChildAction();

	if (!action->HasActionDataString())
		return TRUE;

	OpString name;
	name.Set(GetOpWidget()->GetName());

	if (name.CompareI(action->GetActionDataString()) == 0)
		return TRUE;

	if(action->IsActionDataStringEqualTo(UNI_L("selected toolbar")))
		return TRUE;

	// check for special target toolbar

	if (g_application->GetCustomizeToolbarsDialog() && g_application->GetCustomizeToolbarsDialog()->GetTargetToolbar() == this && action->IsActionDataStringEqualTo(UNI_L("target toolbar")))
		return TRUE;

	return FALSE;
}

OpTypedObject::Type QuickOpWidgetBase::GetParentDesktopWindowType()
{
	DesktopWindow* dw = GetParentDesktopWindow();

	if (dw)
		return dw->GetType();

	return OpTypedObject::UNKNOWN_TYPE;
}

DesktopWindow* QuickOpWidgetBase::GetParentDesktopWindow()
{
	if (GetOpWidget()->GetWidgetContainer())
		return (GetOpWidget()->GetWidgetContainer()->GetParentDesktopWindow());
	
	return NULL;
}

OpWorkspace* QuickOpWidgetBase::GetParentWorkspace()
{
	DesktopWindow* dw = GetParentDesktopWindow();

	if (dw)
		return dw->GetParentWorkspace();

	return NULL;
}

OpWorkspace* QuickOpWidgetBase::GetWorkspace()
{
	DesktopWindow* dw = GetParentDesktopWindow();

	if (dw)
		return dw->GetWorkspace();

	return NULL;
}

OpWidget* QuickOpWidgetBase::GetWidgetById(INT32 id)
{
	if (id == GetOpWidget()->m_id)
		return GetOpWidget();

	OpWidget* child = GetOpWidget()->GetFirstChild();

	while(child)
	{
		OpWidget* hit = child->GetWidgetById(id);

		if (hit)
			return hit;

		child = child->GetNextSibling();
	}

	return NULL;
}

OpWidget* QuickOpWidgetBase::GetWidgetByTypeAndIdAndAction(OpTypedObject::Type type, INT32 id, OpInputAction* action, BOOL only_visible)
{
	if (only_visible && !GetOpWidget()->IsVisible())
		return NULL;

	if (type == GetOpWidget()->GetType() && (id == -1 || id == GetOpWidget()->GetID()) && (!action || action->Equals(GetOpWidget()->GetAction())))
		return GetOpWidget();

	OpWidget* child = GetOpWidget()->GetFirstChild();

	while(child)
	{
		OpWidget* hit = child->GetWidgetByTypeAndIdAndAction(type, id, action, only_visible);

		if (hit)
			return hit;

		child = child->GetNextSibling();
	}

	return NULL;
}

void QuickOpWidgetBase::SetStringID(Str::LocaleString string_id)
{
	m_string_id = string_id;
}

Str::LocaleString QuickOpWidgetBase::GetStringID() const
{
	return Str::LocaleString(m_string_id);
}

void QuickOpWidgetBase::SetSystemFont(OP_SYSTEM_FONT system_font)
{
	if (m_standard_font == (unsigned)system_font)
		return;

	m_standard_font = system_font;
	GetOpWidget()->UpdateSystemFont(TRUE);

	// Part of fix for bug #199381 (UI label text is cut off due to widget padding changes)
	OpWidget* child = GetOpWidget()->GetFirstChild();
	while(child)
	{
		child->SetSystemFont(system_font);
		child = child->GetNextSibling();
	}
}

void QuickOpWidgetBase::SetRelativeSystemFontSize(UINT32 percent)
{
	if (m_relative_system_font_size == percent)
		return;

	m_relative_system_font_size = percent;
	GetOpWidget()->UpdateSystemFont();
}

void QuickOpWidgetBase::SetSystemFontWeight(FontWeight weight)
{
	if (m_system_font_weight == (unsigned)weight)
		return;

	m_system_font_weight = weight;
	GetOpWidget()->UpdateSystemFont();
}

/***********************************************************************************
**
**	Relayout
**
***********************************************************************************/
/*The Quick layout system works by accumulating relayout calls and setting flags on
  widgets that need relayout. When the first relayout is requested a message is send
  to the root widget. By the time the message is received, SyncLayout() is called.
  Which calls OnRelayout and OnLayout for every widget in the widget container that
  needs a relayout or layout. */
void QuickOpWidgetBase::Relayout(BOOL relayout, BOOL invalidate)
{
	if (GetOpWidget()->IsForm()) // Form widgets have their own layout system which we mustn't interfere with
		return;

	// TODO only set this for those needed
	GetOpWidget()->SetMultilingual(TRUE);

	UpdateSkinPadding();

	if (relayout)
	{
		ResetRequiredSize();
	}

	WidgetContainer* widget_container = GetOpWidget()->GetWidgetContainer();

	//Without a container, the widget is not shown anywhere, no layout needed (yet?)
	if (!widget_container)
		return;

	OpWidget* root = widget_container->GetRoot();

	UpdateDirection(root->GetRTL());

	//If a relayout is already pending, wait for that one
	//We do not want to send new layout messages if something that is layouting require
	//any of its parents to relayout (bad design anyway)
	if (GetOpWidget()->IsRelayoutNeeded() && root->IsSyncingLayout())
		return;

	//If a layout is already pending, wait for that one
	if (!relayout && GetOpWidget()->IsLayoutNeeded())
		return;

	if (relayout)
	{
		//Set a flag on this widget, so we know a relayout is on its way
		GetOpWidget()->SetRelayoutNeeded(TRUE);
	}

	//Set a flag on this widget so we know a layout for this widget is on its way
	GetOpWidget()->SetLayoutNeeded(TRUE);

	//invalidate if need. The whole widget will be repainted
	if (invalidate)
		GetOpWidget()->InvalidateAll();

	//If we already are listening to layout messages, a message was posted recently,
	//just wait for that one to process.
	if (!g_main_message_handler->HasCallBack(root, MSG_LAYOUT, (MH_PARAM_1) root))
	{
		//Start listening to layout messages and send a layout message
		//When the message is received, Synclayout is called
		g_main_message_handler->SetCallBack(root, MSG_LAYOUT, (MH_PARAM_1)root);
		g_main_message_handler->PostMessage(MSG_LAYOUT, (MH_PARAM_1)root, 0);
	}
}

void QuickOpWidgetBase::UpdateDirection(BOOL rtl)
{
	if (GetOpWidget()->GetRTL() == rtl)
		return;

	if (GetOpWidget()->font_info.justify == JUSTIFY_LEFT)
		GetOpWidget()->SetJustify(JUSTIFY_RIGHT, FALSE);
	else if (GetOpWidget()->font_info.justify == JUSTIFY_RIGHT)
		GetOpWidget()->SetJustify(JUSTIFY_LEFT, FALSE);

	GetOpWidget()->SetRTL(rtl);
}

/***********************************************************************************
**
**	SyncLayout
**
***********************************************************************************/

void QuickOpWidgetBase::SyncLayout()
{
	WidgetContainer* widget_container = GetOpWidget()->GetWidgetContainer();

	if (!widget_container)
		return;

	OpWidget* root = widget_container->GetRoot();
		
	//Check if we are listening for layout messages, if not,
	//a relayout was not needed, so just skip it
	if (g_main_message_handler->HasCallBack(root, MSG_LAYOUT, (MH_PARAM_1) root))
	{
		// make sure input states have been synced
		g_input_manager->SyncInputStates();

		//relayout and layout all widget that need layouting
		root->m_syncing_layout = true;
		root->GenerateOnRelayout(FALSE);
		root->GenerateOnLayout(FALSE);
		root->m_syncing_layout = false;

		//stop listening to layout messages, not needed anymore until a new relayout is requested
		g_main_message_handler->RemoveDelayedMessage(MSG_LAYOUT, (MH_PARAM_1) root, 0);
		g_main_message_handler->UnsetCallBack(root, MSG_LAYOUT);
	}
}

/***********************************************************************************
**
**	GenerateOnRelayout
**
***********************************************************************************/

void QuickOpWidgetBase::GenerateOnRelayout(BOOL force)
{
	OpWidget* child = GetOpWidget()->GetFirstChild();

	if (GetOpWidget()->IsVisible())
		while(child)
		{
			child->GenerateOnRelayout(force || GetOpWidget()->IsRelayoutNeeded());
			child = child->GetNextSibling();
		}

	if (force || GetOpWidget()->IsRelayoutNeeded())
	{
		GetOpWidget()->OnRelayout();
	}
}

/***********************************************************************************
**
**	GenerateOnLayout
**
***********************************************************************************/

void QuickOpWidgetBase::GenerateOnLayout(BOOL force)
{
	if (force || GetOpWidget()->IsLayoutNeeded())
		GetOpWidget()->OnLayout();

	OpWidget* child = GetOpWidget()->GetFirstChild();

	if (GetOpWidget()->IsVisible())
	{
		while(child)
		{
			child->GenerateOnLayout(force || GetOpWidget()->IsLayoutNeeded());
			child = child->GetNextSibling();
		}
	}

	GetOpWidget()->OnLayoutAfterChildren();
	GetOpWidget()->SetRelayoutNeeded(FALSE);
	GetOpWidget()->SetLayoutNeeded(FALSE);
}

/***********************************************************************************
**
**	GenerateOnSettingsChanged
**
***********************************************************************************/

void QuickOpWidgetBase::GenerateOnSettingsChanged(DesktopSettings* settings)
{
	if (settings->IsChanged(SETTINGS_UI_FONTS))
		GetOpWidget()->UpdateSystemFont();

	Relayout();

	OnSettingsChanged(settings);

	OpWidget* child = GetOpWidget()->GetFirstChild();

	while(child)
	{
		child->GenerateOnSettingsChanged(settings);
		child = child->GetNextSibling();
	}
}

/***********************************************************************************
**
**	GetDragObject
**
***********************************************************************************/

DesktopDragObject* QuickOpWidgetBase::GetDragObject(OpTypedObject::Type type, INT32 x, INT32 y)
{
	OpDragObject* op_drag_object;
	if (OpStatus::IsError(OpDragObject::Create(op_drag_object, type)))
		return NULL;

	DesktopDragObject* drag_object = static_cast<DesktopDragObject*>(op_drag_object);

	OpBitmap* bitmap = NULL;

	RETURN_VALUE_IF_ERROR(OpBitmap::Create(&bitmap, GetOpWidget()->rect.width, GetOpWidget()->rect.height, FALSE, TRUE, 0, 0, TRUE), drag_object);

	OpPainter* painter = bitmap->GetPainter();

	if (painter)
	{
		painter->SetColor(0,0,0,255); // A painters default color is not defined. Set it.
		painter->ClearRect(OpRect(0,0,GetOpWidget()->rect.width, GetOpWidget()->rect.height));

#ifdef DISPLAY_CAP_BEGINPAINT2
		GetOpWidget()->vis_dev->BeginPaint(painter, GetOpWidget()->GetRect(TRUE), OpRect(0, 0, GetOpWidget()->rect.width, GetOpWidget()->rect.height));
#else
		OpPainter* old_painter = m_widget->vis_dev->painter;
		m_widget->vis_dev->painter = painter;
#endif

		INT32 translation_x = -GetOpWidget()->rect.x - GetOpWidget()->vis_dev->GetTranslationX();
		INT32 translation_y = -GetOpWidget()->rect.y - GetOpWidget()->vis_dev->GetTranslationY();

		GetOpWidget()->vis_dev->Translate(translation_x, translation_y);
		GetOpWidget()->GenerateOnPaint(GetOpWidget()->GetBounds(), TRUE);
		GetOpWidget()->vis_dev->Translate(-translation_x, -translation_y);

#ifdef DISPLAY_CAP_BEGINPAINT2
		GetOpWidget()->vis_dev->EndPaint();
#else
		m_widget->vis_dev->painter = old_painter;
#endif
		bitmap->ReleasePainter();

		drag_object->SetBitmap(bitmap);

		// find pos

		OpRect root_rect = GetOpWidget()->GetRect(TRUE);
		OpPoint point(MAX(0, x - root_rect.x), MAX(0, y - root_rect.y));
		drag_object->SetBitmapPoint(point);
	}
	else
	{
		OP_DELETE(bitmap);
	}
	this->GetOpWidget()->GetRect();
	return drag_object;
}

/***********************************************************************************
**
**	GetDropArea
**
***********************************************************************************/

INT32 QuickOpWidgetBase::GetDropArea(INT32 x, INT32 y)
{
	OpRect rect = GetOpWidget()->GetRect();

	INT32 flag = DROP_CENTER;

	if( x >= 0 )
	{
		if( x < rect.width / 2 )
			flag |= DROP_LEFT;
		else if( x >= rect.width / 2 )
			flag |= DROP_RIGHT;
	}

	if( y >= 0 )
	{
		if( y < rect.height / 2 )
			flag |= DROP_TOP;
		else if( y >= rect.height / 2 )
			flag |= DROP_BOTTOM;
	}

	if( flag & (DROP_LEFT|DROP_RIGHT|DROP_TOP|DROP_BOTTOM) )
	{
		flag &= ~DROP_CENTER;
	}

	return flag;
}

/***********************************************************************************
**
**	OnDragStart
**
***********************************************************************************/

void QuickOpWidgetBase::OnDragStart(const OpPoint& point)
{
	if (GetOpWidget()->listener)
		GetOpWidget()->listener->OnDragStart(GetOpWidget(), 0, point.x, point.y);
}

/***********************************************************************************
**
**	OnDragMove
**
***********************************************************************************/

void QuickOpWidgetBase::OnDragMove(OpDragObject* drag_object, const OpPoint& point)
{
	if (GetOpWidget()->listener)
		GetOpWidget()->listener->OnDragMove(GetOpWidget(), drag_object, 0, point.x, point.y);
}

/***********************************************************************************
**
**	OnDragDrop
**
***********************************************************************************/

void QuickOpWidgetBase::OnDragDrop(OpDragObject* drag_object, const OpPoint& point)
{
	if (GetOpWidget()->listener)
		GetOpWidget()->listener->OnDragDrop(GetOpWidget(), drag_object, 0, point.x, point.y);
}

/***********************************************************************************
**
**	UpdateSkinPadding
**
***********************************************************************************/

void QuickOpWidgetBase::UpdateSkinPadding()
{
	INT32 left, top, right, bottom;
	GetOpWidget()->GetBorderSkin()->GetPadding(&left, &top, &right, &bottom);
	GetOpWidget()->SetPadding(left, top, right, bottom);
}

/***********************************************************************************
**
**	UpdateSkinMargin
**
***********************************************************************************/

void QuickOpWidgetBase::UpdateSkinMargin()
{
	INT32 left, top, right, bottom;
	GetOpWidget()->GetBorderSkin()->GetMargin(&left, &top, &right, &bottom);
	GetOpWidget()->SetMargins(left, top, right, bottom);
}

/***********************************************************************************
**
**	GetSpacing
**
***********************************************************************************/

void QuickOpWidgetBase::GetSpacing(INT32* spacing)
{
	if (GetOpWidget()->GetType() == OpTypedObject::WIDGET_TYPE_BUTTON)
	{
		if (static_cast<OpButton*>(GetOpWidget())->m_button_style == OpButton::STYLE_IMAGE_AND_TEXT_CENTER)
		{
			*spacing = DEFAULT_SPACING_BETWEEN_IMAGE_AND_TEXT;
			return ;
		}
	}

	GetOpWidget()->GetBorderSkin()->GetSpacing(spacing);
}

/***********************************************************************************
**
**	GetRequiredSize
**
***********************************************************************************/

void QuickOpWidgetBase::GetRequiredSize(INT32& width, INT32& height)
{
	if (m_required_width == -1)
	{
		GetOpWidget()->GetPreferedSize(&width, &height, 0, 0);
		m_required_width = width;
		m_required_height = height;
	}
	else
	{
		width = m_required_width;
		height = m_required_height;
	}
#ifdef VEGA_OPPAINTER_SUPPORT
	if (m_animation)
	{
		INT32 left_margin, top_margin, right_margin, bottom_margin;
		GetSkinMargins(&left_margin, &top_margin, &right_margin, &bottom_margin);

		width += left_margin + right_margin;
		height += top_margin + bottom_margin;

		m_animation->GetCurrentValue(width, height);

		width -= left_margin + right_margin;
		height -= top_margin + bottom_margin;
	}
#endif
}

/***********************************************************************************
**
**	GetSkinMargins
**
***********************************************************************************/

void QuickOpWidgetBase::GetSkinMargins(INT32 *left, INT32 *top, INT32 *right, INT32 *bottom)
{
	GetOpWidget()->GetBorderSkin()->GetMargin(left, top, right, bottom);
}

/***********************************************************************************
**
**	MarkDeleted
**
***********************************************************************************/

void QuickOpWidgetBase::MarkDeleted()
{
	m_deleted = true;

	if (m_animation)	
		m_animation->RemoveListener();

	ListenToActionStateUpdates(FALSE);
}

OpRect QuickOpWidgetBase::AdjustForDirection(const OpRect& rect) const
{
	OpRect adjusted_rect = rect;
	if (const_cast<OpWidget*>(GetOpWidget())->GetRTL())
	{
		INT32 right = const_cast<OpWidget*>(GetOpWidget())->GetRect().width - rect.x;
		adjusted_rect.x = right - rect.width;
	}
	return adjusted_rect;
}

void QuickOpWidgetBase::SetChildRect(OpWidget* child, const OpRect& rect, BOOL invalidate, BOOL resize_children) const
{
	child->SetRect(AdjustForDirection(rect), invalidate, resize_children);
}

void QuickOpWidgetBase::SetChildOriginalRect(OpWidget* child, const OpRect& rect) const
{
	child->SetOriginalRect(AdjustForDirection(rect));
}

#ifdef PREFS_CAP_UI_DEBUG_SKIN
#ifdef SKIN_CAP_MORE_SKINELEMENT_METHODS

BOOL QuickOpWidgetBase::HasToolTipText(OpToolTip* tooltip)
{
	if (IsDebugToolTipActive())
		return TRUE;

	return FALSE;
}

const char* GetSkinTypeToString(OpWidgetImage *image)
{
	const char* type = "(Skin position: Default)";
	SkinType skin_type = image->GetType();

	switch(skin_type)
	{
	case SKINTYPE_LEFT:
		{
			type = "(Skin position: Left)";
			break;
		}
	case SKINTYPE_RIGHT:
		{
			type = "(Skin position: Right)";
			break;
		}
	case SKINTYPE_BOTTOM:
		{
			type = "(Skin position: Bottom)";
			break;
		}
	case SKINTYPE_TOP:
		{
			type = "(Skin position: Top)";
			break;
		}
	}
	return type;
}

void QuickOpWidgetBase::DumpSkinInformation(OpWidget *widget, OpString8& output)
{
	if(!widget->GetForegroundSkin()->GetImage() && 
		!widget->GetBorderSkin()->GetImage() &&
		!widget->GetBorderSkin()->HasFallbackElement())
	{
		output.AppendFormat("(empty)\n");
		return;
	}
	INT32 p_left, p_top, p_right, p_bottom;
	INT32 m_left, m_top, m_right, m_bottom;
	OpRect size = widget->GetRect();

	widget->GetBorderSkin()->GetPadding(&p_left, &p_top, &p_right, &p_bottom);
	widget->GetBorderSkin()->GetMargin(&m_left, &m_top, &m_right, &m_bottom);
	const OpSkinTextShadow *shadow = NULL;
	OpSkinElement *elm = g_skin_manager->GetCurrentSkin()->GetSkinElement(widget->GetBorderSkin()->GetImage());
	if(elm)
	{
		if(OpStatus::IsError(elm->GetTextShadow(shadow, 0)))
			shadow = NULL;
	}
	output.AppendFormat("Size: x: %d, y: %d, width: %d, height: %d\nForeground: %s%s %s\nBackground: %s %s\nBackground fallback: %s\nBackground paddings: left %d, top: %d, right: %d, bottom: %d\n\
Background margins: left %d, top: %d, right: %d, bottom: %d\nText shadow: %d %d\n", 
						size.Left(), size.Top(), size.width, size.height,
						widget->GetForegroundSkin()->GetImage() ? widget->GetForegroundSkin()->GetImage() : "",
						widget->GetForegroundSkin()->GetImage() ? (widget->GetForegroundSkin()->GetSize() == SKINSIZE_LARGE ? ".large" : "") : "",
						widget->GetForegroundSkin()->GetImage() ? GetSkinTypeToString(widget->GetForegroundSkin()) : "",
						widget->GetBorderSkin()->GetImage() ? widget->GetBorderSkin()->GetImage() : "",
						widget->GetBorderSkin()->GetImage() ? GetSkinTypeToString(widget->GetBorderSkin()) : "",
						widget->GetBorderSkin()->HasFallbackElement() ? widget->GetBorderSkin()->GetFallbackElement() : "",
						p_left, p_top, p_right, p_bottom, m_left, m_top, m_right, m_bottom,
						shadow ? shadow->ofs_x : 0, shadow ? shadow->ofs_y : 0);
}

BOOL QuickOpWidgetBase::IsDebugToolTipActive()
{
	if((g_op_system_info->GetShiftKeyState() & SHIFTKEY_CTRL) == SHIFTKEY_CTRL)
	{
		// Debug skin is active or UI property examiner is on and the Control Key is held down
		if(g_pcui->GetIntegerPref(PrefsCollectionUI::DebugSkin) || g_pcui->GetIntegerPref(PrefsCollectionUI::UIPropertyExaminer))
			return TRUE;
	}
	return FALSE;
}

void QuickOpWidgetBase::GetToolTipText(OpToolTip* tooltip, OpInfoText& text)
{
	// Just bail out ASAP if the debug tooltip is off
	if(!IsDebugToolTipActive())
		return;

	OpString tooltip_string;

	// We must have the UI Property Examiner preference on and the Control Key held down to show the special tooltip
	if((g_op_system_info->GetShiftKeyState() & SHIFTKEY_CTRL) == SHIFTKEY_CTRL)
	{
		if (g_pcui->GetIntegerPref(PrefsCollectionUI::UIPropertyExaminer))
		{
			if (g_scope_manager->desktop_window_manager)
			{
				// Get the widget data into the tooltip string
				g_scope_manager->desktop_window_manager->GetQuickWidgetInfoToolTip(GetOpWidget(), tooltip_string);

			}
			// Add some space if the debug skin is on
			if (g_pcui->GetIntegerPref(PrefsCollectionUI::DebugSkin))
				tooltip_string.AppendFormat(UNI_L("\n"));
		}
		// Check for the debug skin tooltip preference
		if (g_pcui->GetIntegerPref(PrefsCollectionUI::DebugSkin))
		{
			// LarsKL	available: .hover, .pressed, .active, ..., those that can really be used for that element. existing = the ones defined in skin.ini

			OpString8 tmp;
			tmp.AppendFormat("Skin information:\n -----------------\n");

			tmp.AppendFormat("\nHovering element:\n\n");
			DumpSkinInformation(GetOpWidget(), tmp);

			OpWidget *parent = GetOpWidget()->GetParent();

			if(parent)
			{
				if(parent->GetForegroundSkin()->GetImage() || 
					parent->GetBorderSkin()->GetImage() ||
					parent->GetBorderSkin()->HasFallbackElement())
				{
					tmp.AppendFormat("\nParent element:\n\n");

					DumpSkinInformation(parent, tmp);
				}
			}
			//Finally, set the composed string as the tooltip that should be shown
			tooltip_string.Append(tmp.CStr());
		}
		// Set the tooltip text itself
		if(tooltip_string.HasContent())
			text.SetTooltipText(tooltip_string.CStr());
	}
}
#endif // SKIN_CAP_MORE_SKINELEMENT_METHODS
#endif // PREFS_CAP_UI_DEBUG_SKIN

/***********************************************************************************
**
**	OpWidgetLayout
**
***********************************************************************************/

OpWidgetLayout::OpWidgetLayout(INT32 available_width, INT32 available_height) :
	m_available_width(available_width),
	m_available_height(available_height),
	m_min_width(0),
	m_min_height(0),
	m_prefered_width(0),
	m_prefered_height(0),
	m_max_width(INT_MAX),
	m_max_height(INT_MAX)
{
}

void OpWidgetLayout::SetFixedWidth(INT32 width)
{
	m_min_width = width;
	m_prefered_width = width;
	m_max_width = width;
}

void OpWidgetLayout::SetFixedHeight(INT32 height)
{
	m_min_height = height;
	m_prefered_height = height;
	m_max_height = height;
}

void OpWidgetLayout::SetFixedSize(INT32 width, INT32 height)
{
	SetFixedWidth(width);
	SetFixedHeight(height);
}

void OpWidgetLayout::SetFixedFullSize()
{
	SetFixedSize(m_available_width, m_available_height);
}

void OpWidgetLayout::SetConstraintsFromWidget(QuickOpWidgetBase* widget)
{
	m_prefered_width = m_min_width = widget->GetMinWidth();
	m_prefered_height = m_min_height = widget->GetMinHeight();
	m_max_width = widget->GetMaxWidth();
	m_max_height = widget->GetMaxHeight();
}

/***********************************************************************************
**
**	OpDelayedTrigger
**
***********************************************************************************/

OpDelayedTrigger::OpDelayedTrigger() :
	m_listener(NULL),
	m_initial_delay(0),
	m_subsequent_delay(0),
	m_timer(NULL),
	m_previous_invoke(0.0),
	m_is_timer_running(FALSE)
{
}

OpDelayedTrigger::~OpDelayedTrigger()
{
	OP_DELETE(m_timer);
}

void OpDelayedTrigger::InvokeTrigger(InvokeType invoke_type)
{
	if (invoke_type == INVOKE_DELAY && m_is_timer_running)
		return;

	double new_trigger = g_op_time_info->GetRuntimeMS();
	INT32 time_since_last_invoke = (INT32)(new_trigger - m_previous_invoke);
	INT32 delay = invoke_type == INVOKE_NOW ? 0 : m_initial_delay;

	if (invoke_type == INVOKE_DELAY && m_subsequent_delay && time_since_last_invoke < m_subsequent_delay)
		delay = m_subsequent_delay;

	if (m_is_timer_running)
	{
		m_timer->Stop();
		m_is_timer_running = FALSE;
	}

	if (delay)
	{
		if (!m_timer)
		{
			if (!(m_timer = OP_NEW(OpTimer, ())))
				return;

			m_timer->SetTimerListener(this);
		}

		m_timer->Start(delay);
		m_is_timer_running = TRUE;
	}
	else
	{
		InvokeListener();
	}
}

void OpDelayedTrigger::InvokeListener()
{
	m_previous_invoke = g_op_time_info->GetRuntimeMS();

	if (m_listener)
		m_listener->OnTrigger(this);
}

void OpDelayedTrigger::OnTimeOut(OpTimer* timer)
{
	m_is_timer_running = FALSE;
	InvokeListener();
}

void OpDelayedTrigger::CancelTrigger()
{
	if(m_timer)
		m_timer->Stop();

	m_is_timer_running = FALSE;
}
