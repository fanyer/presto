/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- 
 * 
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved. 
 * 
 * This file is part of the Opera web browser.  It may not be distributed 
 * under any circumstances. 
 */ 

#ifndef OP_PLUGINEULADIALOG_H
#define OP_PLUGINEULADIALOG_H

#include "adjunct/quick/managers/PluginInstallManager.h"
#include "adjunct/quick_toolkit/contexts/DialogContext.h"
#include "adjunct/quick_toolkit/contexts/DialogContextListener.h"
#include "adjunct/quick_toolkit/widgets/QuickCheckBox.h"
#include "adjunct/quick_toolkit/widgets/QuickIcon.h"
#include "adjunct/quick_toolkit/widgets/QuickLabel.h"
#include "adjunct/quick_toolkit/widgets/QuickPagingLayout.h"

#ifdef PLUGIN_AUTO_INSTALL

class PluginEULADialog:
	public DialogContext,
	public DialogContextListener,
	public OpPageListener
{
public:

	PluginEULADialog();

	virtual BOOL CanHandleAction(OpInputAction* action);
	virtual BOOL DisablesAction(OpInputAction* action);
	virtual OP_STATUS HandleAction(OpInputAction* action);

	OP_STATUS StartLoadingEULA(const OpStringC& mime_type, const OpStringC& plugin_content_url);

	virtual void OnDialogClosing(DialogContext* context);

	virtual void OnPageLoadingFailed(OpWindowCommander* commander, const uni_char* url);
	virtual void OnPageLoadingFinished(OpWindowCommander* commander, OpLoadingListener::LoadingFinishStatus status, BOOL was_stopped_by_user);
	virtual void OnPageUrlChanged(OpWindowCommander* commander, const uni_char* url);
	virtual void OnPageStartLoading(OpWindowCommander* commander);

private:
	/**
	 * Implementing DialogContext
	 */
	virtual void InitL();

	OP_STATUS OnLoadingFailed();
	OP_STATUS OnLoadingOK();

	OpString m_mime_type;
	OpString m_plugin_content_url;
	QuickIcon* m_loading_icon;
	OpBrowserView* m_browser_view;
	QuickLabel* m_license_label;
	PIM_PluginItem* m_item;

	bool m_eula_failed;
};

#endif // PLUGIN_AUTO_INSTALL

#endif // OP_PLUGINEULADIALOG_H
