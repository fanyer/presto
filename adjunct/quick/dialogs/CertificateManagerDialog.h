/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef CERTMANAGERDIALOG_H
# define CERTMANAGERDIALOG_H

# include "adjunct/desktop_util/treemodel/simpletreemodel.h"
# include "adjunct/quick_toolkit/widgets/Dialog.h"
# include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"

# ifdef _SSL_SUPPORT_
#  include "modules/libssl/sslv3.h"
#  include "modules/libssl/sslopt.h"
#  include "modules/libssl/ssldlg.h"
#  include "modules/libssl/sslrand.h"
#  include "modules/libssl/sslcctx.h"
#  include "modules/windowcommander/src/SSLSecurtityPasswordCallbackImpl.h"
# endif

# include "modules/libssl/certinst_base.h"


struct SSL_cipher_param_st {
	WORD wSSLId;
	SSL_ProtocolVersion SSLID_ver;
	SSL_Options *optionsManager;
};


class CertificateManagerDialog;

/**
 * Helper class for the CertificateManagerDialog that 
 * encapsulates import and export functionality. It 
 * initiates the imports or exports, and registers itself
 * to recieve callbacks when the imports or exports finishes
 * if they're not done immediately.
 *
 * Use:
 *
 * Construct an instance with the parameters needed to initiate 
 * the import/export (see constructor docs). The class initiates 
 * the export in libssl. If it needs to wait for a callback, it 
 * does. When done, it notifies the owning CertificateManagerDialog
 * about it to update the ui. After it's done, it should be 
 * deleted, it can't be reused. If a new import/export needs to
 * be initated, a new instance needs to be created. If the owning
 * dialog can only manage one import/export at a time, it can ask
 * an existing callback if it's done with the IsInProgress 
 * function, and safely delete it if it is not, then create a
 * new one.
 *
 * The owning CertificateManagerDialog should not be deleted while
 * the import/export is in progress. Since the installer that's 
 * running can't be deleted, it must be leaked. Since that sucks, 
 * the dialog should defer closing until import concludes. This is
 * somewhat vulnerable to causing freezes. This is probably less bad 
 * than leaking or, worst of all, prematurely deleting objects.
 * 
 */
class CertImportExportCallback : public MessageObject
{
public:
	/** 
	 * Enumeration of the modes that this class can be initated with. Also used as message id's.
	 */
	enum Mode {
		IMPORT_PERSONAL_CERT_MODE = 1,
		IMPORT_AUTHORITIES_CERT_MODE,
		IMPORT_INTERMEDIATE_CERT_MODE,
		IMPORT_REMEMBERED_CERT_MODE,
		IMPORT_REMEMBERED_REFUSED_CERT_MODE,

		EXPORT_PERSONAL_CERT_MODE,
		EXPORT_AUTHORITIES_CERT_MODE,
		EXPORT_INTERMEDIATE_CERT_MODE,
		EXPORT_REMEMBERED_CERT_MODE,
		EXPORT_REMEMBERED_REFUSED_CERT_MODE
	};
	/**
	 * Constructor. Sets the callback with the main message handler.
	 *
	 * @param mode The import/export mode.
	 * @param window Pointer to the relevant OpWindow
	 * @param dialog the dialog that's creating this callback instance
	 * @param disp_ctx The relevant display context
	 * @param filename filename to export to/import from
	 * @param extension the extension to export to (only relevant for export, can be NULL for import.)
	 */
									CertImportExportCallback(Mode mode, OpWindow* window, CertificateManagerDialog* dialog, SSL_Certificate_DisplayContext* disp_ctx, const uni_char* filename, const uni_char* extension);
	/**
	 * Destructor. Makes sure that all objects owned by the callback are deleted, and that the callback
	 * is unset.
	 */
									~CertImportExportCallback();
	/**
	 * Get a pointer to the config;
	 */
	inline SSL_dialog_config*		GetDialogConfig() { return &m_config; }
	/**
	 * Check if the import export is done (callback arrived)
	 */
	inline BOOL						IsInProgress() { return m_in_progress; }
	/**
	 * MessageObject implementation, use for import/export callbacks.
	 */
	void							HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
private:
	Mode							m_mode;
	CertificateManagerDialog*		m_dialog;
	SSL_dialog_config				m_config;
	BOOL							m_in_progress;
	SSL_Certificate_Installer_Base*	m_installer;
	SSL_Certificate_Export_Base*	m_exporter;
};

