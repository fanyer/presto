/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2003-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef _UNTRUSTED_AUTO_RETRIEVE_H_
#define _UNTRUSTED_AUTO_RETRIEVE_H_

#if defined _NATIVE_SSL_SUPPORT_ && defined LIBSSL_AUTO_UPDATE_ROOTS
#include "modules/libssl/data/ssl_xml_update.h"

/** This class is used to retrieve single certificates to be installed 
 *	in the Untrusted certificate database.
 */
class SSL_Auto_Untrusted_Retriever : public SSL_XML_Updater
{
public:
	SSL_Auto_Untrusted_Retriever();
	~SSL_Auto_Untrusted_Retriever();

	/** Construct the updater object, and prepare the URL and callback message*/
	OP_STATUS Construct(SSL_varvector32 &issuer_id, OpMessage fin_msg);

	virtual OP_STATUS ProcessFile();
	OP_STATUS ProcessCertificate();
};
#endif

#endif // _ROOT_AUTO_RETRIEVE_H_

