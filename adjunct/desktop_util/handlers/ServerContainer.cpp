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
#include "adjunct/desktop_util/handlers/ServerContainer.h"

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
ServerContainer::ServerContainer(OpString * server_name,
								 Image &server_icon)
    : DownloadManager::Container(server_name,
								 server_icon)
{}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
ServerContainer::ServerContainer(OpString * server_name,
								 OpBitmap * server_icon)
    : DownloadManager::Container(server_name,
								 server_icon)
{}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void ServerContainer::Init(OpString * server_name,
						   OpBitmap * server_icon)
{
	InitContainer(server_name, server_icon);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void ServerContainer::Init(OpString * server_name,
						   Image &server_icon)
{
	InitContainer(server_name, server_icon);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void ServerContainer::Empty()
{
	EmptyContainer();
}
