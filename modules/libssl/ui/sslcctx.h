/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef _SSL_CCTX_H
#define _SSL_CCTX_H

#ifdef _NATIVE_SSL_SUPPORT_

// Not compatible with windows identifiers
#define IDM_INSTALL_CA_CERTIFICATE 1
#define IDM_INSTALL_PERSONAL_CERTIFICATE 2
#define IDM_SELECT_USER_CERTIFICATE 3
#define IDM_WARN_CERTIFICATE  4
#define IDM_WRONG_CERTIFICATE_NAME 5
#define IDM_EXPIRED_CERTIFICATE  6
#define IDM_NO_CHAIN_CERTIFICATE 7
#define IDM_NO_CHAIN_CERTIFICATE_INSTALL 8
#define IDM_EXPIRED_SERVER_CERTIFICATE 10
#define IDM_APPLET_CERTIFICATE_GRANTPERM 11
#define IDM_SSL_IMPORT_KEY_AND_CERTIFICATE 12
#define IDM_SSL_IMPORT_KEY_WITHOUT_CERTIFICATE 13
#define IDM_SSL_LOW_ENCRYPTION_LEVEL 14
#define IDM_SSL_ANONYMOUS_CONNECTION 15
#define IDM_SSL_AUTHENTICATED_ONLY 16
#define IDM_SSL_DIS_CIPHER_LOW_ENCRYPTION_LEVEL 17
#define IDM_TRUSTED_CERTIFICATE_BUTT 18
#define IDM_UNTRUSTED_CERTIFICATE_BUTT 19
#define IDM_INTERMEDIATE_CERTIFICATE_BUTT 20

#define IDM_PERSONAL_CERTIFICATES_BUTT  10733
#define IDM_SITE_CERTIFICATES_BUTT      10734
#define IDM_TRUSTED_CERTIFICATES_BUTT   10735 // UI needs to be able to list remembered certificates.

#include "modules/util/str.h"
#include "modules/libssl/options/sslopt.h"
#include "modules/locale/locale-enum.h"
#include "modules/util/simset.h"
#include "modules/windowcommander/OpWindowCommander.h"

enum ssl_cert_cert_origin{
	SSL_CERT_RETRIEVE_FROM_OPTIONS,
	SSL_CERT_RETRIEVE_FROM_CERTLIST,
	SSL_CERT_RETRIEVE_FROM_CERT_CHAIN,
	SSL_CERT_RETRIEVE_FROM_SINGLE_CERT,
	SSL_CERT_RETRIEVE_FROM_CERT_INFO
};

enum ssl_cert_cert_action {
	SSL_CERT_CANCEL_OPERATION,
	SSL_CERT_ACCEPT
	//SSL_CERT_INSTALL_CERT,
	//SSL_CERT_DELETE_CERT
};

enum ssl_cert_button_action {
	SSL_CERT_BUTTON_OK,
	SSL_CERT_BUTTON_CANCEL,
	SSL_CERT_BUTTON_INSTALL_DELETE, //Button 4
	SSL_CERT_ACCEPT_REMEMBER,
	SSL_CERT_REFUSE_REMEMBER
};

enum ssl_cert_display_action {
	SSL_CERT_DO_NOTHING,
	SSL_CERT_CLOSE_WINDOW,
	SSL_CERT_DELETE_CURRENT_ITEM
};

struct SSL_Certificate_Status{
  public:
   BOOL visited, deleted, warn, deny,install;

    SSL_Certificate_Status(){
     visited = deleted = warn = deny = install = FALSE;
  };
    ~SSL_Certificate_Status(){};
};

class SSL_Certificate_DisplayContext;
class SSL_Certificate_Export_Base;
struct SSL_Certificate_Context_Item;
class SSL_Certificate_Installer_Base;

class SSL_Certificate_Comment : public Link
{
private:
	OpString comment;

public:

	OP_STATUS Init(Str::LocaleString string_id, const OpStringC &, const OpStringC &);

