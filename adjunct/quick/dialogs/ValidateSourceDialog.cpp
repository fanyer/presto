/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef _VALIDATION_SUPPORT_

#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "ValidateSourceDialog.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/doc/frm_doc.h"
#include "modules/url/url2.h"
#include "modules/upload/upload.h"
#include "adjunct/quick/WindowCommanderProxy.h"

/***********************************************************************************
**
**	Init
**
***********************************************************************************/

void ValidateSourceDialog::Init(DesktopWindow* parent_window, OpWindowCommander* win_comm)
{
	m_window_commander = win_comm;
	Dialog::Init(parent_window);
}

void ValidateSourceDialog::OnInit()
{
	SetWidgetText("Window_title", WindowCommanderProxy::GetWindowTitle(m_window_commander));

	OpMultilineEdit* edit = (OpMultilineEdit*)GetWidgetByName("Description_label");
	if(edit)
	{
		edit->SetLabelMode();
	}
}

/***********************************************************************************
**
**	OnOk
**
***********************************************************************************/

UINT32 ValidateSourceDialog::OnOk()
{
	WindowCommanderProxy::UploadFileForValidation(m_window_commander);
	return 0;
}

/***********************************************************************************
**
**	OnClose
**
***********************************************************************************/

void ValidateSourceDialog::OnClose(BOOL user_initiated)
{
	TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::ShowValidationDialog, !IsDoNotShowDialogAgainChecked()));
	Dialog::OnClose(user_initiated);
}

#endif // _VALIDATION_SUPPORT_
