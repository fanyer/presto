/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/OpToolTip.h"

#include "modules/pi/OpWindow.h"
#include "modules/pi/OpDragManager.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/OpScreenInfo.h"
#include "modules/dragdrop/dragdrop_manager.h"

#include "adjunct/desktop_util/rtl/uidirection.h"
#include "adjunct/quick/thumbnails/GenericThumbnail.h"
#include "adjunct/quick/thumbnails/TabThumbnailContent.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick/models/DesktopWindowCollection.h"
#include "adjunct/quick/quick-widget-names.h"

#include "modules/widgets/OpMultiEdit.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/style/css.h"
#include "modules/display/FontAtt.h"
#include "modules/display/styl_man.h"
#include "modules/display/vis_dev.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/libgogi/pi_impl/mde_opview.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick/managers/AnimationManager.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick_toolkit/widgets/QuickFlowLayout.h"
#include "adjunct/quick_toolkit/widgets/QuickScrollContainer.h"
#include "adjunct/quick_toolkit/widgets/QuickOpWidgetWrapper.h"

#if defined(_UNIX_DESKTOP_) || defined(MSWIN)
// We need a better interface [espen 2003-03-20]
BOOL IsPanning();
#endif

// when defined, even tooltips without thumbnails will use the skin and not be yellow
#define ALL_TOOLTIPS_SKINABLE

#define MAX_CALLOUT_THUMBNAIL_LAYOUT_WIDTH	800u
#define MAX_CALLOUT_THUMBNAIL_LAYOUT_HEIGHT	600u

#define THUMBNAIL_MINIMUM_WIDTH 	120
#define THUMBNAIL_MINIMUM_HEIGHT	90
#define THUMBNAIL_MAXIMUM_WIDTH		256
#define THUMBNAIL_MAXIMUM_HEIGHT	192


TooltipThumbnailEntry::~TooltipThumbnailEntry()
{
	if (thumbnail && !thumbnail->IsDeleted())
		thumbnail->Delete();
}

// == OpToolTip =============================================

OpToolTipWindow::OpToolTipWindow(OpToolTip *tooltip) :
	m_thumbnail_mode(FALSE)
	, m_close_on_animation_completion(FALSE)
	, m_parent_window(NULL)
	, m_tooltip(tooltip)
	, m_has_preferred_placement(FALSE)
	, m_content(NULL)
	, m_edit(NULL)
	, m_scroll_container(NULL)
{
}

OpToolTipWindow::~OpToolTipWindow()
{
	// may be NULL if Init failed
	if (m_widget_container)
		m_widget_container->SetWidgetListener(NULL);

	m_tooltip->OnTooltipWindowClosing(this);
	if (m_parent_window)
		m_parent_window->RemoveListener(this);

	OP_DELETE(m_scroll_container);
}

void OpToolTipWindow::SetShowThumbnail(BOOL enable)
{
	UINT32 i;

	for(i = 0; i < m_thumbnails.GetCount(); i++)
	{
		TooltipThumbnailEntry *entry = m_thumbnails.Get(i);

		entry->thumbnail->SetVisibility(enable);
	}
	m_edit->SetVisibility(!enable);
}

void OpToolTipWindow::OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated)
{
	if (desktop_window == m_parent_window)
		Close(TRUE);
	else
		QuickAnimationWindowObject::OnDesktopWindowClosing(desktop_window, user_initiated);
}

OP_STATUS OpToolTipWindow::Init(BOOL thumbnail_mode)
{
	m_thumbnail_mode = thumbnail_mode;

	if (m_thumbnail_mode)
	{
		// Linux requires us to get a parent window when using OpWindow::EFFECT_TRANSPARENT.
		// Listen to it so we can delete ourself if it happens to be removed when we're still alive.
		m_parent_window = g_application->GetActiveBrowserDesktopWindow();
		if (m_parent_window)
		{
			m_parent_window->AddListener(this);
		}
	}
	// TODO: MAKE NAME DEFINE
	RETURN_IF_ERROR(DesktopWidgetWindow::Init(TOOLTIP_WINDOW_STYLE, "Tooltip Window", m_parent_window ? m_parent_window->GetOpWindow() : NULL,
											  NULL, m_thumbnail_mode ? OpWindow::EFFECT_TRANSPARENT : OpWindow::EFFECT_DROPSHADOW));
	
	RETURN_IF_ERROR(OpMultilineEdit::Construct(&m_edit));

	m_widget_container->SetWidgetListener(this);	

	OpWidget *root = m_widget_container->GetRoot();
	root->SetHasCssBorder(FALSE);
	root->SetCanHaveFocusInPopup(TRUE);
	root->SetRTL(UiDirection::Get() == UiDirection::RTL);

	m_edit->SetWrapping(m_thumbnail_mode);
	m_edit->SetLabelMode();
	m_edit->SetName(WIDGET_NAME_TOOLTIP_EDIT);


	root->AddChild(m_edit);

	if (m_thumbnail_mode)
	{
		root->GetBorderSkin()->SetImage("Thumbnail Tooltip Skin", "Tooltip Skin");
		root->GetForegroundSkin()->SetImage("Thumbnail Tooltip Skin", "Tooltip Skin");
	}
	else
	{
		// Regular tooltip

		// All platforms should use a real value for OP_SYSTEM_COLOR_TOOLTIP_TEXT, not hardcoded
#if defined(_UNIX_DESKTOP_)
		UINT32 color = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_TOOLTIP_TEXT);
		g_skin_manager->GetTextColor("Tooltip Skin", &color, 0, SKINTYPE_DEFAULT, SKINSIZE_DEFAULT, TRUE);
		m_edit->SetForegroundColor(color);
#endif
		root->GetBorderSkin()->SetImage("Tooltip Skin", "Edit Skin");
		root->GetForegroundSkin()->SetImage("Tooltip Skin", "Edit Skin");
	}
