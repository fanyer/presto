/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef JSDIALOGS_H
# define JSDIALOGS_H

#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "modules/locale/locale-enum.h"
#include "modules/widgets/OpMultiEdit.h"
#include "adjunct/quick/managers/KioskManager.h"

const uni_char* FormatJSDialogTitle(DesktopWindow* parent_window);

/***********************************************************************************
**
**	JSAlertDialog
**
***********************************************************************************/

class JSAlertDialog : public SimpleDialog
{
	public:

		JSAlertDialog()
			: m_show_disable_checkbox(TRUE)
		{

		}
		void Init(DesktopWindow* parent_window,	const uni_char* message, OpDocumentListener::JSDialogCallback* callback, OpWindowCommander* commander, OpBrowserView *parent_browser_view);

		void SetShowDisableScriptOption(BOOL show)
		{
			m_show_disable_checkbox = show;
		}

		Str::LocaleString GetDoNotShowAgainTextID() {return Str::SI_IDCANCELSCRIPTS;}
		BOOL GetDoNotShowAgain() {return m_show_disable_checkbox;}

		void OnClose(BOOL user_initiated);

#ifdef VEGA_OPPAINTER_SUPPORT
		virtual BOOL			GetModality() {return FALSE;}
		virtual BOOL			GetOverlayed() {return TRUE;}
		virtual BOOL			GetDimPage() {return TRUE;}
#endif

		// DocumentWindowListener
		virtual void	OnUrlChanged(DocumentDesktopWindow* document_window, const uni_char* url) { CloseDialog(TRUE, FALSE, FALSE); }
		virtual void	OnLoadingFinished(DocumentDesktopWindow* document_window, OpLoadingListener::LoadingFinishStatus, BOOL was_stopped_by_user) 
		{
			if (was_stopped_by_user)
				CloseDialog(TRUE, FALSE, FALSE);
		}

	private:

		OpDocumentListener::JSDialogCallback* m_callback;
		OpWindowCommander* m_window_commander;
		BOOL m_show_disable_checkbox;
};

/***********************************************************************************
**
**	JSConfirmDialog
**
***********************************************************************************/

class JSConfirmDialog : public SimpleDialog
{
	public:
		JSConfirmDialog()
			: m_show_disable_checkbox(TRUE)
		{

		}

		void Init(DesktopWindow* parent_window, const uni_char* message, OpDocumentListener::JSDialogCallback* callback, OpWindowCommander* commander, OpBrowserView *parent_browser_view);

		void SetShowDisableScriptOption(BOOL show)
		{
			m_show_disable_checkbox = show;
		}

		Str::LocaleString GetDoNotShowAgainTextID() {return Str::SI_IDCANCELSCRIPTS;}
		BOOL GetDoNotShowAgain() {return m_show_disable_checkbox;}

		UINT32 OnOk();
		void OnCancel();

#ifdef VEGA_OPPAINTER_SUPPORT
		virtual BOOL			GetModality() {return FALSE;}
		virtual BOOL			GetOverlayed() {return TRUE;}
		virtual BOOL			GetDimPage() {return TRUE;}
#endif

		// DocumentWindowListener
		virtual void	OnUrlChanged(DocumentDesktopWindow* document_window, const uni_char* url) { CloseDialog(TRUE, FALSE, FALSE); }
		virtual void	OnLoadingFinished(DocumentDesktopWindow* document_window, OpLoadingListener::LoadingFinishStatus, BOOL was_stopped_by_user) 
		{
			if (was_stopped_by_user)
				CloseDialog(TRUE, FALSE, FALSE);
		}

	private:

		OpDocumentListener::JSDialogCallback* m_callback;
		OpWindowCommander* m_window_commander;
		BOOL m_show_disable_checkbox;
};

/***********************************************************************************
**
**	JSPromptDialog
**
***********************************************************************************/

class JSPromptDialog : public Dialog
{
	public:

		Type					GetType()				{return DIALOG_TYPE_ASKTEXT;}
		const char*				GetWindowName()			{return "JavaScript Prompt Dialog";}
		DialogImage				GetDialogImageByEnum()	{return IMAGE_QUESTION;}

		Str::LocaleString GetDoNotShowAgainTextID() {return Str::SI_IDCANCELSCRIPTS;}
		BOOL GetDoNotShowAgain() {return m_show_disable_checkbox;}

		JSPromptDialog()
			: m_show_disable_checkbox(TRUE)
		{

		}

		void Init(DesktopWindow* parent_window, const uni_char* message, const uni_char* default_value, OpDocumentListener::JSDialogCallback* callback, OpWindowCommander* commander, OpBrowserView *parent_browser_view);

		void SetShowDisableScriptOption(BOOL show)
		{
			m_show_disable_checkbox = show;
		}

		void OnInit();
		void OnReset();
		UINT32 OnOk();
		void OnCancel();

#ifdef VEGA_OPPAINTER_SUPPORT
		virtual BOOL			GetModality() {return FALSE;}
		virtual BOOL			GetOverlayed() {return TRUE;}
		virtual BOOL			GetDimPage() {return TRUE;}
#endif

		// DocumentWindowListener
		virtual void	OnUrlChanged(DocumentDesktopWindow* document_window, const uni_char* url) { CloseDialog(TRUE, FALSE, FALSE); }
		virtual void	OnLoadingFinished(DocumentDesktopWindow* document_window, OpLoadingListener::LoadingFinishStatus, BOOL was_stopped_by_user) 
		{
			if (was_stopped_by_user)
				CloseDialog(TRUE, FALSE, FALSE);
		}

	private:

		OpDocumentListener::JSDialogCallback*	m_callback;
		OpWindowCommander*						m_window_commander;
		const uni_char*							m_message;
		const uni_char*							m_default_value;
		BOOL 									m_show_disable_checkbox;
};

#endif // JSDIALOGS_H
