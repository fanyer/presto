/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef BTSelectionDialog_H
#define BTSelectionDialog_H

#include "adjunct/quick/dialogs/DownloadDialog.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"

class BTSelectionDialog : public DownloadDialog
{
public:

	BTSelectionDialog(DownloadItem* download_item);
	virtual ~BTSelectionDialog();

	virtual OP_STATUS	Init(DesktopWindow* parent_window);
	Type				GetType()				{return DIALOG_TYPE_BITTORRENT_SELECTION;}
	const char*			GetWindowName()			{return "BitTorrent Selection Dialog";}
	const char*			GetHelpAnchor()			{return "bittorrent.html";}
	DialogImage			GetDialogImageByEnum()	{return IMAGE_INFO;}
	void   				OnInit();
	BOOL				OnInputAction(OpInputAction *action);

	void				OnChange(OpWidget *widget, BOOL changed_by_mouse);
};

#endif // BTStartDownloadDialog_H
