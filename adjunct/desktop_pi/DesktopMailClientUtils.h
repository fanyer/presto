/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef DESKTOPMAILCLIENT_H
#define DESKTOPMAILCLIENT_H

#ifdef M2_MAPI_SUPPORT

class DesktopMailClientUtils
{
public:
	/**
	 * Destructor
	 */
	virtual ~DesktopMailClientUtils() {}

	/**
	 * Method creates instance of this class
	 * @param desktop_mail_client_utils a pointer, where an address for new object is stored
	 * @return OpStatus::OK if creating new object has finished success
	 */
	static OP_STATUS Create(DesktopMailClientUtils** desktop_mail_client_utils);
	
	/**
	 * Signal, that the mail client is ready to use.
	 * @param initialized true when M2 has just been initialized (called during startup); false when M2 is about to be destroyed (called during shutdown).
	 */
	virtual void SignalMailClientIsInitialized(bool initialized) = 0;

	/**
	 * Signal, that MAPI message has been handled by mail client.
	 * @param id is a identifier for mapi request, which has been handled
	 */
	virtual void SignalMailClientHandledMessage(INT32 id) = 0;

	/**
	 * Check if Opera is default mail client for current user
	 * @return true, if Opera is default mailer; false otherwise
	 */
	virtual bool IsOperaDefaultMailClient() = 0;

	/**
	 * Check if Opera is able to become default mail client for current user
	 * @return true, if Opera isn't the default mailer and opera has been installed for all users; false otherwise
	 */
	virtual bool IsOperaAbleToBecomeDefaultMailClient() = 0;

	/**
	 * Set Opera as default mail client for current user
	 * @return OpStatus::OK if everything goes well
	 */
	virtual OP_STATUS SetOperaAsDefaultMailClient() = 0;

};

#endif //M2_MAPI_SUPPORT

#endif //DESKTOPMAILCLIENT_H