// Defines for each page in the dialog
#define CMD_PERSONAL_PAGE					0
#define CMD_AUTHORITIES_PAGE				1
#define CMD_INTERMEDIATE_PAGE				2
#define CMD_REMEMBERED_CERT_PAGE			3
#define CMD_REMEMBERED_REFUSED_CERT_PAGE	4

class CertificateManagerDialog : public Dialog
{
private:

	SSL_Options*		m_ssloptions;			//global ssl options

	SSL_Certificate_DisplayContext* m_site_cert_context;
	SSL_Certificate_DisplayContext* m_personal_cert_context;
	SSL_Certificate_DisplayContext* m_remembered_cert_context;
	SSL_Certificate_DisplayContext* m_remembered_refused_cert_context;
	SSL_Certificate_DisplayContext* m_intermediate_cert_context;

	SimpleTreeModel		m_personal_certs_model;
	SimpleTreeModel		m_authorities_certs_model;
	SimpleTreeModel		m_remembered_certs_model;
	SimpleTreeModel		m_remembered_refused_certs_model;
	SimpleTreeModel		m_intermediate_certs_model;

	OpTreeView*			m_personal_certs_treeview;
	OpTreeView*			m_authorities_certs_treeview;
	OpTreeView*			m_remembered_certs_treeview;
	OpTreeView*			m_remembered_refused_certs_treeview;
	OpTreeView*			m_intermediate_certs_treeview;

	BOOL				m_importing_exporting_enabled; // Set when importing/exporting so the buttons can be disabled

	CertImportExportCallback* m_callback; ///< Callback that receives messages about completed imports/exports
	friend class ImportCertificateChooserListener;
	friend class ExportCertificateChooserListener;
	CertImportExportCallback* GetCallback(){ return m_callback; }
	void SetCallback(CertImportExportCallback* callback) { m_callback = callback; }
	void EnableDisableImportExportButtons(BOOL enable); ///< Disable all import or export buttons while performing the import or export.
public:

	CertificateManagerDialog();
	~CertificateManagerDialog();

	Type				GetType()				{return DIALOG_TYPE_CERTIFICATE_MANAGER;}
	const char*			GetWindowName()			{return "Manage Certificates Dialog";}
	const char*			GetHelpAnchor()			{return "certificates.html";}

	BOOL				GetShowPagesAsTabs()	{return TRUE;}
	BOOL				OnInputAction(OpInputAction* action);

	void				OnChange(OpWidget *widget, BOOL changed_by_mouse);

	void				OnInit();
	UINT32				OnOk();
	void				OnClose(BOOL user_initiated);

	void				AddImportedCert(CertImportExportCallback::Mode mode);
	void				UpdateCTX(CertImportExportCallback::Mode mode);

	OP_STATUS			GetExpiredTimeString(SSL_Certificate_DisplayContext* context, INT32 index, OpString& time_string);

private:
	BOOL							GetCurrentPageSSLCertDisplayContext(SSL_Certificate_DisplayContext **ssl_cert_display_context, SimpleTreeModel **simple_tree_view, OpTreeView **tree_view);
	CertImportExportCallback::Mode	GetCurrentPageCertImportCallbackMode();
	CertImportExportCallback::Mode	GetCurrentPageCertExportCallbackMode();
	DesktopFileChooser*				m_chooser;
	OpAutoPtr<DesktopFileChooserListener> m_chooser_listener;
};

#endif //CERTMANAGERDIALOG_H
