// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
//
// Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Patricia Aas
//

#include "core/pch.h"
#include "adjunct/desktop_util/handlers/PluginContainer.h"

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
PluginContainer::PluginContainer(OpString * plugin_path,
								 OpString * plugin_description,
								 OpString * plugin_name,
								 OpBitmap * plugin_icon)
    : DownloadManager::Container(plugin_name,
								 plugin_icon)
{
    SetPath(plugin_path);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
const OpStringC& PluginContainer::GetPath()
{
    return m_path;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
const OpStringC& PluginContainer::GetDescription()
{
    return m_description;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void PluginContainer::SetPath(OpString * plugin_path)
{
    if(plugin_path)
	m_path.Set(plugin_path->CStr());
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void PluginContainer::SetDescription(OpString * plugin_description)
{
    if(plugin_description)
	m_description.Set(plugin_description->CStr());
}
