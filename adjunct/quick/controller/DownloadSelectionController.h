/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Erman Doser (ermand)
 */

#ifndef DOWNLOAD_SELECTION_CONTROLLER_H
#define DOWNLOAD_SELECTION_CONTROLLER_H

#include "adjunct/quick_toolkit/contexts/OkCancelDialogContext.h"
#include "modules/windowcommander/OpWindowCommander.h"

class DownloadItem;
class ExternalDownloadManager;

class DownloadSelectionController : public OkCancelDialogContext
{
public:
	DownloadSelectionController(OpDocumentListener::DownloadCallback* callback,	DownloadItem* di);
	virtual ~DownloadSelectionController();

protected:
	void InitL();

	virtual void OnOk();

private: 	
	OpVector<ExternalDownloadManager>* m_managers;
	DownloadItem* m_download_item;
	OpDocumentListener::DownloadCallback* m_download_callback;
	bool m_release_resource;
};

#endif //DOWNLOAD_SELECTION_CONTROLLER_H
