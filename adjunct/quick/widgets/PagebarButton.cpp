/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/quick/widgets/PagebarButton.h"

#include "adjunct/quick/widgets/OpPagebar.h"
#include "adjunct/quick/widgets/OpThumbnailPagebar.h"

#include "adjunct/quick/models/DesktopHistoryModel.h"
#include "adjunct/quick/models/DesktopGroupModelItem.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/widgets/OpToolbarMenuButton.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick/widgets/OpTabGroupButton.h"
#include "adjunct/quick/managers/opsetupmanager.h"
#include "adjunct/quick/quick-widget-names.h"

#include "modules/display/vis_dev.h"
#include "modules/dragdrop/dragdrop_manager.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/util/opswap.h"
#include "modules/widgets/WidgetContainer.h"

#ifdef VEGA_OPPAINTER_SUPPORT
#include "adjunct/quick/managers/AnimationManager.h"
#endif

namespace PagebarButtonConstants
{
	const unsigned long TitleUpdateDelay = 300;
	const int WindowLimitThreshold = 4;
	const int DragThreshold = 3;

	// Thresholds relative to the size of the button they affect
	struct Fraction
	{
		int num;
		int denom;
	};
	const Fraction MoveThreshold = { 1, 5 };
	const Fraction StackThreshold = { 2, 3 };
	const Fraction SwapGroupThreshold = { 4, 5 };
	const Fraction ExitGroupThreshold = { 5, 6 };
};

#ifdef _DEBUG	// Safeguard to avoid printfs in release
// Turn on to get debug output for group number
//#define GROUP_NUMBER_DEBUGGING
//#define TAB_DRAG_DEBUGGING
#endif // _DEBUG


/***********************************************************************************
**
**	PagebarButton
**
***********************************************************************************/

