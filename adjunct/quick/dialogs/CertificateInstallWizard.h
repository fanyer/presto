/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Marius Blomli
 */

#ifndef __CERTIFICATE_INSTALL_DIALOG_H__
#define __CERTIFICATE_INSTALL_DIALOG_H__

/**
 * @file CertificateInstallWizard.h
 *
 * Dialog used as the UI for installing certificates, both CAs and client certificates.
 *
 * Replaces the "Install" mode of the old CertificateInstallDialog. This dialog is also
 * used for client certificate selection.
 */

#include "adjunct/quick_toolkit/widgets/Dialog.h"

#include "modules/libssl/sslv3.h"
#include "modules/libssl/sslcctx.h"
#include "modules/windowcommander/OpWindowCommander.h"

/**
 * Should be: Wizard dialog used to install certificates as defined in the spec.
 *
 * Reality: (Until the new security ui gets fully implemented)
 * Replacement for client certificate/CA installation/selection mode of 
 * CertificateInstallDialog, which is now turning into more of a 
 * certificate error dialog, and can't support this mode any more.
 * In the disentanglement phase this dialog will not be the wizard it is 
 * supposed to become, and it will cover more cases than it should.
 * 
 * This dialog signals that the user made his selection through a callback 
 * in the supplied context.
 */
class CertificateInstallWizard : public Dialog
{
private:
	/**
	 * The display context that holds all information about the certificates, as well as
	 * the callback to use when done.
	 */
//	OpSSLListener::SSLCertificateContext* m_context;
	SSL_Certificate_DisplayContext*	m_context;
	/**
	 * The dialog/wizard can be for either cert selection or installation based 
	 * on what's in the display context.
	 *
	 * This is needed because the dialog covers the two separate cases installing a certificate
	 * and selecting a client certificate to send. It's not supposed to be this way, this is part
	 * of disentangeling the uni-dialog approach where one dialog covers everything.
	 *
	 * When certificate selection is separated into a different dialog, this enum probably
	 * loses its relevance.
	 */
	enum CertInstallWizType
	{
		SELECT_CERT,
		INSTALL_CERT
	};
	/**
	 * Type of the dialog.
	 */
	CertInstallWizType m_type;
	/**
	 * Options available to the return callback.
	 */
	OpSSLListener::SSLCertificateOption m_options;
	/**
	 * Name of server requesting cert/sending cert.
	 */
	OpString m_domain_name;
public:
	/**
	 * Constructor. 
	 *
	 * @param context the display context with all the information needed to populate the wizard.
	 * @param options the options that are allowed in the callback to the context when the user
	 * made his choice.
	 */
						CertificateInstallWizard(OpSSLListener::SSLCertificateContext* context, OpSSLListener::SSLCertificateOption options);

	// These functions are more or less copied from the old CertificateInstallDialog,
	// but with simplifications.
	Type				GetType()						{ return DIALOG_TYPE_INSTALL_CERT_WIZARD; }
	const char*			GetWindowName()					{ return "Certificate Install Wizard"; }
	const char*			GetHelpAnchor()					{ return "certificates.html";}
//	BOOL				GetShowServerNameLabel()		{ return TRUE; }
	BOOL				GetIsBlocking()					{ return FALSE; }
	BOOL				GetProtectAgainstDoubleClick()	{ return TRUE; }
	BOOL				OnInputAction(OpInputAction* action);
	INT32				GetButtonCount();
	void				GetButtonInfo(INT32 index, OpInputAction*& action, OpString& text, BOOL& is_enabled, BOOL& is_default, OpString8& name);
	DialogImage			GetDialogImageByEnum();
	void				OnInit();
	void				OnClick(OpWidget *widget, UINT32 id);
	void				OnChange(OpWidget *widget, BOOL changed_by_mouse);
	UINT32				OnOk();
	void				OnCancel();

	BOOL				HideWhenDesktopWindowInActive() { return m_type == SELECT_CERT; }

};

#endif // __CERTIFICATE_INSTALL_DIALOG_H__