#ifndef ALL_TOOLTIPS_SKINABLE
	if(!m_thumbnail_mode)
	{
		m_edit->SetBackgroundColor(g_op_ui_info->GetUICSSColor(CSS_VALUE_InfoBackground));
		m_edit->SetForegroundColor(g_op_ui_info->GetUICSSColor(CSS_VALUE_InfoText));

		root->SetHasCssBorder(TRUE);

		// Required for proper frame on all platforms
		m_widget_container->SetEraseBg(TRUE);
#if defined(_MACINTOSH_)
		root->SetBackgroundColor(g_op_ui_info->GetUICSSColor(CSS_VALUE_InfoBackground));
#else
		root->SetBackgroundColor(OP_RGB(0, 0, 0));
#endif

	}
	else
 #endif // ALL_TOOLTIPS_SKINABLE
	{
		root->SetHasCssBorder(FALSE);
	}
	RETURN_IF_ERROR(QuickAnimationWindowObject::Init(NULL, NULL, GetWindow()));

	return OpStatus::OK;
}

void OpToolTipWindow::OnAnimationComplete()
{
	QuickAnimationWindowObject::OnAnimationComplete();
	if (m_close_on_animation_completion)
	{
		Show(FALSE);
		Close(FALSE);
	}
}

BOOL OpToolTipWindow::DeleteWidgetFromLayout(QuickWidget *delete_widget)
{
	if(!m_content)
		return FALSE;

	// delete them from the layout
	for(UINT32 n = 0; n < m_content->GetWidgetCount(); n++)
	{
		QuickWidget *widget = m_content->GetWidget(n);
		if(widget == delete_widget)
		{
			m_content->RemoveWidget(n);
			return TRUE;
		}
	}
	return FALSE;
}

BOOL OpToolTipWindow::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_ACTIVATE_GROUP_WINDOW:
		{
			INT32 id = action->GetActionData();
			OP_ASSERT(id);
			if(id)
			{
				UINT32 i;

				for(i = 0; i < m_thumbnails.GetCount(); i++)
				{
					TooltipThumbnailEntry *entry = m_thumbnails.Get(i);

					// update the selected state in our layout
					entry->thumbnail->SetSelected(entry->group_window_id == id);
				}
				DesktopWindow* desktop_window = g_application->GetDesktopWindowCollection().GetDesktopWindowByID(id);
				if(desktop_window)
				{
					desktop_window->Activate(TRUE);
				}
				return TRUE;
			}
		}
		break;

		case OpInputAction::ACTION_THUMB_CLOSE_PAGE:
		{
			INT32 id = action->GetActionData();
			OP_ASSERT(id);
			if(id)
			{
				UINT32 i;

				for(i = 0; i < m_thumbnails.GetCount(); i++)
				{
					TooltipThumbnailEntry *entry = m_thumbnails.Get(i);

					if(entry->group_window_id == id)
					{
						DeleteWidgetFromLayout(entry->quickwidget_thumbnail);
						m_thumbnails.Delete(i);

						UINT32 width = 0, height = 0;
						GetRequiredSize(width, height);

						// TODO: following code is duplicated in OpTooltip, merge.

						if(!m_has_preferred_placement)
						{
							m_has_preferred_placement = m_tooltip->GetListener()->GetPreferredPlacement(m_tooltip, m_ref_screen_rect, m_placement);
						}
						OP_ASSERT(m_has_preferred_placement);

						OpRect rect = GetPlacement(width, height, this->GetWindow(), GetWidgetContainer(), m_has_preferred_placement, m_ref_screen_rect, m_placement);		

						BOOL set_preferred_rectangle = TRUE;

						if (g_animation_manager->GetEnabled())
						{
							QuickAnimationWindowObject::reset();

							double duration = static_cast<double>(THUMBNAIL_ANIMATION_DURATION);

							// Thumbnail was visible, so animate from the old position to the new.
							int x, y;
							UINT32 w, h;
							GetWindow()->GetOuterPos(&x, &y);
							GetWindow()->GetOuterSize(&w, &h);
							OpRect target_rect(x, y, w, h);
							if (!target_rect.Equals(rect))
							{
								QuickAnimationWindowObject::animateRect(target_rect, rect);
								set_preferred_rectangle = FALSE;
								if (IsAnimating()) // Give a little extra boost when moving mouse faster.
									g_animation_manager->startAnimation(this, ANIM_CURVE_SLOW_DOWN, duration);
								else
									g_animation_manager->startAnimation(this, ANIM_CURVE_BEZIER, duration);
							}
						}
						if(!m_thumbnails.GetCount())
						{
							// last tab, close window when animation is done
							m_close_on_animation_completion = TRUE;
						}
						Show(TRUE, set_preferred_rectangle ? &rect : NULL);

						OpInputAction* close_action = OP_NEW(OpInputAction, (OpInputAction::ACTION_CLOSE_WINDOW));
						if (!close_action)
							return TRUE;
						close_action->SetActionData(id);
						g_input_manager->InvokeAction(close_action);

						break;
					}
				}
			}
			return TRUE;
		}
		break;
	}
	return FALSE;
}

#define MAX_THUMBNAILS_PER_ROW	4