OP_STATUS PagebarButton::Construct(PagebarButton** obj, DesktopWindow* desktop_window)
{
	OpAutoPtr<PagebarButton> new_button (OP_NEW(PagebarButton, (desktop_window)));
	if (!new_button.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(new_button->InitPagebarButton());
	*obj = new_button.release();

	return OpStatus::OK;
}

PagebarButton::PagebarButton(DesktopWindow* desktop_window)
		: OpPagebarItem(desktop_window->GetID(), 0)
		, m_maximize_button(NULL)
		, m_minimize_button(NULL)
		, m_indicator_button(NULL)
		, m_old_min_width(0)
		, m_old_max_width(0)
		, m_desktop_window(desktop_window)
		, m_delayed_title_update_in_progress(FALSE)
		, m_is_move_window_button(FALSE)
		, m_is_moving_window(FALSE)
		, m_use_hover_stack_overlay(FALSE)
		, m_is_compact_button(FALSE)
		, m_original_group_number(-1)
		, m_between_groups(FALSE)
		, m_mouse_move_dragging(FALSE)
		, m_can_start_drag(FALSE)
		, m_show_page_loading(false)
		, m_is_delayed_activation_scheduled(false)
{
	if (desktop_window)
	{
		desktop_window->AddListener(this);
	}
	SetOnMoveWanted(TRUE);
}

PagebarButton::~PagebarButton()
{
	m_activation_timer.Stop();
	if (m_desktop_window)
	{
		m_desktop_window->RemoveListener(this);
		if (m_desktop_window->GetType() == WINDOW_TYPE_DOCUMENT)
		{
			DocumentDesktopWindow *win = static_cast<DocumentDesktopWindow *>(m_desktop_window);
			win->RemoveDocumentWindowListener(this);
		}
	}
}

OP_STATUS PagebarButton::InitPagebarButton()
{
	SetEllipsis(g_pcui->GetIntegerPref(PrefsCollectionUI::EllipsisInCenter) == 1 ? ELLIPSIS_CENTER : ELLIPSIS_END );

	RETURN_IF_ERROR(OpButton::Construct(&m_close_button, OpButton::TYPE_CUSTOM));
	m_close_button->SetName(WIDGET_NAME_PAGEBAR_BUTTON_CLOSE);

	AddChild(m_close_button);

	m_indicator_button = OP_NEW(OpIndicatorButton, (m_desktop_window, OpIndicatorButton::RIGHT));
	RETURN_OOM_IF_NULL(m_indicator_button);
	AddChild(m_indicator_button);
	RETURN_IF_ERROR(m_indicator_button->Init("Tabbar Indicator Skin"));
	m_indicator_button->SetIconState(OpIndicatorButton::CAMERA, OpIndicatorButton::ACTIVE);
	m_indicator_button->SetIconState(OpIndicatorButton::GEOLOCATION, OpIndicatorButton::ACTIVE);
	m_indicator_button->SetVisibility(FALSE);

	if(m_desktop_window)
	{
		OpInputAction *action = OP_NEW(OpInputAction, (OpInputAction::ACTION_CLOSE_WINDOW));
		RETURN_OOM_IF_NULL(action);
		action->SetActionData(m_desktop_window->GetID());

		m_close_button->GetBorderSkin()->SetImage("Pagebar Close Button Skin");
		m_close_button->GetBorderSkin()->SetForeground(TRUE);

		m_close_button->SetAction(action);

		SetCloseButtonTooltip();

		if(m_desktop_window->GetType() == WINDOW_TYPE_DOCUMENT)
		{
			DocumentDesktopWindow *win = static_cast<DocumentDesktopWindow *>(m_desktop_window);
			win->AddDocumentWindowListener(this);
		}
	}
	return g_main_message_handler->SetCallBack(this, MSG_QUICK_UPDATE_PAGEBAR_BUTTON_TITLE, 0);
}

void PagebarButton::SetShowPageLoading(bool show_page_loading)
{
	m_show_page_loading = show_page_loading;
}

void PagebarButton::SetIsMoveWindowButton()
{
	m_is_move_window_button = TRUE;

	RETURN_VOID_IF_ERROR(OpButton::Construct(&m_maximize_button, OpButton::TYPE_CUSTOM));
	m_maximize_button->SetName(WIDGET_NAME_PAGEBAR_BUTTON_MAXIMIZE);

	AddChild(m_maximize_button);

	if(m_desktop_window)
	{
		OpInputAction *action = OP_NEW(OpInputAction, (OpInputAction::ACTION_MAXIMIZE_PAGE));
		if (!action)
			return;
		action->SetActionData(m_desktop_window->GetID());

		m_maximize_button->GetBorderSkin()->SetImage("Pagebar Maximize Button Skin");
		m_maximize_button->GetBorderSkin()->SetForeground(TRUE);

		m_maximize_button->SetAction(action);

		// Set maximize button tooltip

		OpString tooltip_string, fmt_string, shortcut_string;
		g_input_manager->GetShortcutStringFromAction(action, shortcut_string, this);

		if (OpStatus::IsSuccess(g_languageManager->GetString(Str::M_PAGE_MAXIMIZE, tooltip_string)))
		{
			if (shortcut_string.IsEmpty())
				fmt_string.Set(tooltip_string);
			else
				fmt_string.AppendFormat(UNI_L("%s (%s)"), tooltip_string.CStr(), shortcut_string.CStr());

			m_maximize_button->GetAction()->GetActionInfo().SetTooltipText(fmt_string.CStr());
		}
	}

	RETURN_VOID_IF_ERROR(OpButton::Construct(&m_minimize_button, OpButton::TYPE_CUSTOM));

	m_minimize_button->SetName(WIDGET_NAME_PAGEBAR_BUTTON_MINIMIZE);
	AddChild(m_minimize_button);

	if(m_minimize_button)
	{
		OpInputAction *action = OP_NEW(OpInputAction, (OpInputAction::ACTION_MINIMIZE_PAGE));
		if (!action)
			return;
		action->SetActionData(m_desktop_window->GetID());

		m_minimize_button->GetBorderSkin()->SetImage("Pagebar Minimize Button Skin");
		m_minimize_button->GetBorderSkin()->SetForeground(TRUE);

		m_minimize_button->SetAction(action);

		// Set minimize button tooltip

		OpString tooltip_string, fmt_string, shortcut_string;
		g_input_manager->GetShortcutStringFromAction(action, shortcut_string, this);

		if (OpStatus::IsSuccess(g_languageManager->GetString(Str::M_MINIMIZE_PAGE, tooltip_string)))
		{
			if (shortcut_string.IsEmpty())
				fmt_string.Set(tooltip_string);
			else
				fmt_string.AppendFormat(UNI_L("%s (%s)"), tooltip_string.CStr(), shortcut_string.CStr());

			m_minimize_button->GetAction()->GetActionInfo().SetTooltipText(fmt_string.CStr());
		}
	}
}

void PagebarButton::UpdateLockedTab()
{
	BOOL is_locked			= IsLockedByUser();
	BOOL is_closable		= IsClosableByUser();
	BOOL show_close_button	= TRUE;
	BOOL need_relayout      = FALSE;
	if (is_locked)
		show_close_button = TRUE; // but with a "pinned" image
	else if (!is_closable)
		show_close_button = FALSE;
	else
		show_close_button = !GetParent()->IsOfType(WIDGET_TYPE_PAGEBAR) || ((OpPagebar*)GetParent())->IsCloseButtonVisible(m_desktop_window, FALSE);

	if(IsCompactButton())
	{
		m_close_button->SetVisibility(FALSE);
	}
	else
	{
		need_relayout = m_close_button->IsVisible() && !show_close_button;

		m_close_button->SetVisibility(show_close_button);
		m_close_button->GetBorderSkin()->SetImage(is_locked ? "Pagebar Locked Button Skin" : "Pagebar Close Button Skin");

		// ignore mouse events so they are handled by PagebarButton instead
		m_close_button->SetIgnoresMouse(is_locked);
		if(is_locked)
		{
			m_close_button->GetAction()->GetActionInfo().SetTooltipText(UNI_L(""));
		}
		else
		{
			SetCloseButtonTooltip();
		}
	}
	if (need_relayout)
		Relayout();
}

void PagebarButton::UpdateCompactStatus()
{
	if(m_desktop_window)
	{
		OpPagebar *pagebar = GetPagebar();
		BOOL is_compact = m_desktop_window->IsLockedByUser() && !IsGrouped() && pagebar && pagebar->IsHorizontal();
		SetIsCompactButton(is_compact);
	}
}

void PagebarButton::SetUseHoverStackOverlay(BOOL use_hover_stack_overlay)
{
	if (m_use_hover_stack_overlay != use_hover_stack_overlay)
	{
		m_use_hover_stack_overlay = use_hover_stack_overlay;
		InvalidateAll();
	}
}


void PagebarButton::SetHidden(BOOL is_hidden)
{
	if (!GetParent())
		return;

	if (is_hidden == IsHidden())
		return;

	if (g_animation_manager->GetEnabled() && !GetAnimation())
	{
		if (!is_hidden) // Done on animation completion
			OpButton::SetHidden(is_hidden);

		OpPagebar *pagebar = GetPagebar();
		bool is_horizontal = pagebar ? !!pagebar->IsHorizontal() : true;
		QuickAnimationWidget *animation = OP_NEW(QuickAnimationWidget, (this, is_hidden ? ANIM_MOVE_SHRINK : ANIM_MOVE_GROW, is_horizontal));
		if (animation)
		{
			animation->SetHideOnCompletion(!!is_hidden);
			g_animation_manager->startAnimation(animation, ANIM_CURVE_SLOW_DOWN, TAB_GROUP_ANIMATION_DURATION, TRUE);
		}
	}
	else
	{
		if (GetAnimation())
		{
			GetAnimation()->SetHideOnCompletion(FALSE);
			if (!is_hidden && GetAnimation()->GetMoveType() == ANIM_MOVE_SHRINK)
				SetAnimation(NULL);
		}

		OpButton::SetHidden(is_hidden);
		InvalidateAll();
		parent->Relayout(TRUE, FALSE);
	}
}

BOOL PagebarButton::IsHiddenOrHiding()
{
	if (GetAnimation())
		return IsHidden() || GetAnimation()->GetHideOnCompletion();
	return IsHidden();
}

void PagebarButton::OnShow(BOOL show)
{
	SetFixedMinMaxWidth((show && m_indicator_button->IsVisible()) || IsCompactButton());
	UpdateMinAndMaxWidth();
	if (show)
	 	Relayout();
}

void PagebarButton::OnRemoving()
{
	OpTabGroupButton* group_button = GetTabGroupButton();
	if (group_button)
	{
		group_button->RefreshIndicatorState();
	}
}

void PagebarButton::OnDeleted()
{
	g_main_message_handler->UnsetCallBack(this, MSG_QUICK_UPDATE_PAGEBAR_BUTTON_TITLE, 0);

	OpButton::OnDeleted();
}

void PagebarButton::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if(msg == MSG_QUICK_UPDATE_PAGEBAR_BUTTON_TITLE && m_desktop_window)
	{
#if defined(GROUP_NUMBER_DEBUGGING) && defined(_DEBUG)
		OpString debug_title, debug_title_format;
		
		debug_title_format.Set(UNI_L("%d "));
		debug_title_format.Append(m_desktop_window->GetTitle());
		debug_title.AppendFormat(debug_title_format, GetGroupNumber());
		
		SetTitle(debug_title);
#else
		SetTitle(m_desktop_window->GetTitle());
#endif // _DEBUG && GROUP_NUMBER_DEBUGGING
		m_delayed_title_update_in_progress = FALSE;
	}
	else
	{
		// we didn't recognize the message, pass it on
		OpButton::HandleCallback(msg, par1, par2);
	}
}

