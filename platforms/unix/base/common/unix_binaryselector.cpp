/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "platforms/unix/base/common/unix_binaryselector.h"

#include "adjunct/quick/managers/CommandLineManager.h"
#include "modules/pi/OpLocale.h"

OpComponentType PosixIpcBinarySelector::GetMappedComponentType(OpComponentType type)
{
	switch (type)
	{
		case COMPONENT_PLUGIN_LINUX_IA32:
			return COMPONENT_PLUGIN;
	}

	return type;
}

PosixIpcBinarySelector* PosixIpcBinarySelector::Create(OpComponentType component, const char* token, const char* logfolder)
{
	OpAutoPtr<UnixBinarySelector> selector (OP_NEW(UnixBinarySelector, ()));
	RETURN_VALUE_IF_NULL(selector.get(), NULL);
	RETURN_VALUE_IF_ERROR(selector->Init(component, token, logfolder), NULL);
	return selector.release();
}

OP_STATUS UnixBinarySelector::Init(OpComponentType component, const char* token, const char* logfolder)
{
	RETURN_IF_ERROR(m_token.Set(token));
	RETURN_IF_ERROR(m_logfolder.Set(logfolder));

	m_args[ARG_MULTIPROC] = "-multiproc";
	m_args[ARG_MULTIPROC_DATA] = m_token.CStr();
	m_args[ARG_LOGFOLDER] = "-logfolder";
	m_args[ARG_LOGFOLDER_DATA] = m_logfolder.CStr();
	m_args[ARG_TYPE] = "-type";
	m_args[ARG_TYPE_DATA] = "native";
	m_args[ARG_COUNT] = NULL;

	switch (component)
	{
		case COMPONENT_PLUGIN_LINUX_IA32:
			m_args[ARG_TYPE_DATA] = "ia32-linux";
			// fall through
		case COMPONENT_PLUGIN:
		{
			OpFile file;
			RETURN_IF_ERROR(file.Construct(UNI_L("operapluginwrapper"), OPFILE_PLUGINWRAPPER_FOLDER));

			OpLocale* locale;
			RETURN_IF_ERROR(OpLocale::Create(&locale));
			OpAutoPtr<OpLocale> holder(locale);
			RETURN_IF_ERROR(locale->ConvertToLocaleEncoding(&m_binary, file.GetFullPath()));
			break;
		}
		default:
			RETURN_IF_ERROR(m_binary.Set(CommandLineManager::GetInstance()->GetBinaryPath()));
			m_args[ARG_TYPE] = NULL;
	}

	m_args[ARG_BINARY] = m_binary.CStr();

	return OpStatus::OK;
}

const char* UnixBinarySelector::GetBinaryPath()
{
	return m_binary.CStr();
}

char *const * UnixBinarySelector::GetArgsArray()
{
	return const_cast<char**>(m_args);
}
