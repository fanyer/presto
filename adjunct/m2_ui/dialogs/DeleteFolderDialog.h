// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
/** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Owner:    Alexander Remen (alexr)
 */

#ifndef DELETEFOLDERDIALOG_H
#define DELETEFOLDERDIALOG_H
	
#include "adjunct/quick/dialogs/SimpleDialog.h"

class DeleteFolderDialog : public SimpleDialog
{
public:
	BOOL GetProtectAgainstDoubleClick() { return FALSE; }
	OP_STATUS Init(UINT32 id, DesktopWindow* parent_window);
	UINT32 OnOk();
	const char* GetHelpAnchor() { return "mail.html"; }

private:
	UINT32 m_id;
};

#endif // DELETEFOLDERDIALOG_H