	OpStringC &GetComment(){return comment;}

	SSL_Certificate_Comment *Suc(){return (SSL_Certificate_Comment *) Link::Suc();}
	SSL_Certificate_Comment *Pred(){return (SSL_Certificate_Comment *) Link::Pred();}

};

class SSL_Certificate_Comment_List : public SSL_Head
{
public:
	SSL_Certificate_Comment *First() const {return (SSL_Certificate_Comment *) SSL_Head::First();}
	SSL_Certificate_Comment *Last()  const {return (SSL_Certificate_Comment *) SSL_Head::Last();}
};

// implement SSLCertificate class, needed by windowcommander API
class WicCertificate : public OpSSLListener::SSLCertificate, public Link
{
public:
	WicCertificate(SSL_Certificate_DisplayContext* context, int number);
	virtual ~WicCertificate() {}

	virtual const uni_char* GetShortName() const { return m_short_name.CStr(); }
	virtual const uni_char* GetFullName()  const { return m_full_name.CStr();  }
	virtual const uni_char* GetIssuer()    const { return m_issuer.CStr();     }
	virtual const uni_char* GetValidFrom() const { return m_valid_from.CStr(); }
	virtual const uni_char* GetValidTo()   const { return m_valid_to.CStr();   }
	virtual const uni_char* GetInfo()      const { return m_info.CStr();       }

	virtual int GetSettings() const;
	virtual void UpdateSettings(int settings);

	unsigned int GetCertificateNumber() const { return m_num; }
	void UpdateInfo(unsigned int cert_number);

private:
	// Implementation of OpCertificate::GetCertificateHash() (stub).
	virtual const char* GetCertificateHash(unsigned int &length) const
		{ OP_ASSERT(!"This function is never called in Native SSL mode."); length = 0; return NULL; }

	// Implementation of OpSSLListener::SSLCertificate::IsInstalled() (stub).
	virtual BOOL IsInstalled() const { OP_ASSERT(!"implement me"); return FALSE; }

	OpString m_short_name;
	OpString m_full_name;
	OpString m_issuer;
	OpString m_valid_from;
	OpString m_valid_to;
	OpString m_info;

	BOOL m_allow;
	BOOL m_warn;

protected:
	unsigned int m_num;
	SSL_Certificate_DisplayContext* m_context;
};

