/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef WINDOWSDESKTOPMAILCLIENT_H
#define WINDOWSDESKTOPMAILCLIENT_H
#ifdef M2_MAPI_SUPPORT
#include "adjunct/desktop_pi/DesktopMailClientUtils.h"
#include "platforms/windows/installer/OperaInstaller.h"

class WindowsDesktopMailClientUtils : public DesktopMailClientUtils
{
public:
	/**
	 * Signal, that the mail client is ready to use.
	 * @param initialized true, when M2 is initialized and ready to use(calling, when Opera is starting);
	 *		  false, when M2 is destroyed(calling, when Opera is closing).
	 */
	virtual void SignalMailClientIsInitialized(bool initialized);

	/**
	 * Signal, that MAPI message has been handled by mail client.
	 * @param id is a identifier for mapi request, which has been handled
	 */
	virtual void SignalMailClientHandledMessage(INT32 id);

	/**
	 * Check if Opera is the default mail client for current user
	 * @return true, if Opera is the default mailer; false otherwise
	 */
	virtual bool IsOperaDefaultMailClient() { return OperaInstaller().IsDefaultMailer() == TRUE; }

	/**
	 * Check if Opera is able to become the default mail client for current user
	 * @return true, if Opera isn't the default mailer and opera has been installed for all users; false otherwise
	 */
	virtual bool IsOperaAbleToBecomeDefaultMailClient() { return !IsOperaDefaultMailClient() && OperaInstaller().GetSettings().all_users == TRUE; }

	/**
	 * Set Opera as default mail client for current user
	 * @return OpStatus::OK if operation has finished successfully
	 */
	virtual OP_STATUS SetOperaAsDefaultMailClient();
private:
	OpAutoHANDLE m_event;
};

#endif //M2_MAPI_SUPPORT
#endif //WINDOWSDESKTOPMAILCLIENT_H