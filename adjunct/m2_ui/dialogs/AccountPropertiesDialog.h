/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef ACCOUNTPROPERTIES_H
# define ACCOUNTPROPERTIES_H

# include "adjunct/m2/src/include/defs.h"
# include "adjunct/quick_toolkit/widgets/Dialog.h"

class Account;

/*****************************************************************************
**
**	AccountPropertiesDialog
**
*****************************************************************************/

class AccountPropertiesDialog : public Dialog
{
	public:
			
								AccountPropertiesDialog(BOOL modality);
		virtual					~AccountPropertiesDialog() {};

		virtual OP_STATUS		Init(UINT16 account_id, DesktopWindow* parent_window);

		DialogType				GetDialogType()			{return TYPE_PROPERTIES;}
		Type					GetType()				{return DIALOG_TYPE_ACCOUNT_PROPERTIES;}
		const char*				GetWindowName()			{return "Account Properties Dialog";}
		BOOL					GetModality()			{return m_modality;}
		const char*				GetHelpAnchor()			{return "mail.html#accounts";}

		virtual UINT32			OnOk();
		virtual void			OnCancel();
		virtual void			OnClick(OpWidget *widget, UINT32 id = 0);
	    virtual void            OnChange(OpWidget *widget, BOOL changed_by_mouse = FALSE);
		virtual BOOL			OnInputAction(OpInputAction* action);

	protected:
		// Dialog override.
		virtual void			OnInitVisibility();
		virtual BOOL			GetShowPage(INT32 page_number);
	
	private:
		void PopulateAuthenticationDropdown(OpDropDown* dropdown, const Account* account, UINT32 supported_authentication, AccountTypes::AuthenticationType selected_method);
		int  GetOfflineFlags(Account* account) const;
		
		enum OfflineFlags
		{
			DOWNLOAD_BODY = 1 << 0,
			KEEP_BODY     = 1 << 1
		};

		BOOL m_modality;
		UINT16 m_account_id;
};

#endif //ACCOUNTPROPERTIES_H
