/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA. All rights reserved.
 *
 * This file is part of the Opera web browser. It may not be distributed
 * under any circumstances.
 */
#ifndef DOWNLOAD_EXTENSION_BAR_H
#define DOWNLOAD_EXTENSION_BAR_H

#include "adjunct/quick/widgets/OpInfobar.h"

#include "adjunct/quick/extensions/ExtensionInstaller.h"

/** Toolbar class responsible for displaying extension download progress. */
class DownloadExtensionBar : public OpInfobar,
	public ExtensionInstaller::Listener
{
public:

	static OP_STATUS Construct(DownloadExtensionBar** obj);
	virtual ~DownloadExtensionBar();

	virtual BOOL OnInputAction(OpInputAction* action);

	virtual const char* GetStatusFieldName() { return "tbl_ExtensionDownloadStatus"; }
	const char* GetProgressFieldName() { return "tbp_extension_download_progress"; }

	// ExtensionInstaller::Listener
	/**
	 * Shows toolbar and registers DownloadExtensionBarListener.
	 * @param extension_installer will be registered as this object listener.
	 */
	virtual void OnExtensionDownloadStarted(ExtensionInstaller& extension_installer);

	/**
	 * Updates progress of extension download.
	 * @param loaded size of already downloaded extension data.
	 * @param total total size of extension.
	 */
	virtual void OnExtensionDownloadProgress(OpFileLength loaded, OpFileLength total);

	/**
	 * Hides toolbar and unregisters DownloadExtensionBarListener.
	 */
	virtual void OnExtensionDownloadFinished();

	virtual void OnExtensionInstalled(const OpGadget& extension,const OpStringC& source) { OnExtensionDownloadFinished(); }
	virtual void OnExtensionInstallAborted() { OnExtensionDownloadFinished(); }
	virtual void OnExtensionInstallFailed() { OnExtensionDownloadFinished(); }

private:
	DownloadExtensionBar() :
		OpInfobar(PrefsCollectionUI::DummyLastIntegerPref,
		          PrefsCollectionUI::DummyLastIntegerPref),
		m_extension_installer(NULL) {}

	ExtensionInstaller* m_extension_installer;
};

#endif // DOWNLOAD_EXTENSION_BAR_H
