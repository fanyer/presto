/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef OPML_H
#define OPML_H

class OpFile;

// ***************************************************************************
//
//	OPMLImportHandler
//
// ***************************************************************************

class OPMLImportHandler
{
public:
	// Virtual methods.
	virtual OP_STATUS OnOPMLBegin() { return OpStatus::OK; }
	virtual OP_STATUS OnHeadTitle(const OpStringC& title) { return OpStatus::OK; }
	virtual OP_STATUS OnOutlineBegin(const OpStringC& text, const OpStringC& xml_url, const OpStringC& title, const OpStringC& description) { return OpStatus::OK; }
	virtual OP_STATUS OnOutlineEnd(BOOL folder) { return OpStatus::OK; }
	virtual OP_STATUS OnOPMLEnd() { return OpStatus::OK; }

protected:
	// Construction.
	OPMLImportHandler() {}
    // Destructor must be virtual:
    virtual ~OPMLImportHandler() {}

private:
	// No copy or assignment.
	OPMLImportHandler(const OPMLImportHandler& other);
	OPMLImportHandler& operator=(const OPMLImportHandler& other);
};

// ***************************************************************************
//
//	OPML
//
// ***************************************************************************

class OPML
{
public:
	// Static methods.
	static OP_STATUS Import(const OpStringC& file_path, OPMLImportHandler& import_handler);
	static OP_STATUS Export(const OpStringC& file_path);

private:
	// No construction, copy or assignment.
	OPML();
	OPML(const OPML& other);
	OPML& operator=(const OPML& other);
};

#endif