OP_STATUS OpToolTipWindow::RecreateLayout()
{
	OpWidget *root = m_widget_container->GetRoot();

	// Create the main stack layout, the old m_content is deleted in the SetContent call further down
	m_content = OP_NEW(QuickFlowLayout, ());
	if(!m_content)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	m_content->KeepRatio(true);
	if(!m_scroll_container)
	{
		m_scroll_container = OP_NEW(QuickScrollContainer, (QuickScrollContainer::VERTICAL, m_thumbnails.GetCount() > 16 ? QuickScrollContainer::SCROLLBAR_AUTO : QuickScrollContainer::SCROLLBAR_NONE));
		if(!m_scroll_container)
		{
			OP_DELETE(m_content);
			return OpStatus::ERR_NO_MEMORY;
		}
		RETURN_IF_ERROR(m_scroll_container->Init());

		m_scroll_container->SetRespectScrollbarMargin(false);
		m_scroll_container->SetParentOpWidget(root);
	}
	m_scroll_container->SetContent(m_content);	// will delete any previous content

	for (UINT32 n = 0; n < m_thumbnails.GetCount(); n++)
	{
		TooltipThumbnailEntry* entry = m_thumbnails.Get(n);
		QuickWidget* widget = QuickWrap(entry->thumbnail);
		RETURN_OOM_IF_NULL(widget);
		RETURN_IF_ERROR(m_content->InsertWidget(widget));

		widget->SetMinimumWidth(THUMBNAIL_MINIMUM_WIDTH);
		widget->SetMinimumHeight(THUMBNAIL_MINIMUM_HEIGHT);

		entry->quickwidget_thumbnail = widget;
		entry->thumbnail->SetSelected(entry->active);
	}
	m_content->SetMaxBreak(MAX_THUMBNAILS_PER_ROW);

	DoLayout();

	return OpStatus::OK;
}

// force a layout
void OpToolTipWindow::DoLayout()
{
	UINT32 w, h;

	m_window->GetOuterSize(&w, &h);

	OpRect rect(0, 0, w, h);

	m_widget_container->GetRoot()->GetBorderSkin()->AddPadding(rect);

	if(m_thumbnails.GetCount())
	{
		m_scroll_container->Layout(rect);
	}
	else
	{
		m_edit->SetRect(rect);
	}
}

void OpToolTipWindow::OnResize(UINT32 width, UINT32 height)
{
	WidgetWindow::OnResize(width, height);

	OpRect rect(0, 0, width, height);

	m_widget_container->GetRoot()->GetBorderSkin()->AddPadding(rect);

	if(m_thumbnails.GetCount())
	{
		m_scroll_container->SetScrollbarBehavior(m_thumbnails.GetCount() > 16 ? QuickScrollContainer::SCROLLBAR_AUTO : QuickScrollContainer::SCROLLBAR_NONE);
		m_scroll_container->Layout(rect);
	}
	else
	{
		m_edit->SetRect(rect);
	}
}

void OpToolTipWindow::GetRequiredSize(UINT32& width, UINT32& height)
{
	INT32 left, top, right, bottom;

	width = height = 0;

	if(m_thumbnails.GetCount() && m_scroll_container)
	{
		width = min(m_scroll_container->GetNominalWidth(), MAX_CALLOUT_THUMBNAIL_LAYOUT_WIDTH);
		height = min(m_scroll_container->GetNominalHeight(width), MAX_CALLOUT_THUMBNAIL_LAYOUT_HEIGHT);
	}
	else
	{
		OpRect edit_rect = m_edit->GetRect();
		if (m_edit->IsWrapping())
		{
			// Use same width as the thumbnailed mode when wrapping is on.
			// We must set it here so the wrapping is done consistently,
			// because GetContentWidth depends on it.
			width = THUMBNAIL_MAXIMUM_WIDTH;
			height = THUMBNAIL_MAXIMUM_HEIGHT;

			m_edit->SetRect(OpRect(edit_rect.x, edit_rect.y, width, edit_rect.height));
		}
		m_edit->GetPadding(&left, &top, &right, &bottom);

		width = m_edit->GetMaxBlockWidth() + left + right;
		height = m_edit->GetContentHeight() + top + bottom;

		if (m_edit->IsWrapping())
		{
			// Restore edit rect now.
			m_edit->SetRect(edit_rect);	
		}
	}
	m_widget_container->GetRoot()->GetPadding(&left, &top, &right, &bottom);

	width += left + right;
	height += top + bottom;
}

void OpToolTipWindow::Update()
{
	// Maybe used this instead
	// m_edit->SetFont(OP_SYSTEM_FONT_UI_DIALOG);
	// m_edit->SetDefaultUIFont(m_edit->font_info.justify); // re-calc height

	FontAtt font;
	g_pcfontscolors->GetFont(OP_SYSTEM_FONT_UI_TOOLTIP, font);

	if (const OpFontInfo *font_info = styleManager->GetFontInfo(font.GetFontNumber()))
	{
		if (!m_thumbnails.GetCount())
		{
			const JUSTIFY justify = UiDirection::Get() == UiDirection::LTR ? JUSTIFY_LEFT : JUSTIFY_RIGHT;
			m_edit->SetFontInfo(font_info, font.GetSize(), font.GetItalic(), font.GetWeight(), justify);
		}
	}

	if (!m_thumbnail_mode)
	{
		// Regular tooltip

		// All platforms should use a real value for OP_SYSTEM_COLOR_TOOLTIP_TEXT, not hardcoded
#if defined(_UNIX_DESKTOP_)
		UINT32 color = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_TOOLTIP_TEXT);
		g_skin_manager->GetTextColor("Tooltip Skin", &color, 0, SKINTYPE_DEFAULT, SKINSIZE_DEFAULT, TRUE);
		m_edit->SetForegroundColor(color);
#endif
	}
}

