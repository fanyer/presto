/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef SIMPLEDIALOG_H
# define SIMPLEDIALOG_H

# include "adjunct/quick_toolkit/widgets/Dialog.h"
# include "adjunct/quick/Application.h"
# include "adjunct/quick/data_types/OpenURLSetting.h"
# include "modules/windowcommander/OpWindowCommander.h"

class DocumentDesktopWindow;

/***********************************************************************************
**
**  SimpleDialog
**
***********************************************************************************/
class OpenURLSetting;

class SimpleDialog : public Dialog
{
	INT32				m_default_button_index;
	public:

						SimpleDialog() : m_inited(FALSE), m_ok_text_id(0), m_cancel_text_id(0), m_protection_type(Default) {}
	virtual				~SimpleDialog() {}

    virtual OP_STATUS	Init(const OpStringC8& specific_name, const OpStringC& title, const OpStringC& message,
								DesktopWindow* parent_window = NULL,
								DialogType dialog_type = TYPE_OK_CANCEL,
								DialogImage dialog_image = IMAGE_WARNING,
								BOOL is_blocking = FALSE,
								INT32* result = NULL,
								BOOL* do_not_show_again = NULL,
								const char* help_anchor = NULL,
								int default_button_index = 0,
								OpBrowserView *parent_browser_view = NULL);

	/** Helper functions to easily create dialogs without needing a full object */
	static OP_STATUS ShowDialog(const OpStringC8& specific_name, DesktopWindow* parent_window, const uni_char* message, 
								const uni_char* title, Dialog::DialogType dialog_type, Dialog::DialogImage dialog_image, 
								DialogListener * dialog_listener, BOOL* do_not_show_again = NULL, const char* help_anchor = NULL, 
								INT32 default_button = 0);

	static OP_STATUS ShowDialog(const OpStringC8& specific_name, DesktopWindow* parent_window, Str::LocaleString message_id, 
								Str::LocaleString title_id, Dialog::DialogType dialog_type, Dialog::DialogImage dialog_image, 
								DialogListener * dialog_listener, BOOL* do_not_show_again = NULL, const char* help_anchor = NULL);

	static OP_STATUS ShowDialogFormatted(const OpStringC8& specific_name, DesktopWindow* parent_window, const uni_char *message, 
										 const uni_char* title, Dialog::DialogType dialog_type, Dialog::DialogImage dialog_image, 
										 DialogListener * dialog_listener, BOOL* do_not_show_again, ... );

	/** 
	 * Helper functions to easily create dialogs without needing a full object. To be deprecated/removed. 
	 * Don't use blocking dialogs, they don't close on 10.5 due to changed message loop handling. 
	 * Use above functions and DialogListeners instead. 
	 */
	static INT32 ShowDialog(const OpStringC8& specific_name, DesktopWindow* parent_window, const uni_char* message, 
							const uni_char* title, Dialog::DialogType dialog_type, Dialog::DialogImage dialog_image, 
							BOOL* do_not_show_again = NULL, const char* help_anchor = NULL, INT32 default_button = 0);

	static INT32 ShowDialog(const OpStringC8& specific_name, DesktopWindow* parent_window, Str::LocaleString message_id, 
							Str::LocaleString title_id, Dialog::DialogType dialog_type, Dialog::DialogImage dialog_image, 
							BOOL* do_not_show_again = NULL, const char* help_anchor = NULL);

	static INT32 ShowDialogFormatted(const OpStringC8& specific_name, DesktopWindow* parent_window, const uni_char *message, 
									 const uni_char* title, Dialog::DialogType dialog_type, Dialog::DialogImage dialog_image, 
									 BOOL* do_not_show_again, ...);

