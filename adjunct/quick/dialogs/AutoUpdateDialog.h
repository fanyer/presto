/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifndef AUTO_UPDATE_DIALOG_H
#define AUTO_UPDATE_DIALOG_H

#include "adjunct/quick/managers/AutoUpdateManager.h"
#include "adjunct/quick_toolkit/widgets/Dialog.h"

class OpProgressBar;
class OpLabel;


/***********************************************************************************
 *
 *  @class AutoUpdateDialog
 *
 *	@brief	Dialog that informs about the progress of an ongoing update.
 *  @author Manuela Hutter
 *
 *
 ***********************************************************************************/
class AutoUpdateDialog : public Dialog
	, public AutoUpdateManager::AUM_Listener
{
public:
							AutoUpdateDialog();
							~AutoUpdateDialog();


	///--------------------- Dialog ---------------------///
	Type					GetType()				{return DIALOG_TYPE_AUTOUPDATE_STATUS;}
	const char*				GetWindowName()			{return "Auto Update Dialog";}
	DialogImage				GetDialogImageByEnum()	{return IMAGE_NONE;}
	const char*				GetHelpAnchor()			{return "autoupdate.html";}	// STUB
	virtual BOOL			GetIsResizingButtons()	{return TRUE;}
	virtual BOOL			GetModality()			{return FALSE;}

	const uni_char*			GetOkText();
	OpInputAction*			GetOkAction();

	const uni_char*			GetCancelText();		// cancel means minimize
	OpInputAction*			GetCancelAction()		{return OP_NEW(OpInputAction, (OpInputAction::ACTION_MINIMIZE_AUTO_UPDATE_DIALOG));} // cancel means minimize

	void					OnInit();
	BOOL					OnInputAction(OpInputAction* action);
	void					OnCancel();

	// others
	OP_STATUS				SetContext(UpdateProgressContext* context);

	// AUM_Listener implementation ----------------------------------------------------------
	virtual void			OnUpToDate();
	virtual void			OnChecking();
	virtual void			OnUpdateAvailable(AvailableUpdateContext* context);
	virtual void			OnDownloading(UpdateProgressContext* context);
	virtual void			OnDownloadingFailed();
	virtual void			OnPreparing(UpdateProgressContext* context);
	virtual void			OnReadyToInstall(UpdateProgressContext* context);
	virtual void			OnError(UpdateErrorContext* context);

	virtual void			OnMinimizedStateChanged(BOOL minimized);

	virtual void			OnDownloadingDone(UpdateProgressContext* context) {}
	virtual void			OnFinishedPreparing() {}

private:
	void					RefreshButtons();
	void					SetDownloadingContext(UpdateProgressContext* context);
	void					SetPreparingContext(UpdateProgressContext* context);
	void					SetReadyContext(UpdateProgressContext* context);
	void					AddUACShieldIfNeeded();

private:
	// button strings
	OpString				m_cancel_text;
	OpString				m_minimize_text;
	OpString				m_downloading_info_text;

	// widgets that need to be updated
	OpProgressBar*			m_progress;
	OpLabel*				m_summary;
	OpLabel*				m_info;

	BOOL					m_ready_for_install;
};

#endif //AUTO_UPDATE_DIALOG_H
