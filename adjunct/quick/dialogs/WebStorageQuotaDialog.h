/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Petter Nilsen (pettern)
 */

#ifndef WEBSTORAGE_QUOTA_DIALOG_H
#define WEBSTORAGE_QUOTA_DIALOG_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "modules/locale/locale-enum.h"

enum QuotaType
{
	WebStorageQuota,
	ApplicationCacheQuota
};

/***********************************************************************************
**  @class	WebStorageQuotaDialog
**	@brief	Shows web storage quota extension dialog to the user
**
************************************************************************************/
class WebStorageQuotaDialog : public Dialog
{
public:
	WebStorageQuotaDialog(const uni_char *server_name, OpFileLength current_limit, OpFileLength requested_limit, UINTPTR id = 0);

	void SetApplicationCacheCallback(OpApplicationCacheListener::QuotaCallback* callback);

	//===== Dialog implementations =====
	virtual Str::LocaleString	GetOkTextID()		{ return Str::D_WEBSTORAGE_QUOTA_DIALOG_ALLOW; } // todo: make const in Dialog
	virtual Str::LocaleString	GetCancelTextID()	{ return Str::D_WEBSTORAGE_QUOTA_DIALOG_REJECT; } // todo: make const in Dialog

	BOOL				GetModality()			{return FALSE;}
	BOOL				GetOverlayed()			{return TRUE;}
	BOOL				GetDimPage()			{return TRUE;}
	void				GetOverlayPlacement(OpRect& initial_placement, OpWidget* overlay_layout_widget);
	BOOL				HideWhenDesktopWindowInActive()	{ return TRUE; }

	virtual void				OnInit();
	virtual UINT32				OnOk();
	virtual void				OnCancel();	
	virtual void				OnClose(BOOL user_initiated);
	virtual BOOL				GetIsResizingButtons() { return TRUE; } // allow buttons to be wider if needed in other languages

	//===== DesktopWindow implementations =====
	virtual const char*			GetWindowName() {return "Web Storage Quota Dialog";}

	//===== OpInputContext implementations =====
	virtual Type				GetType()		{ return DIALOG_TYPE_WEBSTORAGE_QUOTA_DIALOG; }

	UINTPTR				GetApplicationCacheCallbackID() { return m_id; }
private:
	OpFileLength	m_current_limit;	// bytes
	OpFileLength	m_requested_limit;	// bytes
	OpString		m_server_name;

	QuotaType		m_quota_type;

	// Application cache
	UINTPTR m_id;
	OpApplicationCacheListener::QuotaCallback* m_callback;
};

#endif // WEBSTORAGE_QUOTA_DIALOG_H