class SSL_Certificate_DisplayContext
: public OpSSLListener::SSLCertificateContext
{
public:
	// from OpSSLListener::SSLCertificateContext
	virtual unsigned int GetNumberOfCertificates() const;
	virtual OpSSLListener::SSLCertificate* GetCertificate(unsigned int certificate);
	virtual unsigned int GetNumberOfComments() const;
	virtual const uni_char* GetCertificateComment(unsigned int i) const;
	virtual OP_STATUS GetServerName(OpString* server_name) const;
#ifdef SECURITY_INFORMATION_PARSER
    virtual OP_STATUS CreateSecurityInformationParser(OpSecurityInformationParser ** osip);
#endif // SECURITY_INFORMATION_PARSER
	virtual void OnCertificateBrowsingDone(BOOL ok, OpSSLListener::SSLCertificateOption options);
	void InitCertificates();
private:
	Head m_certificates;

private:
	// URL/connection info
	SSL_ProtocolVersion			CipherVersion;
	OpString CipherName;
	OpString Path;

	// Certificates
	SSL_CertificateHandler *	CertChain;
	SSL_ASN1Cert_list *	CertVector;
	SSL_CertificateHandler_ListHead *Certselectlist;

	URL				CurrentCertificateInfo;
	URL_InUse		info_url;

	SSL_Certificate_Comment_List certificate_comments;
	BOOL add_comments_to_certificate_view;

	// Options
	SSL_Options *optionsManager;

	// what to tell the user
	Str::LocaleString title_id;
	Str::LocaleString message_id;

    // The window associated with the certificate.
    OpWindow *window;

	// Internal flags
	SSL_CertificateStore		show_cert_store;
	BOOL		with_options;
	BOOL		hide_ok;
	BOOL		ok_button_is_accept;
	BOOL		show_delete_install;
	BOOL		delete_install_is_install;
	BOOL		use_protocoldata;
	BOOL		hide_cancel;

	BOOL		show_import;
	BOOL		show_export;

	BOOL		remember_accept_refuse;
	URL			url;
	ServerName *servername;
	unsigned short serverport;
	BOOL		is_expired;

	// Where to get the certificates
	ssl_cert_cert_origin	cert_origin;
	// which certificate is the currently displayed
	// Set by creator if a single certificate is to displayed
	unsigned int	current_certificate;
	unsigned int	certificate_count;

	SSL_Certificate_Status *CertificateStatus;
	BOOL Warn, Deny;

	// which action to take on exit
	ssl_cert_cert_action		selected_action;

	// Which Alert to raise if the user does not accept a certificate
	SSL_Alert		alert_if_failed;

	OpMessage complete_msg;
	MH_PARAM_1 complete_id;

	int action;

private:

	BOOL CheckOptionsManager();

	void InternalInit(WORD action);
	void InternalDestruct();

public:

	SSL_Certificate_DisplayContext(WORD action);
	virtual ~SSL_Certificate_DisplayContext();

	void SetExternalOptionsManager(SSL_Options *optman){
		if(optionsManager == NULL && optman != NULL)
		{
			optionsManager= optman;
			optman->inc_reference();
		}
	}
	SSL_Options *GetOptionsManager(){CheckOptionsManager(); return optionsManager;}

	void SetCipherName(const OpStringC8 &text){CipherName.Set(text);}
	void SetPath(const OpStringC &text){Path.Set(text);}
	void SetCipherVersion(const SSL_ProtocolVersion &ver){CipherVersion = ver;}

	OpStringC &GetCipherName(){return CipherName;}
	OpStringC &GetPath(){return Path;}
	const SSL_ProtocolVersion &SetCipherVersion(){return CipherVersion;}

	void AddCertificateComment(Str::LocaleString string_id, const OpStringC &, const OpStringC &);
	const OpStringC *AccessCertificateComment(unsigned int i) const;
	void SetDisplayCertCommentsInView(BOOL flag){add_comments_to_certificate_view = flag;}
	BOOL GetDisplayCertCommentsInView(){return add_comments_to_certificate_view;}

	void SetTitle(Str::LocaleString tit){title_id = tit;}
	void SetMessage(Str::LocaleString msg){message_id = msg;}
    void SetWindow(OpWindow *win) {window = win;}
	/** Takes ownership of certificate handler */
	void SetCertificateChain(SSL_CertificateHandler *chain, BOOL allow_root_install=FALSE);
	void SetCertificateList(SSL_ASN1Cert_list &vector);
	void SetCertificateSelectList(SSL_CertificateHandler_ListHead *sellist){Certselectlist = sellist;}
	SSL_CertificateHandler_ListHead *GetClientCertificateCandidates(){return Certselectlist;}

	Str::LocaleString GetTitle()   const {return title_id;}
	Str::LocaleString GetMessage() const {return message_id;}

    OpWindow *GetWindow() {return window;}

	BOOL GetShowCertificateOptions() const{return with_options;} // Show warn/accept checkboxes
	BOOL GetHideOK() const{return hide_ok;}
	BOOL GetOKButtonIsAcceptButton() const{return ok_button_is_accept;} // Show Accept text on OK button
	BOOL GetShowDeleteInstallButton() const{return show_delete_install;} // Display the fourth button for delete/install of certificates
	BOOL GetDeleteInstallButtonIsInstall() const{return delete_install_is_install;} // The fourth button in an Install button if true
	BOOL GetHideCancel() const{return hide_cancel;}
	void GetGlobalFlags(BOOL &deny_flag, BOOL &warn_flag) const{deny_flag = Deny; warn_flag = Warn;}
	BOOL GetShowImportButton() const{return show_import;};
	BOOL GetShowExportButton() const{return show_export;}

	BOOL GetRemberAcceptAndRefuse() const{return remember_accept_refuse;}
	void SetRemberAcceptAndRefuse(BOOL flag){remember_accept_refuse=flag;}
	void SetServerInformation(ServerName *server, unsigned short port){servername = server; serverport = port;}
	ServerName *GetServerName(){return servername;}
	unsigned short GetServerPort() const {return serverport;};
	void SetCertificateIsExpired(BOOL flag){is_expired = flag;}

	void SetURL(URL &a_url){url = a_url; if(!url.IsEmpty()){servername = url.GetServerName();serverport = url.GetAttribute(URL::KResolvedPort);}}
	URL  GetURL(){return url;}

	BOOL IsSingleOnlyCert(){return (cert_origin == SSL_CERT_RETRIEVE_FROM_SINGLE_CERT || cert_origin == SSL_CERT_RETRIEVE_FROM_CERT_INFO);} // Only one certificate is displayed

	void SetCurrentCertificateNumber(int item){current_certificate = item;}
	int GetCurrentCertificateNumber(){return current_certificate;}

	BOOL GetCertificateShortName(OpString &name, unsigned int i); // return TRUE if a name is returned
	BOOL GetIssuerShortName(OpString &name, unsigned int i); // return TRUE if a name is returned
	time_t GetTrustedUntil(unsigned int i);
	// Access only after GetCertificateShortName have returned FALSE
	void GetCertificateTexts(OpString &subject, OpString &issuer, OpString &data,
                             BOOL &allow_flag, BOOL &warn_flag, unsigned int cert_number);
	// Access only after GetCertificateShortName have returned FALSE
	void UpdateCurrentCertificate(BOOL warn, BOOL allow);

	URL	AccessCurrentCertificateInfo(BOOL &warn, BOOL &deny, OpString &text_field,
		Str::LocaleString description=Str::NOT_A_STRING, SSL_Certinfo_mode info_type= SSL_Cert_XML);
	URL	AccessCertificateInfo(unsigned int cert_number, BOOL &warn, BOOL &deny, OpString &text_field,
		Str::LocaleString description=Str::NOT_A_STRING, SSL_Certinfo_mode info_type= SSL_Cert_XML);

	void GetCertificateExpirationDate(OpString &date_string, unsigned int i, BOOL to_date);

	// Access only after GetCertificateShortName have returned FALSE
	ssl_cert_display_action PerformAction(ssl_cert_button_action action);
	ssl_cert_cert_action GetFinalAction(){return selected_action;}

	// Access only after GetCertificateShortName have returned FALSE
	void SaveCertSettings();
	ssl_cert_display_action DeleteInstallCertificate();

	SSL_Alert &GetAlertMessage(){return alert_if_failed;};
	void SetAlertMessage(SSL_Alert &alert){alert_if_failed = alert;};
	void SetCompleteMessage(OpMessage msg, MH_PARAM_1 id){complete_msg = msg; complete_id = id;};
	OpMessage GetCompleteMessage(){return complete_msg;}
	MH_PARAM_1 GetCompleteId(){return complete_id;}

	void UpdatedCertificates();
	int GetAction() const { return action; }

#ifdef USE_SSL_CERTINSTALLER
	BOOL ImportCertificate(const OpStringC &filename,
							SSL_dialog_config &dialog_config,
							SSL_Certificate_Installer_Base *&installer
		);
	OP_STATUS ExportCertificate(const OpStringC &filename, const OpStringC &extension,
							   SSL_dialog_config *dialog_config, SSL_Certificate_Export_Base *&exporter);
#endif
};

// These Functions Must exist
int InitSecurityCertBrowsing(OpWindow *parent_window, SSL_Certificate_DisplayContext *ctx);

#endif // _NATIVE_SSL_SUPPORT_
#endif // _SSLCCTX_H
