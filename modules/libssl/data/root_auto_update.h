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

#ifndef _ROOT_AUTO_UPDATE_H_
#define _ROOT_AUTO_UPDATE_H_

#if defined _NATIVE_SSL_SUPPORT_ && defined LIBSSL_AUTO_UPDATE_ROOTS
#include "modules/libssl/data/ssl_xml_update.h"

class SSL_Auto_Root_Updater : public SSL_XML_Updater
{
public:
	SSL_Auto_Root_Updater();
	~SSL_Auto_Root_Updater();

	/** Construct the updater object, and prepare the URL and callback message*/
	OP_STATUS Construct(OpMessage fin_msg);

	virtual OP_STATUS ProcessFile();
	OP_STATUS ProcessRepository();
	OP_STATUS ProcessCertificate();
};
#endif

#endif // _ROOT_AUTO_UPDATE_H_