	Type				GetType()				{return DIALOG_TYPE_SIMPLE;}
	virtual const char* GetSpecificWindowName();
	virtual const char*	GetWindowName()         {return "Simple Dialog";}
	virtual DialogType	GetDialogType()			{return m_dialog_type;}
	virtual DialogImage	GetDialogImageByEnum()	{return m_dialog_image;}
	virtual BOOL		GetIsBlocking()			{return m_is_blocking;}
	virtual BOOL		GetDoNotShowAgain()		{return m_do_not_show_again != NULL;}
	virtual	const char*	GetHelpAnchor()			{return m_help_anchor;}

	virtual Str::LocaleString	GetOkTextID();
	OP_STATUS					SetOkTextID(Str::LocaleString ok_text_id);
	virtual Str::LocaleString	GetCancelTextID();
	OP_STATUS					SetCancelTextID(Str::LocaleString cancel_text_id);

	enum DoubleClickProtectionType
	{
		ForceOn,	// GetProtectAgainstDoubleClick() will return TRUE
		ForceOff,	// GetProtectAgainstDoubleClick() will return FALSE
		Default		//< use Dialog's default implementation of double-click protection
	};

	virtual BOOL		GetProtectAgainstDoubleClick();
	/**
	 **	Allows you to override the default doulbe-click protection done by Dialog, where every
	 ** (ie. two-button) dialog with a warning image is double-click protected.
	 */
	virtual void		SetProtectAgainstDoubleClick(DoubleClickProtectionType type);

	virtual BOOL		HasCenteredButtons() {return !GetDoNotShowAgain() && (!GetHelpAnchor() || !*GetHelpAnchor());}

	virtual BOOL		IsScalable()			{return FALSE;}

	virtual void		OnReset();
	virtual void		OnInit();

	virtual UINT32		OnOk();
	virtual void		OnClose(BOOL user_initiated);
	BOOL                OnInputAction(OpInputAction* action);

private:

	DialogType			m_dialog_type;
	DialogImage			m_dialog_image;
	INT32*				m_result;
	const uni_char*		m_title;
	const uni_char*		m_message;
	const char*			m_help_anchor;
	BOOL				m_is_blocking;
	BOOL*				m_do_not_show_again;
	BOOL				m_inited;
	int					m_ok_text_id;
	int					m_cancel_text_id;
	DoubleClickProtectionType m_protection_type;
	OpString8           m_specific_dialog_name;
};

class SkinVersionError : public SimpleDialog
{
	public:
		OP_STATUS Init(DesktopWindow* parent_window);
		UINT32 OnOk() {	return 0; }
};

class SetupVersionError : public SimpleDialog
{
	public:
		OP_STATUS Init(DesktopWindow* parent_window);
		UINT32 OnOk() { return 0; }
};

class AskAboutFormRedirectDialog : public SimpleDialog
{		OpDocumentListener::DialogCallback* callback;
	public:
		AskAboutFormRedirectDialog(OpDocumentListener::DialogCallback * callback) : callback(callback) {}
		virtual		~AskAboutFormRedirectDialog();
		OP_STATUS	Init(DesktopWindow* parent_window, const uni_char * source_url, const uni_char * target_url);
		BOOL		HideWhenDesktopWindowInActive() { return TRUE; }
		UINT32		OnOk();
		void		OnCancel();
};

class FormRequestTimeoutDialog: public SimpleDialog
{		OpDocumentListener::DialogCallback*	callback;
	public:
		FormRequestTimeoutDialog(OpDocumentListener::DialogCallback * callback) : callback(callback) {}
		virtual		~FormRequestTimeoutDialog();
		OP_STATUS	Init(DesktopWindow * parent_window, const uni_char * url);
		BOOL		HideWhenDesktopWindowInActive() { return TRUE; }
		UINT32		OnOk();
		void		OnCancel();
};

# ifdef QUICK_USE_DEFAULT_BROWSER_DIALOG
class DefaultBrowserDialog : public SimpleDialog
{
		BOOL		m_do_not_show_again;
	public:
		OP_STATUS	Init(DesktopWindow* parent_window);
		UINT32		OnOk();
		void		OnCancel();
};
# endif // QUICK_USE_DEFAULT_BROWSER_DIALOG
#endif //SIMPLEDIALOG_H
