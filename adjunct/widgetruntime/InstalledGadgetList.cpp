/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/widgetruntime/InstalledGadgetList.h"


InstalledGadgetList::InstalledGadgetList()
{
}


OP_STATUS InstalledGadgetList::Init()
{
	PlatformGadgetList* platform_list = NULL;
	RETURN_IF_ERROR(PlatformGadgetList::Create(&platform_list));
	OP_ASSERT(NULL != platform_list);
	RETURN_IF_ERROR(Init(platform_list));
	return OpStatus::OK;
}


OP_STATUS InstalledGadgetList::Init(PlatformGadgetList* platform_list)
{
	return m_cache.Init(platform_list);
}


BOOL InstalledGadgetList::Contains(const OpStringC& gadget_display_name) const
{
	BOOL contains = FALSE;

	const OpAutoVector<GadgetInstallerContext>& infos(m_cache.Get());
	for (UINT32 i = 0; i < infos.GetCount(); ++i)
	{
		if (gadget_display_name == infos.Get(i)->m_display_name)
		{
			contains = TRUE;
			break;
		}
	}

	return contains;
}

InstalledGadgetList::Cache::Cache()
	: m_timestamp(0)
{
}


OP_STATUS InstalledGadgetList::Cache::Init(PlatformGadgetList* platform_list)
{
	m_platform_list.reset(platform_list);
	return OpStatus::OK;
}


OpAutoVector<GadgetInstallerContext>& InstalledGadgetList::Cache::Get()
{
	if (m_platform_list->HasChangedSince(m_timestamp))
	{
		m_platform_list->GetAll(m_gadget_infos);
		m_timestamp = op_time(NULL);
	}
	return m_gadget_infos;
}

#endif // WIDGET_RUNTIME_SUPPORT