void PagebarButton::OnStartLoading(DocumentDesktopWindow* document_window)
{
	if (document_window->HasURLChanged())
		SetShowPageLoading(true);

	const OpWidgetImage *image = m_desktop_window->GetWidgetImage();
	if(image)
	{
		GetIconSkin()->SetWidgetImage(image);
	}
}

void PagebarButton::OnLoadingFinished(DocumentDesktopWindow* document_window, OpLoadingListener::LoadingFinishStatus status, BOOL was_stopped_by_user)
{
	SetShowPageLoading(false);
	UpdateTextAndIcon(true, true, true);
}

void PagebarButton::OnAuthenticationRequired(OpWindowCommander* commander, OpAuthenticationCallback* callback)
{
	SetShowPageLoading(false);
	UpdateTextAndIcon(true, true, true);
}

BOOL PagebarButton::IsDocumentLoading()
{
	if(GetDesktopWindow() && GetDesktopWindow()->GetType() == WINDOW_TYPE_DOCUMENT)
	{
		DocumentDesktopWindow *win = static_cast<DocumentDesktopWindow *>(GetDesktopWindow());
		return win->IsLoading();
	}

	return FALSE;
}

void PagebarButton::UpdateTextAndIcon(bool update_icon, bool update_text, bool delay_update)
{
	bool update_text_now = false;

	if(update_text)
	{
		if(delay_update)
		{
			// only delay update if we already have a title
			OpString old_title;
			GetTitle(old_title);

			if(old_title.HasContent())
			{
				if(!m_delayed_title_update_in_progress)
				{
					m_delayed_title_update_in_progress = TRUE;
					g_main_message_handler->PostDelayedMessage(MSG_QUICK_UPDATE_PAGEBAR_BUTTON_TITLE, (MH_PARAM_1)this, (MH_PARAM_2)0, PagebarButtonConstants::TitleUpdateDelay);
				}
			}
			else 
			{
				update_text_now = true;
			}
		}
		else
		{
			update_text_now = true;
		}
	}

	if(update_text_now && m_desktop_window)
	{
		SetTitle(m_desktop_window->GetTitle());
	}

	if(update_icon && !m_show_page_loading)
	{
		BOOL show_icon = ShouldShowFavicon();
		if(show_icon && ShouldUpdateIcon() && m_desktop_window)
		{
			const OpWidgetImage *image = m_desktop_window->GetWidgetImage();
			if(g_skin_manager->GetOptionValue("Inverted Pagebar Icons", 0))
			{
				OpString8 image_name;

				image_name.Set(image->GetImage());
				if(image_name.FindI(" Inverted") == KNotFound)
				{
					image_name.Append(" Inverted");
				}
				if(g_skin_manager->GetSkinElement(image_name.CStr()))
				{
					GetIconSkin()->SetImage(image_name.CStr());
				}
				else
				{
					GetIconSkin()->SetWidgetImage(image);
					GetIconSkin()->SetRestrictImageSize(TRUE);
				}
			}
			else
			{
				GetIconSkin()->SetWidgetImage(image);
			}
		}
	}
}

OpToolTipListener::TOOLTIP_TYPE PagebarButton::GetToolTipType(OpToolTip* tooltip) 
{ 
	// Check for the debug tooltip first
	if (IsDebugToolTipActive())
		return OpToolTipListener::TOOLTIP_TYPE_NORMAL;

	DesktopGroupModelItem* group = GetGroup();
	if (group && group->IsCollapsed())
		return OpToolTipListener::TOOLTIP_TYPE_MULTIPLE_THUMBNAILS;

	return OpToolTipListener::TOOLTIP_TYPE_THUMBNAIL;
}

void PagebarButton::GetToolTipThumbnailCollection(OpToolTip* tooltip, OpVector<OpToolTipThumbnailPair>& thumbnail_collection) 
{ 
	OpPagebar *pagebar = GetPagebar();
	if(!pagebar)
	{
		return;
	}
	for(INT32 index = 0; index < pagebar->GetWidgetCount(); index++)
	{
		OpWidget *widget = pagebar->GetWidget(index);
		if(widget && widget->IsOfType(WIDGET_TYPE_PAGEBAR_BUTTON))
		{
			PagebarButton *button = static_cast<PagebarButton *>(widget);
			if(button->GetGroupNumber() == GetGroupNumber())
			{
				OpToolTipThumbnailPair *entry = OP_NEW(OpToolTipThumbnailPair, ());
				if(entry)
				{
					DesktopWindow *window = button->GetDesktopWindow();
					if(window)
					{
						entry->title.Set(window->GetTitle());
						entry->thumbnail = window->GetThumbnailImage(THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT, FALSE);
						entry->is_fixed_image = window->HasFixedThumbnailImage();
						entry->show_close_button = TRUE;
						entry->window_id = window->GetID();
						entry->active = window->IsActive();
					}

					thumbnail_collection.Add(entry);
				}
			}
		}
	}
}

void PagebarButton::GetToolTipText(OpToolTip* tooltip, OpInfoText& text) 
{
	// Check for the debug tooltip first
	if (IsDebugToolTipActive())
		OpWidget::GetToolTipText(tooltip, text);
	else
		m_desktop_window->GetToolTipText(tooltip, text);
}

BOOL PagebarButton::GetPreferredPlacement(OpToolTip* tooltip, OpRect &ref_rect, PREFERRED_PLACEMENT &placement)
{
	if (parent->GetType() == WIDGET_TYPE_PAGEBAR)
	{
		ref_rect = GetRect(TRUE);
		CoreView *view = GetWidgetContainer()->GetView();
		OpPoint container_pos = view->ConvertToScreen(OpPoint());
		ref_rect.OffsetBy(container_pos.x, container_pos.y);

		OpPagebar *pagebar = (OpPagebar *) parent;
		switch (pagebar->GetAlignment())
		{
		case OpBar::ALIGNMENT_LEFT:
			placement = PREFERRED_PLACEMENT_RIGHT;
			break;
		case OpBar::ALIGNMENT_RIGHT:
			placement = PREFERRED_PLACEMENT_LEFT;
			break;
		case OpBar::ALIGNMENT_BOTTOM:
			placement = PREFERRED_PLACEMENT_TOP;
			break;
		default:
			placement = PREFERRED_PLACEMENT_BOTTOM;
			break;
		};
		return TRUE;
	}
	return FALSE;
}

