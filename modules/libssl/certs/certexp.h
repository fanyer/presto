/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/


#ifndef SSL_CERTEXP_H
#define SSL_CERTEXP_H

#if defined _NATIVE_SSL_SUPPORT_ && defined USE_SSL_CERTINSTALLER && defined LIBOPEAY_PKCS12_SUPPORT
#include "modules/libssl/ssl_api.h"
#include "modules/libssl/certs/certexp_base.h"
#include "modules/libssl/handshake/asn1certlist.h"

class SSLSecurtityPasswordCallbackImpl;


class SSL_PKCS12_Certificate_Export : public SSL_Certificate_Export_Base, public MessageObject
{
private:
	SSL_ASN1Cert_list original_cert;
	OpString	target_filename;

	SSLSecurtityPasswordCallbackImpl *ask_password;
	OpWindow *window;
	MessageHandler *message_handler;
	OpMessage finished_message;
	MH_PARAM_1 finished_id;

	BOOL finished_advanced_export;
	BOOL advanced_export_success;

	SSL_Options *optionsManager;
	int certificate_number;

	SSL_secure_varvector32 private_key;
	OpString8 password_export;

	enum export_state {
		Export_Not_started,
		Export_Retrieve_Key,
		Export_Asking_Security_Password,
		Export_Asking_Cert_Password,
		Export_Write_cert,
		Export_Finished
	} export_state;

public: 

	SSL_PKCS12_Certificate_Export();
	virtual ~SSL_PKCS12_Certificate_Export();
	
	OP_STATUS Construct(SSL_Options *optManager, int cert_number,
		const OpStringC &filename, SSL_dialog_config &config);

	virtual OP_STATUS StartExport();
	virtual BOOL Finished();
	virtual BOOL ExportSuccess();

	
	virtual void	HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

private:

	OP_STATUS ProgressExport();

	void FinishedExport(BOOL status);
};

#endif // USE_SSL_CERTINSTALLER

#endif // SSL_CERTEXP_H
