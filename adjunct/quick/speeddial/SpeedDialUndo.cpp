// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Huib Kleinhout
//

#include "core/pch.h"

#ifdef SUPPORT_SPEED_DIAL

#include "SpeedDialUndo.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/managers/SpeedDialManager.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"	//cast from ddw to dw

#define SPEED_DIAL_UNDO_REDO_OUT_OF_SYNC "Undo/Redo out of sync with SpeedDialManager"

SpeedDialUndo::MoveItem::MoveItem(INT32 from_pos, INT32 to_pos)
	: m_from_pos(from_pos)
	, m_to_pos(to_pos)
{
}

OP_STATUS SpeedDialUndo::MoveItem::Undo()
{
	return g_speeddial_manager->MoveSpeedDial(m_to_pos, m_from_pos);
}

OP_STATUS SpeedDialUndo::MoveItem::Redo()
{
	return g_speeddial_manager->MoveSpeedDial(m_from_pos, m_to_pos);
}


SpeedDialUndo::ModifyItem::~ModifyItem()
{
	OP_DELETE(m_data);
}

size_t SpeedDialUndo::ModifyItem::BytesUsed() const
{
	return sizeof(*this) + (m_data ? m_data->BytesUsed() : 0);
}


OP_STATUS SpeedDialUndo::InsertRemoveItem::CreateInsert(INT32 pos, InsertRemoveItem*& item)
{
	item = OP_NEW(InsertRemoveItem, ());
	RETURN_OOM_IF_NULL(item);
	item->m_pos = pos;
	return OpStatus::OK;
}

OP_STATUS SpeedDialUndo::InsertRemoveItem::CreateRemove(INT32 pos, InsertRemoveItem*& item)
{
	OpAutoPtr<InsertRemoveItem> new_item(OP_NEW(InsertRemoveItem, ()));
	RETURN_OOM_IF_NULL(new_item.get());

	new_item->m_pos = pos;
	new_item->m_data = OP_NEW(SpeedDialData, ());
	RETURN_OOM_IF_NULL(new_item->m_data);

	const SpeedDialData* to_remove = g_speeddial_manager->GetSpeedDial(pos);
	if (to_remove == NULL)
	{
		OP_ASSERT(!SPEED_DIAL_UNDO_REDO_OUT_OF_SYNC);
		return OpStatus::ERR_OUT_OF_RANGE;
	}
	RETURN_IF_ERROR(new_item->m_data->Set(*to_remove));

	item = new_item.release();
	return OpStatus::OK;
}

OP_STATUS SpeedDialUndo::InsertRemoveItem::Reinsert()
{
	if (m_data == NULL)
	{
		OP_ASSERT(FALSE);
		return OpStatus::ERR;
	}
	RETURN_IF_ERROR(g_speeddial_manager->InsertSpeedDial(m_pos, m_data));

	OP_DELETE(m_data);
	m_data = NULL;

	return OpStatus::OK;
}

OP_STATUS SpeedDialUndo::InsertRemoveItem::Reremove()
{
	OP_ASSERT(m_data == NULL);
	m_data = OP_NEW(SpeedDialData, ());
	RETURN_OOM_IF_NULL(m_data);

	const DesktopSpeedDial* to_remove = g_speeddial_manager->GetSpeedDial(m_pos);
	if (to_remove == NULL)
	{
		OP_ASSERT(!SPEED_DIAL_UNDO_REDO_OUT_OF_SYNC);
		return OpStatus::ERR_OUT_OF_RANGE;
	}
	RETURN_IF_ERROR(m_data->Set(*to_remove));

	g_speeddial_manager->AnimateThumbnailOutForUndoOrRedo(m_pos);

	return OpStatus::OK;
}


OP_STATUS SpeedDialUndo::ReplaceItem::Create(INT32 pos, ReplaceItem*& item)
{
	item = OP_NEW(ReplaceItem, ());
	RETURN_OOM_IF_NULL(item);

	item->m_pos = pos;
	item->m_data = OP_NEW(SpeedDialData, ());
	RETURN_OOM_IF_NULL(item->m_data);

	const SpeedDialData* to_replace = g_speeddial_manager->GetSpeedDial(pos);
	if (to_replace == NULL)
	{
		OP_ASSERT(!SPEED_DIAL_UNDO_REDO_OUT_OF_SYNC);
		return OpStatus::ERR_OUT_OF_RANGE;
	}
	return item->m_data->Set(*to_replace);
}