void PagebarButton::SetCloseButtonTooltip()
{
	OpInputAction *action = OP_NEW(OpInputAction, (OpInputAction::ACTION_CLOSE_PAGE));
	if (!action)
		return;
	action->SetActionData(1);

	OpString tooltip_string, fmt_string, shortcut_string;
	g_input_manager->GetShortcutStringFromAction(action, shortcut_string, this);

	if (OpStatus::IsSuccess(g_languageManager->GetString(Str::MI_IDM_MENU_PAGEBAR_REMOVE, tooltip_string)))
	{
		fmt_string.AppendFormat(UNI_L("%s (%s)"), tooltip_string.CStr(), shortcut_string.CStr());

		m_close_button->GetAction()->GetActionInfo().SetTooltipText(fmt_string.CStr());
	}
	OP_DELETE(action);
}

void PagebarButton::OnMouseMove(const OpPoint &point)
{
	OpButton::OnMouseMove(point);

	if (!m_desktop_window)
		return;

	if (GetParentDesktopWindow())
	{
		OpInfoText text;
		m_desktop_window->GetToolTipText(NULL, text);
		GetParentDesktopWindow()->SetStatusText(text.GetStatusText());
	}

	if (m_is_moving_window)
		OnMouseMoveWindow(point);
	else if (parent->GetType() == WIDGET_TYPE_PAGEBAR)
		OnMouseMoveOnPagebar(point);
}

void PagebarButton::OnMouseMoveWindow(const OpPoint& point)
{
	INT32 x, y;
	m_desktop_window->GetOuterPos(x, y);
	x += point.x - m_mousedown_point.x;
	y += point.y - m_mousedown_point.y;
	OpWindow *parent_window = m_desktop_window->GetOpWindow()->GetParentWindow();
	if (parent_window)
	{
		// Limit position within the parent so the user can move it back again.
		UINT32 parent_w, parent_h;
		parent_window->GetInnerSize(&parent_w, &parent_h);
		x = MAX(x, -m_mousedown_point.x + PagebarButtonConstants::WindowLimitThreshold);
		y = MAX(y, -m_mousedown_point.y + PagebarButtonConstants::WindowLimitThreshold);
		x = MIN(x, (INT32)parent_w - m_mousedown_point.x - PagebarButtonConstants::WindowLimitThreshold);
		y = MIN(y, (INT32)parent_h - m_mousedown_point.y - PagebarButtonConstants::WindowLimitThreshold);
	}
	m_desktop_window->SetOuterPos(x, y);
}

void PagebarButton::DelayedActivation(unsigned time)
{
	m_activation_timer.Stop();
	m_activation_timer.SetTimerListener(this);
	m_activation_timer.Start(time);
	m_is_delayed_activation_scheduled = true;
}

void PagebarButton::CancelDelayedActivation()
{
	m_is_delayed_activation_scheduled = false;
	m_activation_timer.Stop();
}

void PagebarButton::OnTimeOut(OpTimer* timer)
{
	if (timer != &m_activation_timer)
	{
		OpPagebarItem::OnTimeOut(timer);
	}
	else if (m_is_delayed_activation_scheduled)
	{
		m_is_delayed_activation_scheduled = false;
		DesktopDragObject* drag_object = static_cast<DesktopDragObject*>(g_drag_manager->GetDragObject());
		if (drag_object)
			drag_object->SetRestoreCaptureWhenLost(true);

		Click();

		if (drag_object)
			drag_object->SetRestoreCaptureWhenLost(false);
	}
}

bool PagebarButton::HasScheduledDelayedActivation()
{
	return m_is_delayed_activation_scheduled;
}

void PagebarButton::OnMouseMoveOnPagebar(const OpPoint& point)
{
	OpPagebar* pagebar = static_cast<OpPagebar*>(parent);
	int delta = pagebar->IsHorizontal() ? point.x - m_mousedown_point.x : point.y - m_mousedown_point.y;

	// Drag far enough out of pagebar, and we will start drag'n'drop instead!
	if (!parent->GetBounds().InsetBy(-20).Contains(OpPoint(point.x + rect.x, point.y + rect.y)) ||
		// Use old dragging for wrapping toolbar for now. Smooth dragging doesn't handle multiple lines.
		(packed2.is_down && op_abs(delta) > PagebarButtonConstants::DragThreshold && pagebar->GetWrapping() == OpBar::WRAPPING_NEWLINE))
	{
		if (m_can_start_drag)
		{
			StopMoving();
			pagebar->ClearAllHoverStackOverlays();
			m_can_start_drag = FALSE;
			StartDrag(point);
		}
		return;
	}

	
	// Start floating mode so we can move the button along the toolbar, overriding the layout.
	if (packed2.is_down && !IsFloating() && op_abs(delta) > PagebarButtonConstants::DragThreshold && !g_drag_manager->IsDragging())
		SetMouseMoveDragging(TRUE);

	// Handle positioning of the button and reordering of tabs as we go.
	if (m_mouse_move_dragging)
		OnMouseMoveDragging(point, delta);
}

void PagebarButton::StartDrag(const OpPoint& point)
{
#ifdef _MACINTOSH_
	// Widgets must not be deleted while handling the drag, since on Mac, this operation is tracked
	// inside the StartDrag call and we may end up with the PagebarButton being deleted before we return.
	LockDeletedWidgetsCleanup lock;
#endif

	OpPagebar* const pagebar = static_cast<OpPagebar*>(parent);
	pagebar->StartDrag(this, 0, point.x, point.y);
	SetMouseMoveDragging(FALSE);
}

void PagebarButton::OnMouseMoveDragging(const OpPoint& point, int delta)
{
	OpPagebar* const pagebar = static_cast<OpPagebar*>(parent);

	// calculate new position
	INT32 button_length = pagebar->IsHorizontal() ? rect.width : rect.height;
	INT32 move_threshold = button_length * PagebarButtonConstants::MoveThreshold.num / PagebarButtonConstants::MoveThreshold.denom;
	INT32 min_move = pagebar->GetPagebarButtonMin() - (move_threshold + 1);
	INT32 max_move = pagebar->GetPagebarButtonMax() + (move_threshold + 1);
	max_move -= button_length;

	int x = rect.x;
	int y = rect.y;
	int& moving = pagebar->IsHorizontal() ? x : y;

	moving += delta;
	moving = MAX(moving, min_move);
	moving = MIN(moving, max_move);

	SetRect(OpRect(x, y, rect.width, rect.height));

	pagebar->ClearAllHoverStackOverlays();

	// force button to the front if it's not already
	if(op_abs(delta) >= PagebarButtonConstants::DragThreshold && m_desktop_window && !m_desktop_window->IsActive())
	{
		Click();
	}

	// Hide tab group button while moving collapsed stacks
	DesktopGroupModelItem* group = GetGroup();
	if (group && group->IsCollapsed())
		pagebar->HideGroupOpTabGroupButton(this, TRUE);

	// Check which widgets we might be hovering over
	OpRect nomargin = GetRectWithoutMargins();

	PagebarButton* target_before = pagebar->GetPagebarButton(nomargin.TopLeft());
	OpPoint delta_to_before = target_before ? rect.TopLeft() - target_before->rect.TopLeft() : OpPoint();

	PagebarButton* target_after = pagebar->GetPagebarButton(nomargin.BottomRight() - OpPoint(1, 1));
	OpPoint delta_to_after = target_after ? target_after->rect.BottomRight() - rect.BottomRight(): OpPoint();

	if (pagebar->IsHorizontal() && GetRTL())
	{
		op_swap(target_before, target_after);
		op_swap(delta_to_before, delta_to_after);
	}

	if (target_before)
		return OnDragOver(target_before, delta_to_before, FALSE);
	else if (target_after)
		return OnDragOver(target_after, delta_to_after, TRUE);

	// Check if we're perhaps a grouped item floating on the edge
	DesktopWindowCollectionItem* item = GetModelItem();
	DesktopWindowCollectionItem* parent = pagebar->GetWorkspace()->GetModelItem();

	if (!GetGroupNumber())
		return;

	if (pagebar->IsHorizontal() && GetRTL())
		op_swap(min_move, max_move);

	if (moving == min_move)
		g_application->GetDesktopWindowCollection().ReorderByItem(*item, parent, NULL);
	else if (moving == max_move)
		g_application->GetDesktopWindowCollection().ReorderByItem(*item, parent, parent->GetLastChildItem());
}

