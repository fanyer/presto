/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "platforms/mac/pi/MacPosixBinarySelector.h"

#include "adjunct/quick/managers/CommandLineManager.h"
#include "modules/pi/OpLocale.h"

OpComponentType PosixIpcBinarySelector::GetMappedComponentType(OpComponentType type)
{
	switch (type)
	{
		case COMPONENT_PLUGIN_MAC_32:
			return COMPONENT_PLUGIN;
	}

	return type;
}

#ifdef NS4P_COMPONENT_PLUGINS

PosixIpcBinarySelector* PosixIpcBinarySelector::Create(OpComponentType component, const char* token, const char* logfolder)
{
	OpAutoPtr<MacPosixBinarySelector> selector (OP_NEW(MacPosixBinarySelector, ()));
	RETURN_VALUE_IF_NULL(selector.get(), NULL);
	RETURN_VALUE_IF_ERROR(selector->Init(component, token, logfolder), NULL);
	return selector.release();
}

OP_STATUS MacPosixBinarySelector::Init(OpComponentType component, const char* token, const char* logfolder)
{
	RETURN_IF_ERROR(m_token.Set(token));
	RETURN_IF_ERROR(m_logfolder.Set(logfolder));

	m_args[ARG_MULTIPROC] = const_cast<char*>("-multiproc");
	m_args[ARG_MULTIPROC_DATA] = m_token.CStr();
	m_args[ARG_LOGFOLDER] = const_cast<char*>("-logfolder");
	m_args[ARG_LOGFOLDER_DATA] = m_logfolder.CStr();
	m_args[ARG_COUNT] = NULL;

	switch (component)
	{
		case COMPONENT_PLUGIN_MAC_32:
		case COMPONENT_PLUGIN:
		{
			OpFile file;
#ifdef SIXTY_FOUR_BIT
			RETURN_IF_ERROR(file.Construct(component == COMPONENT_PLUGIN_MAC_32 ? UNI_L("PluginWrapper32.app/Contents/MacOS/PluginWrapper32") : UNI_L("PluginWrapper64.app/Contents/MacOS/PluginWrapper64"), OPFILE_PLUGINWRAPPER_FOLDER));
#else
			RETURN_IF_ERROR(file.Construct(UNI_L("PluginWrapper32.app/Contents/MacOS/PluginWrapper32"), OPFILE_PLUGINWRAPPER_FOLDER));
#endif // SIXTY_FOUR_BIT
			
			OpLocale* locale;
			RETURN_IF_ERROR(OpLocale::Create(&locale));
			OpAutoPtr<OpLocale> holder(locale);
			RETURN_IF_ERROR(locale->ConvertToLocaleEncoding(&m_binary, file.GetFullPath()));
			break;
		}
		default:
			RETURN_IF_ERROR(m_binary.Set(CommandLineManager::GetInstance()->GetBinaryPath()));
	}
	
	m_args[ARG_BINARY] = m_binary.CStr();

	return OpStatus::OK;
}

const char* MacPosixBinarySelector::GetBinaryPath()
{
	return m_binary.CStr();
}

char *const * MacPosixBinarySelector::GetArgsArray()
{
	return m_args;
}

#else

PosixIpcBinarySelector* PosixIpcBinarySelector::Create(OpComponentType component, const char* token, const char* logfolder)
{
	return NULL;
}

#endif // NS4P_COMPONENT_PLUGINS

