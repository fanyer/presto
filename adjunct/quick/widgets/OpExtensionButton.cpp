/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Patryk Obara <pobara@opera.com>
 */
#include "core/pch.h"

#include "adjunct/quick/widgets/OpExtensionButton.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/extensions/ExtensionUtils.h"
#include "adjunct/quick/managers/DesktopExtensionsManager.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick/WindowCommanderProxy.h"

#include "modules/widgets/OpButton.h"

#ifdef _MACINTOSH_
#define BADGE_FONT_SIZE 90
#elif defined(MSWIN)
#define BADGE_FONT_SIZE 90
#else
#define BADGE_FONT_SIZE 80
#endif
#define BADGE_FONT_WEIGHT 7


OP_STATUS OpExtensionButton::Construct(OpExtensionButton** obj, INT32 extension_id)
{
	*obj = OP_NEW(OpExtensionButton, (extension_id));
	if (*obj == NULL)
		return OpStatus::ERR_NO_MEMORY;
	if (OpStatus::IsError((*obj)->init_status))
	{
		OP_DELETE(*obj);
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}


OpExtensionButton::OpExtensionButton(INT32 extension_id)
	: m_extension_id(extension_id)
	, m_badge(NULL)
	, m_has_badge_text(FALSE)
	, m_enabled(TRUE)
	, m_popup_switch(TRUE)
{
	SetFixedImage(FALSE); // otherwise action image will override icon
	GetForegroundSkin()->SetRestrictImageSize(TRUE);
	GetForegroundSkin()->SetRestrictingImage("Extension icon");

	OpInputAction* action = OP_NEW(OpInputAction, (OpInputAction::ACTION_EXTENSION_ACTION));
	if (!action)
	{
		init_status = OpStatus::ERR_NO_MEMORY;
		return;
	}
	action->SetActionData(m_extension_id);
	SetAction(action);
	
	init_status = OpButton::Construct(&m_badge, OpButton::TYPE_CUSTOM, OpButton::STYLE_TEXT);
	RETURN_VOID_IF_ERROR(init_status);
	m_badge->SetDead(TRUE);
	AddChild(m_badge);
	m_badge->SetIgnoresMouse(TRUE); // :)
	m_badge->SetRelativeSystemFontSize(BADGE_FONT_SIZE);

	m_badge->font_info.weight = BADGE_FONT_WEIGHT;
	SetBadgeTextColor(OP_RGB(0xff, 0xff, 0xff));

	m_badge->GetBorderSkin()->SetImage("Extension Button Badge Skin");
	SetBadgeBackgroundColor(OP_RGB(0xfd, 0x00, 0x03)); // official "opera red"
}


OpExtensionButton::~OpExtensionButton()
{
}


void OpExtensionButton::OnLayout()
{
	m_badge->SetVisibility(m_has_badge_text);

	OpWidget::OnLayout();

	OpRect rect = GetBounds();

	// INT32 left = rect.x, top = rect.y, 
	INT32 right = rect.width, bottom = rect.height;

	if (m_has_badge_text)
	{
		// sometimes weight is reset after changing text
		// that's why we re-apply it every time
		// hard to reproduce, happens sometimes with reddited extension
		m_badge->font_info.weight = BADGE_FONT_WEIGHT;

		// these +1 +1 are here to adjust badge position/clipping
		// we want to make badge rect appear as close as possible to
		// right/bottom side of button rect;
		// this is required because of skin inflexibility when dealing
		// with padded area around badge text :( [pobara 2010-01-07]
		bottom += 1;
		right += 1;

		INT32 prefered_width, prefered_height;
		m_badge->GetPreferedSize(&prefered_width, &prefered_height, 1, 1);
		prefered_width = MIN(prefered_width, right);

#ifdef _MACINTOSH_
		prefered_height = 14; // Mac is weird
#endif

		m_badge->SetRect(OpRect(
					right - prefered_width, 
					bottom - prefered_height, 
					prefered_width,
					prefered_height));
	}
}


BOOL OpExtensionButton::OnContextMenu(const OpPoint& point, const OpRect* avoid_rect, BOOL keyboard_invoked)
{
	BOOL developer_mode = FALSE;
	if (OpStatus::IsError(ExtensionUtils::IsDevModeExtension(m_gadget_id, developer_mode)))
		return FALSE;

	g_application->GetMenuHandler()->ShowPopupMenu(
			developer_mode ? "Dev Extension Toolbar Item Popup Menu" : "Extension Toolbar Item Popup Menu",
			PopupPlacement::AnchorAtCursor(), 0, keyboard_invoked);
	return TRUE;
}


void OpExtensionButton::GetPreferedSize(INT32 *w,	INT32 *h,
										INT32 cols, INT32 rows)
{
	GetInfo()->GetPreferedSize(this,
			OpTypedObject::WIDGET_TYPE_BUTTON, w, h, cols, rows);
}


void OpExtensionButton::OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	m_popup_switch = TRUE;
	OpButton::OnMouseDown(point,button,nclicks);
}