void PagebarButton::OnDragOver(PagebarButton* target, const OpPoint& delta_coords, BOOL target_after)
{
	if (!GetDesktopWindow() || !target->GetDesktopWindow())
		return;

	OpPagebar* const pagebar = static_cast<OpPagebar*>(parent);
	DesktopWindowCollectionItem* item = GetModelItem();
	DesktopWindowCollectionItem* target_item = target->GetModelItem();

	int delta = pagebar->IsHorizontal() ? delta_coords.x : delta_coords.y;
	int target_length = pagebar->IsHorizontal() ? target->rect.width : target->rect.height;
	using namespace PagebarButtonConstants;

	if (delta * MoveThreshold.denom <= target_length * MoveThreshold.num)
	{
		// Move the item in front of or after the target
		DesktopWindowCollectionItem* move_after = target_after ? target_item : target_item->GetPreviousItem();
		DesktopWindowCollectionItem* move_parent = move_after ? move_after->GetParentItem() : target_item->GetParentItem();

		return g_application->GetDesktopWindowCollection().ReorderByItem(*item, move_parent, move_after);
	}
	else if (delta * StackThreshold.denom <= target_length * StackThreshold.num &&
			(target_item->GetType() == WINDOW_TYPE_GROUP || !target->GetGroupNumber()))
	{
		// Stack possibility
		return target->SetUseHoverStackOverlay(TRUE);
	}
	else if (delta * SwapGroupThreshold.denom <= target_length * SwapGroupThreshold.num &&
			target_item->GetType() != WINDOW_TYPE_GROUP && target->GetGroupNumber() != GetGroupNumber() &&
			(target->GetGroupNumber() || item->GetType() != WINDOW_TYPE_GROUP))
	{
		// Group swap
		DesktopWindowCollectionItem* move_after = target_after ? target_item->GetPreviousItem() : target_item;
		return g_application->GetDesktopWindowCollection().ReorderByItem(*item, target_item->GetParentItem(), move_after);
	}
	else if (delta * ExitGroupThreshold.denom <= target_length * ExitGroupThreshold.num &&
			GetGroupNumber() && target->GetGroupNumber() && target->GetGroupNumber() != GetGroupNumber())
	{
		// Exit the group
		DesktopWindowCollectionItem* target_group = target->GetGroup();
		DesktopWindowCollectionItem* move_after = target_after ? target_group->GetPreviousItem() : target_group;
		DesktopWindowCollectionItem* parent = target_group->GetParentItem();

		return g_application->GetDesktopWindowCollection().ReorderByItem(*item, parent, move_after);
	}
}

DesktopWindowCollectionItem* PagebarButton::GetModelItem()
{
	DesktopGroupModelItem* group = GetGroup();

	if (group && group->IsCollapsed())
		return group;

	return &m_desktop_window->GetModelItem();
}

void PagebarButton::StopMoving()
{
	m_is_moving_window = FALSE;
	m_between_groups = FALSE;
	OpPagebar* pagebar = GetPagebar();
	if (pagebar)
	{
		if (GetGroupNumber())
			pagebar->HideGroupOpTabGroupButton(this, FALSE);

		if (m_original_group_number > -1)
			GetPagebar()->ResetOriginalGroupNumber();
	}

	m_original_group_number = -1;
	
	if (parent && IsFloating())
	{
		parent->Relayout();
		SetMouseMoveDragging(FALSE);

#ifdef VEGA_OPPAINTER_SUPPORT
		// Make the tab animate back to its non-floating position (if it's not already doing that or some other animation).
		if (g_animation_manager->GetEnabled() && !GetAnimation())
		{
			QuickAnimationParams params(this);

			params.curve     = ANIM_CURVE_SLOW_DOWN;
			params.move_type = ANIM_MOVE_RECT_TO_ORIGINAL;

			g_animation_manager->startAnimation(params);
		}
#endif
	}
}

void PagebarButton::OnMouseLeave()
{
	if (m_is_moving_window || m_mouse_move_dragging)
		StopMoving();

	OpButton::OnMouseLeave();
}

void PagebarButton::OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	m_can_start_drag = TRUE;

	m_selected_before_click = GetValue() ? TRUE : FALSE;
	m_mousedown_point = point;
	OpButton::OnMouseDown(point, button, nclicks);

	if (button == MOUSE_BUTTON_1 && m_is_move_window_button)
	{
		m_is_moving_window = TRUE;
		m_desktop_window->Activate();
	}
	if (button == MOUSE_BUTTON_2)
		m_can_start_drag = FALSE;

	BOOL speeddial_active = FALSE;
	DocumentDesktopWindow * win = g_application->GetActiveDocumentDesktopWindow();
	if(win) 
	{
		speeddial_active = win->IsSpeedDialActive();
	}

	// Click button immediately on mouse down to achieve better perceived performance.
	// We also get around Z-order problems for the floating button when rearranged.
	if (!speeddial_active && button == MOUSE_BUTTON_1 && !m_selected_before_click)
		Click();
}

BOOL PagebarButton::HasSpeedDialActive()
{
	BOOL active = FALSE;
	if(m_desktop_window && m_desktop_window->GetType() == WINDOW_TYPE_DOCUMENT)
	{
		active = static_cast<DocumentDesktopWindow *>(m_desktop_window)->IsSpeedDialActive(); 
	}
	return active;
}

