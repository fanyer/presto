/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- 
 * 
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved. 
 * 
 * This file is part of the Opera web browser.  It may not be distributed 
 * under any circumstances. 
 */ 

#ifndef OP_PLUGIMISSINGBAR_H
#define OP_PLUGIMISSINGBAR_H

#include "adjunct/quick/widgets/OpInfobar.h"
#include "adjunct/quick/managers/PluginInstallManager.h"
#include "adjunct/quick/windows/DocumentWindowListener.h"

#ifdef PLUGIN_AUTO_INSTALL

class OpLabel;

/**
 *  OpPluginMissingBar 
 *
 * Informs the user the current status of the missing plugin, allows to trigger actions for the current status, 
 * i.e. starting download of a plugin that is available for downloading.
 * The Plugin Missing Bar will change it's content depending on the current status of the plugin item pointed to
 * the mime-type that is assigned to it.
 *
 * There should be one Plugin Missing Bar per missing plugin (i.e. per mime-type) per window. The bars will in different
 * windows will automatically adapt to the current missing plugin item state.
 * DocumentDesktopWindow maintains the Plugin Missing Bar for itself on its own, creating them and deleting basing on the
 * currently shown page and callbacks from core.
 */

class OpPluginMissingBar:
	public OpInfobar,
	public DocumentWindowListener,
	public PIM_PluginInstallManagerListener
{
public:
	static OP_STATUS	Construct(OpPluginMissingBar** obj);

	/**
	 * Retrieve the mime-type that is tracked by this bar.
	 */
	const OpStringC& GetMimeType() const { return m_mime_type; }

	/**
	 * Set the mime-type that should be tracked with this Plugin Missing Bar.
	 * The Plugin Missing Bar willnot show at all if the mime-type is left empty.
	 *
	 * @param mime_type - the mime-type of the missing plugin to track.
	 * @return - OpStatus::OK if everything went well, ERR otherwise.
	 */
	OP_STATUS			SetMimeType(const OpStringC& mime_type);

	/**
	 * Schedule showing this Plugin Missing Bar when the document in the owning window finishes loading.
	 * Prevents showing the bar too early.
	 */
	void		ShowWhenLoaded();

	/**
	 * Hide the Plugin Missing Bar and focus the current page. None of these will happen if the bar is
	 * already hidden.
	 *
	 * @return - FALSE if hiding the bar failed or if it was already hidden, TRUE otherwise.
	 */
    BOOL		Hide();

	virtual void OnLayout(BOOL compute_size_only, INT32 available_width, INT32 available_height, INT32& used_width, INT32& used_height);

protected:
	OpPluginMissingBar();
	~OpPluginMissingBar();

	/**
	 * Show the Plugin Missing Bar, only succeeds if the mime-type was set earlier with SetMimeType().
	 *
	 * @return - TRUE if the bar was shown, FALSE if there was an error showing it or if the mime-type
	 *           is empty for this Plugin Missing Bar.
	 */
	BOOL		Show();

	/**
	 * Changes the content of this Plugin Missing Bar depending on the current state of the tracked missing
	 * plugin. Changes the strings, visibility of the buttons and the OpProgressField.
	 * To be used internally by this class. Note that this method may be called within the OnLayout() event
	 * for the toolbar, calling any methods causing a layout will in such case cause a loop with both methods 
	 * calling each other. To avoid that do not call any methods causing the toolbar to relayout when do_not_layout
	 * is set to TRUE.
	 *
	 * @param new_state - the new state of the tracked missing plugin item.
	 * @param do_not_layout - indicates that it is forbidden to call any methods causing the OnLayout() callback.
	 * @returns - OpStatus::ERR on error, OK otherwise.
	 */
	OP_STATUS			ConfigureCurrentLayout(PIM_ItemState new_state, bool do_not_layout = false);

	/**
	 * Implementation of PIM_PluginInstallManagerListener
	 */
	virtual void OnPluginResolved(const OpStringC& mime_type);
	virtual void OnPluginAvailable(const OpStringC& mime_type);
	virtual void OnPluginDownloaded(const OpStringC& mime_type);
	virtual void OnPluginDownloadStarted(const OpStringC& mime_type);
	virtual void OnPluginDownloadCancelled(const OpStringC& mime_type);
	virtual void OnPluginInstalledOK(const OpStringC& mime_type);
	virtual void OnPluginInstallFailed(const OpStringC& mime_type);
	virtual void OnPluginsRefreshed();
	virtual void OnFileDownloadProgress(const OpStringC& mime_type, OpFileLength total_size, OpFileLength downloaded_size, double kbps, unsigned long estimate);
	virtual void OnFileDownloadDone(const OpStringC& mime_type, OpFileLength total_size);
	virtual void OnFileDownloadFailed(const OpStringC& mime_type);
	virtual void OnFileDownloadAborted(const OpStringC& mime_type);
 
    /**
	 * Implementation of OpInputContext 
	 */
	virtual BOOL            OnInputAction(OpInputAction* action); 
	virtual const char*     GetInputContextName() { return "Plugin Missing Bar"; }

	/**
	 * Implementation of DocumentWindowListener
	 */
	void OnUrlChanged(DocumentDesktopWindow* document_window, const uni_char* url);
	void OnStartLoading(DocumentDesktopWindow* document_window);
	void OnLoadingFinished(DocumentDesktopWindow* document_window, OpLoadingListener::LoadingFinishStatus, BOOL was_stopped_by_user = FALSE);

private:
	OpString    m_mime_type;
	OpString	m_plugin_name;
	BOOL		m_is_loading;
	BOOL		m_show_when_loaded;
	double		m_show_time;
	PIM_ItemState	m_current_state;
};

#endif // PLUGIN_AUTO_INSTALL

#endif // OP_PLUGIMISSINGBAR_H 
