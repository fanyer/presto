/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 */

#ifndef EXTENSION_SPEED_DIAL_HANDLER_H
#define EXTENSION_SPEED_DIAL_HANDLER_H

#include "adjunct/quick/models/ExtensionsModel.h"
#include "adjunct/quick/speeddial/SpeedDialListener.h"
#include "adjunct/quick/extensions/ExtensionInstaller.h"

class OpInputAction;

class ExtensionSpeedDialHandler : public ExtensionsModel::Listener,
								  public SpeedDialListener,
								  public ExtensionInstaller::Listener
{
public:
	ExtensionSpeedDialHandler();
	
	//
	// ExtensionModel::Listener
	//
	void OnBeforeExtensionsModelRefresh(const ExtensionsModel::RefreshInfo& info) {}
	void OnAfterExtensionsModelRefresh() {}
	void OnNormalExtensionAdded(const ExtensionsModelItem& model_item) {}
	void OnDeveloperExtensionAdded(const ExtensionsModelItem& model_item) {}
	void OnExtensionUninstall(const ExtensionsModelItem& model_item) {}
	void OnExtensionEnabled(const ExtensionsModelItem& model_item);
	void OnExtensionDisabled(const ExtensionsModelItem& model_item);
	void OnExtensionUpdateAvailable(const ExtensionsModelItem& model_item) {}
	void OnExtensionExtendedNameUpdate(const ExtensionsModelItem& model_item){}
													

	//
	// SpeedDialListener
	//
	void OnSpeedDialAdded(const DesktopSpeedDial& entry);
	void OnSpeedDialRemoving(const DesktopSpeedDial& entry);
	void OnSpeedDialReplaced(const DesktopSpeedDial& old_entry, const DesktopSpeedDial& new_entry);
	void OnSpeedDialMoved(const DesktopSpeedDial& from_entry, const DesktopSpeedDial& to_entry) {}
	void OnSpeedDialPartnerIDAdded(const uni_char* partner_id) {}
	void OnSpeedDialPartnerIDDeleted(const uni_char* partner_id) {}
	void OnSpeedDialsLoaded(){}

	//
	// ExtensionInstaller::Listener
	//
	virtual void OnExtensionDownloadStarted(ExtensionInstaller&) {}
	virtual void OnExtensionDownloadProgress(OpFileLength, OpFileLength) {}
	virtual void OnExtensionDownloadFinished() {}
	virtual void OnExtensionInstalled(const OpGadget& extension,const OpStringC& source);
	virtual void OnExtensionInstallAborted() {}
	virtual void OnExtensionInstallFailed(){};

	BOOL HandleAction(OpInputAction* action);

	/**
	 * Adds extension's speed dial represented by instance of class gadget
	 * to the end of all speed dials.
	 *
	 * @parameter gadget gadget which represents extension's speed dial
	 * @return status
	 */
	OP_STATUS AddSpeedDial(OpGadget& gadget);

	static void AnimateInSpeedDial(const OpGadget& gadget);

private:
	struct AutoLock
	{
		explicit AutoLock(bool& lock) : m_lock(lock) { m_lock = true; }
		~AutoLock() { m_lock = false; }
		bool& m_lock;
	};

	/** @return index into SpeedDialManager, or @c -1 if not found */
	static INT32 FindSpeedDial(const OpGadget& extension);

	static bool ShouldShowInSpeedDial(const OpGadget& extension);

	static void ShowNewExtension(const OpGadget& extension);

	bool m_lock;
};

#endif // EXTENSION_SPEED_DIAL_HANDLER_H
