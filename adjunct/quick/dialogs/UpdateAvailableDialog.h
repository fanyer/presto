/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** @author Manuela Hutter (manuelah)
*/

#ifdef AUTO_UPDATE_SUPPORT

#ifndef UPDATE_AVAILABLE_DIALOG_H
#define UPDATE_AVAILABLE_DIALOG_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/quick/managers/AutoUpdateManager.h"


/***********************************************************************************
 *
 *  @class UpdateAvailableDialog
 *
 *	@brief	Dialog that informs about an available browser update.
 *  @author Manuela Hutter
 *
 *
 ***********************************************************************************/
class UpdateAvailableDialog : public Dialog, OpPageListener
{
public:
						UpdateAvailableDialog(AvailableUpdateContext* context);
						~UpdateAvailableDialog();


	///--------------------- Dialog ---------------------///
	Dialog::DialogType	GetDialogType();
	Type				GetType()				{return DIALOG_TYPE_AUTOUPDATE_AVAILABLE;}
	const char*			GetWindowName()			{return "Auto Update Available Dialog";}
	DialogImage			GetDialogImageByEnum()	{return IMAGE_NONE;}
	const char*			GetHelpAnchor()			{return "autoupdate.html";}
	virtual BOOL		GetIsResizingButtons()	{return TRUE;}

	OpInputAction*		GetOkAction();
	OpInputAction*		GetCancelAction();
	const uni_char*		GetOkText()				{return m_ok_text.CStr();}
	const uni_char*		GetCancelText()			{return m_cancel_text.CStr();}

	void				OnInit();
	BOOL				OnInputAction(OpInputAction* action);
	UINT32				OnOk();
	void				OnCancel();


	///----------- OpPageListener -----------------///
	BOOL 				OnPagePopupMenu(OpWindowCommander* commander, OpDocumentContext& context);
	void				OnPageLoadingFinished(OpWindowCommander* commander, OpLoadingListener::LoadingFinishStatus status, BOOL stopped_by_user);

private:
	void				ShowBrowserView();
	void				ShowBrowserViewError();


private:
	OpFileLength		m_update_size;
	OpString			m_update_info_url;

	OpString			m_ok_text;
	OpString			m_cancel_text;

};

#endif //UPDATE_AVAILABLE_DIALOG_H

#endif // AUTO_UPDATE_SUPPORT
