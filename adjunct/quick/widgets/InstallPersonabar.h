/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */
#ifndef PERSONA_INSTALLBAR_H
#define PERSONA_INSTALLBAR_H

#include "adjunct/quick/widgets/OpInfobar.h"
#include "adjunct/quick/managers/opsetupmanager.h"
#include "modules/windowcommander/src/TransferManager.h"
#include "modules/skin/OpSkinManager.h"
#include "adjunct/quick/dialogs/SetupApplyDialog.h"

class OpTransferItem;
class URL;

/***********************************************************************************
 **
 **	InstallPersonaBar
 **
 ***********************************************************************************/

class InstallPersonaBar : public OpInfobar, public OpTransferListener, public OpDelayedTriggerListener, public SetupApplyListener
{
protected:

	InstallPersonaBar(PrefsCollectionUI::integerpref prefs_setting = PrefsCollectionUI::DummyLastIntegerPref, PrefsCollectionUI::integerpref autoalignment_prefs_setting = PrefsCollectionUI::DummyLastIntegerPref);

public:

	static OP_STATUS Construct(InstallPersonaBar** obj);

	virtual ~InstallPersonaBar();
		
	virtual BOOL OnInputAction(OpInputAction* action);

	OP_STATUS SetURLInfo(const OpStringC & full_url, URLContentType content_type);

	// OpTransferListener
	void OnProgress(OpTransferItem* transferItem, TransferStatus status);
	void OnReset(OpTransferItem* transferItem) {   }
	void OnRedirect(OpTransferItem* transferItem, URL* redirect_from, URL* redirect_to);
	void SetTransferItem(OpTransferItem * transferItem);

	// OpDelayedTriggerListener
	virtual void OnTrigger(OpDelayedTrigger*);

	// SetupApplyListener
	void				OnSetupCancelled();
	void				OnSetupAccepted();

	virtual const char* GetStatusFieldName() { return "tbl_SkinInstallStatus"; }
	const char* GetProgressFieldName() { return "tbp_skin_progress"; }

private:
	OP_STATUS StartDownloading();
	OP_STATUS SaveDownload(OpStringC directory, OpString& setupfilename);
	void UndoSkinInstall();
	void RegularSetup();
	void AbortSetup();
	BOOL IsAThreat(OpTransferItem* transferItem);
	void UpdateVisibility(BOOL show_progress);
	void UpdateProgress(OpFileLength len, OpFileLength total);

private:
	OpString						m_setupfilename;
	OpString						m_urlstring;
	OpSetupType						m_setuptype;
#ifdef PERSONA_SKIN_SUPPORT
	OpFile							*m_old_persona_file;
#endif // PERSONA_SKIN_SUPPORT
	OpFile							*m_old_skin_file;
	OpTransferItem*					m_transfer;
	URL*							m_url;
	OpSkinManager::SkinFileType		m_skin_type;			// what skin type we installed
	OpDelayedTrigger				m_delayed_trigger;
};

#endif // PERSONA_INSTALLBAR_H
