/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef BTStartDownloadDialog_H
#define BTStartDownloadDialog_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"

class BTInfo;

class BTDownloadInfo
{
public:
	BTDownloadInfo() :
		btinfo(NULL)
	{}

	OpString		savepath;
	OpString		loadpath;
	BTInfo*			btinfo;
	URL				url;
};

class BTStartDownloadDialog : public Dialog
{
public:

	BTStartDownloadDialog();
	virtual ~BTStartDownloadDialog();

	OP_STATUS			Init(DesktopWindow* parent_window, BTDownloadInfo *info, INT32 start_page = 0);
	Type				GetType()				{return DIALOG_TYPE_BITTORRENT_STARTDOWNLOAD;}
	DialogType			GetDialogType()			{return TYPE_YES_NO;}
	const char*			GetWindowName()			{return "BitTorrent Start Download Dialog";}
	const char*			GetHelpAnchor()			{return "bittorrent.html";}
	DialogImage			GetDialogImageByEnum()	{return IMAGE_WARNING;}
	void				OnInit();
	void				OnCancel();
	UINT32				OnOk();
	BOOL				OnInputAction(OpInputAction *action);


private:

	BTDownloadInfo	*m_info;
};


#endif // BTStartDownloadDialog_H
