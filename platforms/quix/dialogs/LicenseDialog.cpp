/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */
#include "core/pch.h"

#include "LicenseDialog.h"
#include "modules/util/opfile/opfile.h"

#include <unistd.h>

#include "modules/locale/oplanguagemanager.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/encodings/detector/charsetdetector.h"

extern BOOL g_log_locale; // quick.cpp

// static
BOOL LicenseDialog::ShowLicense()
{
	INT32 result = DIALOG_RESULT_CANCEL;
	LicenseDialog* dialog = OP_NEW(LicenseDialog, ());
	dialog->Init(&result);

	return result == Dialog::DIALOG_RESULT_OK ? TRUE : FALSE;
}

void LicenseDialog::Init( INT32* result )
{
	m_result = result;

	TRAPD(err, g_languageManager->GetStringL(Str::D_I_AGREE, m_ok_text));
	TRAP(err, g_languageManager->GetStringL(Str::D_I_DO_NOT_AGREE, m_cancel_text));

	Dialog::Init(0);
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
	if (m_result)
		*m_result = DIALOG_RESULT_OK;

	return 0;
}


void LicenseDialog::ReadFile( OpString& message )
{
	// Step 1: Open file
	OpFile file;
	enum LicenseLangWhere {
		/* Various locations to look in, in reverse order.
		 * LicenseLangCount must be last in enum.
		 */
		LicenseLangTop, // implicitly zero
		LicenseLangEn,
		LicenseLangUser,
//#ifdef LOCALE_SUPPORT_GETLANGUAGEFILE
		LicenseLangManaged,
//#endif
		LicenseLangCount
	};
	for (int i = (int)LicenseLangCount; i-- > 0; )
	{
		OpString filename;
		OpFileFolder folder = OPFILE_LANGUAGE_FOLDER;
		switch ((enum LicenseLangWhere)i) 
		{
#ifdef LOCALE_SUPPORT_GETLANGUAGEFILE
			case LicenseLangManaged: 
			{ 
				// Same directory as the language file.
				OpFile* f = (OpFile*)((LanguageManager*)g_languageManager)->GetLanguageFile();
				filename.Set(f->GetFullPath());
				int pos = filename.FindLastOf(PATHSEPCHAR);
				if (pos != KNotFound)
					filename.Delete(pos + 1);
				folder = OPFILE_ABSOLUTE_FOLDER;
				break;
			} 
#endif
			case LicenseLangUser: // Locale direcory + contry prefix from language file
				filename.Set(g_languageManager->GetLanguage());
				filename.Append(PATHSEP);
				break;

			case LicenseLangEn: // Locale direcory + "en" prefix
				filename.Set("en");
				filename.Append(PATHSEP);
				break;
			case LicenseLangTop: // Do nothing. Fallback.
				break;
		}

		filename.Append("license.txt");
		RETURN_VOID_IF_ERROR(file.Construct(filename, folder));

		BOOL exists;
		RETURN_VOID_IF_ERROR(file.Exists(exists));
		if( g_log_locale )
		{
			OpString8 tmp;
			tmp.Set(file.GetFullPath());
			printf(exists ? "opera: Locale. License: %s\n" : "opera: Locale. No license at: %s\n", tmp.CStr() );
		}

		if (exists) 
			break;
	}

	RETURN_VOID_IF_ERROR(file.Open(OPFILE_READ));

	// Step 2: Read file.
	OpFileLength size;
	RETURN_VOID_IF_ERROR(file.GetFileLength(size));

	if( size > 0 )
	{
		char* buffer = OP_NEWA(char, size+1);
		if( buffer )
		{
			file.Read(buffer, size);
			buffer[size] = 0;

			CharsetDetector detector;
			const char* encoding = detector.GetUTFEncodingFromBOM(buffer, size);
			if( encoding )
			{
				TRAPD(rc, message.SetFromEncodingL(encoding, buffer, size));
			}
			else
			{
				TRAPD(rc, message.SetFromEncodingL("windows-1252", buffer, size));
			}
			OP_DELETEA(buffer);
		}
	}
}