OP_STATUS OpToolTipWindow::CreateThumbnailEntry(TooltipThumbnailEntry **entry, OpToolTipThumbnailPair* entry_pair)
{
	OpAutoPtr<TabThumbnailContent> content(OP_NEW(TabThumbnailContent, ()));
	RETURN_OOM_IF_NULL(content.get());
	RETURN_IF_ERROR(content->Init(*entry_pair));

	OpAutoPtr<GenericThumbnail> gen_thumb(OP_NEW(GenericThumbnail, ()));
	RETURN_OOM_IF_NULL(gen_thumb.get());
	GenericThumbnail::Config thumb_config;
	thumb_config.m_title_border_image = "Thumbnail Title Label Skin";
	thumb_config.m_close_border_image = "Thumbnail Close Button Skin";
	thumb_config.m_close_foreground_image = "Speeddial Close";
	thumb_config.m_drag_type = DRAG_TYPE_THUMBNAIL;
	RETURN_IF_ERROR(gen_thumb->Init(thumb_config));
	RETURN_IF_ERROR(gen_thumb->SetContent(content.release()));
	gen_thumb->GetBorderSkin()->SetImage("Thumbnail Widget Skin");

	*entry = OP_NEW(TooltipThumbnailEntry, (gen_thumb.get(), entry_pair->is_fixed_image, entry_pair->show_close_button, entry_pair->window_id, entry_pair->active));
	RETURN_OOM_IF_NULL(*entry);
	gen_thumb.release();

	return OpStatus::OK;
}

OP_STATUS OpToolTipWindow::SetThumbnailEntry(UINT32 offset, OpToolTipThumbnailPair* entry_pair)
{
	OpWidget *root = m_widget_container->GetRoot();

	TooltipThumbnailEntry *entry = m_thumbnails.Get(offset);
	if(entry && entry->thumbnail && entry_pair->show_close_button != entry->show_close_button)
	{
		m_thumbnails.Remove(offset);

		RETURN_IF_ERROR(CreateThumbnailEntry(&entry, entry_pair));

		OP_STATUS s = m_thumbnails.Insert(offset, entry);
		if(OpStatus::IsError(s))
		{
			OP_DELETE(entry);
			return s;
		}
		root->Relayout();
	}
	if(!entry)
	{
		RETURN_IF_ERROR(CreateThumbnailEntry(&entry, entry_pair));

		OP_STATUS s = m_thumbnails.Add(entry);
		if(OpStatus::IsError(s))
		{
			OP_DELETE(entry);
			return s;
		}
	}
	entry->is_fixed_image = entry_pair->is_fixed_image;
	entry->thumbnail->SetEnabled(TRUE);
	entry->thumbnail->SetVisibility(m_thumbnail_mode);

	// Set Name (for access through scope)
	OpString8 name;
	if (OpStatus::IsSuccess(name.AppendFormat("Thumbnail %d", offset)))
	{
		entry->thumbnail->SetName(name.CStr());
	}

	root->Relayout();

#ifdef _DEBUG
//	uni_char buf[1024];
//	wsprintf(buf, UNI_L("SetThumbnailEntry(%d), image valid: %s, visible: %s\n"), offset, entry->thumbnail->IsEmpty() ? UNI_L("NO") : UNI_L("YES"), entry->thumbnail->IsVisible() ? UNI_L("YES") : UNI_L("NO"));
//	OutputDebugStringW(buf);
#endif

	return OpStatus::OK;
}

void OpToolTipWindow::ClearThumbnailEntries(UINT32 max_entries)
{
//#ifdef _DEBUG
//	uni_char buf[1024];
//	wsprintf(buf, UNI_L("ClearThumbnailEntries(%d)\n"), max_entries);
//	OutputDebugStringW(buf);
//#endif

	while(m_thumbnails.GetCount() > max_entries)
	{
		UINT32 offset = m_thumbnails.GetCount() - 1;

		DeleteWidgetFromLayout(m_thumbnails.Get(offset)->quickwidget_thumbnail);

		m_thumbnails.Delete(offset);
	}
}

// == OpToolTip =============================================

OpToolTip::OpToolTip()
	: m_thumb_window(NULL)
	, m_text_window(NULL)
	, m_listener(NULL)
	, m_item(0)
	, m_thumb_visible(FALSE)
	, m_text_visible(FALSE)
	, m_timer(NULL)
	, m_is_fixed_image(FALSE)
	, m_is_sticky(FALSE)
	, m_invalidate_thumbnails(FALSE)
	, m_show_on_hover(FALSE)
{
}

OpToolTip::~OpToolTip()
{
	if (m_timer)
	{
		m_timer->Stop();
		OP_DELETE(m_timer);
	}
	if (m_thumb_window)
		m_thumb_window->Close(TRUE);
	if (m_text_window)
		m_text_window->Close(TRUE);
}

