/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 * 
 * George Refseth, rfz@opera.com
 */

#ifndef NETSCAPEIMPORTER_H
#define NETSCAPEIMPORTER_H

#include "adjunct/m2/src/import/MboxImporter.h"

class JsPrefsFile;

class NetscapeImporter : public MboxImporter
{
public:
	NetscapeImporter();
	virtual ~NetscapeImporter();

	OP_STATUS Init();

	OP_STATUS ImportSettings();
	OP_STATUS ImportMessages();

	OP_STATUS AddImportFile(const OpStringC& path);
	void GetDefaultImportFolder(OpString& path);

private:
	OP_STATUS InsertMailBoxes(const OpStringC& basePath, const OpStringC& m2FolderPath, INT32 parentIndex);
	void StripDoubleBS(OpString& path);
	
	NetscapeImporter(const NetscapeImporter&);
	NetscapeImporter& operator =(const NetscapeImporter&);

private:
	JsPrefsFile*	m_prefs_js;
};




#endif//NETSCAPEIMPORTER_H
