/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
*
* Copyright (C) 2008-2009 Opera Software AS.  All rights reserved.
*
* This file is part of the Opera web browser.	It may not be distributed
* under any circumstances.
*/

#include "core/pch.h"

#ifdef WIDGETS_IMS_SUPPORT

#include "modules/widgets/OpIMSObject.h"
#include "modules/widgets/OpListBox.h"

/**
 * Implementation of OpIMSUpdateList for a single select list.
 * Only keeps track of one changed item.
 */
class OpIMSUpdateSingle : public OpIMSUpdateList
{
private:
	int m_index;
	int m_value;
public:
	OpIMSUpdateSingle() : m_index(-1), m_value(-1) {}

	void Clear(void) { m_index = -1; m_value = -1; }
	int GetFirstIndex(void) { return m_index; }
	int GetNextIndex(int index) { return -1; }
	int GetValueAt(int index) { return m_index == index ? m_value : -1;}
	void Update(int index, BOOL value) { m_index = index; m_value = value ? 1 : 0; }
};

/**
 * Implementation of OpIMSUpdateList for a multiple select list.
 */
class OpIMSUpdateListMulti : public OpIMSUpdateList
{
	struct ImsItem
	{
		int value;
	};
public:
	OpIMSUpdateListMulti();
	~OpIMSUpdateListMulti();

	/** Construct the update list. Must be called after ctor. */
	OP_STATUS Construct(int count);

	void Clear(void);
	int GetFirstIndex(void);
	int GetNextIndex(int index);
	int GetValueAt(int index);
	void Update(int index, BOOL value);
private:
	ImsItem* m_items;
	int m_count;
};

OpIMSObject::OpIMSObject() :
m_updates(NULL), m_listener(NULL), m_ih(NULL), has_scrollbar(FALSE), m_dropdown(FALSE)
{
}

OpIMSObject::~OpIMSObject()
{
	OP_DELETE(m_updates); m_updates = NULL;
}

void OpIMSObject::SetListener(OpIMSListener* listener)
{
	m_listener = listener;
}

void OpIMSObject::SetItemHandler(ItemHandler* ih)
{
	m_ih = ih;
}

BOOL OpIMSObject::IsIMSActive()
{
	return (m_updates != NULL);
}

OP_STATUS OpIMSObject::StartIMS(WindowCommander* windowcommander)
{
	if (IsIMSActive())
		return OpStatus::ERR;

	RETURN_IF_ERROR(CreateUpdatesObject());

	OP_STATUS status = windowcommander->GetDocumentListener()->OnIMSRequested(windowcommander, this);
	if (status == OpStatus::ERR_NOT_SUPPORTED)
	{
		OP_DELETE(m_updates); m_updates = NULL;
	}
	return status;
}

OP_STATUS OpIMSObject::UpdateIMS(WindowCommander* windowcommander)
{
	if (!IsIMSActive())
		return OpStatus::ERR;

	RETURN_IF_ERROR(CreateUpdatesObject());

	windowcommander->GetDocumentListener()->OnIMSUpdated(windowcommander, this);
	return OpStatus::OK;
}

void OpIMSObject::DestroyIMS(WindowCommander* windowcommander)
{
	if (IsIMSActive())
	{
		OP_DELETE(m_updates); m_updates = NULL;
		windowcommander->GetDocumentListener()->OnIMSCancelled(windowcommander, this);
	}
}

void OpIMSObject::SetIMSAttribute(IMSAttribute attr, int value)
{
	if (attr == HAS_SCROLLBAR)
		has_scrollbar = !!value;
	if (attr == DROPDOWN)
		m_dropdown = !!value;
}

void OpIMSObject::SetRect(const OpRect& rect)
{
	m_rect = rect;
}

