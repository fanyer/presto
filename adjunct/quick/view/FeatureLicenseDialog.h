/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifndef FEATURE_LICENSE_DIALOG_H
#define FEATURE_LICENSE_DIALOG_H

// Webpage for the My.opera terms and conditions
#define MYOPERA_LICENSE_TERMS	UNI_L("http://redir.opera.com/community/terms/")

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "modules/locale/locale-enum.h"

/***********************************************************************************
**  @class	FeatureLicenseDialog
**	@brief	Shows the lincense info for Opera features
**
************************************************************************************/
class FeatureLicenseDialog : public Dialog, OpPageListener
{
public:
	//===== Dialog implementations =====
	virtual Str::LocaleString	GetOkTextID()		{ return Str::D_FEATURE_LICENSE_ACCEPT; } // todo: make const in Dialog
	virtual Str::LocaleString	GetCancelTextID()	{ return Str::D_FEATURE_LICENSE_DECLINE; } // todo: make const in Dialog

	virtual BOOL				IsCascaded() { return TRUE; } //< don't show dialog centered on top of setup wizard

	virtual void				OnInit();
	virtual UINT32				OnOk();
	virtual void				OnCancel();	
	virtual BOOL				GetIsResizingButtons() { return TRUE; } // allow buttons to be wider if needed in other languages

	//===== DesktopWindow implementations =====
	virtual const char*			GetWindowName() {return "Feature License Dialog";}

	//===== OpInputContext implementations =====
	virtual Type				GetType()		{ return DIALOG_TYPE_FEATURE_LICENSE_DIALOG; }
	virtual BOOL				OnInputAction(OpInputAction* action);

	//===== OpPageListener implementations =====
	BOOL				OnPagePopupMenu(OpWindowCommander* commander, OpDocumentContext* ctx);
	void				OnPageLoadingFinished(OpWindowCommander* commander, OpLoadingListener::LoadingFinishStatus status, BOOL stopped_by_user);

private:
	void					ShowBrowserView();
	void					ShowBrowserViewError();

};

#endif // FEATURE_LICENSE_DIALOG_H
