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

#ifndef _SSL_XML_UPDATE_H_
#define _SSL_XML_UPDATE_H_

#if defined _NATIVE_SSL_SUPPORT_ && defined LIBSSL_AUTO_UPDATE
#include "modules/xmlutils/xmlfragment.h"
#include "modules/xmlutils/xmlnames.h"

#include "modules/libssl/data/updaters.h"
#include "modules/updaters/xml_update.h"

#include "modules/libssl/auto_update_version.h"

class SSL_XML_Updater : 
	public XML_Updater
{

protected:
	SSL_Options *optionsManager;

public:

	/** Constructor */
	SSL_XML_Updater();

	/** Destructor */
	~SSL_XML_Updater();

protected:

	BOOL CheckOptionsManager(int stores);

	virtual void SetFinished(BOOL success);

};
#endif

#endif // _SSL_XML_UPDATE_H_