void OpToolTip::SetListener(OpToolTipListener* listener, uni_char key)
{
	BOOL changed = m_listener != listener;
	BOOL item_changed = FALSE;

	if(m_show_on_hover && m_thumb_window && m_thumb_visible)
	{
		VisualDevice* visual_device = m_thumb_window->GetWidgetContainer()->GetRoot()->GetVisualDevice();

		OpPoint point;

		visual_device->GetView()->GetMousePos(&point.x, &point.y);

		point.x = visual_device->ScaleToDoc(point.x);
		point.y = visual_device->ScaleToDoc(point.y);

		OpRect rect = m_thumb_window->GetWidgetContainer()->GetRoot()->GetRect();

		// stick means we only allow it to close when the mouse leaves the area
		if(key != OP_KEY_ESCAPE && rect.Contains(point))
		{
			SetSticky(TRUE);
			m_thumb_window->SetFocus(FOCUS_REASON_OTHER);
			return;
		}
		else
		{
			m_thumb_window->GetWidgetContainer()->GetRoot()->ReleaseFocus(FOCUS_REASON_OTHER);
			SetSticky(FALSE);
		}
	}
	m_listener = listener;
	m_show_on_hover = listener && listener->CanHoverToolTip();

	if (m_listener)
	{
		INT32 new_item = listener->GetToolTipItem(this);

		item_changed = new_item != m_item;
		changed = changed || abs(new_item - m_item) == 1;

		m_item = new_item;
	}
	else
	{
		m_item = 0;
	}

	if (!m_listener)
	{
		Hide();
		m_timer->Stop();
	}
	else
	{
		BOOL fast_change = changed && (m_thumb_visible || m_text_visible) && listener->HasToolTipText(this);
		UINT32 delay = 0;

		if(changed)
		{
			m_thumbnail_collection.DeleteAll();

			if(m_listener->GetToolTipType(this) == OpToolTipListener::TOOLTIP_TYPE_MULTIPLE_THUMBNAILS)
			{
				m_listener->GetToolTipThumbnailCollection(this, m_thumbnail_collection);

				if(!m_thumbnail_collection.GetCount())
				{
					Hide();
					return;
				}
			}
//#ifdef _DEBUG
//			uni_char buf[1024];
//			wsprintf(buf, UNI_L("Invalidating thumbnails, listener: %08x, listener type: %d\n"), listener, listener->GetToolTipType(this));
//			OutputDebugStringW(buf);
//#endif
			m_invalidate_thumbnails = TRUE;
		}
		BOOL direct_hide = TRUE;
		if (fast_change)
		{
			// Find out if the switch is from thumbnail to text.
			// If it is, we don't want a fast change, and we don't want
			// to hide the thumbnail immediately either.

			if (m_thumb_visible && m_listener->GetToolTipType(this) == OpToolTipListener::TOOLTIP_TYPE_NORMAL)
			{
				fast_change = FALSE;
				direct_hide = FALSE;

				// Get the delay here without the image set since it's affected by having a image set or not.
				Image old_image = m_image;
				m_image.Empty();
				delay = listener->GetToolTipDelay(this);
				m_image = old_image;
			}
		}

		if (fast_change)
		{
			Show();
		}
		else if ((!m_thumb_visible && !m_text_visible) || changed || item_changed)
		{
			if ((changed || item_changed) && direct_hide)
			{
				Hide();
			}

			m_timer->Stop();

			if (listener->HasToolTipText(this))
			{
				m_timer->Start(delay ? delay : listener->GetToolTipDelay(this));
			}
		}
	}
}

OP_STATUS OpToolTip::Init()
{
	m_timer = OP_NEW(OpTimer, ());
	if (!m_timer)
		return OpStatus::ERR;
	m_timer->SetTimerListener(this);

	return OpStatus::OK;
}

void OpToolTip::OnTooltipWindowClosing(OpToolTipWindow *window)
{
	if (window == m_thumb_window)
		m_thumb_window = NULL;
	else if (window == m_text_window)
		m_text_window = NULL;
}

#define VERTICAL_OFFSET 21

