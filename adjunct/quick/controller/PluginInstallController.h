/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- 
 * 
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved. 
 * 
 * This file is part of the Opera web browser.  It may not be distributed 
 * under any circumstances. 
 */ 

#ifndef OP_PLUGININSTALLDIALOG_H
#define OP_PLUGININSTALLDIALOG_H

#include "adjunct/quick_toolkit/contexts/DialogContext.h"
#include "adjunct/quick/managers/PluginInstallManager.h"
#include "adjunct/quick_toolkit/bindings/QuickBinder.h"
#include "adjunct/quick_toolkit/contexts/DialogContextListener.h"
#include "adjunct/quick_toolkit/widgets/QuickCheckBox.h"
#include "adjunct/quick_toolkit/widgets/QuickLabel.h"
#include "adjunct/quick_toolkit/widgets/QuickPagingLayout.h"

#ifdef PLUGIN_AUTO_INSTALL

class PluginInstallDialog:
	public DialogContext,
	public OpWidgetListener,
	public DialogContextListener,
	public PIM_PluginInstallManagerListener
{
public:
	enum PID_Page
	{
		PIDP_WELCOME,
		PIDP_INSTALLING_AUTO,
		PIDP_AUTO_INSTALL_FAILED,
		PIDP_NO_AUTOINSTALL,
		PIDP_INSTALLING_MANUAL,
		PIDP_MANUAL_INSTALL_FAILED,
		PIDP_MANUAL_INSTALL_OK,
		PIDP_NEEDS_DOWNLOADING,
		PIDP_IS_DOWNLOADING,
		PIDP_REDIRECT,
		PIDP_XP_NON_ADMIN,

		PIDP_LAST
	};

	PluginInstallDialog();
	~PluginInstallDialog();

	OP_STATUS ConfigureDialogContent(PIM_DialogMode mode, const OpStringC& mime_type, const OpStringC& plugin_content_url);

	virtual BOOL CanHandleAction(OpInputAction* action);
	virtual BOOL DisablesAction(OpInputAction* action);
	virtual OP_STATUS HandleAction(OpInputAction* action);

	virtual void OnChange(OpWidget *widget, BOOL changed_by_mouse = FALSE);
	virtual void OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);
	virtual void OnDialogClosing(DialogContext* context);

	virtual void OnPluginInstalledOK(const OpStringC& a_mime_type);
	virtual void OnPluginInstallFailed(const OpStringC& a_mime_type);

private:

	/**
	 * Implementing DialogContext
	 */
	virtual void InitL();
	OP_STATUS InitWelcomePage();

	OP_STATUS SwitchPage(PID_Page page_id);
	OP_STATUS MoveFromWelcomePage();

	OP_STATUS InitWidgets();
	OP_STATUS InitWidgetsWithPluginName();
	OP_STATUS InitWidgetsWithoutPluginName();
	OP_STATUS InitStringForWidget(const OpStringC8& widget_name, Str::LocaleString string_id);

	OP_STATUS ShowEULADialog();

	QuickButton* GetButton(const char* button_name);
	OP_STATUS ChangeButtons();
	OP_STATUS HideAllButtons();
	BOOL OKActionFilter();
	BOOL CancelActionFilter();

	void CloseDialog(bool closing_for_eula = FALSE);

	OP_STATUS ReadPluginProperties();
	void ReadWidgetsL();
	OP_STATUS SetTitle();

	OP_STATUS ShowButton(const char* button_name, bool show);

	OP_STATUS GoToWebPage();

	PIM_PluginItem* m_plugin_item;

	OpString m_mime_type;
	OpString m_plugin_name;
	OpString m_eula_url;
	OpString m_plugin_content_url;

	QuickSelectable* m_eula_checkbox;
	QuickMultilineLabel* m_eula_label;
	QuickMultilineLabel* m_general_info_label;

	PID_Page m_current_page_id;
	QuickPagingLayout* m_pages;

	bool m_plugin_can_auto_install;
	PIM_DialogMode m_dialog_mode;

	OpProperty<bool> m_can_install_auto;
	OpProperty<bool> m_can_install_manual;	
};

#endif // PLUGIN_AUTO_INSTALL

#endif // OP_PLUGININSTALLDIALOG_H
