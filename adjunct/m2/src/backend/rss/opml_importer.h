/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef OPML_IMPORTER_H
#define OPML_IMPORTER_H

#include "adjunct/m2/src/backend/rss/opml.h"

class Account;
class Indexer;
class Index;

// ***************************************************************************
//
//	OPMLM2ImportHandler
//
// ***************************************************************************

class OPMLM2ImportHandler
:	public OPMLImportHandler
{
public:
	// Construction.
	OPMLM2ImportHandler();
    // Destructor must be virtual:
	virtual ~OPMLM2ImportHandler() {}
	OP_STATUS Init();

private:
	// OPMLImportHandler.
	virtual OP_STATUS OnOutlineBegin(const OpStringC& text, const OpStringC& xml_url, const OpStringC& title, const OpStringC& description);
	virtual OP_STATUS OnOutlineEnd(BOOL folder);
	virtual OP_STATUS OnOPMLEnd();

	// Members.
	Indexer*	m_indexer;
	Account*	m_newsfeed_account;
	UINT		m_import_count;
	Index*		m_parent_folder;
};


#endif //OPML_IMPORTER_H
