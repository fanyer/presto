// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
//
// Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Patricia Aas
//

#ifndef __MIME_TYPE_HANDLER_H__
#define __MIME_TYPE_HANDLER_H__

#include "adjunct/desktop_util/handlers/DownloadManager.h"

class HandlerContainer :
	public DownloadManager::Container
{
public:

//  -------------------------
//  Public member functions:
//  -------------------------

	HandlerContainer() : DownloadManager::Container() {}

    HandlerContainer(OpString * handler_command,
					 OpString * handler_name,
					 OpBitmap * handler_icon);

    HandlerContainer(const OpStringC & handler_command,
					 const OpStringC & handler_name,
					 const OpStringC8 & handler_icon);

	void Init(const OpStringC & handler_command,
			  const OpStringC & handler_name,
			  const OpStringC8 & handler_icon);

	const OpStringC & GetCommand();

	void SetHandlerMode(HandlerMode mode) { m_mode = mode; }

	HandlerMode GetHandlerMode() { return m_mode; }

	virtual ContainerType GetType() { return CONTAINER_TYPE_HANDLER; }

private:

//  ------------------------
//  Private member functions:
//  -------------------------

    void SetCommand(const OpStringC & handler_command);

//  -------------------------
//  Private member variables:
//  -------------------------

    OpString m_command;

	HandlerMode m_mode;
};

#endif //__MIME_TYPE_HANDLER_H__
