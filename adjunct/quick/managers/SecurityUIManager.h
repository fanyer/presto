/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Marius Blomli
 * George Refseth
 */

#ifndef __SECURITY_UI_MANAGER_H__
#define __SECURITY_UI_MANAGER_H__

#include "adjunct/quick/managers/DesktopManager.h"
#include "adjunct/quick_toolkit/contexts/DialogContextListener.h"
#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/quick_toolkit/widgets/DialogListener.h"
#include "modules/windowcommander/OpWindowCommander.h"

#define g_security_ui_manager (SecurityUIManager::GetInstance())

/**
 * Manager class for controlling ui elements relating to certificates,
 * passwords and other security related aspects of browsing. This class
 * should manage as much as possible of the interaction between quick and
 * the core security classes.
 *
 * Note: This manager does not replace all existing libssl-quick interfaces
 * at the moment, but it should be used to implement changes and
 * gradually take over from the previous systems. - mariusab 20070423
 *
 * @author mariusab@opera.com
 * @author rfz@opera.com
 */
class SecurityUIManager : public DesktopManager<SecurityUIManager>, public OpSSLListener
{
	void OnSecurityPasswordCancel(OpWindowCommander *,OpSSLListener::SSLSecurityPasswordCallback *) {}
	class SecurityDialogWrapper : public ListElement<SecurityDialogWrapper>, public DialogListener
	{
		OpWindowCommander *				m_windowcommander;
		Dialog*							m_dialog;
		SSLCertificateContext*			m_context;
		SSLSecurityPasswordCallback*	m_callback;

	public:
		SecurityDialogWrapper(OpWindowCommander * windowcommander, Dialog* dialog, SSLCertificateContext* context) :
			m_windowcommander(windowcommander),
			m_dialog(dialog),
			m_context(context),
			m_callback(NULL)
			{ if (m_dialog) m_dialog->SetDialogListener(this); }

		SecurityDialogWrapper(OpWindowCommander * windowcommander, Dialog* dialog, SSLSecurityPasswordCallback* callback) :
			m_windowcommander(windowcommander),
			m_dialog(dialog),
			m_context(NULL),
			m_callback(callback)
			{ if (m_dialog) m_dialog->SetDialogListener(this); }
		~SecurityDialogWrapper()
		{
			if (m_dialog)
				m_dialog->SetDialogListener(NULL);
		}

		BOOL Equals(OpWindowCommander * windowcommander, SSLCertificateContext * context)
		{
			return windowcommander == m_windowcommander && context == m_context;
		}

		BOOL Equals(OpWindowCommander * windowcommander, SSLSecurityPasswordCallback * callback)
		{
			return windowcommander == m_windowcommander && (callback == m_callback || !callback);
		}

		Dialog * GetDialog () { return m_dialog; }

		OpWindowCommander * GetWindowCommander() { return m_windowcommander; }
		
		void OnClose(Dialog * dialog)
		{
			if (dialog == m_dialog)
			{
				m_dialog->SetDialogListener(NULL);
				m_dialog = NULL;
				Out();
				OP_DELETE(this);
			}
		}
	};

	class UiControllerHolder : public ListElement<UiControllerHolder>, public DialogContextListener
	{
	public:
		UiControllerHolder(DialogContext& ui_controller, OpWindowCommander* window_commander, void* context);
		virtual ~UiControllerHolder();

		bool Matches(OpWindowCommander* window_commander, void* context) const
			{ return (window_commander == m_window_commander || window_commander == NULL) && (context == m_context || context == NULL); }

		OpWindowCommander* GetWindowCommander() const { return m_window_commander; }

		// DialogContextListener
		virtual void OnDialogClosing(DialogContext* ui_controller);

	private:
		DialogContext* m_ui_controller;
		OpWindowCommander* m_window_commander;
		void* m_context;
	};

	AutoDeleteList<SecurityDialogWrapper> certList;
	AutoDeleteList<SecurityDialogWrapper> passList;
	AutoDeleteList<UiControllerHolder> m_cert_ui_controllers;

	SecurityDialogWrapper * FindWrapper(OpWindowCommander * commander, SSLCertificateContext* context);
	SecurityDialogWrapper * FindWrapper(OpWindowCommander * commander, SSLSecurityPasswordCallback* callback = NULL);
	UiControllerHolder* FindUiController(OpWindowCommander* commander, SSLCertificateContext* context);
	
public:
	SecurityUIManager();
	~SecurityUIManager();
	OP_STATUS Init() { return OpStatus::OK; }

	/**
	 * OpSSLListener implementation
	 *
	 */
	void OnCertificateBrowsingNeeded(OpWindowCommander* wic, SSLCertificateContext* context, SSLCertificateReason reason, SSLCertificateOption options);
	/**
	 * OpSSLListener implementation.
	 *
	 */
	void OnSecurityPasswordNeeded(OpWindowCommander* wic, SSLSecurityPasswordCallback* callback);
	/**
	 * OpSSLListener implementation.
	 *
	 */
	void OnCertificateBrowsingCancel(OpWindowCommander* wic, SSLCertificateContext* context);
	/**
	 * OpSSLListener implementation.
	 *
	 */
	void OnSecurityPasswordCancel(OpWindowCommander* wic);

public:
	BOOL HasCertReqProcessed(OpWindowCommander *commander);

private:
	void UpdateDocumentIcon(OpWindowCommander *commander);
};

#endif // __SECURITY_UI_MANAGER_H__
