/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifndef WEBSERVER_ADVANCED_SETTINGS_DIALOG_H
#define WEBSERVER_ADVANCED_SETTINGS_DIALOG_H

#ifdef WEBSERVER_SUPPORT

#include "adjunct/quick_toolkit/widgets/Dialog.h"

class WebServerSettingsContext;

/***********************************************************************************
**  @class	WebServerAdvancedSettingsDialog
**	@brief	
************************************************************************************/
class WebServerAdvancedSettingsDialog : public Dialog
{
public:
	WebServerAdvancedSettingsDialog(const WebServerSettingsContext * preset_settings, WebServerSettingsContext * out_settings);

	//===== Dialog implementations ====
	virtual Type		GetType()				{return DIALOG_TYPE_WEBSERVER_ADVANCED_SETTINGS;}
	virtual const char*	GetWindowName()			{return "Webserver Advanced Settings Dialog";}
	virtual const char*	GetHelpAnchor()			{return "unite.html";}
	virtual BOOL		GetProtectAgainstDoubleClick()	{return FALSE;}	
	virtual	void		OnInit();
	virtual UINT32		OnOk();

	//===== OpWidgetListener implementations ====
	//virtual void		OnChange(OpWidget *widget, BOOL changed_by_mouse);


private:
	const WebServerSettingsContext*	m_preset_settings;
	WebServerSettingsContext*		m_out_settings;
	OpString						m_unlimited_text;
};

#endif // WEBSERVER_SUPPORT
#endif // WEBSERVER_ADVANCED_SETTINGS_DIALOG_H
