/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 * 
 * TESTCASES: featuresetupwizard.ot
 */

#ifndef FEATURE_SETUP_WIZARD_H
#define FEATURE_SETUP_WIZARD_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"

#include "adjunct/quick/managers/OperaAccountManager.h"
#include "adjunct/quick/controller/FeatureController.h"

class OperaAccountController;
class OperaAccountContext;


/***********************************************************************************
**  @class	FeatureDialog
**	@brief	Managing how to set up Opera Account Features (Link, Unite..).
**
************************************************************************************/
class FeatureDialog : 
	public Dialog,
	public FeatureControllerListener,
	public OperaAccountController::OAC_Listener,
	public PrivacyManagerCallback
{
public:
	// constructors/ destructors
	explicit FeatureDialog(OperaAccountController * controller, OperaAccountContext* context, BOOL needs_setup);
	virtual ~FeatureDialog() {}

	// for login only
	virtual OP_STATUS			InitLoginDialog(BOOL * logged_in, DesktopWindow* parent_window = NULL);
	virtual void				SetBlockingForReturnValue(BOOL * retval);
	virtual BOOL				SwitchingUserWanted() { return TRUE; } // override if you need to check for user switch

	//===== OpInputContext implementations =====
	virtual Type				GetType();
	virtual BOOL				OnInputAction(OpInputAction* action);

	//===== Dialog implementations =====
	virtual DialogType			GetDialogType();	// todo: make const in Dialog
	virtual	const char*			GetHelpAnchor() = 0;	// every feature must have a help button
	virtual UINT32				OnForward(UINT32 page_number);
	virtual BOOL				OnValidatePage(INT32 page_number);
	virtual BOOL				IsLastPage(INT32 page_number);
	virtual void				OnInit();
	virtual void				OnInitPage(INT32 page_number, BOOL first_time);
	virtual void				OnCancel();
	virtual BOOL				GetIsBlocking();
	virtual BOOL				GetIsResizingButtons() { return TRUE; } // allow buttons to be wider if needed in other languages

	//===== OpWidgetListener implementations ====
	virtual void				OnChange(OpWidget *widget, BOOL changed_by_mouse);

	/*
	**  Gets the name of the dialog used for loading a section in dialog.ini
	**  NOTE: don't override this function in your subclass! Don't use a separate
	**  section in dialog.ini, but add groups for your introduction and settings page
	**  in the [Feature Setup Wizard] section.
	*/
	virtual const char*			GetWindowName();
	virtual void				OnClose(BOOL user_initiated);

	//===== OperaAccountController::OAC_Listener implementations =====
	virtual void OnOperaAccountCreate(OperaAuthError error, OperaRegistrationInformation& reg_info);
	virtual void OnOperaAccountAuth(OperaAuthError error, const OpStringC &shared_secret);
	virtual void OnOperaAccountDeviceCreate(OperaAuthError error, const OpStringC& shared_secret, const OpStringC& server_message) {}

	//===== FeatureControllerListener implementations =====
	virtual void	OnFeatureEnablingSucceeded();
	virtual void	OnFeatureSettingsChangeSucceeded();

	virtual void	HandleError(OperaAuthError error);

	//===== PrivacyManagerCallback implementations =====
	virtual void OnPasswordRetrieved(const OpStringC& password);
	virtual void OnPasswordFailed();

protected:
	virtual FeatureController*				GetFeatureController() = 0;
	virtual const FeatureSettingsContext*	GetPresetFeatureSettings() const = 0;
	virtual FeatureSettingsContext*			ReadCurrentFeatureSettings() = 0;
	virtual BOOL			IsFeatureEnabled() const = 0;

	virtual const char*		GetFeatureIntroProgressSpinnerName() const = 0;
	virtual const char*		GetFeatureIntroProgressLabelName() const = 0;
	virtual const char*		GetFeatureSettingsProgressSpinnerName() const = 0;
	virtual const char*		GetFeatureSettingsProgressLabelName() const = 0;
	virtual const char*		GetFeatureSettingsErrorLabelName() const = 0;

	// page inits
	virtual void	InitTitle();
	virtual void	InitIntroPage() = 0;
	virtual void	InitCreateAccountPage();
	virtual void	InitLoginPage();
	virtual void	InitSettingsPage() = 0;

	BOOL			IsLoggedIn() const;
	BOOL			IsUserCredentialsSet() const;

	virtual void	Login();
	void			CreateAccount();
	void			EnableFeature();
	void			ChangeFeatureSettings();

	BOOL			HasValidInput(INT32 page_number);
	virtual BOOL	HasValidNewAccountInfo();	// todo: make const
	virtual BOOL	HasValidLoginInfo();		// todo: make const
	virtual BOOL	HasValidSettings() = 0;

	virtual BOOL	NeedsSetup()	{ return m_needs_setup; }

	/* Function to show/hide widgets that show errors */
	void			ShowError(Str::LocaleString error_string_ID);
	void			HideError();
	void			SetInProgress(BOOL in_progress);

	/* Functions to indicate progress */
	void			ShowProgressInfo(Str::LocaleString progress_info_ID);
	void			ShowProgressInfo(const OpStringC & progress_info_string);
	void			HideProgressInfo();

	BOOL			HasToRevertUserCredentials();

	// input actions
	virtual void	ShowLicenseDialog(); // needs to be virtual for selftests
	INT32			GetPageByName(const OpStringC8 & name);
	
	//Functions to show the correct pages
	virtual const char*	GetCreateAccountPageName()		const;
	virtual const char*	GetLoginPageName()				const;
	virtual const char*	GetFeatureIntroPageName()		const	= 0;
	virtual const char*	GetFeatureSettingsPageName()	const	= 0;

	BOOL				IsCreateAccountPage(INT32 page_number) const;
	BOOL				IsLoginPage(INT32 page_number) const;
	BOOL				IsFeatureIntroPage(INT32 page_number) const;
	BOOL				IsFeatureSettingsPage(INT32 page_number) const;
	const char*			GetProgressSpinnerName(INT32 page_number) const;

	OP_STATUS			GetUsername(OpString& username);
private:
	void				SetLicenseAccepted(BOOL is_accepted);
	void				GoToTargetPage(INT32 target_page);

	const char*			GetErrorLabelName(INT32 page_number) const;
	const char*			GetProgressLabelName(INT32 page_number) const;

	void				HandleCreateAccountAction();
	void				HandleLoginAction();

	void				SetUsernamePasswordNoOnchange(const OpStringC& username,const OpStringC& password);

private:
	struct ToRevert
	{
		OpString		login_uname; // has logged in with this username within this dialog
		OpString		login_pword; // has logged in with this password within this dialog
	} m_to_revert;		// things that have to be reverted when cancelling

	OperaAccountController*		m_account_controller;
	OperaAccountContext*		m_account_context;	//< account info. not owned by the wizard
	BOOL				m_needs_setup;	// defines if it's a setup or a settings dialog
	BOOL				m_in_progress;
	BOOL				m_login_only;	// only login to opera account and don't do anything with the feature
	BOOL				m_blocking;
	BOOL				m_password_saved;
	BOOL *				m_retval;
	INT32				m_scheduled_page_idx;
	INT32				m_previous_page_idx;
	OpString8			m_incorrect_input_field;
};


#endif // FEATURE_SETUP_WIZARD_H
