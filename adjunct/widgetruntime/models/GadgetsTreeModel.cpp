/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/widgetruntime/models/DefaultGadgetItem.h"
#include "adjunct/widgetruntime/models/GadgetsTreeModel.h"
#include "adjunct/widgetruntime/models/GadgetsTreeModelItem.h"

namespace
{
	// Platform gadget list polling interval [ms].
	const UINT32 POLLING_INTERVAL = 2000;

	const time_t FOREVER = 0;
}


class GadgetsTreeModel::PlatformSource : public GadgetsTreeModel::Source
{
public:
	explicit PlatformSource(const PlatformGadgetList& platform_list)
		: m_platform_list(&platform_list)
	{}

	virtual OP_STATUS Refresh()
	{
		m_gadget_infos.DeleteAll();
		return m_platform_list->GetAll(m_gadget_infos, TRUE);
	}

	virtual UINT32 GetItemCount() const
	{
		return m_gadget_infos.GetCount();
	}

	virtual const OpGadgetClass& GetItemInfo(UINT32 i) const
	{
		OP_ASSERT(i < m_gadget_infos.GetCount());
		return *m_gadget_infos.Get(i)->m_gadget_class;
	}

	virtual GadgetsTreeModelItemImpl* CreateItemImpl(UINT32 i) const
	{
		OP_ASSERT(i < m_gadget_infos.GetCount());
		if (i >= m_gadget_infos.GetCount())
		{
			return NULL;
		}

		return OP_NEW(DefaultGadgetItem, (*m_gadget_infos.Get(i)));
	}

private:
	typedef OpAutoVector<GadgetInstallerContext> GadgetInfos;

	GadgetInfos m_gadget_infos;
	const PlatformGadgetList* m_platform_list;
};



GadgetsTreeModel::GadgetsTreeModel()
	: m_platform_timestamp(FOREVER), m_active(FALSE)
{
}

GadgetsTreeModel::~GadgetsTreeModel()
{
	m_platform_polling_clock.Stop();
}


OP_STATUS GadgetsTreeModel::Init()
{
	PlatformGadgetList* platform_gadget_list = NULL;
	RETURN_IF_ERROR(PlatformGadgetList::Create(&platform_gadget_list));
	OP_ASSERT(NULL != platform_gadget_list);
	m_platform_gadget_list.reset(platform_gadget_list);

	m_platform_polling_clock.SetTimerListener(this);

	// Add the default gadget source.
	m_sources.DeleteAll();
	RETURN_IF_ERROR(AddSource(OpAutoPtr<Source>(
					OP_NEW(PlatformSource, (*m_platform_gadget_list)))));

	return OpStatus::OK;
}


const GadgetsTreeModel::Source& GadgetsTreeModel::GetDefaultSource() const
{
	OP_ASSERT(m_sources.GetCount() > 0);
	return *m_sources.Get(0);
}


OP_STATUS GadgetsTreeModel::AddSource(OpAutoPtr<Source> source)
{
	RETURN_OOM_IF_NULL(source.get());
	RETURN_IF_ERROR(m_sources.Add(source.get()));
	source.release();
	return OpStatus::OK;
}


void GadgetsTreeModel::SetActive(BOOL active)
{
	if (!m_active && active)
	{
		if (FOREVER == m_platform_timestamp)
		{
			// The model has never been synced with the platform before.
			RefreshModel();
		}
		m_platform_polling_clock.Start(POLLING_INTERVAL);
	}
	m_active = active;
}


void GadgetsTreeModel::OnTimeOut(OpTimer* timer)
{
	if (&m_platform_polling_clock == timer && m_active)
	{
		OnPlatformPollingTick();
		m_platform_polling_clock.Start(POLLING_INTERVAL);
	}
}


void GadgetsTreeModel::OnPlatformPollingTick()
{
	if (m_platform_gadget_list->HasChangedSince(m_platform_timestamp))
	{
		RefreshModel();
	}
}


void GadgetsTreeModel::RefreshModel()
{
	// Refreshing the model involves deleting all existing items.  We can't do
	// that if any item is in use (DSK-292233).
	for (INT32 i = 0; i < GetItemCount(); ++i)
	{
		if (GetItemByIndex(i)->IsLocked())
		{
			return;
		}
	}

	// Refresh all gadget sources.

	for (UINT32 i = 0; i < m_sources.GetCount(); ++i)
	{
		const OP_STATUS status = m_sources.Get(i)->Refresh();
		OP_ASSERT(OpStatus::IsSuccess(status)
				|| !"Failed to refresh gadget source");
	}
	
	m_platform_timestamp = op_time(NULL);

	// (Re)build the model.
	ModelLock model_lock(this);
	DeleteAll();

	for (UINT32 i = 0; i < m_sources.GetCount(); ++i)
	{
		const Source& source = *m_sources.Get(i);
		for (UINT32 j = 0; j < source.GetItemCount(); ++j)
		{
			GadgetsTreeModelItemImpl* item_impl = source.CreateItemImpl(j);
			if (NULL != item_impl)
			{
				AddLast(OP_NEW(GadgetsTreeModelItem, (item_impl)));
			}
		}
	}
}

#endif // WIDGET_RUNTIME_SUPPORT
