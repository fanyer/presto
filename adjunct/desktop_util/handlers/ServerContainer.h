// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
//
// Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Patricia Aas
//

#ifndef __SERVER_CONTAINER_H__
#define __SERVER_CONTAINER_H__

#include "adjunct/desktop_util/handlers/DownloadManager.h"

class ServerContainer : 
	public DownloadManager::Container
{
public:

//  -------------------------
//  Public member functions:
//  -------------------------

	ServerContainer() : DownloadManager::Container() {}

    ServerContainer(OpString * server_name,
					OpBitmap * server_icon);
    
    ServerContainer(OpString * server_name,
					Image &server_icon);

	void Init(OpString * server_name,
			  OpBitmap * server_icon);
	
	void Init(OpString * server_name,
			  Image &server_icon);
    
	void Empty();

	virtual ContainerType GetType() { return CONTAINER_TYPE_SERVER; }

private:

//  ------------------------
//  Private member functions:
//  -------------------------

//  -------------------------
//  Private member variables:
//  -------------------------
};

#endif //__SERVER_CONTAINER_H__
