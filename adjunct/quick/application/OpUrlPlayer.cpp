/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author spoon / Patricia Aas (psmaas)
 */

#include "core/pch.h"

#include "adjunct/quick/application/OpUrlPlayer.h"

#include "adjunct/quick/managers/CommandLineManager.h"
#include "adjunct/quick/WindowCommanderProxy.h"

OpUrlPlayer::OpUrlPlayer()
{
	OpMessage msglist[1];
	msglist[0] = MSG_EXECUTE_NEXT_URLLIST_ENTRY;

	g_main_message_handler->SetCallBackList(this, 0, msglist, 1);
}

OpUrlPlayer::~OpUrlPlayer()
{
	g_main_message_handler->UnsetCallBacks(this);
}

void OpUrlPlayer::PlayUrl()
{
	UrlListPlayerEntry *entry = m_url_list_player.GetFirstUrl();
	if(entry)
	{
		WindowCommanderProxy::StopLoadingActiveWindow();
		
		// always open in the current page
		OpenURLSetting setting;
		OpString8 url;
		
		entry->GetUrl(url);			//url
		setting.m_address.Set(url.CStr());
		setting.m_from_address_field = FALSE;			//misc flag for user inputted urls
		setting.m_save_address_to_history = FALSE;
		setting.m_new_page = NO;
		setting.m_in_background = NO;
		setting.m_new_window = NO;
		
		g_application->OpenURL( setting );
		
		if(entry->GetTimeout())
		{
			g_main_message_handler->PostMessage(MSG_EXECUTE_NEXT_URLLIST_ENTRY, (MH_PARAM_1)this, 0, entry->GetTimeout() * 1000);
		}
		m_url_list_player.SetIsStarted(TRUE);
		m_url_list_player.RemoveEntry(entry);
	}
	else
	{
		// we're done with all URLs, just exit
		g_input_manager->InvokeAction(OpInputAction::ACTION_EXIT, 1);
	}
}

void OpUrlPlayer::LoadUrlList()
{
	CommandLineArgument *urllist = CommandLineManager::GetInstance()->GetArgument(CommandLineManager::UrlList);
	if(urllist)
	{
		CommandLineArgument *urllist_timeout = CommandLineManager::GetInstance()->GetArgument(CommandLineManager::UrlListLoadTimeout);
		OpStatus::Ignore(m_url_list_player.LoadFromFile(urllist->m_string_value, urllist_timeout ? urllist_timeout->m_int_value : 10));

		// kick off the urlplayer if we actually have at least one url
		if(m_url_list_player.GetFirstUrl())
		{
			// wait 2 seconds before loading the first so session stored urls won't override the first url.  Sortof a hack, I guess.
			g_main_message_handler->PostMessage(MSG_EXECUTE_NEXT_URLLIST_ENTRY, (MH_PARAM_1)this, 0, 2000);
		}
	}
}

void OpUrlPlayer::Play()
{
	CommandLineArgument *urllist = CommandLineManager::GetInstance()->GetArgument(CommandLineManager::UrlList);
	if(urllist)
	{
		CommandLineArgument *urllist_timeout = CommandLineManager::GetInstance()->GetArgument(CommandLineManager::UrlListLoadTimeout);
		OpStatus::Ignore(m_url_list_player.LoadFromFile(urllist->m_string_value, urllist_timeout ? urllist_timeout->m_int_value : 10));

		// kick off the urlplayer if we actually have at least one url
		if(m_url_list_player.GetFirstUrl())
		{
			// wait 2 seconds before loading the first so session stored urls won't override the first url.  Sortof a hack, I guess.
			g_main_message_handler->PostMessage(MSG_EXECUTE_NEXT_URLLIST_ENTRY, (MH_PARAM_1)this, 0, 2000);
		}
	}
}

void OpUrlPlayer::StopDelay()
{
	// remove any old message we might have queued and fired a new one right away to go to the next url
	g_main_message_handler->RemoveDelayedMessage(MSG_EXECUTE_NEXT_URLLIST_ENTRY, (MH_PARAM_1)this, 0);
	g_main_message_handler->PostMessage(MSG_EXECUTE_NEXT_URLLIST_ENTRY, (MH_PARAM_1)this, 0);
}

/*
**
** UrlList code for handling automated test runs based on a list of URLs from a file
**
*/
OP_STATUS OpUrlPlayer::UrlListPlayer::LoadFromFile(OpString& filename, UINT32 default_timeout)
{
	OpFile file;
	OpString8 line;

	RETURN_IF_ERROR(file.Construct(filename.CStr()));
	RETURN_IF_ERROR(file.Open(OPFILE_READ));

	if(file.IsOpen())
	{
		while(!file.Eof())
		{
			RETURN_IF_ERROR(file.ReadLine(line));
			if(line.Length())
			{
				char first_char = line[0];
				if(first_char == '#' || first_char == ';')
				{
					// line is commented out, skip it
					continue;
				}
				UINT32 timeout = default_timeout;
				int pos = line.FindFirstOf(' ');
				OpString8 url;
				if(pos != -1)
				{
					UINT32 total_len = line.Length();
					UINT32 url_len = line.Length() - (total_len - pos);
					RETURN_IF_ERROR(url.Set(line.CStr(), url_len));
					timeout = op_atoi(&line.CStr()[pos]);
				}
				else
				{
					url.TakeOver(line);
				}

				UrlListPlayerEntry *entry = OP_NEW(UrlListPlayerEntry, (url, timeout));

				if(!entry)
				{
					return OpStatus::ERR_NO_MEMORY;
				}
				entry->Into(&m_entries);
				m_is_active = TRUE;
			}
		}
	}
	return OpStatus::OK;
}

void OpUrlPlayer::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if(msg == MSG_EXECUTE_NEXT_URLLIST_ENTRY)
	{
		PlayUrl();
	}
}
