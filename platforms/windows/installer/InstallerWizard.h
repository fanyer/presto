// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
//
// Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Deepak Arora
//

#ifndef INSTALLERSTARTUPDIALOG_H
#define INSTALLERSTARTUPDIALOG_H

#include "adjunct/desktop_pi/desktop_file_chooser.h"
#include "adjunct/quick_toolkit/widgets/Dialog.h"

#include "modules/hardcore/timer/optimer.h"

#include "platforms/windows/installer/OperaInstaller.h"

#define REMOVE_PROFILE_WIDGETS_COUNT 6

class ProcessModel;

class InstallerWizard : public Dialog, public DesktopFileChooserListener, public DialogListener
{

public: //Public constant

	enum InstallerPage
	{
		INSTALLER_DLG_STARTUP,			//Map to group 0;  a first page  of a dialog group
		INSTALLER_DLG_OPTIONS,			//Map to group 1;  a second page of a dialog group
		INSTALLER_DLG_HANDLE_LOCKED,	//Map to group 2;  listing applications which locking install process
		INSTALLER_DLG_UNINSTALL,		//Map to group 3;  page for confirming uninstallation
		INSTALLER_DLG_REMOVE_PROFILE,	//Map to group 4;  page allowing the user to select specific parts of the profile to remove
		INSTALLER_DLG_PROGRESS_INFO,	//Map to group 5;  presenting installation process
		INSTALLER_DLG_TERM_OF_SERVICE,	//Map to group 6;  final page of a dialog group for Terms of service.
	};

	enum BtnId 
	{
		BTN_ID_OPTIONS,					//Map to BACK button of TYPE_WIZARD dialog 
		BTN_ID_ACCEPT_INSTALL,			//Map to FORWARD button of TYPE_WIZARD dialog 
		BTN_ID_CANCEL,					//Map to CANCEL button of TYPE_WIZARD dialog
	};

	enum InstallationType
	{
		INSTALLATION_TYPE_ALL_USERS,
		INSTALLATION_TYPE_JUST_ME,
		INSTALLATION_TYPE_PORTABLE_DEVICE,
	};

private: //Private member variable

	typedef struct LocaleInfo	
	{
		OpString locale_file_path;
		OpString locale_code;
	}LocaleInfo;


public:

	InstallerWizard(OperaInstaller* opera_installer);
	virtual ~InstallerWizard();
	
	OP_STATUS	Init(InstallerPage page = INSTALLER_DLG_STARTUP);
	void		SetProgressSteps(OpFileLength steps);
	void		IncProgress();
	void		SetInfo(uni_char* info_text);
	void		ForbidClosing(BOOL forbid_closing);

	virtual BOOL IsLastPage(INT32 page_number) { return FALSE; };

	// Call this to show the file handle in use page (file_name is the file that we test for)
	OP_STATUS   ShowLockingProcesses(const uni_char* file_name, OpVector<ProcessItem>& processes);

protected:
	//OpInputContext	
	virtual BOOL				IsInputDisabled() {return FALSE;}

	//Events listener from Dialog
	virtual void				OnInit();
	virtual BOOL				OnInputAction(OpInputAction* action);
	virtual void				OnSetFocus() { FocusButton(BTN_ID_ACCEPT_INSTALL); }
	virtual void				OnLayout();
	  
	//Events listener from OpWidgetListener
	virtual void				OnChangeWhenLostFocus(OpWidget *widget);
	virtual	void				OnChange(OpWidget *widget, BOOL changed_by_mouse);
	virtual	void				OnClick(OpWidget *widget, UINT32 id/* = 0*/);
	virtual BOOL				OnContextMenu(OpWidget* widget, INT32 pos, INT32 x, INT32 y);