void PagebarButton::SetIsCompactButton(BOOL compact)
{
	if (compact == m_is_compact_button)
		return;

	m_is_compact_button = compact;

	if (compact)
	{
		SetFixedMinMaxWidth(TRUE);
		SetText(UNI_L(""));
		if(!m_old_min_width && m_old_min_width != 16)
		{
			m_old_min_width = GetMinWidth();
		}
		if(!m_old_max_width && m_old_max_width != 16)
		{
			m_old_max_width = GetMaxWidth();
		}
		UpdateMinAndMaxWidth();
	}
	else
	{
		SetFixedMinMaxWidth(FALSE);
		UpdateTextAndIcon(false, true);
		if(m_old_min_width)
		{
			SetMinWidth(m_old_min_width);
		}
		if(m_old_max_width)
		{
			SetMaxWidth(m_old_max_width);
		}
	}
	UpdateLockedTab();
	ResetRequiredSize();
	OnCompactChanged();
}

void PagebarButton::OnResize(INT32* new_w, INT32* new_h)
{
	OpButton::OnResize(new_w, new_h);
	InvalidateGroupRect();
}

void PagebarButton::OnMove()
{
	OpButton::OnMove();
	InvalidateGroupRect();
}

void PagebarButton::InvalidateGroupRect()
{
	// Invalidate old rect for group skin
	if (parent && !m_old_group_skin_rect.IsEmpty())
	{
		if (!GetGroupNumber())
		{
			// Since we leave the group in the opposite direction of the group, we will have to extend the
			// area a bit to cover the edge within the group (The part that is not in this button change too)
			m_old_group_skin_rect.InsetBy(-20, -20);
		}

		parent->Invalidate(m_old_group_skin_rect);
		m_old_group_skin_rect.Empty();
	}
	OpPagebar *pagebar = GetPagebar();
	if (pagebar && GetGroupNumber())
	{
		// Make sure the areas painted with the group skin is repainted when the button is moved.
		OpSkinElement *elm = pagebar->GetGroupBackgroundSkin(CanUseThumbnails());
		if(elm)
		{
			INT32 left, right, top, bottom;

			elm->GetMargin(&left, &top, &right, &bottom, 0);

			OpRect rect = GetRect();
			if (IsFloating())
			{
				// Extend area to everything between new and old position
				rect.UnionWith(GetOriginalRect());
			}

			rect.x += left;
			rect.y += top;
			rect.width -= left + right;
			rect.height -= top + bottom;

			pagebar->Invalidate(rect);
			m_old_group_skin_rect = rect;
		}
	}
}

void PagebarButton::SetMouseMoveDragging(BOOL is_dragging)
{
	if (m_mouse_move_dragging == is_dragging)
		return;

	m_mouse_move_dragging = is_dragging;
	SetFloating(is_dragging);

	if (!is_dragging)
		return;

	SetOriginalRect(GetRect());

	OpPagebar* pagebar = GetPagebar();
	if (!pagebar)
		return;

	// If this is a collapsed group we need to save the original number 
	// for this tab and all of the other ones in its current group
	DesktopGroupModelItem* group = GetGroup();
	if (group && group->IsCollapsed())
	{
		m_original_group_number = GetGroupNumber();
		pagebar->SetOriginalGroupNumber(GetGroupNumber());
	}
}

// Can you use the point to figure out where exactly it was dropped?
void PagebarButton::OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	BOOL was_floating = IsFloating();

	OpPagebarItem::OnMouseUp(point, button, nclicks);
	OpPagebar* pagebar = GetPagebar();

	if (pagebar && !(parent && IsFloating()) && button == MOUSE_BUTTON_1 && m_selected_before_click &&
		g_pcui->GetIntegerPref(PrefsCollectionUI::ClickToMinimize) && m_desktop_window)
		m_desktop_window->Minimize();

	if (pagebar && m_original_group_number > -1 &&
		IsGrouped() &&
		(UINT32)m_original_group_number != GetGroupNumber() &&
		pagebar->IsGroupExpanded(GetGroupNumber()))
	{
		// This was a collapsed group being dragged into an expanded group, blend in to the other group
		g_application->GetDesktopWindowCollection().UnGroup(GetModelItem());
		pagebar->SetGroupCollapsed(GetGroupNumber(), FALSE);
	}

	StopMoving();

	if (m_is_move_window_button && button == MOUSE_BUTTON_1 && nclicks == 2 && m_desktop_window)
	{
		m_desktop_window->Maximize();
	}

	// double click on a group -> expand it inline or compact if expanded
 	if(!g_pcui->GetIntegerPref(PrefsCollectionUI::ClickToMinimize) && IsGrouped() && button == MOUSE_BUTTON_1 && nclicks == 2)
 	{
 		SetMouseMoveDragging(FALSE);
		if (DesktopGroupModelItem* group = GetGroup())
			group->SetCollapsed(!group->IsCollapsed());
 	}
 	else if(pagebar && was_floating)
	{
 		PagebarButton* target_button = pagebar->FindDropButton();
		if(target_button && (!target_button->IsGrouped() || target_button->GetGroup()->IsCollapsed()))
 		{
			// Add to group if it's not already in a group or it is in a different group
			g_application->GetDesktopWindowCollection().MergeInto(*target_button->GetModelItem(), *GetModelItem(), GetDesktopWindow());
 		}

 		// clear all overlays from stacking
 		pagebar->ClearAllHoverStackOverlays();
	}
}

