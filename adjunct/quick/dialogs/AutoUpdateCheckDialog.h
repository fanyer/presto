/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 */

#ifndef AUTOUPDATE_CHECK_DIALOG_H
#define AUTOUPDATE_CHECK_DIALOG_H

#include "adjunct/quick/managers/AutoUpdateManager.h"
#include "adjunct/quick_toolkit/widgets/Dialog.h"


/***********************************************************************************
 *
 *  @class AutoUpdateCheckDialog
 *
 *	@brief	Dialog that informs about the progress of an ongoing check.
 *
 *
 ***********************************************************************************/
class AutoUpdateCheckDialog : public Dialog, public AutoUpdateManager::AUM_Listener
{
public:
							AutoUpdateCheckDialog();
							~AutoUpdateCheckDialog();


	///--------------------- Dialog ---------------------///
	Type					GetType()				{return DIALOG_TYPE_AUTOUPDATE_CHECK;}
	const char*				GetWindowName()			{return "Auto Update Check Dialog";}
	virtual BOOL			GetIsResizingButtons()	{return TRUE;}

	const uni_char*			GetOkText();
	OpInputAction*			GetOkAction();

	const uni_char*			GetCancelText();		// cancel means minimize
	OpInputAction*			GetCancelAction()		{return OP_NEW(OpInputAction, (OpInputAction::ACTION_MINIMIZE_AUTO_UPDATE_DIALOG));} // cancel means minimize

	void					OnInit();
	BOOL					OnInputAction(OpInputAction* action);
	void					OnCancel();

	// AUM_Listener implementation ----------------------------------------------------------
	virtual void			OnUpToDate();
	virtual void			OnChecking();
	virtual void			OnUpdateAvailable(AvailableUpdateContext* context);
	virtual void			OnDownloading(UpdateProgressContext* context) {}
	virtual void			OnPreparing(UpdateProgressContext* context) {}
	virtual void			OnReadyToInstall(UpdateProgressContext* context) {}
	virtual void			OnError(UpdateErrorContext* context);

	virtual void			OnMinimizedStateChanged(BOOL minimized);


	virtual void			OnDownloadingDone(UpdateProgressContext* context) {}
	virtual void			OnDownloadingFailed() {}
	virtual void			OnFinishedPreparing() {}

private:
	OpString				m_cancel_text;
	OpString				m_minimize_text;
};

#endif //AUTOUPDATE_CHECK_DIALOG_H
