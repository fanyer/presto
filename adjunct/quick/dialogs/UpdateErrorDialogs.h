/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** @author Manuela Hutter (manuelah)
*/

#ifdef AUTO_UPDATE_SUPPORT

#ifndef UPDATE_ERROR_DIALOGS_H
#define UPDATE_ERROR_DIALOGS_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/quick/managers/AutoUpdateManager.h"


/***********************************************************************************
 *
 *  @class	UpdateErrorDialog
 *
 *	@brief	Dialog that informs about an error during auto-update
 *  @author Manuela Hutter
 *
 *
 ***********************************************************************************/
class UpdateErrorDialog : public Dialog
{
public:
						UpdateErrorDialog(AutoUpdateError error);
						~UpdateErrorDialog();

	///--------------------- Dialog ---------------------///
	DialogType			GetDialogType()			{return TYPE_YES_NO_CANCEL;}
	Type				GetType()				{return DIALOG_TYPE_AUTOUPDATE_ERROR;}
	const char*			GetWindowName()			{return "Auto Update Error Dialog";}
	DialogImage			GetDialogImageByEnum()	{return IMAGE_ERROR;}
	const char*			GetHelpAnchor()			{return "autoupdate.html";}
	virtual BOOL		GetIsResizingButtons()	{return TRUE;}

	const uni_char*		GetOkText()				{return m_resume_text.CStr();}				// 'yes' button in yes-no-cancel
	const uni_char*		GetCancelText()			{return m_install_manually_text.CStr();}	// 'no' button in yes-no-cancel


	void				OnInit();
	UINT32				OnOk();
	void				OnCancel();
	void				OnYNCCancel();

private:
	AutoUpdateError		m_error;
	OpString			m_resume_text;
	OpString			m_install_manually_text;

};


/***********************************************************************************
 *
 *  @class	UpdateErrorResumingDialog
 *
 *	@brief	Dialog that informs about an error during auto-update, with auto-resuming.
 *  @author Manuela Hutter
 *
 *
 ***********************************************************************************/
class UpdateErrorResumingDialog : public Dialog, public AutoUpdateManager::AUM_Listener
{
public:
						UpdateErrorResumingDialog(UpdateErrorContext* context);
						~UpdateErrorResumingDialog();

	///--------------------- Dialog ---------------------///
	Type				GetType()				{return DIALOG_TYPE_AUTOUPDATE_ERROR;}
	const char*			GetWindowName()			{return "Auto Update Error Resuming Dialog";}
	DialogImage			GetDialogImageByEnum()	{return IMAGE_ERROR;}
	const char*			GetHelpAnchor()			{return "autoupdate.html";}
	virtual BOOL		GetIsResizingButtons()	{return TRUE;}

	const uni_char*		GetOkText()				{return m_resume_now_text.CStr();}

	void				OnInit();
	UINT32				OnOk();
	void				OnCancel();

	///--------------------- AUM_Listener ---------------------///
	virtual void		OnUpToDate()	{}
	virtual void		OnChecking()	{}
	virtual void		OnUpdateAvailable(AvailableUpdateContext* context)	{}
	virtual void		OnDownloading(UpdateProgressContext* context)		{}
	virtual void		OnPreparing(UpdateProgressContext* context)			{}
	virtual void		OnReadyToInstall(UpdateProgressContext* context)		{}
	virtual void		OnMinimizedStateChanged(BOOL minimized)		{}
	virtual void		OnDownloadingDone(UpdateProgressContext* context) {}
	virtual void		OnDownloadingFailed() {}
	virtual void		OnFinishedPreparing() {}

	virtual void		OnError(UpdateErrorContext* context)
							{SetResumingInfo(context->seconds_until_retry);}


private:
	void				SetResumingInfo(INT32 seconds_until_retry);

private:
	UpdateErrorContext	m_context;
	OpString			m_resume_now_text;

};

#endif //UPDATE_ERROR_DIALOGS_H

#endif // AUTO_UPDATE_SUPPORT
