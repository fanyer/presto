// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
//
// Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Patricia Aas
//

#ifndef __PLUGIN_CONTAINER_H__
#define __PLUGIN_CONTAINER_H__

#include "adjunct/desktop_util/handlers/DownloadManager.h"

class PluginContainer :
	public DownloadManager::Container
{
public:

//  -------------------------
//  Public member functions:
//  -------------------------

    PluginContainer(OpString * plugin_path,
					OpString * plugin_description,
					OpString * plugin_name,
					OpBitmap * plugin_icon);

    const OpStringC & GetPath();
    const OpStringC & GetDescription();

	virtual ContainerType GetType() { return CONTAINER_TYPE_PLUGIN; }

private:

//  ------------------------
//  Private member functions:
//  -------------------------

    void SetPath(OpString * plugin_path);
    void SetDescription(OpString * plugin_description);

//  -------------------------
//  Private member variables:
//  -------------------------

    OpString m_path;
    OpString m_description;
};

#endif //__PLUGIN_CONTAINER_H__
