/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/speeddial/SpeedDialDragHandler.h"
#include "adjunct/desktop_pi/DesktopDragObject.h"
#include "adjunct/quick/managers/SpeedDialManager.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/widgets/DocumentView.h"

#include "modules/pi/OpDragManager.h"


SpeedDialDragHandler::SpeedDialDragHandler(const DesktopSpeedDial& entry)
	: m_entry(&entry)
{}

bool SpeedDialDragHandler::AcceptsDragDrop(DesktopDragObject& drag_object) const
{
	switch (drag_object.GetType())
	{
		case OpTypedObject::DRAG_TYPE_BUTTON:
		case OpTypedObject::DRAG_TYPE_BOOKMARK:
		case OpTypedObject::DRAG_TYPE_LINK:
		case OpTypedObject::DRAG_TYPE_WINDOW:
			 // Easiest way to filter out incorrect draggable links and buttons, that can't be turned into speeddials
			 if (drag_object.GetURL() && !DocumentView::IsUrlRestrictedForViewFlag(drag_object.GetURL(), DocumentView::ALLOW_ADD_TO_SPEED_DIAL))
				return false;
			 return OpStringC(drag_object.GetURL()).HasContent() ? true : false; // to squelch "BOOL to bool" compiler warnings

		default:
			return false;
	}
}

bool SpeedDialDragHandler::HandleDragStart(DesktopDragObject& drag_object) const
{
	const INT32 pos = g_speeddial_manager->FindSpeedDial(m_entry);
	if (pos >= 0)
	{
		drag_object.SetID(pos);
		drag_object.SetURL(m_entry->GetURL().CStr());
		drag_object.SetTitle(m_entry->GetTitle().CStr());
		return true;
	}
	return false;
}

bool SpeedDialDragHandler::HandleDragDrop(DesktopDragObject& drag_object)
{
	const INT32 target_pos = g_speeddial_manager->FindSpeedDial(m_entry);
	if (target_pos < 0)
		return false;

	switch (drag_object.GetType())
	{
		case OpTypedObject::DRAG_TYPE_BUTTON:
		case OpTypedObject::DRAG_TYPE_BOOKMARK:
		case OpTypedObject::DRAG_TYPE_LINK:
		case OpTypedObject::DRAG_TYPE_WINDOW:
		{
			DesktopSpeedDial new_sd;
			new_sd.SetURL(drag_object.GetURL());
			new_sd.SetTitle(drag_object.GetTitle(), TRUE);
			OpStatus::Ignore(g_speeddial_manager->ReplaceSpeedDial(target_pos, &new_sd));
			return true;
		}

		default:
			return false;
	}
}