	//Events listener from DesktopFileChooserListener
	virtual void				OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result); 

	//Events listener for Dialog's group
	virtual void				OnInitPage(INT32 page_number, BOOL first_time);

	//Methods inherited from the class base classes
	virtual BOOL				GetIsBlocking()			{return FALSE;}
	virtual DialogType			GetDialogType()			{return TYPE_WIZARD;}
	virtual BOOL				GetCenterPages()		{return FALSE;}
	virtual BOOL				IsMouseGestureAllowed() {return FALSE;}

	//Based on 'DIALOG_TYPE_INSTALLER_WIZARD' an icon is set on the windows titlebar / capationbar
	virtual Type				GetType()				{return DIALOG_TYPE_INSTALLER_WIZARD;}

	//!!!Imp: Search the string 'Installer Wizard Dialog' in the source code to understand skinning behaviour!!!
	virtual const char*			GetWindowName()			{return "Installer Wizard Dialog";}

	//DesktopWindow::Listener
	virtual BOOL				OnDesktopWindowIsCloseAllowed(DesktopWindow* desktop_window);
	virtual void				OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated) {RemoveListener(this);};

	//DialogListener event listener
	virtual void				OnOk(Dialog* dialog, UINT32 result);
	virtual void				OnCancel(Dialog* dialog);

	//DesktopWindow message handler
	virtual void				HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	
private:

	void		SetWindowTitle();
	void		EnableShortcutsOptions(BOOL is_visible = TRUE, BOOL is_enable = TRUE);
	void		EnableDefaultBrowserOption(BOOL is_visible = TRUE, BOOL is_enable = TRUE);
	void		EnableWidgetInstallationType(BOOL is_enable = TRUE, InstallationType installation_type=INSTALLATION_TYPE_ALL_USERS);
	void		LaunchFolderExplorer();	
	void		SetInstallFolderAndUpdateOptions(const OpString& path);
	void		ToggleTooltipVisibility();
	void		InitializeStartUpDlgGroup(BOOL first_time);
	void		InitializeOptionsDlgGroup(BOOL first_time);
	void		InitializeProgressInfoDlgGroup(BOOL first_time);
	void		InitializeHandleLockedDlgGroup(BOOL first_time);
	void		InitializeUnintstallDlgGroup(BOOL first_time);
	void		InitializeRemoveProfileDlgGroup(BOOL first_time);
	void		InitializeToSDlgGroup(BOOL first_time);
	void		ShowShieldIconIfNeeded();
	void		ShowSpinnginWaitCursor(BOOL show_progressbar);
	void		SetAndFetchUpdatedInstallerSettings(OperaInstaller::Settings& setting);
	UINT		GetDriveType(uni_char *fullpath);
	BOOL		OperaExeExists();
	//This method tells whether the strings displayed in the dialog should indicate to the user that an upgrade will happen
	//It should not be used to affect the dialog functionally.
	BOOL		IsAnUpgrade();
	OP_STATUS	PopulateDropdownAndTranslateInstallationTypes();
	OP_STATUS   OverrideButtonProperty(BtnId butten_id, Str::LocaleString str_id, OpInputAction action_id, INTPTR action_data = NULL);
	OP_STATUS	GetCurrentLanguageFile(OpString &lng_file_path, OpString &lng_code);
	OP_STATUS	GetInstallerPath(OpString &path);
	OP_STATUS	TranslateString();
	OP_STATUS	UpdateTermOfServiceLabel(const char* label_name);
	OP_STATUS	HandleActionCancel();
	OP_STATUS	SelectDefaultSkin();
	OP_STATUS	ActivateNewLanguage(LocaleInfo *locale_info);
	OP_STATUS	PopulateLocaleDropDown();
	OP_STATUS	ReadEndUserLicenseAgreement();

private:

	typedef struct WidgetToTranslateStrMapping
	{
		char *widget;
		int str_code;
	} WidgetToTranslateStrMapping;

	static const WidgetToTranslateStrMapping m_widget_str[];

	DesktopFileChooserRequest	m_file_chooser_request;
	OperaInstaller::Settings	m_settings;
	DesktopFileChooser*			m_file_chooser;
	OperaInstaller*				m_opera_installer;
	ProcessModel*				m_process_model;
	INT32						m_selected_install_type_index;
	BOOL						m_forbid_closing;
	BOOL						m_initiated_process_termination;
	OpProgressBar*				m_progressbar;
	Image						m_uac_shield;

	char*						m_dialog_skin;
	char*						m_graphic_skin;

	BOOL						m_show_yandex_checkbox;


	BOOL m_remove_full_profile_old_value;
	BOOL m_remove_profile_old_value[REMOVE_PROFILE_WIDGETS_COUNT];
	static const OpStringC8 m_remove_profile_widget_names[REMOVE_PROFILE_WIDGETS_COUNT];
};
#endif // INSTALLERSTARTUPDIALOG_H
