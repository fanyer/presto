/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/widgetruntime/GadgetInstallerContext.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"
#include "modules/util/opfile/unistream.h"
#include "platforms/windows/WindowsGadgetUninstaller.h"

namespace
{
	const char SCRIPT_TEMPLATE[] =
		"Const widget_normalized_name = \"%s\"\n"
		"Const widget_install_folder = \"%s\"\n"
		"Const title = \"%s\"\n"
		"Const ForReading = 1\n"
		"Const ForAppending = 8\n"
		"Const AsUnicode = -1\n"
		"On Error Resume Next\n"
		"\n"
		"Set fs = WScript.CreateObject(\"Scripting.FileSystemObject\")\n"
		"Set sh = WScript.CreateObject(\"WScript.Shell\")\n"
		"\n"
		"force = False\n"
		"quiet = False\n"
		"For Each arg In Wscript.Arguments\n"
		"	If arg = \"/f\" Then\n"
		"		force = True\n"
		"	ElseIf arg = \"/q\" Then\n"
		"		quiet = True\n"
		"	ElseIf arg = \"/u\" Then\n"
		"		force = True\n"
		"		quiet = True\n"
		"		update = True\n"
		"	End If\n"
		"Next\n"
		"\n"
		"If Not update Then\n"
		"	' Bail out if the widget is running\n"
		"	Err.Clear\n"
		"	exe_path = widget_install_folder & \"\\\" & widget_normalized_name & \".exe\"\n"
		"	Set exe_handle = fs.OpenTextFile(exe_path, ForAppending, False)\n"
		"	If Err.Number = 70 Then\n"
		"		 result = MsgBox(\"%s\", 48, title)\n"
		"		WScript.Quit(-1)\n"
		"	Else\n"
		"		exe_handle.Close\n"
		"	End If\n"
		"End If\n"
		"\n"
		"' Ask for confirmation\n"
		"If Not force Then\n"
		"	result = MsgBox(\"%s\", vbQuestion + vbOKCancel + vbDefaultButton2, title)\n"
		"	If result = vbCancel Then\n"
		"		WScript.Quit\n"
		"	End If\n"
		"End If\n"
		"\n"
		"' Put the log entries into an array.\n"
		"Err.Clear\n"
		"Dim log_entries()\n"
		"entry_count = 0\n"
		"log_path = widget_install_folder & \"\\install.log\"\n"
		"Set log_stream = fs.OpenTextFile(log_path, ForReading, False, AsUnicode)\n"
		"If Err.Number = 0 Then\n"
		"	Do While Not log_stream.AtEndOfStream\n"
		"		entry_count = entry_count + 1\n"
		"		ReDim Preserve log_entries(entry_count - 1)\n"
		"		log_entries(entry_count - 1) = log_stream.ReadLine\n"
		"	Loop\n"
		"	log_stream.Close\n"
		"Else\n"
		"	message = \"Uninstallation failed on opening \" & log_path _\n"
		"		& \": \" & Err.Description\n"
		"	result = MsgBox(message, vbCritical, \"Error\")\n"
		"	WScript.Quit(-1)\n"
		"End If\n"
		"\n"
		"' Process the log entries in reverse order.\n"
		"For i = (entry_count - 1) To 0 Step -1\n"
		"	If \"File=\" = Left(log_entries(i), 5) Then\n"
		"		If Not (update And ( \".lnk\" = Right(log_entries(i),4) Or \".exe\" = Right(log_entries(i),4) ) ) Then\n"
		"			fs.DeleteFile(Mid(log_entries(i), 6))\n"
		"		End If\n"
		"	ElseIf \"Dir=\" = Left(log_entries(i), 4) Then\n"
		"		Set folder = fs.GetFolder(Mid(log_entries(i), 5))\n"
		"		If 0 = folder.Files.Count And 0 = folder.SubFolders.Count Then\n"
		"			folder.Delete()\n"
		"		End If\n"
		"	ElseIf \"Reg=\" = Left(log_entries(i), 4) Then\n"
		"		sh.RegDelete(Mid(log_entries(i), 5))\n"
		"	End If\n"
		"Next\n"
		"\n"
		"If Not quiet Then\n"
		"	result = MsgBox(\"%s\", vbInformation, \"%s\")\n"
		"End If\n"
	;
}


OP_STATUS WindowsGadgetUninstaller::WriteScript(const OpStringC& script_path,
		const GadgetInstallerContext& context)
{
	OpString vb_display_name; 
	RETURN_IF_ERROR(vb_display_name.Set(context.m_display_name));
	RETURN_IF_ERROR(EscapeCharForVBSStr(vb_display_name));

	OpString main_title;
	RETURN_IF_ERROR(g_languageManager->GetString(
				Str::D_WIDGET_UNINSTALLER_TITLE, main_title));

	OpString complete_title;
	RETURN_IF_ERROR(g_languageManager->GetString(
				Str::D_WIDGET_UNINSTALLER_COMPLETE, complete_title));

	OpString format;

	OpString running_message;
	RETURN_IF_ERROR(g_languageManager->GetString(
				Str::D_WIDGET_UNINSTALLER_RUNNING_WARNING, format));
	RETURN_IF_ERROR(running_message.AppendFormat(format,
				vb_display_name.CStr()));

	OpString confirm_message;
	RETURN_IF_ERROR(g_languageManager->GetString(
				Str::D_WIDGET_UNINSTALLER_CONFIRM, format));
	RETURN_IF_ERROR(confirm_message.AppendFormat(format,
				vb_display_name.CStr()));

	OpString uninstalled_message;
	RETURN_IF_ERROR(g_languageManager->GetString(
				Str::D_WIDGET_UNINSTALLER_UNINSTALLED, format));
	RETURN_IF_ERROR(uninstalled_message.AppendFormat(format,
				vb_display_name.CStr()));

	OpString script_template;
	RETURN_IF_ERROR(script_template.Set(SCRIPT_TEMPLATE));
	OpString script;
	RETURN_IF_ERROR(script.AppendFormat(script_template,
				context.m_normalized_name.CStr(),
				context.m_dest_dir_path.CStr(),
				main_title.CStr(),
				running_message.CStr(),
				confirm_message.CStr(),
				uninstalled_message.CStr(),
				complete_title.CStr()));
	
	UnicodeFileOutputStream uninstaller_file;
	RETURN_IF_ERROR(uninstaller_file.Construct(script_path, "UTF-16"));
	RETURN_IF_LEAVE(uninstaller_file.WriteStringL(script));

	return OpStatus::OK;
}


OP_STATUS WindowsGadgetUninstaller::EscapeCharForVBSStr(OpString& str)
{
	if (str.IsEmpty())
	{ 
		return OpStatus::OK;
	}
	OpString vb_str;
	 
	const uni_char scr_delimiter= '"';

	if (str.FindFirstOf(UNI_L("\""))!= KNotFound)
	{
		for ( int i = 0; i < str.Length(); i++)
		{
			if ( str[i] == scr_delimiter)
			{
				RETURN_IF_ERROR(vb_str.Append(UNI_L("\" & chr(34) & \"")));
			}
			else
			{
				RETURN_IF_ERROR(vb_str.Append(&str[i],1));
			}
		}
	}
	else
	{
		RETURN_IF_ERROR(vb_str.Append(str));
	}

	RETURN_IF_ERROR(str.Set(vb_str));
	return OpStatus::OK;  
}

#endif // WIDGET_RUNTIME_SUPPORT