void PagebarButton::OnLayout()
{
	bool is_close_button_visible = IsCloseButtonVisible();
	OpRect inner_rect = GetBounds();

	GetBorderSkin()->AddPadding(inner_rect);

	UpdateCompactStatus();

	OpRect rect;
	INT32 width, height, indicator_button_shift = 0;

	if(!IsCompactButton() && is_close_button_visible)
	{
		rect = inner_rect;

		m_close_button->UpdateActionStateIfNeeded();
		m_close_button->GetBorderSkin()->AddMargin(rect);

		m_close_button->GetBorderSkin()->GetSize(&width, &height);

		if(!g_skin_manager->GetOptionValue("PageCloseButtonOnLeft", 0))
		{
			rect.x = rect.x + rect.width - width;
			inner_rect.width -= width;
			indicator_button_shift = width;
		}
		else
		{
			inner_rect.x += width;
			inner_rect.width -= width;
		}
		rect.width = width;
		if(g_skin_manager->GetOptionValue("PageCloseButtonOnTop", 0))
		{
			rect.y = 0;
		}
		else
		{
			rect.y = rect.y + (rect.height - height) / 2;
		}
		rect.height = height;

		SetChildRect(m_close_button, rect);
	}

	if (m_indicator_button->IsVisible())
	{
		m_indicator_button->ShowSeparator(is_close_button_visible);
		rect = GetBounds();
		GetBorderSkin()->AddPadding(rect);
		INT32 width = m_indicator_button->GetPreferredWidth();
		rect.x += rect.width - width;
		inner_rect.width = rect.width - width;
		rect.width = width;
		rect.x -= indicator_button_shift;
		SetChildRect(m_indicator_button, rect);
	}

	if (IsCompactButton())
	{
		return;
	}

	if (m_maximize_button)
	{
		rect = inner_rect;
		m_maximize_button->UpdateActionStateIfNeeded();
		m_maximize_button->GetBorderSkin()->AddMargin(rect);

		INT32 width, height;

		m_maximize_button->GetBorderSkin()->GetSize(&width, &height);

		if(!g_skin_manager->GetOptionValue("PageCloseButtonOnLeft", 0))
		{
			rect.x = rect.x + rect.width - width;
			inner_rect.width -= width;
		}
		else
		{	
			inner_rect.x += width;
			inner_rect.width -= width;
		}
		rect.width = width;
		
		if(g_skin_manager->GetOptionValue("PageCloseButtonOnTop", 0))
		{
			rect.y = 0;
		}
		else
		{
			rect.y = rect.y + (rect.height - height) / 2;
		}
		rect.height = height;

		SetChildRect(m_maximize_button, rect);
	}
	
	if (m_minimize_button)
	{
		rect = inner_rect;
		m_minimize_button->UpdateActionStateIfNeeded();
		m_minimize_button->GetBorderSkin()->AddMargin(rect);

		INT32 width, height;

		m_minimize_button->GetBorderSkin()->GetSize(&width, &height);

		if(!g_skin_manager->GetOptionValue("PageCloseButtonOnLeft", 0))
		{
			rect.x = rect.x + rect.width - width;
		}
		rect.width = width;
		if(g_skin_manager->GetOptionValue("PageCloseButtonOnTop", 0))
		{
			rect.y = 0;
		}
		else
		{
			rect.y = rect.y + (rect.height - height) / 2;
		}
		rect.height = height;

		SetChildRect(m_minimize_button, rect);
	}


	UpdateLockedTab();
}

bool PagebarButton::IsCloseButtonVisible()
{
	// Don't show close button for pin tabs.
	if (m_is_compact_button)
	{
		return false;
	}

	// If pagebar is missing it is an unmaximized window and we
	// should always show close button for such windows (DSK-363950)
	if (!GetPagebar())
	{
		return true;
	}

	BOOL is_locked = IsLockedByUser();
	BOOL is_closable = IsClosableByUser();

	if (!is_locked && (!is_closable || !g_pcui->GetIntegerPref(PrefsCollectionUI::ShowCloseButtons)))
	{
		return false;
	}

	return (is_locked || is_closable) &&
	       g_pcui->GetIntegerPref(PrefsCollectionUI::ShowCloseButtons) &&
	       GetPagebar() && GetPagebar()->IsCloseButtonVisible(m_desktop_window, TRUE);
}

void PagebarButton::GetPadding(INT32* left, INT32* top, INT32* right, INT32* bottom)
{
	OpButton::GetPadding(left, top, right, bottom);

	if (m_indicator_button->IsVisible())
	{
		*right += m_indicator_button->GetPreferredWidth();
	}

	if(IsCompactButton())
		return;

	INT32 width = 0;
	INT32 height = 0;
	if (IsCloseButtonVisible())
	{
		m_close_button->GetBorderSkin()->GetSize(&width, &height);
	}

	INT32 left_margin, top_margin, right_margin, bottom_margin;

	m_close_button->GetBorderSkin()->GetMargin(&left_margin, &top_margin, &right_margin, &bottom_margin);

	if(!g_skin_manager->GetOptionValue("PageCloseButtonOnLeft", 0))
		*right += width + left_margin + right_margin;
	else
		*left += width + left_margin + right_margin;

	if (m_maximize_button)
	{
		m_maximize_button->GetBorderSkin()->GetSize(&width, &height);

		m_close_button->GetBorderSkin()->GetMargin(&left_margin, &top_margin, &right_margin, &bottom_margin);

		if(!g_skin_manager->GetOptionValue("PageCloseButtonOnLeft", 0))
			*right += width + left_margin + right_margin;
		else
			*left += width + left_margin + right_margin;
	}

	if (m_minimize_button)
	{
		m_minimize_button->GetBorderSkin()->GetSize(&width, &height);

		m_close_button->GetBorderSkin()->GetMargin(&left_margin, &top_margin, &right_margin, &bottom_margin);

		if(!g_skin_manager->GetOptionValue("PageCloseButtonOnLeft", 0))
			*right += width + left_margin + right_margin;
		else
			*left += width + left_margin + right_margin;
	}
}

void PagebarButton::GetSkinMargins(INT32 *left, INT32 *top, INT32 *right, INT32 *bottom)
{
	OpButton::GetSkinMargins(left, top, right, bottom);

	// Use the group skin spacing for the extra gaph for the group start
	OpPagebar* pagebar = GetPagebar();
	if (pagebar && IsGrouped())
	{
		BOOL first_in_group = !m_desktop_window->GetModelItem().GetPreviousItem() || GetGroup()->IsCollapsed();
		BOOL soon_first_in_group = pagebar->IsGroupCollapsing(GetGroupNumber()) && IsActiveTabForGroup();
		if (first_in_group || soon_first_in_group)
		{
			INT32 spacing = pagebar->GetGroupSpacing(CanUseThumbnails());
			if (pagebar->IsHorizontal())
				*left += spacing;
			else
				*top += spacing;
		}
	}
}

INT32 PagebarButton::GetDropArea( INT32 x, INT32 y )
{
	INT32 flag = DROP_CENTER;

	if( x >= 0 )
	{
		if( x < rect.width/4 )
			flag |= DROP_LEFT;
		else if( x > (rect.width*3)/4 )
			flag |= DROP_RIGHT;
	}

	if( y >= 0 )
	{
		if( y < rect.height/4 )
			flag |= DROP_TOP;
		else if( y > (rect.height*3)/4 )
			flag |= DROP_BOTTOM;
	}

	if( flag & (DROP_LEFT|DROP_RIGHT|DROP_TOP|DROP_BOTTOM) )
	{
		flag &= ~DROP_CENTER;
	}

	return flag;
}