OP_STATUS SpeedDialUndo::ReplaceItem::Replace()
{
	if (m_data == NULL)
	{
		OP_ASSERT(FALSE);
		return OpStatus::ERR;
	}

	const SpeedDialData* to_replace = g_speeddial_manager->GetSpeedDial(m_pos);
	if (to_replace == NULL)
	{
		OP_ASSERT(!SPEED_DIAL_UNDO_REDO_OUT_OF_SYNC);
		return OpStatus::ERR_OUT_OF_RANGE;
	}
	SpeedDialData tmp_data;
	RETURN_IF_ERROR(tmp_data.Set(*to_replace));

	RETURN_IF_ERROR(g_speeddial_manager->ReplaceSpeedDial(m_pos, m_data));

	return m_data->Set(tmp_data);
}


SpeedDialUndo::~SpeedDialUndo()
{
	//remove itself as DesktopWindow::Listener
	SetLastUndoDesktopWindow(NULL);
}

void SpeedDialUndo::Add(GenericUndoItem* item)
{
	AddItem(item);
	SetLastUndoDesktopWindow(g_application->GetActiveDocumentDesktopWindow());
}

OP_STATUS SpeedDialUndo::AddInsert(INT32 pos)
{
	if (m_locked)
		return OpStatus::OK;

	InsertRemoveItem* item = NULL;
	RETURN_IF_ERROR(InsertRemoveItem::CreateInsert(pos, item));
	OP_ASSERT(item != NULL);
	Add(item);
	return OpStatus::OK;
}

OP_STATUS SpeedDialUndo::AddRemove(INT32 pos)
{
	if (m_locked)
		return OpStatus::OK;

	InsertRemoveItem* item = NULL;
	RETURN_IF_ERROR(InsertRemoveItem::CreateRemove(pos, item));
	OP_ASSERT(item != NULL);
	Add(item);
	return OpStatus::OK;
}

OP_STATUS SpeedDialUndo::AddReplace(INT32 pos)
{
	if (m_locked)
		return OpStatus::OK;

	ReplaceItem* item = NULL;
	RETURN_IF_ERROR(ReplaceItem::Create(pos, item));
	OP_ASSERT(item != NULL);
	Add(item);
	return OpStatus::OK;
}

OP_STATUS SpeedDialUndo::AddMove(INT32 from_pos, INT32 to_pos)
{
	if (m_locked)
		return OpStatus::OK;

	MoveItem* item = OP_NEW(MoveItem, (from_pos, to_pos));
	RETURN_OOM_IF_NULL(item);
	Add(item);
	return OpStatus::OK;
}

OP_STATUS SpeedDialUndo::Undo()
{
	m_locked = true;
	if (OpStatus::IsSuccess(GenericUndoStack::Undo()))
	{	
		//save changes
		OpStatus::Ignore(g_speeddial_manager->Save());
		m_locked = false;
		return OpStatus::OK;
	}
	m_locked = false;
	return OpStatus::ERR;
}

OP_STATUS SpeedDialUndo::Redo()
{
	m_locked = true;
	if (OpStatus::IsSuccess(GenericUndoStack::Redo()))
	{	
		//save changes
		OpStatus::Ignore(g_speeddial_manager->Save());	
		m_locked = false;
		return OpStatus::OK;
	}
	m_locked = false;
	return OpStatus::ERR;
}


void SpeedDialUndo::OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated)
{
	SetLastUndoDesktopWindow(NULL);
	if (user_initiated && (g_speeddial_manager->GetSpeedDialTabsCount() == 0))
	{
		// The desktop window in which the most recent speed dial changes took place has been deleted
		// and there are no speed dials shown anymore. At this point we are quite sure that the user
		// isn't going to undo the changes. (unless the window is reopened, ignoring that for now)
		// Delete all speed dial undo information:
		Empty();
	}
	// Keep the undo information in memory if it might be used sometime
}

void SpeedDialUndo::SetLastUndoDesktopWindow(DesktopWindow* desktopwindow)
{
	if (m_last_desktopwindow_with_undo)
	{
		//stop listening to current window	
		m_last_desktopwindow_with_undo->RemoveListener(this);
	}

	if (desktopwindow != NULL)
	{
		//start listening to the new window
		desktopwindow->AddListener(this);
	}
	
	//remember the new window, in case the manager dies earlier then the window
	m_last_desktopwindow_with_undo = desktopwindow;
}

#endif // SUPPORT_SPEED_DIAL