OpRect GetPlacement(int width, int height, OpWindow *window, WidgetContainer* widget_container,
					BOOL has_preferred_placement, const OpRect &ref_screen_rect, 
					OpToolTipListener::PREFERRED_PLACEMENT placement)
{
	OpPoint screen_pos;
	widget_container->GetView()->GetMousePos(&screen_pos.x, &screen_pos.y);
	screen_pos = widget_container->GetView()->ConvertToScreen(screen_pos);
	screen_pos.y += VERTICAL_OFFSET;
	OpRect rect(screen_pos.x, screen_pos.y, width, height);

	widget_container->GetRoot()->GetBorderSkin()->GetArrow()->Reset();

	if (has_preferred_placement)
	{
		INT32 margin_left, margin_top, margin_right, margin_bottom;
		widget_container->GetRoot()->GetBorderSkin()->GetMargin(&margin_left, &margin_top, &margin_right, &margin_bottom);

		widget_container->GetRoot()->GetBorderSkin()->GetArrow()->SetOffset(50);

		OpScreenProperties screen_props;
		g_op_screen_info->GetProperties(&screen_props, window, &screen_pos);
		OpRect screen(screen_props.screen_rect);

		// Thumbnails can be clipped if there is no external compositing
		DesktopWindow* dw = g_application->GetActiveDesktopWindow(TRUE);
		if (dw && !dw->SupportsExternalCompositing())
		{
			dw->GetInnerPos(screen.x, screen.y);
			dw->GetInnerSize((UINT32&)screen.width, (UINT32&)screen.height);
			INT32 top, ignore;
			dw->GetOpWindow()->GetMargin(&ignore, &top, &ignore, &ignore);
			screen.height += top;
			screen.IntersectWith(screen_props.workspace_rect);
		}

		int attempt = 0;
		while (attempt++ < 2)
		{
			switch (placement)
			{
				case OpToolTipListener::PREFERRED_PLACEMENT_RIGHT:
					rect.x = ref_screen_rect.x + ref_screen_rect.width;
					rect.y = ref_screen_rect.y + (ref_screen_rect.height - rect.height) / 2;
					widget_container->GetRoot()->GetBorderSkin()->GetArrow()->SetLeft();
					rect.x += margin_left;
					break;

				case OpToolTipListener::PREFERRED_PLACEMENT_BOTTOM:
					rect.x = ref_screen_rect.x + (ref_screen_rect.width - rect.width) / 2;
					rect.y = ref_screen_rect.y + ref_screen_rect.height;
					widget_container->GetRoot()->GetBorderSkin()->GetArrow()->SetTop();
					rect.y += margin_top;
					break;

				case OpToolTipListener::PREFERRED_PLACEMENT_LEFT:
					rect.x = ref_screen_rect.x - rect.width;
					rect.y = ref_screen_rect.y + (ref_screen_rect.height - rect.height) / 2;
					widget_container->GetRoot()->GetBorderSkin()->GetArrow()->SetRight();
					rect.x -= margin_right;
					break;

				case OpToolTipListener::PREFERRED_PLACEMENT_TOP:
					rect.x = ref_screen_rect.x + (ref_screen_rect.width - rect.width) / 2;
					rect.y = ref_screen_rect.y - rect.height;
					widget_container->GetRoot()->GetBorderSkin()->GetArrow()->SetBottom();
					rect.y -= margin_bottom;
					break;

				default:
					OP_ASSERT(!"Unknown placement");
			}

			// If outside, try again using the opposite side
			//if (screen.Contains(rect))
			//	break;
			if ((placement == OpToolTipListener::PREFERRED_PLACEMENT_LEFT ||
				placement == OpToolTipListener::PREFERRED_PLACEMENT_RIGHT) &&
				rect.x >= screen.x &&
				rect.x < screen.x + screen.width - rect.width)
				break;
			if ((placement == OpToolTipListener::PREFERRED_PLACEMENT_TOP ||
				placement == OpToolTipListener::PREFERRED_PLACEMENT_BOTTOM) &&
				rect.y >= screen.y &&
				rect.y < screen.y + screen.height - rect.height)
				break;

			switch (placement)
			{
				case OpToolTipListener::PREFERRED_PLACEMENT_LEFT:
					placement = OpToolTipListener::PREFERRED_PLACEMENT_RIGHT; break;
				case OpToolTipListener::PREFERRED_PLACEMENT_TOP:
					placement = OpToolTipListener::PREFERRED_PLACEMENT_BOTTOM; break;
				case OpToolTipListener::PREFERRED_PLACEMENT_RIGHT:
					placement = OpToolTipListener::PREFERRED_PLACEMENT_LEFT; break;
				case OpToolTipListener::PREFERRED_PLACEMENT_BOTTOM:
					placement = OpToolTipListener::PREFERRED_PLACEMENT_TOP; break;
				default:
					OP_ASSERT(!"Unknown placement");
			}
		}
		// Adjust the rect so it really is inside the screen.
		rect.x = MAX(rect.x, screen.x);
		rect.x = MIN(rect.x, screen.x + screen.width - rect.width);
		rect.y = MAX(rect.y, screen.y);
		rect.y = MIN(rect.y, screen.y + screen.height - rect.height);

		if(placement == OpToolTipListener::PREFERRED_PLACEMENT_TOP || 
			placement == OpToolTipListener::PREFERRED_PLACEMENT_BOTTOM)
		{
			// center the arrow on the tab associated with the tooltip. We don't need this for tabs on the sides, though.

			OpWidgetImage * skin = widget_container->GetRoot()->GetBorderSkin();
			OpSkinElement * skinelm = skin->GetSkinElement();
			SkinArrow * arrow = skin->GetArrow();

			double pos0 = rect.x;
			double pos100 = rect.x + rect.width;

			if (skinelm)
			{
				UINT8 border_left, border_top, border_right, border_bottom;
				if (OpStatus::IsSuccess(skinelm->GetStretchBorder(&border_left, &border_top, &border_right, &border_bottom, 0)))
				{
					pos0 += border_left;
					pos100 -= border_right;
				};

				INT32 arrow_width, arrow_height;
				if (OpStatus::IsSuccess(skinelm->GetArrowSize(*arrow, &arrow_width, &arrow_height, 0)))
				{
					pos0 += arrow_width/2;
					pos100 -= arrow_width/2;
				};
			};

			double offset_pixels = ref_screen_rect.x + ref_screen_rect.width / 2.0 - pos0;
			double offset_ratio = offset_pixels / (pos100-pos0);
			arrow->SetOffset((int)(offset_ratio * 100.0));
		}
	}
	else
	{
		if (UiDirection::Get() == UiDirection::RTL)
			rect.x -= rect.width;

		// Keep inside screen edges
		OpScreenProperties screen_props;
		g_op_screen_info->GetProperties(&screen_props, window, &screen_pos);
		const OpRect screen = screen_props.screen_rect;

		rect.x = MAX(rect.x, screen.x);
		if (rect.Right() > screen.Right())
		{
			rect.x = MAX(screen.Right() - rect.width, screen.x);
		}
		if (rect.Right() > screen.Right())
		{
			rect.width = screen.width;
		}

		if (rect.y + rect.height > screen.y + screen.height)
		{
			rect.y = rect.y - rect.height - VERTICAL_OFFSET - 8;
		}

		if (rect.y < screen.y)
		{
			rect.y = screen.y;

			if (rect.x < screen_pos.x)
			{
				rect.x = screen_pos.x - rect.width - VERTICAL_OFFSET;
			}
			else
			{
				rect.x += VERTICAL_OFFSET;
			}
		}
	}
	return rect;
}

