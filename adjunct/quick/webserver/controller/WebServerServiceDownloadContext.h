/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifndef WEBSERVER_SERVICE_DOWNLOAD_CONTEXT_H
#define WEBSERVER_SERVICE_DOWNLOAD_CONTEXT_H

#ifdef GADGET_SUPPORT
#ifdef WEBSERVER_SUPPORT

#include "modules/windowcommander/OpWindowCommander.h"

/***********************************************************************************
**  @class	WebServerServiceDownloadContext
**	@brief	Paramaters that need to be known for downloading/installing a webserver service.
************************************************************************************/
class WebServerServiceDownloadContext : public DownloadContext, public URLInformation::DoneListener
{
public:
	explicit	WebServerServiceDownloadContext(URLInformation * url_info);
	virtual		~WebServerServiceDownloadContext();

	OP_STATUS			Init();
	OP_STATUS			StartDownload();
	void				Abort();

	const OpStringC &	GetDownloadLocation() const;
	const OpStringC &	GetRepositoryURL() const;
	OpString&			GetLocalLocation();
	BOOL				IsTrustedServer() const;

	void				SetDownloadTransferListener(OpTransferListener * transfer_listener);

	//===== DoneListener ==========
	virtual BOOL		OnDone(URLInformation* url_info);

	//===== DownloadContext ==========
	virtual OpWindowCommander*	GetWindowCommander();

	virtual BOOL				IsShownDownload() const; 
	virtual OpTransferListener* GetTransferListener() const { return m_transfer_listener; }

private:
	OpString				m_repository_url;
	OpString				m_download_location;
	OpString				m_local_location;
	URLInformation *		m_download;
	OpTransferListener * m_transfer_listener;
};

#endif // WEBSERVER_SUPPORT
#endif // GADGET_SUPPORT

#endif // WEBSERVER_SERVICE_DOWNLOAD_CONTEXT_H
