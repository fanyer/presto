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
#include "adjunct/desktop_util/handlers/HandlerContainer.h"

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
HandlerContainer::HandlerContainer(OpString * handler_command,
								   OpString * handler_name,
								   OpBitmap * handler_icon)
	: DownloadManager::Container(handler_name,
								 handler_icon)
{
    SetCommand(*handler_command);
	m_mode = HANDLER_MODE_EXTERNAL;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
HandlerContainer::HandlerContainer(const OpStringC & handler_command,
								   const OpStringC & handler_name,
								   const OpStringC8 & handler_icon)
	: DownloadManager::Container(handler_name,
								 handler_icon)
{
	SetCommand(handler_command);
	m_mode = HANDLER_MODE_EXTERNAL;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void HandlerContainer::Init(const OpStringC & handler_command,
							const OpStringC & handler_name,
							const OpStringC8 & handler_icon)
{
	InitContainer(handler_name, handler_icon);
	SetCommand(handler_command);
	m_mode = HANDLER_MODE_EXTERNAL;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
const OpStringC& HandlerContainer::GetCommand()
{
    return m_command;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void HandlerContainer::SetCommand(const OpStringC & handler_command)
{
	m_command.Set(handler_command.CStr());
}