void OpToolTip::Show()
{
#if defined(_UNIX_DESKTOP_) || defined(MSWIN)
	if( IsPanning() )
	{
		Hide();
		return;
	}
#endif

	m_image.Empty();
	m_additional_text.Empty();
	if (!m_listener || !m_listener->HasToolTipText(this) || g_application->GetMenuHandler()->IsShowingMenu())
	{
		Hide();
		return;
	}

#ifdef DRAG_SUPPORT
	if (g_drag_manager->IsDragging() &&  !m_listener->GetShowWhileDragging(this))
	{
		// Something is being dragged and the tooltip is not intended to be shown while dragging
		Hide();
		return;
	}
#endif

	OpInfoText text;
	OpToolTipListener::TOOLTIP_TYPE type = m_listener->GetToolTipType(this);

	BOOL show_thumb_win = (type == OpToolTipListener::TOOLTIP_TYPE_THUMBNAIL || type == OpToolTipListener::TOOLTIP_TYPE_MULTIPLE_THUMBNAILS);
	BOOL show_text_win = !show_thumb_win;

	m_listener->GetToolTipText(this, text);

	if (!text.HasTooltipText())
	{
		Hide();
		return;
	}

	m_title.Empty();
	m_url_string.Empty();
	m_url = URL();
	m_listener->GetToolTipThumbnailText(this, m_title, m_url_string, m_url);

	if (m_text_visible && text.GetTooltipText().Compare(m_text.GetTooltipText()) == 0)
	{
		if (m_listener->GetToolTipUpdateDelay(this))
		{
			m_timer->Start(m_listener->GetToolTipUpdateDelay(this));
		}
		return;
	}
	if (show_thumb_win && m_title.IsEmpty())
	{
		// We want title or nothing in the new style tooltips. DSK-300793.
		Hide();
		return;
	}
	m_text.Copy(text);

	// Set up tooltip window

	if (!m_additional_text.IsEmpty())
		show_text_win = TRUE;

	// If we're switching to thumbnail tooltip we should hide the text tooltip, or the other way around.
	if (show_text_win && m_thumb_visible)
		Hide(FALSE, TRUE);
	if (show_thumb_win && m_text_visible)
		Hide(TRUE, FALSE);

	// Create text tooltip if that's what we want
	if (!m_text_window && show_text_win)
	{
		m_text_window = OP_NEW(OpToolTipWindow, (this));
		if (!m_text_window || OpStatus::IsError(m_text_window->Init(FALSE)))
		{
			OP_DELETE(m_text_window);
			m_text_window = NULL;
			return;
		}
	}

	// Create thumbnail tooltip if that's what we want
	if (!m_thumb_window && show_thumb_win)
	{
		m_thumb_window = OP_NEW(OpToolTipWindow, (this));
		if (!m_thumb_window || OpStatus::IsError(m_thumb_window->Init(TRUE)))
		{
			OP_DELETE(m_thumb_window);
			m_thumb_window = NULL;
			return;
		}
		m_invalidate_thumbnails = TRUE;
	}
	OpRect ref_screen_rect;
	OpToolTipListener::PREFERRED_PLACEMENT placement;
	BOOL has_preferred_placement = m_listener->GetPreferredPlacement(this, ref_screen_rect, placement);

	if(show_thumb_win)
	{
		m_thumb_window->Update();

		BOOL show_thumb = m_thumbnail_collection.GetCount() || (!m_image.IsEmpty() && !m_listener->GetHideThumbnail(this));
		m_thumb_window->SetShowThumbnail(show_thumb);
		if (show_thumb)
		{
			if(m_invalidate_thumbnails)
			{
				m_thumb_window->ClearThumbnailEntries(0); // m_thumbnail_collection.GetCount() ? m_thumbnail_collection.GetCount() : 1);

				if(m_thumbnail_collection.GetCount())
				{
					for(UINT32 index = 0; index < m_thumbnail_collection.GetCount(); index++)
					{
						OpToolTipThumbnailPair *entry = m_thumbnail_collection.Get(index);

						m_thumb_window->SetThumbnailEntry(index, entry);
					}
				}
				else if(!m_image.IsEmpty())
				{
					OpToolTipThumbnailPair entry;

					entry.window_id = m_listener->GetToolTipWindowID();
					entry.can_activate_button = TRUE;
					entry.thumbnail = m_image;
					entry.is_fixed_image = m_is_fixed_image;
					entry.title.Set(m_title.CStr());

					m_thumb_window->SetThumbnailEntry(0, &entry);
				}
				OpStatus::Ignore(m_thumb_window->RecreateLayout());
				m_invalidate_thumbnails = FALSE;

//#ifdef _DEBUG
//				uni_char buf[1024];
//				wsprintf(buf, UNI_L("Recreating layout, thumbnail count: %d\n"), m_thumbnail_collection.GetCount());
//				OutputDebugStringW(buf);
//#endif
			}
		}
		else
		{
			// We must set text alignment to left here, so the required width
			// is calculated consistently below.
			m_thumb_window->ClearThumbnailEntries(0);
			m_thumb_window->m_edit->SetJustify(JUSTIFY_LEFT, FALSE);
			m_thumb_window->m_edit->SetText(m_title.CStr());

			OpStatus::Ignore(m_thumb_window->RecreateLayout());
			m_invalidate_thumbnails = FALSE;
		}

		UINT32 width = 0, height = 0;
		m_thumb_window->GetRequiredSize(width, height);

		OpScreenProperties sp;
		g_op_screen_info->GetProperties(&sp, m_thumb_window->GetWindow());
		width = min(width, sp.width);
		height = min(height, sp.height);

		if (!show_thumb)
		{
			// Must be done after GetRequiredSize
			m_thumb_window->m_edit->SetJustify(JUSTIFY_CENTER, FALSE);
		}
		OpRect rect = GetPlacement(width, height, m_thumb_window->GetWindow(), m_thumb_window->GetWidgetContainer(), has_preferred_placement, ref_screen_rect, placement);

		BOOL set_preferred_rectangle = TRUE;

		if (g_animation_manager->GetEnabled())
		{
			m_thumb_window->reset();
			if (!m_thumb_visible && m_thumb_window->IsAnimating())
			{
				m_thumb_visible = TRUE;
				//m_thumb_window->animateOpacity(0.0, 1.0);
			}
			if (m_thumb_visible)
			{
				double duration = static_cast<double>(THUMBNAIL_ANIMATION_DURATION);

				// Thumbnail was visible, so animate from the old position to the new.
				int x, y;
				UINT32 w, h;
				m_thumb_window->GetWindow()->GetOuterPos(&x, &y);
				m_thumb_window->GetWindow()->GetOuterSize(&w, &h);
				OpRect target_rect(x, y, w, h);
				if (target_rect.IsEmpty())
				{
					// sometimes the start rect is empty for some reason, so just fade in then
					m_thumb_visible = FALSE;

					// Fade in
					m_thumb_window->animateOpacity(0.0, 1.0);
					g_animation_manager->startAnimation(m_thumb_window, ANIM_CURVE_LINEAR);
				}
				else if(!target_rect.Equals(rect))
				{
					m_thumb_window->animateRect(target_rect, rect);
					set_preferred_rectangle = FALSE;
					if (m_thumb_window->IsAnimating()) // Give a little extra boost when moving mouse faster.
						g_animation_manager->startAnimation(m_thumb_window, ANIM_CURVE_SLOW_DOWN, duration);
					else
						g_animation_manager->startAnimation(m_thumb_window, ANIM_CURVE_BEZIER, duration);
				}
			}
			else
			{
				// Fade in
				m_thumb_window->animateOpacity(0.0, 1.0);
				g_animation_manager->startAnimation(m_thumb_window, ANIM_CURVE_LINEAR);
			}
		}

		m_thumb_window->Show(TRUE, set_preferred_rectangle ? &rect : NULL);
		m_thumb_visible = TRUE;

		// If the text window should also be shown, update the placement so it's relative to the thumb window.
		ref_screen_rect = rect;
		if (placement != OpToolTipListener::PREFERRED_PLACEMENT_TOP)
			placement = OpToolTipListener::PREFERRED_PLACEMENT_BOTTOM;
	}
	if (m_text_window && show_text_win)
	{
		m_text_window->Update();

		//calculate the required size of the tooltip
		if (!m_additional_text.IsEmpty())
			m_text_window->m_edit->SetText(m_additional_text.CStr());
		else
			m_text_window->m_edit->SetText(m_text.GetTooltipText().CStr());

		UINT32 width, height;
		m_text_window->GetRequiredSize(width, height);

		OpRect rect = GetPlacement(width, height, m_text_window->GetWindow(), m_text_window->GetWidgetContainer(), has_preferred_placement, ref_screen_rect, placement);

		if (g_pcui->GetIntegerPref(PrefsCollectionUI::PopupButtonHelp))
		{
			m_text_window->Show(TRUE, &rect);
		}
		m_text_visible = TRUE;
	}

	if(!m_listener)
	{
		Hide();
		return;
	}
	if(m_listener->GetToolTipUpdateDelay(this))
	{
		m_timer->Start(m_listener->GetToolTipUpdateDelay(this));
	}
}

