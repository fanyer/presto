// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2010 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Adam Minchinton
//

#ifndef __DOWNLOAD_MANAGER_MANAGER_H__
#define __DOWNLOAD_MANAGER_MANAGER_H__

#include "adjunct/quick/managers/DesktopManager.h"
#include "modules/pi/OpBitmap.h"

#define g_download_manager_manager (DownloadManagerManager::GetInstance())

class OpDropDown;
class ExternalDownloadManager;

// A class that managers download managers
class DownloadManagerManager : public DesktopManager<DownloadManagerManager>
{
public:
	DownloadManagerManager();
	~DownloadManagerManager();

	OP_STATUS	Init();
	OP_STATUS	InitManagers();

	// Null indicates Opera itself
	ExternalDownloadManager* GetDefaultManager();
	void SetDefaultManager(ExternalDownloadManager* manager);

	BOOL GetExternalManagerEnabled();
	BOOL GetShowDownloadManagerSelectionDialog();
	BOOL GetHasExternalDownloadManager();
	void PopulateDropdown(OpDropDown*);

	OpVector<ExternalDownloadManager>& GetExternalManagers(){return m_managers;}



private:
	ExternalDownloadManager* GetByName(const uni_char* name);

	OpVector<ExternalDownloadManager> m_managers;
	BOOL m_inited;
};

/** A generic class representing an external download manager.
  */
class ExternalDownloadManager
{
public:

	virtual OP_STATUS Run(const uni_char* url) = 0;
	virtual Image GetIcon(){return application_icon;}
	virtual const uni_char* GetName(){return name.CStr();}

	//OpString	path;	// The path of binary that would be executed with the url as a parameter 
	Image		application_icon;	// the icon of the application that would be used in UI.
	OpString	name;	// The name of the application that would be used in UI.
};


#endif // __DOWNLOAD_MANAGER_MANAGER_H__
