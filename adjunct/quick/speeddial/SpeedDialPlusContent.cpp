/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/speeddial/SpeedDialPlusContent.h"

#ifdef _X11_SELECTION_POLICY_
# include "adjunct/quick/managers/DesktopClipboardManager.h"
#endif
#include "adjunct/quick/managers/SpeedDialManager.h"

#include "modules/widgets/OpButton.h"


SpeedDialPlusContent::SpeedDialPlusContent(const DesktopSpeedDial& entry)
	: m_entry(&entry)
	, m_button(NULL)
	, m_drag_handler(entry)
{
}

SpeedDialPlusContent::~SpeedDialPlusContent()
{
	if (m_button && !m_button->IsDeleted())
		m_button->Delete();
}

OP_STATUS SpeedDialPlusContent::Init()
{
	RETURN_IF_ERROR(OpButton::Construct(&m_button, OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE));
	OP_ASSERT(m_button != NULL);
	m_button->GetBorderSkin()->SetImage(SPEEDDIAL_THUMBNAIL_IMAGE_SKIN);
	m_button->GetForegroundSkin()->SetImage("Speed Dial Add Button");

	return OpStatus::OK;
}

OP_STATUS SpeedDialPlusContent::GetButtonInfo(ButtonInfo& info) const
{
	RETURN_IF_ERROR(info.m_name.Set("Speed Dial Plus Button"));

	const INT32 pos = g_speeddial_manager->FindSpeedDial(m_entry);

	info.m_accessibility_text.Empty();
	RETURN_IF_ERROR(info.m_accessibility_text.AppendFormat(UNI_L("Thumbnail %d"), pos + 1));

	info.m_action = OpInputAction::ACTION_THUMB_CONFIGURE;
	info.m_action_data = g_speeddial_manager->GetSpeedDialActionData(m_entry);
	return OpStatus::OK;
}

OP_STATUS SpeedDialPlusContent::HandleMidClick()
{
#ifdef _X11_SELECTION_POLICY_
	if (!g_desktop_clipboard_manager->HasText(true))
		return OpStatus::OK;

	OpString text;
	RETURN_IF_ERROR(g_desktop_clipboard_manager->GetText(text, true));

	DesktopSpeedDial new_entry;
	RETURN_IF_ERROR(new_entry.SetTitle(text, TRUE));
	RETURN_IF_ERROR(new_entry.SetURL(text));
	RETURN_IF_ERROR(g_speeddial_manager->ReplaceSpeedDial(m_entry, &new_entry));
#endif // _X11_SELECTION_POLICY_

	return OpStatus::OK;
}

bool SpeedDialPlusContent::AcceptsDragDrop(DesktopDragObject& drag_object) const
{
	return m_drag_handler.AcceptsDragDrop(drag_object);
}

void SpeedDialPlusContent::HandleDragDrop(DesktopDragObject& drag_object)
{
	OP_ASSERT(AcceptsDragDrop(drag_object));
	m_drag_handler.HandleDragDrop(drag_object);
}
