/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */
#include "core/pch.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/dialogs/LicenseDialog.h"
#include "adjunct/desktop_pi/DesktopGlobalApplication.h"
#include "modules/encodings/detector/charsetdetector.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/util/opfile/opfile.h"
#include "modules/widgets/OpMultiEdit.h"

Str::LocaleString	LicenseDialog::GetOkTextID()
{
	return Str::D_I_AGREE;
}

Str::LocaleString	LicenseDialog::GetCancelTextID()
{
	return Str::D_I_DO_NOT_AGREE;
}

void LicenseDialog::OnInitVisibility()
{
	SetTitle(UNI_L("Opera"));
	OpMultilineEdit* edit = (OpMultilineEdit*) GetWidgetByName("Edit");

	if (edit)
	{
		OpString message;
		ReadFile(message);
		edit->SetText(message.CStr());
		edit->SetReadOnly(TRUE);
		edit->SetTabStop(TRUE);
		g_input_manager->SetKeyboardInputContext(edit, FOCUS_REASON_ACTIVATE);
	}
}


UINT32 LicenseDialog::OnOk()
{
	INT32 flag = g_pcui->GetIntegerPref(PrefsCollectionUI::AcceptLicense);
	// Value for 7.50
	INT32 mask = 0x01;

	TRAPD(rc, g_pcui->WriteIntegerL(PrefsCollectionUI::AcceptLicense, flag|mask));
	OP_ASSERT(OpStatus::IsSuccess(rc)); // FIXME: handle error sensibly
	TRAP(rc, g_prefsManager->CommitL());
	OP_ASSERT(OpStatus::IsSuccess(rc)); // FIXME: handle error sensibly
	g_main_message_handler->PostMessage(MSG_QUICK_APPLICATION_START_CONTINUE, 0, 0);

	return 0;
}

void LicenseDialog::OnCancel()
{
	//Cancelling start up dialog means that the user doesn't want to run Opera after all
	g_desktop_global_application->Exit();
}

void LicenseDialog::ReadFile( OpString& message )
{
	// Step 1: Open file
	OpFile file;
	
	if (OpStatus::IsError(OpenLicenseFile(OPFILE_LANGUAGE_FOLDER, file)) &&
		OpStatus::IsError(OpenLicenseFile(OPFILE_INI_FOLDER, file)))
		return;

	// Step 2: Read file.
	OpFileLength size;
	RETURN_VOID_IF_ERROR(file.GetFileLength(size));

	if( size > 0 )
	{
		char* buffer = OP_NEWA(char, size+1);
		if( buffer )
		{
			OpFileLength bytes_read = 0;
			file.Read(buffer, size, &bytes_read);
			buffer[size] = 0;

			CharsetDetector detector;
			const char* encoding = detector.GetUTFEncodingFromBOM(buffer, size);
			if( encoding )
			{
				TRAPD(rc, SetFromEncodingL(&message, encoding, buffer, size));
			}
			else
			{
				TRAPD(rc, SetFromEncodingL(&message, "windows-1252", buffer, size));
			}
			OP_DELETEA(buffer);
		}
	}
}

OP_STATUS LicenseDialog::OpenLicenseFile(OpFileFolder folder, OpFile& file)
{
	RETURN_IF_ERROR(file.Construct(UNI_L("license.txt"), folder));
	return file.Open(OPFILE_READ);
}
