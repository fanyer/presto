/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifdef AUTO_UPDATE_SUPPORT

#ifndef OP_MINIMIZED_UPDATE_DIALOG_BUTTON_H
#define OP_MINIMIZED_UPDATE_DIALOG_BUTTON_H

#include "adjunct/quick/managers/AutoUpdateManager.h"
#include "modules/widgets/OpButton.h"

class OpProgressBar;

/***********************************************************************************
 *
 *  @class OpMinimizedUpdateDialogButton
 *
 *	@brief Toolbar button that shows auto-update status in 'minimized mode'
 *  @author Manuela Hutter
 *
 *  Toolbar button, should be added in toolbar.ini as 'MinimizedUpdate'. 
 *  It listenes to changes through the AUM_Listener interface. It has no text/action
 *  set by default, and will only show itself when OnMinimizedStateChanged(TRUE)
 *  is called.
 *  It doesn't show text when up-to-date or while checking for updates.
 *  While being configured (Appearance->Toolbars), it does show a default text in any
 *  case. While in progress, a progress bar is shown with the status text on top of it.
 *
 *  If active (m_minimized == TRUE), it has ACTION_RESTORE_AUTO_UPDATE_DIALOG set as
 *  its action. Like all AUM_Listeners, it is managed by AutoUpdateManager.
 * 
 *  @see OpToolbar::OnReadContent
 *  @see AutoUpdateManager::AUM_Listener
 *  @see AutoUpdateManager
 *
 ***********************************************************************************/
class OpMinimizedUpdateDialogButton : public OpButton
	, public AutoUpdateManager::AUM_Listener
{
public:
	static OP_STATUS Construct(OpMinimizedUpdateDialogButton** obj);

protected:
	// Constructor / Destructor ----------------------------------------------------
	OpMinimizedUpdateDialogButton();
	~OpMinimizedUpdateDialogButton() {}

	// OpWidget implementation -----------------------------------------------------
	virtual void OnAdded();		//< Adds itself to AutoUpdateManager's listeners
	virtual void OnRemoved();	//< Removes itself to AutoUpdateManager's listeners

	virtual void OnDragStart(const OpPoint& point);

	virtual void OnLayout();	//< Layouts progressbar and takes care of the customizing state
	virtual void OnBeforePaint();
	
	// AUM_Listener implementation -------------------------------------------------
	virtual void OnUpToDate();
	virtual void OnChecking();
	virtual void OnUpdateAvailable(AvailableUpdateContext* context);
	virtual void OnDownloading(UpdateProgressContext* context);
	virtual void OnPreparing(UpdateProgressContext* context);
	virtual void OnReadyToInstall(UpdateProgressContext* context);
	virtual void OnError(UpdateErrorContext* context);
	virtual void OnMinimizedStateChanged(BOOL minimized);
	virtual void OnDownloadingDone(UpdateProgressContext* context) {}
	virtual void OnDownloadingFailed() {}
	virtual void OnFinishedPreparing() {}

	// OpWidgetListener implementation
	virtual void OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);

	virtual Type GetType() { return WIDGET_TYPE_MINIMIZED_AUTOUPDATE_STATUS_BUTTON; }
	
	virtual void GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);
	
private:
	void	SetUpdateMinimized(BOOL minimized);			//< Shows/hides the toolbar button
	void	Clear();									//< Clears the button - workaround for SetVisible(FALSE)
	void	SetHasProgress(BOOL has_progress);			//< Sets if the progress bar needs to be shown or not
	void	UpdateText(const uni_char* text = NULL);	//< Updates the text on the button
	void	UpdateText(Str::LocaleString str);			//< Updates the text on the button based on local string
	BOOL	HandleCustomizingMode();					//< Present different text if the toolbar is currently being customized

private:
	OpString		m_text_str;				//< Text on the button
	BOOL			m_minimized;			//< if the AutoUpdate process is minimized (this button is visible) or not
	OpProgressBar*	m_progress;				//< Progress bar on the button
	BOOL			m_has_progress;			//< If in a state where the progress bar is needed or not
	BOOL			m_in_customizing_mode;	//< If button is currently being customized

};

#endif // OP_MINIMIZED_UPDATE_DIALOG_BUTTON_H

#endif // AUTO_UPDATE_SUPPORT
