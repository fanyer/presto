/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#include "core/pch.h"

#ifdef GADGET_SUPPORT
#ifdef WEBSERVER_SUPPORT

#include "adjunct/quick/webserver/controller/WebServerServiceDownloadContext.h"

#include "modules/viewers/viewers.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/windowcommander/src/TransferManagerDownload.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/models/ServerWhiteList.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"

/***********************************************************************************
**  WebServerServiceDownloadContext::WebServerServiceDownloadContext
************************************************************************************/
WebServerServiceDownloadContext::WebServerServiceDownloadContext(URLInformation * url_info) :
	m_repository_url(),
	m_download_location(),
	m_download(url_info),
	m_transfer_listener(NULL)
{
	OP_ASSERT(m_download);
}

/***********************************************************************************
**  WebServerServiceDownloadContext::~WebServerServiceDownloadContext
************************************************************************************/
WebServerServiceDownloadContext::~WebServerServiceDownloadContext()
{
	m_download->SetDoneListener(NULL);
}

/***********************************************************************************
**  WebServerServiceDownloadContext::Init
************************************************************************************/
OP_STATUS
WebServerServiceDownloadContext::Init()
{
	OP_ASSERT(m_download);
	m_download->SetDoneListener(this);

	g_server_whitelist->GetRepositoryFromAddress(m_repository_url, m_download->URLName());
	RETURN_IF_ERROR(m_download_location.Set(m_download->URLName()));

	return OpStatus::OK;
}

/***********************************************************************************
**  WebServerServiceDownloadContext::StartDownload
************************************************************************************/
OP_STATUS
WebServerServiceDownloadContext::StartDownload()
{
	OP_ASSERT(m_download);

	OpString filename;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_TEMPDOWNLOAD_FOLDER, filename));
	if( filename.HasContent() && filename[filename.Length()-1] != PATHSEPCHAR )
		RETURN_IF_ERROR(filename.Append(PATHSEP));

	return m_download->DownloadDefault(this, filename.CStr());
}

/***********************************************************************************
**  WebServerServiceDownloadContext::FinishHandling
************************************************************************************/
void
WebServerServiceDownloadContext::Abort()
{
	((OpDocumentListener::DownloadCallback*)m_download)->Abort();
}

/***********************************************************************************
**  WebServerServiceDownloadContext::GetDownloadLocation
************************************************************************************/
const OpStringC &
WebServerServiceDownloadContext::GetDownloadLocation() const
{
	return m_download_location;
}

/***********************************************************************************
**  WebServerServiceDownloadContext::GetRepositoryURL
************************************************************************************/
const OpStringC &
WebServerServiceDownloadContext::GetRepositoryURL() const
{
	return m_repository_url;
}

OpString&
WebServerServiceDownloadContext::GetLocalLocation()
{
	return m_local_location;
}

/***********************************************************************************
**  WebServerServiceDownloadContext::IsTrustedServer
************************************************************************************/
BOOL
WebServerServiceDownloadContext::IsTrustedServer() const
{
	OP_ASSERT(m_download);
	OpStringC url(m_download->URLName());

	// don't care about local files
	if (url.FindFirstOf(UNI_L("file://")) == 0)
	{
		return TRUE;
	}

	return g_server_whitelist->FindMatch(url.CStr());
}

/***********************************************************************************
**  WebServerServiceDownloadContext::SetDownloadTransferListener
************************************************************************************/
void
WebServerServiceDownloadContext::SetDownloadTransferListener(OpTransferListener * transfer_listener)
{
	m_transfer_listener = transfer_listener;
}

/***********************************************************************************
**  WebServerServiceDownloadContext::OnDone
************************************************************************************/
/*virtual*/ BOOL
WebServerServiceDownloadContext::OnDone(URLInformation* url_info)
{
	return FALSE;
}

/***********************************************************************************
**  WebServerServiceDownloadContext::GetWindowCommander
************************************************************************************/
OpWindowCommander*
WebServerServiceDownloadContext::GetWindowCommander()
{
	DesktopWindow *window = g_application->GetActiveDocumentDesktopWindow();

	return window ? window->GetWindowCommander() : NULL;
}


/***********************************************************************************
**  WebServerServiceDownloadContext::IsShownDownload
************************************************************************************/
BOOL
WebServerServiceDownloadContext::IsShownDownload() const
{
	return FALSE;
}


#endif // WEBSERVER_SUPPORT
#endif // GADGET_SUPPORT
