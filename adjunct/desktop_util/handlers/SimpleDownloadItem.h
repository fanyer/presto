/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Blazej Kazmierczak (bkazmierczak)
 */

#ifndef SIMPLE_DOWNLOAD_ITEM_H
#define SIMPLE_DOWNLOAD_ITEM_H

#include "modules/hardcore/mh/messobj.h"

/** 
	Class allows to download file from url pointed as a string. 
	File is saved to temp folder. You have to implement listener to use this class. 
*/

class SimpleDownloadItem : 
		public MessageObject
{
public:

	class Listener
	{
		public:

			/**
			 * @param path Downloaded file, stored in temp folder, 
			 * should be deleted by the listener.
			 */
			virtual void DownloadSucceeded(const OpStringC& path) = 0;
			virtual void DownloadFailed() = 0;
			virtual void DownloadInProgress(const URL&) = 0;
	};

	SimpleDownloadItem(MessageHandler &mh, Listener& update_controller);
	virtual ~SimpleDownloadItem();

	OP_STATUS Init(const uni_char* url_name);
	OP_STATUS Init(const URL& url);

	URL& DownloadUrl() { return m_download_url; }

private:

	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	OP_STATUS SetCallbacks();
	void UnsetCallbacks();

	OP_STATUS HandleFinished();
	void HandleError();

	MessageHandler* m_mh;
	URL m_download_url;
	Listener* m_listener;
};

#endif // SIMPLE_DOWNLOAD_ITEM_H
