/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_LOCALSTORAGEBAR_H
#define OP_LOCALSTORAGEBAR_H

#include "adjunct/quick/widgets/OpInfobar.h"
#include "adjunct/quick/application/BrowserApplicationCacheListener.h"
#include "modules/windowcommander/OpWindowCommander.h"

/***********************************************************************************
**
**	OpLocalStorageBar
**
**  Toolbar that asks user if web page requires to save data locally.(Application cache, local sotrage etc)
**  Automatically dissapears and deny request after a time if not used.
**
***********************************************************************************/

class OpLocalStorageBar : public OpInfobar
{
protected:
	OpLocalStorageBar();
	~OpLocalStorageBar();

public:
	static OP_STATUS	Construct(OpLocalStorageBar** obj);

	void Show(OpApplicationCacheListener::InstallAppCacheCallback* callback,
			ApplicationCacheRequest request,
			UINTPTR id,
			const uni_char* url);

	void Hide(BOOL focus_page = TRUE);
	void Cancel();

	// == Hooks ======================

	virtual void		OnAlignmentChanged();

	// == OpInputContext ======================

	virtual BOOL			OnInputAction(OpInputAction* action);
	virtual const char*		GetInputContextName() {return "Local Storage Bar";}

	UINTPTR GetRequestID() {return m_id;}
	ApplicationCacheRequest GetRequest() {return m_request;}

private:
	void HandleRequest(BOOL accept);
	void Clear();

	OpApplicationCacheListener::InstallAppCacheCallback* m_callback;
	ApplicationCacheRequest m_request;
	UINTPTR m_id;

	OpString m_url;
	OpString m_domain;
};

#endif // OP_LOCALSTORAGEBAR_H