void OpToolTip::Hide(BOOL hide_text_tooltip, BOOL hide_thumb_tooltip)
{
	SetSticky(FALSE);

	if (m_thumb_window && m_thumb_visible && hide_thumb_tooltip)
	{
		if (g_animation_manager->GetEnabled())
		{
			m_thumb_window->reset();
			m_thumb_window->animateOpacity(1.0, 0.0);
			//m_thumb_window->animateHideWhenCompleted(TRUE);
			//g_animation_manager->startAnimation(m_thumb_window, ANIM_CURVE_LINEAR);
			//Fade out and destroy. A new instance will be created & faded in when needed
			m_thumb_window->m_close_on_animation_completion = TRUE;
			g_animation_manager->startAnimation(m_thumb_window, ANIM_CURVE_LINEAR);
			m_thumb_window = NULL;
		}
		else
			m_thumb_window->Show(FALSE);
		m_thumb_visible = FALSE;
	}
	if (m_text_window && m_text_visible && hide_text_tooltip)
	{
		m_text_window->Show(FALSE);
		m_text_visible = FALSE;
	}
}

void OpToolTip::OnTimeOut(OpTimer* timer)
{
	if (timer == m_timer)
		Show();
}

void OpToolTip::SetImage(Image image, BOOL fixed_image)
{
	m_image = image;
	m_is_fixed_image = fixed_image;

	if(m_thumb_window && m_thumbnail_collection.GetCount() < 2)
	{
		OpToolTipThumbnailPair entry;

		entry.thumbnail = m_image;
		entry.is_fixed_image = fixed_image;
		entry.title.Set(m_title.CStr());

		m_thumb_window->ClearThumbnailEntries(1);
		m_thumb_window->SetThumbnailEntry(0, &entry);
	}
}

void OpToolTip::UrlChanged()
{
	if (m_thumb_visible || m_text_visible)
	{
		Hide();
		Show();
	}
}

void OpToolTipWindow::OnMouseMove(const OpPoint &point)
{

}

void OpToolTipWindow::OnMouseLeave()
{
	if(m_tooltip->IsSticky())
	{
		m_tooltip->SetSticky(FALSE);
	}
}

