/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifndef WEBSERVER_SERVICE_SETTINGS_DIALOG
#define WEBSERVER_SERVICE_SETTINGS_DIALOG

#ifdef GADGET_SUPPORT
#ifdef WEBSERVER_SUPPORT

#include "adjunct/quick_toolkit/widgets/Dialog.h"

#include "adjunct/quick/webserver/controller/WebServerServiceSettingsContext.h"

class WebServerServiceController;


/***********************************************************************************
**  @class	WebServerServiceSettingsDialog
**	@brief	Dialog to change the settings of a Unite service.
************************************************************************************/
class WebServerServiceSettingsDialog : public Dialog
{
public:
	WebServerServiceSettingsDialog(WebServerServiceController * service_controller, WebServerServiceSettingsContext* service_context);
	~WebServerServiceSettingsDialog();

	//===== OpInputContext implementations =====
	virtual Type			GetType();
	virtual BOOL			OnInputAction(OpInputAction* action);

	//===== Dialog implementations =====
	virtual void			OnInit();
	virtual UINT32			OnOk();

	//===== DesktopWindow implementations =====
	virtual const char*		GetWindowName();

	//===== OpWidgetListener implementations ====
	virtual void			OnChange(OpWidget *widget, BOOL changed_by_mouse);

protected:
	virtual BOOL			HasRequiredFieldsFilled();
	BOOL					HasSharedFolderFilled();
	BOOL					HasValidSharedFolder();
	void					SaveSettingsToContext();

protected:
	enum ServiceSettingsError
	{
		NoError,
		InvalidServiceNameInURL,
		ServiceNameAlreadyInUse,
		InvalidSharedFolderPath
	};

	WebServerServiceController *		m_service_controller; //< the controller who performs service actions. not owned by this class
	WebServerServiceSettingsContext	*	m_service_context;	//< the context of the widget to be installed. owned by this class

	void	ShowError(ServiceSettingsError error);
	void	HideError();

private:
	BOOL	IsAbsolutePath(const OpStringC & shared_folder);
	void	UpdateURL(const OpStringC & service_address);

private:
	OpWidget *							m_incorrect_input_field;
};

#endif // WEBSERVER_SUPPORT
#endif // GADGET_SUPPORT

#endif // WEBSERVER_SERVICE_SETTINGS_DIALOG
