/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 */
#ifndef DOCUMENT_DESKTOP_WINDOW_LISTENER_H
#define DOCUMENT_DESKTOP_WINDOW_LISTENER_H

#include "modules/windowcommander/OpWindowCommander.h"

class DocumentDesktopWindow;

class DocumentWindowListener
{
public:
	virtual ~DocumentWindowListener() {}

	virtual void	OnUrlChanged(DocumentDesktopWindow* document_window, const uni_char* url) {}
	virtual void	OnScaleChanged(DocumentDesktopWindow* document_window, UINT32 newScale) {}
	virtual void	OnStartLoading(DocumentDesktopWindow* document_window) {}
	virtual void	OnLoadingProgress(DocumentDesktopWindow* document_window, const LoadingInformation* info) {}
	virtual void	OnLoadingFinished(DocumentDesktopWindow* document_window, OpLoadingListener::LoadingFinishStatus, BOOL was_stopped_by_user = FALSE) {}
	virtual void	OnHover(DocumentDesktopWindow* document_window, const uni_char* url, const uni_char* link_title, BOOL is_image) {}
	virtual void	OnNoHover(DocumentDesktopWindow* document_window) {}
	virtual void	OnPopup(DocumentDesktopWindow* document_window, const uni_char *url, const uni_char *name, int left, int top, int width, int height, BOOL3 scrollbars, BOOL3 location) {}
	virtual void	OnSecurityModeChanged(DocumentDesktopWindow* document_window) {}
	virtual void	OnTrustRatingChanged(DocumentDesktopWindow* document_window) {}
	virtual void	OnOnDemandPluginStateChange(DocumentDesktopWindow* document_window) {}
#ifdef SUPPORT_SPEED_DIAL
	virtual void	OnSpeedDialViewCreated(DocumentDesktopWindow* document_window) {}
#endif
	virtual void	OnAuthenticationRequired(OpWindowCommander* commander, OpAuthenticationCallback* callback) {}


	/**
	* The document in the window will be changed
	* @param document_window that is changing
	*/
	virtual void OnDocumentChanging(DocumentDesktopWindow* document_window) {}
	virtual void OnTransparentAreaChanged(DocumentDesktopWindow* document_window, int height) {}
};

#endif //DOCUMENT_DESKTOP_WINDOW_LISTENER_H
