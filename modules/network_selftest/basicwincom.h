/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2004-2008 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve N. Pettersen
 */
#ifndef _BASIC_LISTENER_H
#define _BASIC_LISTENER_H

#ifdef SELFTEST

#include "modules/network_selftest/urldoctestman.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/windowcommander/src/WindowCommanderManager.h"

class BasicWindowListener
	: public NullLoadingListener
{
private:
	URL_DocSelfTest_Manager *test_manager;
	OpLoadingListener *fallback;

public:
	BasicWindowListener (URL_DocSelfTest_Manager *manager, OpLoadingListener *fallback)
		: test_manager(manager), fallback(fallback) {}

	virtual ~BasicWindowListener();
	OpLoadingListener *GetFallback(){return fallback;}

	virtual void OnAuthenticationRequired(OpWindowCommander* commander, OpAuthenticationCallback* callback);

	virtual void OnUrlChanged(OpWindowCommander* commander, const uni_char* url);
	virtual void OnStartLoading(OpWindowCommander* commander);
	virtual void OnLoadingProgress(OpWindowCommander* commander, const LoadingInformation* info);
	virtual void OnLoadingFinished(OpWindowCommander* commander, LoadingFinishStatus status);
	virtual void OnStartUploading(OpWindowCommander* commander);
	virtual void OnUploadingFinished(OpWindowCommander* commander, LoadingFinishStatus status);
	virtual BOOL OnLoadingFailed(OpWindowCommander* commander, int msg_id, const uni_char* url);
#ifdef EMBROWSER_SUPPORT
	virtual void OnUndisplay(OpWindowCommander* commander);
	virtual void OnLoadingCreated(OpWindowCommander* commander);
#endif // EMBROWSER_SUPPORT

	void ReportFailure(URL &url, const char *format, ...);
};

#endif // SELFTEST
#endif // _BASIC_LISTENER_H
