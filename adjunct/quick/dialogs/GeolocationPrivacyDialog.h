/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Petter Nilsen (pettern)
 */

#ifndef GEOLOCATION_PRIVACY_DIALOG_H
#define GEOLOCATION_PRIVACY_DIALOG_H

#ifdef DOM_GEOLOCATION_SUPPORT

// Webpage for the geolocation terms and conditions
#define GEOLOCATION_PRIVACY_TERMS	UNI_L("http://redir.opera.com/geolocation/privacy/")

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "modules/locale/locale-enum.h"


class DocumentDesktopWindow;

/***********************************************************************************
**  @class	GeolocationPrivacyDialog
**	@brief	Shows privacy request to the user
**
************************************************************************************/
class GeolocationPrivacyDialog : public Dialog, OpPageListener
{
public:

	GeolocationPrivacyDialog(DocumentDesktopWindow& window, OpPermissionListener::PermissionCallback::PersistenceType persistence)
		: m_desktop_window(window)
		, m_persistence(persistence)
	{}
	~GeolocationPrivacyDialog();

	//===== Dialog implementations =====
	virtual Str::LocaleString	GetOkTextID()		{ return Str::D_GEOLOCATION_PRIVACY_DIALOG_ACCEPT; } // todo: make const in Dialog
	virtual Str::LocaleString	GetCancelTextID()	{ return Str::D_GEOLOCATION_PRIVACY_DIALOG_LATER; } // todo: make const in Dialog

	virtual BOOL				GetModality()		{ return TRUE; }		// we want it to be modal

	virtual void				OnInit();
	virtual UINT32				OnOk();
	virtual void				OnCancel();	
	virtual BOOL				GetIsResizingButtons() { return TRUE; } // allow buttons to be wider if needed in other languages

	//===== DesktopWindow implementations =====
	virtual const char*			GetWindowName() {return "Geolocation Privacy Dialog";}

	//===== OpInputContext implementations =====
	virtual Type				GetType()		{ return DIALOG_TYPE_GEOLOCATION_PRIVACY_DIALOG; }

	// == OpWidgetListener ======================
	virtual void				OnChange(OpWidget *widget, BOOL changed_by_mouse);

	//===== OpPageListener implementations =====
	virtual BOOL		OnPagePopupMenu(OpWindowCommander* commander, OpDocumentContext& context);
	virtual void		OnPageLoadingFinished(OpWindowCommander* commander, OpLoadingListener::LoadingFinishStatus status, BOOL was_stopped_by_user);
	
private:
	void					ShowBrowserView();
	void					ShowBrowserViewError();

	DocumentDesktopWindow& m_desktop_window;
	OpPermissionListener::PermissionCallback::PersistenceType m_persistence;
};

#endif // DOM_GEOLOCATION_SUPPORT
#endif // GEOLOCATION_PRIVACY_DIALOG_H