OP_STATUS OpIMSObject::CreateUpdatesObject()
{
	OP_DELETE(m_updates);

	if (m_ih->is_multiselectable)
	{
		m_updates = OP_NEW(OpIMSUpdateListMulti, ());
		if (m_updates && OpStatus::IsError(static_cast<OpIMSUpdateListMulti*>(m_updates)->Construct(m_ih->CountItemsAndGroups())))
		{
			OP_DELETE(m_updates);
			m_updates = NULL;
		}
	}
	else
		m_updates = OP_NEW(OpIMSUpdateSingle, ());

	if (!m_updates)
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

/* virtual */
void OpIMSObject::OnCommitIMS()
{
	if (IsIMSActive())
	{
		m_listener->OnCommitIMS(m_updates);
		OP_DELETE(m_updates); m_updates = NULL;
	}
}

/* virtual */
void OpIMSObject::OnUpdateIMS(int item, BOOL selected)
{
	if (IsIMSActive())
		m_updates->Update(item, selected);
}

/* virtual */
void OpIMSObject::OnCancelIMS()
{
	if (IsIMSActive())
	{
		m_listener->OnCancelIMS();
		OP_DELETE(m_updates); m_updates = NULL;
	}
}

int OpIMSObject::GetIMSAttributeAt(int index, IMSItemAttribute attr)
{
	if (m_ih == NULL || index < 0 || index >= m_ih->CountItemsAndGroups())
		return -1;

	OpStringItem* item = m_ih->GetItemAtIndex(index);

	switch (attr)
	{
	case SELECTED:
		return item->IsSelected();
	case ENABLED:
		return item->IsEnabled();
	case SEPARATOR:
		return item->IsSeperator();
	case GROUPSTART:
		return item->IsGroupStart();
	case GROUPSTOP:
		return item->IsGroupStop();
	}
	return 0;
}

int OpIMSObject::GetItemCount(void)
{
	if (m_ih)
		return m_ih->CountItemsAndGroups();
	return 0;
}

int OpIMSObject::GetFocusedItem()
{
	if (m_ih && GetItemCount() > 0)
		return m_ih->focused_item;
	return -1;
}

int OpIMSObject::GetIMSAttribute(IMSAttribute attr)
{
	switch (attr)
	{
	case HAS_SCROLLBAR:
		return has_scrollbar ? 1 : 0;
	case MULTISELECT:
		return m_ih->is_multiselectable;
	case DROPDOWN:
		return m_dropdown ? 1 : 0;
	}
	return -1;
}

const uni_char * OpIMSObject::GetIMSStringAt(int index)
{
	if (m_ih == NULL || index < 0 || index >= m_ih->CountItemsAndGroups())
		return NULL;

	OpStringItem* item = m_ih->GetItemAtIndex(index);
	return item->string.Get();
}

OpRect OpIMSObject::GetIMSRect()
{
	return m_rect;
}

OpIMSUpdateListMulti::OpIMSUpdateListMulti()
  : m_items(NULL), m_count(0)
{
}

OpIMSUpdateListMulti::~OpIMSUpdateListMulti()
{
	OP_DELETEA(m_items);
}

OP_STATUS OpIMSUpdateListMulti::Construct(int count)
{
	m_count = count;
	OP_DELETEA(m_items); m_items =  NULL;

	m_items = OP_NEWA(ImsItem, count);
	if (NULL == m_items)
		return OpStatus::ERR_NO_MEMORY;

	Clear();
	return OpStatus::OK;
}

void OpIMSUpdateListMulti::Clear(void)
{
	if (m_items == NULL)
		return;

	int i;
	for (i=0;i<m_count;i++)
		m_items[i].value = -1;
}

int OpIMSUpdateListMulti::GetFirstIndex(void)
{
	if (m_items == NULL)
		return -1;

	int i;
	for (i=0;i<m_count;i++)
	{
		if (m_items[i].value != -1)
			return i;
	}
	return -1;
}

int OpIMSUpdateListMulti::GetNextIndex(int index)
{
	if (m_items == NULL)
		return -1;

	int i;
	for (i=index+1;i<m_count;i++)
	{
		if (m_items[i].value != -1)
			return i;
	}

	return -1;
}

int OpIMSUpdateListMulti::GetValueAt(int index)
{
	if (m_items == NULL || index < 0 || index >= m_count)
		return -1; //TODO OOM

	return m_items[index].value;
}

void OpIMSUpdateListMulti::Update(int index, BOOL value)
{
	if (m_items == NULL || index < 0 || index >= m_count)
		return; //TODO OOM

	m_items[index].value = value ? 1 : 0;
}

#endif // WIDGETS_IMS_SUPPORT
