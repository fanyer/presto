/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "adjunct/quick/managers/CommandLineManager.h"
#include "adjunct/widgetruntime/GadgetStartup.h"
#include "adjunct/widgetruntime/pi/PlatformGadgetUtils.h"
#include "modules/gadgets/OpGadgetClass.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/selftest/optestsuite.h"
#include "modules/util/path.h"

#ifdef _MACINTOSH_
#include "platforms/mac/util/CocoaPlatformUtils.h"
#endif

namespace
{
	const uni_char* GADGET_ARCHIVE_EXTENSIONS[] = { UNI_L(".wgt"), UNI_L(".zip") };

	/**
	 * Detrmines if gadget pointed by path has correct extension (wgt or zip)
	 *
	 * @param path Path to file which is probably a gadget archive file
	 * @return TRUE iff path has one of gadget archive extension
	 */
	BOOL HasGadgetArchiveExtension(const OpStringC& path)
	{
		for (size_t i = 0; i < ARRAY_SIZE(GADGET_ARCHIVE_EXTENSIONS); ++i)
		{
			const int extension_pos =
					path.Length() - uni_strlen(GADGET_ARCHIVE_EXTENSIONS[i]);
			OpString extension;
			if (extension_pos >= 0)
			{
				RETURN_VALUE_IF_ERROR(extension.Set(path.SubString(extension_pos)),
						FALSE);
			}
			if (0 == extension.CompareI(GADGET_ARCHIVE_EXTENSIONS[i]))
			{
				return TRUE;
			}
		}

		return FALSE;
	}

	/**
	 * Obtains the profile name to be used by the gadget installer.  This should
	 * always be the same profile, separate from the browser's and any of the
	 * gadgets' profiles.
	 *
	 * @param profile_name Receives the gadget installer profile name
	 * @return Error status
	 */
	OP_STATUS GetGadgetInstallerProfileName(OpString& profile_name)
	{
		// FIXME: Avoid magic strings.
		return profile_name.Set(UNI_L("Opera Widget Installer"));
	}

#ifdef _UNIX_DESKTOP_
	OP_STATUS GetGadgetManagerProfileName(OpString& profile_name)
	{
		// FIXME: Avoid magic strings.
		return profile_name.Set(UNI_L("Opera Widget Manager"));
	}
#endif // _UNIX_DESKTOP_

} // Anonymous namespace


BOOL GadgetStartup::IsGadgetStartupRequested()
{
	OpString path;
	RETURN_VALUE_IF_ERROR(GetRequestedGadgetFilePath(path), FALSE);

	return OpStringC(GADGET_CONFIGURATION_FILE) == OpPathGetFileName(path);
}

OP_STATUS GadgetStartup::GetRequestedGadgetFilePath(OpString& path)
{
	if (OpStatus::IsSuccess(PlatformGadgetUtils::GetImpliedGadgetFilePath(path)))
	{
		return OpStatus::OK;
	}

	CommandLineArgument* widget_arg = CommandLineManager::GetInstance()->
		GetArgument(CommandLineManager::Widget);
	if (NULL == widget_arg)
	{
		return OpStatus::ERR;
	}

	return path.Set(widget_arg->m_string_value);
}

OP_STATUS GadgetStartup::GetProfileName(OpString& profile_name)
{
	profile_name.Empty();

	if (IsGadgetStartupRequested())
	{
		OpString gadget_path;
		if (OpStatus::IsSuccess(GetRequestedGadgetFilePath(gadget_path)))
		{
			OpStatus::Ignore(PlatformGadgetUtils::GetGadgetProfileName(
						gadget_path, profile_name));
		}
#ifdef _UNIX_DESKTOP_
		return OpStatus::OK;
#endif // _UNIX_DESKTOP_
	}

	if (profile_name.IsEmpty())
	{
		printf("Failed to determine Opera personal directory.\n");
	}

	return profile_name.HasContent() ? OpStatus::OK : OpStatus::ERR;
}

OP_STATUS GadgetStartup::HandleStartupFailure(const OpStringC& gadget_path)
{
#ifdef SELFTEST
	if (g_selftest.suite->DoRun())
	{
		return OpStatus::OK;
	}
#endif // SELFTEST

	OpString title;
	RETURN_IF_ERROR(g_languageManager->GetString(
				Str::D_WIDGET_STARTUP_FAIL_TITLE, title));
	OpString message_format;
	RETURN_IF_ERROR(g_languageManager->GetString(
				Str::D_WIDGET_STARTUP_FAIL_MESSAGE, message_format));
	OpString message;
	RETURN_IF_ERROR(message.AppendFormat(message_format, gadget_path.CStr()));

	SimpleDialog::ShowDialog(WINDOW_NAME_WIDGET_STARTUP_FAILURE, NULL, message.CStr(), title.CStr(),
			SimpleDialog::TYPE_CLOSE, SimpleDialog::IMAGE_ERROR);

	return OpStatus::OK;
}

BOOL GadgetStartup::IsBrowserStartupRequested()
{
	BOOL browser_startup = FALSE;

	// An implicit gadget or gadget installer start-up request makes the command line irrelevant.
	if (!IsGadgetStartupRequested())
	{
		browser_startup =
				NULL == CommandLineManager::GetInstance()->GetArgument(
						CommandLineManager::Widget);
	}
	return browser_startup;
}

#endif // WIDGET_RUNTIME_SUPPORT
