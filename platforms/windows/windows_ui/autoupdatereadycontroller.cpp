/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef AUTO_UPDATE_SUPPORT
#ifdef AUTOUPDATE_PACKAGE_INSTALLATION

#include "platforms/windows/windows_ui/autoupdatereadycontroller.h"

#include "adjunct/autoupdate/updater/pi/aufileutils.h" 
#include "adjunct/quick/managers/CommandLineManager.h"
#include "adjunct/quick_toolkit/widgets/QuickButton.h"

#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"

#include "modules/locale/locale-enum.h"

AutoUpdateReadyController::AutoUpdateReadyController(BOOL needs_elevation) :
	 SimpleDialogController(TYPE_YES_NO,IMAGE_INFO,"AutoUpdateReadyController",Str::D_AUTOUPDATER_UPDATE_READY_MESSAGE,Str::D_AUTOUPDATER_UPDATE_READY_DIALOG_TITLE)
	,m_needs_elevation(needs_elevation)
{
}

void AutoUpdateReadyController::InitL()
{
	SimpleDialogController::InitL();

	if (m_needs_elevation)
	{
		//Get UAC shield icon
		ANCHORD(Image, uacShield);
		if (OpStatus::IsSuccess(static_cast<DesktopOpSystemInfo*>(g_op_system_info)->GetImage(DesktopOpSystemInfo::PLATFORM_IMAGE_SHIELD, uacShield)))
		{
			m_ok_button->SetImage(uacShield);
			m_ok_button->GetOpWidget()->SetButtonStyle(OpButton::STYLE_IMAGE_AND_TEXT_CENTER);
		}
	}
}

void AutoUpdateReadyController::OnCancel()
{
	AUFileUtils* au_fileutils = AUFileUtils::Create();
	if(!au_fileutils)
		return;
	uni_char* wait_file_path = NULL;
	if(au_fileutils->GetWaitFilePath(&wait_file_path, TRUE) != AUFileUtils::OK) // Look in exec folder as we are already in update folder
	{
		OP_DELETE(au_fileutils);
		return;
	}
	au_fileutils->CreateEmptyFile(wait_file_path);
	OP_DELETE(au_fileutils);
	OP_DELETEA(wait_file_path);
}


#endif // AUTOUPDATE_PACKAGE_INSTALLATION
#endif // AUTO_UPDATE_SUPPORT
