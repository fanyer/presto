/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/speeddial/SpeedDialExtensionContent.h"

#include "adjunct/quick/extensions/ExtensionUtils.h"
#include "adjunct/quick/managers/SpeedDialManager.h"
#include "adjunct/quick/managers/DesktopExtensionsManager.h"
#include "adjunct/quick/speeddial/BitmapButton.h"
#include "adjunct/quick/speeddial/DesktopSpeedDial.h"

#include "modules/libgogi/pi_impl/mde_bitmap_window.h"

SpeedDialExtensionContent::SpeedDialExtensionContent(const DesktopSpeedDial& entry)
	: m_entry(&entry)
	, m_button(NULL)
	, m_drag_handler(entry)
	, m_generic_handler(entry)
	, m_busy(FALSE)
{
}

SpeedDialExtensionContent::~SpeedDialExtensionContent() 
{
	if (m_button && !m_button->IsDeleted())
	{
		m_button->Delete();
		m_button = NULL;
	}
}

OP_STATUS SpeedDialExtensionContent::Init()
{
	m_button = OP_NEW(BitmapButton, ());
	RETURN_OOM_IF_NULL(m_button);

	BitmapOpWindow* bitmap_window =
			ExtensionUtils::GetExtensionBitmapWindow(m_entry->GetExtensionWUID());
			
	if (bitmap_window == NULL)
	{
		m_busy = TRUE;	
		m_button->GetForegroundSkin()->SetImage(m_busy ? "Extensions 72 Gray" : NULL);
		return OpStatus::OK;
	}

	RETURN_IF_ERROR(m_button->Init(bitmap_window));

	m_button->GetBorderSkin()->SetImage("Speed Dial Thumbnail Image Skin");

	return OpStatus::OK;
}

OpButton* SpeedDialExtensionContent::GetButton() const
{ 
	return m_button;
}

const uni_char* SpeedDialExtensionContent::GetTitle() const
{
	return m_entry->GetTitle().CStr();
}

int SpeedDialExtensionContent::GetNumber() const
{
	return m_generic_handler.GetNumber();
}

OP_STATUS  SpeedDialExtensionContent::GetCloseButtonInfo(ButtonInfo& info) const
{
	 return m_generic_handler.GetCloseButtonInfo(info);
}

GenericThumbnailContent::MenuInfo SpeedDialExtensionContent::GetContextMenuInfo() const
{
	BOOL is_devmode = FALSE;
	if (OpStatus::IsError(ExtensionUtils::IsDevModeExtension(m_entry->GetExtensionWUID(), is_devmode)))
		is_devmode = FALSE;

	return m_generic_handler.GetContextMenuInfo(
		is_devmode ?
		"Speed Dial Dev Extension Popup Menu" :
		"Speed Dial Extension Popup Menu");
}

OP_STATUS  SpeedDialExtensionContent::GetButtonInfo(ButtonInfo& info) const
{
	return m_generic_handler.GetButtonInfo(info);
}


OP_STATUS SpeedDialExtensionContent::HandleMidClick()
{
	if (m_entry->HasInternalExtensionURL())
	{
		m_button->Click();
		return OpStatus::OK;
	}
	else 
	{
		return m_generic_handler.HandleMidClick();
	}
}


bool SpeedDialExtensionContent::HandleDragStart(DesktopDragObject& drag_object) const
{
	return m_drag_handler.HandleDragStart(drag_object);
}


void  SpeedDialExtensionContent::HandleDragDrop(DesktopDragObject& drag_object)
{
	OP_ASSERT(AcceptsDragDrop(drag_object));
	if (m_drag_handler.HandleDragDrop(drag_object))
		return;

	const INT32 target_pos = g_speeddial_manager->FindSpeedDial(m_entry);
	if (target_pos < 0)
		return;

	if (drag_object.GetType() == OpTypedObject::DRAG_TYPE_SPEEDDIAL)
		g_speeddial_manager->MoveSpeedDial(drag_object.GetID(), target_pos);
}

bool SpeedDialExtensionContent::AcceptsDragDrop(DesktopDragObject& drag_object) const
{
	return (m_drag_handler.AcceptsDragDrop(drag_object) ||
			drag_object.GetType() == OpTypedObject::DRAG_TYPE_SPEEDDIAL);
}

OP_STATUS SpeedDialExtensionContent::Refresh()
{
	if (!g_desktop_extensions_manager->ReloadExtension(m_entry->GetExtensionWUID()))
		return OpStatus::ERR;
	return OpStatus::OK;
}