void OpExtensionButton::Click(BOOL plus_action /* = FALSE */)
{
	if (m_connected && m_enabled && m_popup_switch)
	{
		ShowExtensionPopup();
		OpButton::Click(plus_action);
	}
}


void OpExtensionButton::ShowExtensionPopup()
{
	g_desktop_extensions_manager->ShowPopup(this,m_extension_id);
		return;
}


BOOL OpExtensionButton::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();
			
			OP_ASSERT(child_action);
			
			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_SHOW_EXTENSION_OPTIONS:
				{
					child_action->SetEnabled(
						 g_desktop_extensions_manager->CanShowOptionsPage(m_gadget_id));
					return TRUE;
				}
			}
			break;
		}

		case OpInputAction::ACTION_SHOW_EXTENSION_OPTIONS:
			OpStatus::Ignore(g_desktop_extensions_manager->OpenExtensionOptionsPage(m_gadget_id));
			return TRUE;
			
		case OpInputAction::ACTION_EXTENSION_BUTTON_DISABLE:
		
			g_desktop_extensions_manager->CloseDevExtensionPopup();

			OpStatus::Ignore(g_desktop_extensions_manager->DisableExtension(m_gadget_id));
			return TRUE;
			
		case OpInputAction::ACTION_EXTENSION_BUTTON_UNINSTALL:
		{
	
			g_desktop_extensions_manager->CloseDevExtensionPopup();

			RETURN_VALUE_IF_ERROR(g_desktop_extensions_manager->ShowExtensionUninstallDialog(m_gadget_id), FALSE);

			return TRUE;
		}

		case OpInputAction::ACTION_RELOAD_EXTENSION:
		{
			ExtensionButtonComposite *button = g_desktop_extensions_manager->GetButtonFromModel(m_extension_id);
			if (button)
				return g_desktop_extensions_manager->ReloadExtension(button->GetGadgetId());

			return TRUE;
		}
	}

	//DSK-317493
	if(g_desktop_extensions_manager->HandlePopupAction(action))
		return TRUE;
	
	return OpButton::OnInputAction(action);
}


void OpExtensionButton::MenuClosed()
{
	m_popup_switch = FALSE;
}

void OpExtensionButton::OnAdded() 
{
	ExtensionButtonComposite* button_composite =
		g_desktop_extensions_manager->GetButtonFromModel(m_extension_id);
	
	if (button_composite)
	{
		button_composite->Add(this);
		// And update gadget id
		m_gadget_id.Set(button_composite->GetGadgetId());
		
	}
}

void OpExtensionButton::OnDeleted()
{

	g_desktop_extensions_manager->CloseDevExtensionPopup();
	g_desktop_extensions_manager->CloseRegExtensionPopup();
	g_desktop_extensions_manager->CheckUIButtonForInstallNotification(this, FALSE);

	ExtensionButtonComposite* button_composite =
		g_desktop_extensions_manager->GetButtonFromModel(m_extension_id);
	if (button_composite)
	{
		button_composite->Remove(this);
		g_desktop_extensions_manager->OnButtonRemovedFromComposite(m_extension_id);
	}

	OpButton::OnDeleted();
}


void OpExtensionButton::SetBadgeTextColor(COLORREF color)
{
	m_badge->SetForegroundColor(color);
	Relayout(FALSE, TRUE);
}


void OpExtensionButton::SetBadgeBackgroundColor(COLORREF color)
{
	m_badge->GetBorderSkin()->ApplyColorization(color);
	Relayout(FALSE, TRUE);
}


OP_STATUS OpExtensionButton::UpdateIcon(Image &icon)
{
	if (icon.IsEmpty())
	{
		GetForegroundSkin()->SetImage(NULL);
		OP_ASSERT(!GetForegroundSkin()->HasContent());
	}
	else
	{
		GetForegroundSkin()->SetBitmapImage(icon, FALSE);
		OP_ASSERT(GetForegroundSkin()->HasContent());
	}
	return OpStatus::OK;
}


void OpExtensionButton::SetBadgeText(const OpStringC& text)
{
	m_has_badge_text = text.HasContent();
	m_badge->SetText(text);
}


void OpExtensionButton::SetDisabled(BOOL disabled)
{
	m_enabled = !disabled;

	// DO NOT REMOVE THIS LINE!
	// it is redundant, but it makes drawing of buttons a lot faster
	GetAction()->SetEnabled(m_enabled);

	if (!m_enabled)
	{
		g_desktop_extensions_manager->CloseDevExtensionPopup();
	}
}


void OpExtensionButton::SetTitle(const OpStringC& title)
{
	SetText(title);
	OpString8 name;
	name.Set(title);
	SetName(name);
}


void OpExtensionButton::OnShow(BOOL show)
{
	
	g_desktop_extensions_manager->CheckUIButtonForInstallNotification(this, show);
	
	if (!show)
	{
		g_desktop_extensions_manager->CloseDevExtensionPopup();
		g_desktop_extensions_manager->CloseRegExtensionPopup();
	}
}

#undef BADGE_FONT_WEIGHT
#undef BADGE_FONT_SIZE

