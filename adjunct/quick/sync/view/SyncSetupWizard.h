/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifndef SYNC_SETUP_WIZARD_H
#define SYNC_SETUP_WIZARD_H

#ifdef SUPPORT_DATA_SYNC

#include "adjunct/quick/view/FeatureSetupWizard.h"
#include "adjunct/quick/sync/controller/SyncSettingsContext.h"

class AccountContext;
class SyncController;

/***********************************************************************************
**  @class	SyncDialog
**	@brief	Sets up syncronization, goes through creating account & logging in.
**
************************************************************************************/
class SyncDialog : public FeatureDialog
{
public:
	SyncDialog(SyncController* controller, SyncSettingsContext* settings_context, OperaAccountController * account_controller, OperaAccountContext* account_context, BOOL needs_setup);
	
	virtual	const char*		GetHelpAnchor();
	virtual BOOL			SwitchingUserWanted();

	//===== OpWidgetListener implementations ====
	virtual void			OnChange(OpWidget *widget, BOOL changed_by_mouse);

protected:
	virtual void				OnInit();
	virtual void				OnClose(BOOL user_initiated);
	virtual void				OnCancel();

	virtual FeatureController*				GetFeatureController();
	virtual const FeatureSettingsContext*	GetPresetFeatureSettings() const;
	virtual FeatureSettingsContext*			ReadCurrentFeatureSettings();
	virtual BOOL			IsFeatureEnabled() const;

	virtual void			InitIntroPage();
	virtual void			InitCreateAccountPage();
	virtual void			InitLoginPage();
	virtual void			InitSettingsPage();

	virtual const char*		GetFeatureIntroPageName() const;
	virtual const char*		GetFeatureSettingsPageName() const;

	virtual const char*		GetFeatureIntroProgressSpinnerName() const;
	virtual const char*		GetFeatureIntroProgressLabelName() const;
	virtual const char*		GetFeatureSettingsProgressSpinnerName() const;
	virtual const char*		GetFeatureSettingsProgressLabelName() const;
	virtual const char*		GetFeatureSettingsErrorLabelName() const;

	virtual BOOL			HasValidSettings();

private:
	void					ReadSettings(SyncSettingsContext & settings);	//< read settings from dialog widgets to settings
	void					SetInputFieldsEnabled(BOOL enabled);

	/**
	 * When user's Opera Account password is too weak in our standards,
	 * we want to tell about it and ask the user to change his password
	 * to one that is stronger (it can be seen by password strength indicator).
	 *
	 * @return TRUE iff we think user's Opera Account password is too weak.
	 */
	BOOL					ShouldShowPasswordImprovementDialog();

	SyncController*			m_controller;
	SyncSettingsContext*	m_preset_settings;	//< Settings linked in the constructor
	SyncSettingsContext		m_current_settings; //< Settings as set in the dialog

	BOOL					m_passwd_checkbox_initial;
	BOOL					m_passwd_checkbox_changed;
};

#endif // SUPPORT_DATA_SYNC

#endif // SYNC_SETUP_WIZARD_H