void PagebarButton::OnImageChanged(OpWidgetImage* widget_image)
{
	if(widget_image == GetForegroundSkin() && GetParent())
	{
		INT32 left = 0, top = 0, right = 0, bottom = 0;
		GetPadding(&left, &top, &right, &bottom);
		OpRect rect = GetBounds();
		OpRect content_rect(rect.x + left, rect.y + top, rect.width - left - right, rect.height - top - bottom);

		OpRect image_rect = content_rect;

		BOOL invalidate = !GetParent()->IsOfType(WIDGET_TYPE_PAGEBAR);

		switch (m_button_style)
		{
			case OpButton::STYLE_IMAGE_AND_TEXT_ON_RIGHT:
			{
				image_rect = GetForegroundSkin()->CalculateScaledRect(image_rect, FALSE, TRUE);
				Relayout(TRUE, invalidate);
				Invalidate(image_rect);
				return;
			}
			case OpButton::STYLE_IMAGE_AND_TEXT_ON_LEFT:
			{
				image_rect = GetForegroundSkin()->CalculateScaledRect(image_rect, FALSE, TRUE);
				image_rect.x = content_rect.x + content_rect.width - image_rect.width;
				Relayout(TRUE, invalidate);
				Invalidate(image_rect);
				return;
			}
		}
	}
	OpButton::OnImageChanged(widget_image);
}

OP_STATUS PagebarButton::SetTitle(const OpStringC& title)
{
	if(IsCompactButton())
	{
		return OpStatus::OK;
	}

	// Set script to allow better font switch
	if (m_desktop_window && m_desktop_window->GetWindowCommander())
		m_script = m_desktop_window->GetWindowCommander()->GetPreferredScript(FALSE);

	return SetText(title.CStr()); 
}

OpPagebar* PagebarButton::GetPagebar()
{
	return (parent && parent->GetType() == WIDGET_TYPE_PAGEBAR) ? static_cast<OpPagebar*>(parent) : NULL;
}

void PagebarButton::SetGroupNumber(UINT32 group_number, BOOL update_active_and_hidden) 
{
	if (GetGroupNumber() == group_number)
		return;

	// Change the group number
	OpPagebarItem::SetGroupNumber(group_number); 

	InvalidateGroupRect();
}

UINT32 PagebarButton::GetGroupNumber(BOOL prefer_original_number) 
{ 
	if (prefer_original_number && m_original_group_number > -1)
		return (UINT32)m_original_group_number;
	
	return OpPagebarItem::GetGroupNumber(prefer_original_number); 
}

BOOL PagebarButton::IsActiveTabForGroup()
{
	DesktopGroupModelItem* group = GetGroup();
	return group && group->GetActiveDesktopWindow() == GetDesktopWindow();
}

DesktopGroupModelItem* PagebarButton::GetGroup()
{
	DesktopWindow* window = GetDesktopWindow();
	return window ? window->GetParentGroup() : NULL;
}

void PagebarButton::OnPaintAfterChildren(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	if(GetUseHoverStackOverlay())
	{
		OpSkinElement *elm = g_skin_manager->GetSkinElement("Pagebar Hover Overlay Skin");
		if(elm)
		{
			elm->DrawOverlay(GetVisualDevice(), GetBounds(), GetBorderSkin()->GetState(), GetBorderSkin()->GetHoverValue(), NULL);
		}
	}
}

DesktopDragObject* PagebarButton::GetDragObject(OpTypedObject::Type type, INT32 x, INT32 y)
{
	DesktopWindow* window = (DesktopWindow*) GetUserData();

	if (!window)
	{
		return OpWidget::GetDragObject(type, x, y);
	}
	Image image = window->GetThumbnailImage(THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT, TRUE);
	if(image.IsEmpty())
	{
		return OpWidget::GetDragObject(type, x, y);
	}
	OpDragObject* op_drag_object;
	if (OpStatus::IsError(OpDragObject::Create(op_drag_object, type)))
		return NULL;
	DesktopDragObject* drag_object = static_cast<DesktopDragObject*>(op_drag_object);

	OpBitmap* bitmap = NULL;

	OpRect rect(0, 0, image.Width(), image.Height());

	RETURN_VALUE_IF_ERROR(OpBitmap::Create(&bitmap, rect.width, rect.height, FALSE, TRUE, 0, 0, TRUE), drag_object);

	VisualDevice vd;

	vd.painter = bitmap->GetPainter();

	if (vd.painter)
	{
		vd.painter->SetColor(OP_RGB(255, 255, 255));
		vd.painter->FillRect(rect);

		vd.ImageOut(image, rect, rect, NULL);

		vd.painter = NULL;

		bitmap->ReleasePainter();

		drag_object->SetBitmap(bitmap);

		// find pos
		OpPoint point(0, 0);
		drag_object->SetBitmapPoint(point);
	}
	else
	{
		OP_DELETE(bitmap);
	}
	return drag_object;
}

OpRect PagebarButton::GetRectWithoutMargins()
{
	INT32 left, top, right, bottom;
	GetSkinMargins(&left, &top, &right, &bottom);

	OpRect rect(GetRect());
	rect.x -= left;
	rect.y -= top;
	rect.width += right;
	rect.height += bottom;
	
	return rect;
}

void PagebarButton::SetValue(INT32 value)
{
	m_indicator_button->SetVisibility(value ? FALSE : TRUE);
	BOOL is_indicator_visible = m_indicator_button->IsVisible();
	SetFixedMinMaxWidth(is_indicator_visible || m_is_compact_button);
	UpdateMinAndMaxWidth();
	OpButton::SetValue(value);
}

void PagebarButton::UpdateMinAndMaxWidth()
{
	INT32 min_width = -1;

	if (m_indicator_button->IsVisible())
	{
		min_width = m_indicator_button->GetPreferredWidth();
		INT32 left, top, right, bottom;
		OpButton::GetPadding(&left, &top, &right, &bottom);
		min_width += left + right;
	}
	else if (m_is_compact_button)
	{
		min_width = 16;
	}

	SetMinWidth(min_width);
	if (m_is_compact_button)
	{
		SetMaxWidth(min_width);
	}
}

void PagebarButton::AddIndicator(OpIndicatorButton::IndicatorType type)
{
	m_indicator_button->AddIndicator(type);
	OpTabGroupButton* group_button = GetTabGroupButton();
	if (group_button)
	{
		group_button->RefreshIndicatorState();
	}
	UpdateMinAndMaxWidth();
}

void PagebarButton::RemoveIndicator(OpIndicatorButton::IndicatorType type)
{
	m_indicator_button->RemoveIndicator(type);
	OpTabGroupButton* group_button = GetTabGroupButton();
	if (group_button)
	{
		group_button->RefreshIndicatorState();
	}
	UpdateMinAndMaxWidth();
}

OpTabGroupButton* PagebarButton::GetTabGroupButton()
{
	OpTabGroupButton* button = NULL;
	OpWidget* pagebar = GetParent();
	if (pagebar && pagebar->GetType() == WIDGET_TYPE_PAGEBAR)
	{
		button = static_cast<OpPagebar*>(pagebar)->FindTabGroupButtonByGroupNumber(GetGroupNumber());
	}
	return button;
}
