/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patricia Aas (psmaas) and Eirik Byrkjeflot Anonsen (eirik)
 */

#ifndef VIEWIX_PLATFORM_API_H
#define VIEWIX_PLATFORM_API_H

class ViewixPlatformAPI
{
public:

	virtual ~ViewixPlatformAPI() {}

	static ViewixPlatformAPI& GetViewixPlatformAPI();

	/**
	 * @return TRUE if the current desktop environment is KDE
	 */
	virtual BOOL IsKDERunning() = 0;

	/**
	 * @return TRUE if the current desktop environment is Gnome
	 */
	virtual BOOL IsGnomeRunning() = 0;

	/**
	 * @return an OpBitmap with the image indicated by the path
	 */
	virtual OpBitmap* GetBitmapFromPath(const OpStringC& path) = 0;

	/**
	 * Gives the opportunity of asking the user about a folder handler,
	 * before invoking it.
	 *
	 * @return TRUE if the handler specified is appropriate for the folder
	 */
	virtual BOOL ValidateFolderHandler(const OpStringC & folder,
									   const OpStringC & handler,
									   BOOL& do_not_show_again) = 0;

	/**
	 * Gives the opportunity of asking the user about an executable handler,
	 * before invoking it.
	 *
	 * @return TRUE if the handler specified is allowed for the executable file
	 */
	virtual BOOL AcceptExecutableHandler(BOOL found_security_issue,
										 const OpStringC& filename,
										 const OpStringC& handler,
										 BOOL& do_not_show_again) = 0;

	/**
	 * Let the platform handle the protocol with the appropriate protocol handler.
	 *
	 * @param protocol for example 'telnet' or 'news'
	 * @param parameters to be given to the handler
	 *
	 * @return TRUE if the protocol was handled
	 */
	virtual BOOL HandleProtocol(const OpStringC& protocol,
								const OpStringC& parameters) = 0;


	virtual BOOL LaunchHandler(const OpStringC& app,
							   const OpStringC& params,
							   BOOL in_terminal) = 0;

	virtual void RunExternalApplication(const OpStringC& handler,
										const OpStringC& path) = 0;

	virtual void OnExecuteFailed(const OpStringC& app,
								 const OpStringC& protocol) = 0;


	virtual void NativeString(const OpStringC& input,
							  OpString& output) = 0;
};

#endif // VIEWIX_PLATFORM_API_H
