/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifndef WEBSERVER_SETUP_WIZARD_H
#define WEBSERVER_SETUP_WIZARD_H

#ifdef WEBSERVER_SUPPORT

#include "adjunct/quick/view/FeatureSetupWizard.h"
#include "adjunct/quick/webserver/controller/WebServerSettingsContext.h"

#ifdef GADGET_UPDATE_SUPPORT
#include "adjunct/autoupdate_gadgets/GadgetUpdateController.h"
#endif //GADGET_UPDATE_SUPPORT

class AccountContext;
class WebServerController;

/***********************************************************************************
**  @class	WebServerDialog
**	@brief	Wizard to create account/login and do the initial setup of the webserver.
************************************************************************************/
class WebServerDialog : public FeatureDialog
#ifdef GADGET_UPDATE_SUPPORT
						,public GadgetUpdateListener
#endif //GADGET_UPDATE_SUPPORT
{
public:
	WebServerDialog(WebServerController * controller, 
		const WebServerSettingsContext* settings_context,
		OperaAccountController * account_controller,
		OperaAccountContext* account_context, 
		BOOL needs_setup);

	virtual FeatureController*				GetFeatureController();
	virtual const FeatureSettingsContext*	GetPresetFeatureSettings() const;
	virtual FeatureSettingsContext*			ReadCurrentFeatureSettings();
	virtual BOOL							IsFeatureEnabled() const;

	//===== Dialog implementations ====
	virtual	const char*		GetHelpAnchor();
	virtual void			OnInit();
	virtual void			OnCancel();

	//===== DesktopWindow implementations =====
	virtual void			OnClose(BOOL user_initiated);

	//===== OpWidgetListener implementations ====
	virtual void			OnChange(OpWidget *widget, BOOL changed_by_mouse);

	//===== OpInputContext implementations =====
	virtual BOOL			OnInputAction(OpInputAction* action);

	//===== OperaAccountController::OAC_Listener implementations =====
	virtual void	OnOperaAccountReleaseDevice(OperaAuthError error);
	virtual void	OnOperaAccountDeviceCreate(OperaAuthError error, 
											   const OpStringC& shared_secret, 
											   const OpStringC& server_message);

	virtual void	HandleError(OperaAuthError error);

#ifdef GADGET_UPDATE_SUPPORT
	void			OnGadgetUpdateFinish(GadgetUpdateInfo* data,
									  GadgetUpdateController::GadgetUpdateResult result);
#endif 

protected:
	virtual void	InitIntroPage();
	virtual void	InitCreateAccountPage();
	virtual void	InitLoginPage();
	virtual void	InitSettingsPage();

	virtual const char*				GetFeatureIntroPageName()		const;
	virtual const char*				GetFeatureSettingsPageName()	const;


	virtual const char*				GetFeatureIntroProgressSpinnerName() const;
	virtual const char*				GetFeatureIntroProgressLabelName() const;
	virtual const char*				GetFeatureSettingsProgressSpinnerName() const;
	virtual const char*				GetFeatureSettingsProgressLabelName() const;
	virtual const char*				GetFeatureSettingsErrorLabelName() const;

	virtual BOOL					HasValidSettings();

private:
	void		ReadSettings(WebServerSettingsContext & settings);	//< read settings from dialog widgets, save them settings
	void		SetInputFieldsEnabled(BOOL enabled);
	void		UpdateURL(const OpStringC & computername);	//< Update URL info to show the Unite URL

	WebServerController *				m_controller;
	const WebServerSettingsContext*		m_preset_settings;	//< Settings linked in the constructor
	WebServerSettingsContext			m_current_settings; //< Settings as set in the dialog

	BOOL								m_in_progress;

#ifdef  GADGET_UPDATE_SUPPORT
	BOOL								m_update_lstnr_registered;
#endif //GADGET_UPDATE_SUPPORT
};

#endif // WEBSERVER_SUPPORT

#endif // WEBSERVER_SETUP_WIZARD_H
