/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef NEWACCOUNTWIZARD_H
# define NEWACCOUNTWIZARD_H

# include "adjunct/m2/src/include/defs.h"
# include "adjunct/m2/src/engine/listeners.h"
# include "adjunct/desktop_pi/desktop_file_chooser.h"
# include "adjunct/desktop_util/mail/mailto.h"
# include "adjunct/quick_toolkit/widgets/Dialog.h"
# include "adjunct/quick/widgets/DesktopFileChooserEdit.h"

class AccountCreator;

/***********************************************************************************
**
**	NewAccountWizard
**
***********************************************************************************/

class NewAccountWizard : public Dialog, EngineListener, DesktopFileChooserListener
{
	public:

								NewAccountWizard();
		virtual					~NewAccountWizard();

		void					Init(AccountTypes::AccountType type, DesktopWindow* parent_window, const OpStringC& incoming_dropdown_value = OpStringC());
		void					SetMailToInfo(const MailTo& mailto, BOOL force_background, BOOL new_window, const OpStringC *attachment);

		DialogType				GetDialogType()			{return TYPE_WIZARD;}
		Type					GetType()				{return DIALOG_TYPE_NEW_ACCOUNT;}
		const char*				GetWindowName()			{return "New Account Wizard";}
		BOOL					GetModality()			{return TRUE;}
		const char*				GetHelpAnchor()			{return m_type == AccountTypes::IMPORT ? "mail.html#import" : "mail.html#new";}

		virtual BOOL			IsLastPage(INT32 page_number);

		virtual UINT32			OnForward(UINT32 page_number);
		virtual void			OnInit();
		virtual void			OnInitPage(INT32 page_number, BOOL first_time);
		virtual UINT32			OnOk();
		virtual void			OnCancel();
		virtual void			OnClick(OpWidget *widget, UINT32 id);
		virtual void			OnChange(OpWidget *widget, BOOL changed_by_mouse);

		// Implementing the MessageEngine listener interfaces

		void					OnProgressChanged(const ProgressInfo& progress, const OpStringC& progress_text) {}
		void					OnImporterProgressChanged(const Importer* importer, const OpStringC& infoMsg, OpFileLength current, OpFileLength total, BOOL simple = FALSE);
		void					OnImporterFinished(const Importer* importer, const OpStringC& infoMsg);
		void					OnIndexChanged(UINT32 index_id) {}
		void					OnIndexRemoved(UINT32 index_id) {}
		void					OnActiveAccountChanged() {}
		void					OnReindexingProgressChanged(INT32 progress, INT32 total) {}
		void					OnNewMailArrived() {}

		// Implementing the OpFileChooserListener interface
		void					OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result);

		void					OnCancelled() {  }

		// Implementing OpWidgetListener interface

		void					OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);

	private:

		BOOL 						m_modality;
		AccountTypes::AccountType	m_type;

		void						GuessFields(BOOL guess_userconfig = TRUE);
		void						DoMailImport();
		void						InitImportFields(BOOL all);
		void						PopulateMoveToSent(OpTreeView* accountTreeView);
		void						GetServerInfo(BOOL incoming, OpString& host, unsigned& port, OpString& account_name);
		void						UpdateUIAfterAccountCreation();

		AccountTypes::AccountType	m_init_type;
		Importer*					m_importer_object;
		EngineTypes::ImporterId		m_import_type;
		OpString					m_import_path;

		BOOL						m_should_show_subscribe_dialog;
		OpString					m_incoming_dropdown_value;

		AccountCreator*				m_account_creator;
		BOOL						m_found_provider;
		BOOL						m_has_guessed_fields;

		DesktopFileChooserRequest	m_request;
		DesktopFileChooser*			m_chooser;

		MailTo						m_mailto;
		BOOL						m_mailto_force_background;
		BOOL						m_mailto_new_window;
		OpString					m_mailto_attachment;
		BOOL						m_has_mailto;
		BOOL						m_reset_mail_panel;
};

#endif //NEWACCOUNTWIZARD_H
