/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl) / Alexander Remen (alexr)
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "modules/dragdrop/dragdrop_manager.h"
#include "adjunct/m2_ui/models/accesstreeviewlistener.h"

#include "adjunct/m2/src/engine/accountmgr.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/indexer.h"
#include "adjunct/m2_ui/models/accessmodel.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "modules/widgets/WidgetContainer.h"



void AccessTreeViewListener::OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y)
{
	OpTreeModelItem* item = m_treeview.GetItemByPosition(pos);
	if (widget == &m_treeview && item)
	{
		DesktopDragObject* drag_object = m_treeview.GetDragObject(OpTypedObject::DRAG_TYPE_MAIL_INDEX, x, y);

		if (drag_object)
		{
			drag_object->SetID(item->GetID());
			g_drag_manager->StartDrag(drag_object, NULL, FALSE);
		}
	}
}

void AccessTreeViewListener::OnDragMove(OpWidget* widget, OpDragObject* op_drag_object, INT32 pos, INT32 x, INT32 y)
{
	DesktopDragObject *drag_object = static_cast<DesktopDragObject *>(op_drag_object);
	OpTreeModelItem* item = m_treeview.GetItemByPosition(pos);

	UINT32 source = (UINT32) drag_object->GetID();
	UINT32 dest = item ? (UINT32) item->GetID() : 0;

	drag_object->SetDesktopDropType(DROP_NOT_AVAILABLE);

	switch (drag_object->GetType())
	{
	case OpTypedObject::DRAG_TYPE_MAIL:
		if (CanDragMail(dest))
		{
			drag_object->SetDesktopDropType(GetMailDropType(source, dest));
		}
		break;

	case OpTypedObject::DRAG_TYPE_MAIL_INDEX:
		if (CanDragIndex(source))
		{
			drag_object->SetDesktopDropType(DROP_MOVE);
			drag_object->SetInsertType(GetIndexInsertType(dest, drag_object->GetSuggestedInsertType()));
		}
		break;
	}
}

void AccessTreeViewListener::OnDragDrop(OpWidget* widget, OpDragObject* op_drag_object, INT32 pos, INT32 x, INT32 y)
{
	DesktopDragObject *drag_object = static_cast<DesktopDragObject *>(op_drag_object);
	OpTreeModelItem* item = m_treeview.GetItemByPosition(pos);

	UINT32 source = (UINT32) drag_object->GetID();
	UINT32 dest = item ? (UINT32) item->GetID() : 0;

	switch (drag_object->GetType())
	{
		case OpTypedObject::DRAG_TYPE_MAIL:
			if (CanDragMail(dest))
			{
				OnDropMail(drag_object, source, dest);
			}
			break;
	
		case OpTypedObject::DRAG_TYPE_MAIL_INDEX:
			if (CanDragIndex(source))
			{
				OnDropIndex(drag_object, source, dest);
			}
			break;
	}
}

BOOL AccessTreeViewListener::CanDragMail(index_gid_t dest)
{
	switch (INDEX_ID_TYPE(dest))
	{
		case INDEX_ID_TYPE(IndexTypes::FIRST_FOLDER):
		case INDEX_ID_TYPE(IndexTypes::FIRST_LABEL):
		case INDEX_ID_TYPE(IndexTypes::FIRST_IMAP):
		case INDEX_ID_TYPE(IndexTypes::FIRST_ARCHIVE):
		case INDEX_ID_TYPE(IndexTypes::FIRST_ACCOUNT):
			return TRUE;
	}

	return dest == IndexTypes::TRASH || dest == IndexTypes::SPAM;
}

void AccessTreeViewListener::OnDropMail(DesktopDragObject* drag_object, index_gid_t source, index_gid_t dest)
{
	OpINT32Vector message_ids;

	for (int i = 0; i < drag_object->GetIDCount(); i++)
		RETURN_VOID_IF_ERROR(message_ids.Add(drag_object->GetID(i)));

	BOOL move = GetMailDropType(source, dest) == DROP_MOVE;
	RETURN_VOID_IF_ERROR(g_m2_engine->ToClipboard(message_ids, source, move));
	g_m2_engine->PasteFromClipboard(dest);
}

