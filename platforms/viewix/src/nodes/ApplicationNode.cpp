/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#include "core/pch.h"

#include "platforms/viewix/src/nodes/ApplicationNode.h"
#include "platforms/viewix/src/FileHandlerManagerUtilities.h"

ApplicationNode::ApplicationNode(const OpStringC & desktop_file_name,
								 const OpStringC & path,
								 const OpStringC & command,
								 const OpStringC & icon):
    FileHandlerNode(desktop_file_name, icon)
{
    m_path.Set(path);
    m_command.Set(command);
	m_default_path.Set("/usr/share/applications/");
    m_in_terminal   = FALSE;
    m_takes_urls    = MAYBE;
	m_is_executable = MAYBE;
}


ApplicationNode::~ApplicationNode()
{

}

const OpStringC & ApplicationNode::GetPath()
{
	if(m_path.HasContent())
		return m_path;

	return m_default_path;
}

void ApplicationNode::SetCommand(const OpStringC & command)
{
	m_command.Set(command);
	m_command.Strip();
	m_clean_command.Empty();
}

const OpStringC & ApplicationNode::GetCleanCommand()
{
    if(m_clean_command.IsEmpty() && m_command.HasContent())
    {
		FileHandlerManagerUtilities::GetCleanCommand(m_command, m_clean_command);

		if(GetName().IsEmpty())
			SetName(m_clean_command.CStr());

		if(m_clean_command.HasContent() && GetInTerminal())
		{
			OpString term_command;
			FileHandlerManagerUtilities::GetTerminalCommand(term_command);
			m_clean_command.Insert(0, term_command.CStr());
		}
    }

    return m_clean_command;
}

void ApplicationNode::SetName(const OpStringC & name)
{
	OpString clean_name;
	FileHandlerManagerUtilities::StripPath(clean_name, name);
	FileHandlerNode::SetName(clean_name);
}

OP_STATUS ApplicationNode::GuessIconName(OpString & word)
{
	if(GetIcon().HasContent())
		return word.Set(GetIcon());

	if(GetInTerminal())
		return word.Set("terminal");

	OpString clean_name;
	FileHandlerManagerUtilities::StripPath(clean_name, m_command);

	OpAutoVector<OpString> words;
	FileHandlerManagerUtilities::SplitString(words, clean_name, ' ');

	if(words.GetCount())
	{
		if(words.Get(0)->Compare("kioclient") == 0)
			return word.Set("konqueror");

		return word.Set(words.Get(0)->CStr());
	}

	return OpStatus::ERR;
}

BOOL ApplicationNode::IsExecutable()
{
	if(m_is_executable == MAYBE)
	{
		OpAutoVector<OpString> words;

		FileHandlerManagerUtilities::SplitString(words, m_command, ' ');

		OpString full_application_path;

		if(words.GetCount())
			m_is_executable = (BOOL3) FileHandlerManagerUtilities::ExpandPath( words.Get(0)->CStr(),
																			   X_OK,
																			   full_application_path );
	}

	return m_is_executable == TRUE;
}

BOOL ApplicationNode::GetTakesUrls()
{
    if(m_takes_urls == MAYBE && m_command.HasContent())
    {
		m_takes_urls = (BOOL3) FileHandlerManagerUtilities::TakesUrls(m_command);
    }

    if(m_takes_urls == MAYBE)
		m_takes_urls = (BOOL3) FALSE;

    return m_takes_urls == TRUE;
}
