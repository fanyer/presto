/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef NEWSFEED_SUBSCRIBE_DIALOG_H
#define NEWSFEED_SUBSCRIBE_DIALOG_H

#include "adjunct/quick/dialogs/SimpleDialog.h"


/** @brief Dialog to ask user whether to subscribe to a newsfeed
  */
class NewsfeedSubscribeDialog : public SimpleDialog
{
	public:
		/** Initialization
		  * @param parent_window
		  * @param newsfeed_title Title of the newsfeed, will be used to refer to feed in dialog
		  * @param newsfeed_url URL to subscribe if user clicks Yes
		  */
		OP_STATUS		Init(DesktopWindow* parent_window,
							 const OpStringC& newsfeed_title,
							 const OpStringC& newsfeed_url);

		/** Initialization
		  * @param parent_window
		  * @param downloaded_newsfeed_url URL object that contains already downloaded newsfeed
		  */
		OP_STATUS		Init(DesktopWindow* parent_window,
							 URL& downloaded_newsfeed_url);

		// From SimpleDialog
		virtual UINT32	OnOk();

	private:
		OpString m_newsfeed_url;
		URL	     m_downloaded_newsfeed_url;
};

#endif // NEWSFEED_SUBSCRIBE_DIALOG_H