DropType AccessTreeViewListener::GetMailDropType(index_gid_t source, index_gid_t dest)
{
	if (m_treeview.GetWidgetContainer()->GetView()->GetShiftKeys() & SHIFTKEY_CTRL)
		return DROP_COPY;

	if (INDEX_ID_TYPE(dest) == INDEX_ID_TYPE(IndexTypes::FIRST_IMAP))
		return DROP_MOVE;

	switch (INDEX_ID_TYPE(source))
	{
		case INDEX_ID_TYPE(IndexTypes::FIRST_FOLDER):
		case INDEX_ID_TYPE(IndexTypes::FIRST_LABEL):
			return DROP_MOVE;
		case INDEX_ID_TYPE(IndexTypes::FIRST_IMAP):
			return INDEX_ID_TYPE(dest) == INDEX_ID_TYPE(IndexTypes::FIRST_IMAP) ? DROP_MOVE : DROP_COPY;
	}

	if (source == IndexTypes::TRASH || dest == IndexTypes::TRASH)
		return DROP_MOVE;

	return INDEX_ID_TYPE(dest) == INDEX_ID_TYPE(IndexTypes::FIRST_ARCHIVE) ? DROP_MOVE : DROP_COPY;
}

BOOL AccessTreeViewListener::CanDragIndex(index_gid_t source)
{
	// drag and drop only in the same category
	if (m_treeview.GetItemByID(source) == -1)
		return FALSE;

	AccessModel* model = static_cast<AccessModel*>(m_treeview.GetTreeModel());
	switch (model->GetCategoryID())
	{
		case IndexTypes::CATEGORY_LABELS:
		case IndexTypes::CATEGORY_MAILING_LISTS:
			return TRUE;
	}

	Account* rss_account = g_m2_engine->GetAccountManager()->GetRSSAccount(FALSE);
	return rss_account && model->GetCategoryID() == (UINT32)IndexTypes::FIRST_ACCOUNT + rss_account->GetAccountId();
}

DesktopDragObject::InsertType AccessTreeViewListener::GetIndexInsertType(index_gid_t dest, DesktopDragObject::InsertType suggested)
{
	Index* index = g_m2_engine->GetIndexById(dest);
	AccessModel* model = static_cast<AccessModel*>(m_treeview.GetTreeModel());
	
	if (suggested == DesktopDragObject::INSERT_INTO && index && index->GetType() != IndexTypes::UNIONGROUP_INDEX && model->GetCategoryID() != IndexTypes::CATEGORY_LABELS)
		return DesktopDragObject::INSERT_AFTER;

	return suggested;
}

void AccessTreeViewListener::OnDropIndex(DesktopDragObject* drag_object, index_gid_t source, index_gid_t dest)
{
	AccessModel* model = static_cast<AccessModel*>(m_treeview.GetTreeModel());
	Indexer* indexer = g_m2_engine->GetIndexer();

	if (dest == 0)
		dest = model->GetCategoryID();
	else if (drag_object->GetInsertType() == DesktopDragObject::INSERT_BEFORE || drag_object->GetInsertType() == DesktopDragObject::INSERT_AFTER)
		dest = indexer->GetIndexById(dest)->GetParentId();

	if (!CanDropIndex(source, dest))
		return;

	// corner case, changing the order of two folders
	OpINT32Vector children;
	indexer->GetChildren(source, children, FALSE);
	if (children.Find(dest) != -1)
	{
		Index* index = indexer->GetIndexById(dest);
			
		while (index->GetParentId() != source)
		{
			index = indexer->GetIndexById(index->GetParentId());
		}

		index->SetParentId(indexer->GetIndexById(source)->GetParentId());
	}

	indexer->GetIndexById(source)->SetParentId(dest);
}

BOOL AccessTreeViewListener::CanDropIndex(index_gid_t source, index_gid_t dest)
{
	AccessModel* model = static_cast<AccessModel*>(m_treeview.GetTreeModel());

	if (source == dest)
		return FALSE;

	if (dest == g_m2_engine->GetIndexById(source)->GetParentId())
		return FALSE;

	// drop only allowed into folders, root level or where ever for filters
	if (dest == model->GetCategoryID())
		return TRUE;

	if (model->GetCategoryID() == IndexTypes::CATEGORY_LABELS)
		return TRUE;

	if (g_m2_engine->GetIndexById(dest)->GetType() == IndexTypes::UNIONGROUP_INDEX)
		return TRUE;

	return FALSE;
}

void AccessTreeViewListener::OnItemEdited(OpWidget *widget, INT32 pos, INT32 column, OpString& text)
{
	INT32 id = m_treeview.GetItemByPosition(pos)->GetID();
	Index* index = g_m2_engine->GetIndexById(id);
	index->SetName(text.CStr());
	g_m2_engine->UpdateIndex(index->GetId());
}

#endif // M2_SUPPORT
