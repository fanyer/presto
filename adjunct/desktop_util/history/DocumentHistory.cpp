/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/desktop_util/history/DocumentHistory.h"
#include "adjunct/quick/managers/opsetupmanager.h"
#include "adjunct/quick/WindowCommanderProxy.h"

DocumentHistory::DocumentHistory(OpWindowCommander* commander)
	: m_window_commander(commander)
	, m_back_history_info(DocumentHistoryInformation::HISTORY_NAVIGATION)
	, m_forward_history_info(DocumentHistoryInformation::HISTORY_NAVIGATION)
	, m_fast_forward_ref_url(DocumentHistoryInformation::HISTORY_NAVIGATION)
	, m_is_fast_forward_list_updated(false)
{
}

bool DocumentHistory::HasBackHistory() const
{
	return !!m_window_commander->HasPrevious();
}

bool DocumentHistory::HasForwardHistory() const
{
	return !!m_window_commander->HasNext();
}

bool DocumentHistory::HasRewindHistory() const
{
	return GetCurrentHistoryPos() > 1 ? true : false;
}

bool DocumentHistory::HasFastForwardHistory() const
{
	return m_is_fast_forward_list_updated && m_fast_forward_list.GetCount();
}

int DocumentHistory::GetCurrentHistoryPos() const
{
	return m_window_commander->GetCurrentHistoryPos();
}

int DocumentHistory::GetHistoryLength() const
{
	int min, max;
	m_window_commander->GetHistoryRange(&min, &max);

	return max - min + 1;
}

int DocumentHistory::MoveInHistory(bool back)
{
	if (back)
		m_window_commander->Previous();
	else
		m_window_commander->Next();

	return GetCurrentHistoryPos();
}

DocumentHistory::Position DocumentHistory::MoveInFastHistory(bool back)
{
	Position pos;

	if (back)
	{
		OpAutoVector<DocumentHistoryInformation> history_list;
		RETURN_VALUE_IF_ERROR(GetFastHistoryList(history_list, !back), pos);
		UINT32 count = history_list.GetCount();
		if (!count)
			return pos;

		pos.type = Position::NORMAL;
		pos.index = history_list.Get(0)->number;

		GotoHistoryPos(pos);
	}
	else
	{
		// m_fast_forward_list gets created in GotoHistoryPos
		GotoHistoryPos(Position(Position::FAST_FORWARD, 0));

		if (m_fast_forward_list.GetCount())
		{
			pos.type = Position::FAST_FORWARD;
			pos.index = 0;
		}
	}

	return pos;
}

OP_STATUS DocumentHistory::GetHistoryInformation(Position pos, HistoryInformation* result) const
{
	if (pos.type == Position::INVALID)
		return OpStatus::ERR;

	if (pos.type == Position::NORMAL)
		return m_window_commander->GetHistoryInformation(pos.index, result) ? OpStatus::OK : OpStatus::ERR;

	// Fast forward related info handling
	if (!m_is_fast_forward_list_updated)
	{
		RETURN_IF_ERROR(CreateFastForwardList());
		m_is_fast_forward_list_updated = true;
	}

	if (!m_fast_forward_list.GetCount())
		return OpStatus::OK;

	const DocumentHistoryInformation* history_info =  m_fast_forward_list.Get(pos.index);
	if (!history_info)
		return OpStatus::ERR;

	result->number = history_info->number;
	result->url = history_info->url.CStr();
	result->title = history_info->title.CStr();

	return OpStatus::OK;
}

OP_STATUS DocumentHistory::GotoHistoryPos(Position pos)
{
	if (pos.type == Position::INVALID)
		return OpStatus::ERR;

	if (pos.type == Position::NORMAL)
	{
		m_window_commander->SetCurrentHistoryPos(pos.index);
		return OpStatus::OK;
	}

	// Fast forward related handling
	if (!m_is_fast_forward_list_updated)
	{
		RETURN_IF_ERROR(CreateFastForwardList());
		m_is_fast_forward_list_updated = true;
	}

	if (!m_fast_forward_list.GetCount())
		return OpStatus::ERR;

	const DocumentHistoryInformation* history_info =  m_fast_forward_list.Get(pos.index);
	RETURN_VALUE_IF_NULL(history_info, OpStatus::ERR);

	if (history_info->number)
		return GotoHistoryPos(Position(Position::NORMAL, history_info->number));
	else
	{
		switch (history_info->type)
		{
		case DocumentHistoryInformation::HISTORY_WAND_LOGIN:
			return WindowCommanderProxy::ProcessWandLogin(m_window_commander);

		case DocumentHistoryInformation::HISTORY_IMAGE:
		case DocumentHistoryInformation::HISTORY_NAVIGATION:
			return m_window_commander->FollowURL(history_info->url.CStr(), m_window_commander->GetCurrentURL(FALSE), FALSE, m_window_commander->GetURLContextID()) ? OpStatus::OK : OpStatus::ERR;
		}
	}

	return OpStatus::ERR;
}

OP_STATUS DocumentHistory::GetHistoryList(OpAutoVector<DocumentHistoryInformation>& history_list, bool forward) const
{
	int curr_pos = m_window_commander->GetCurrentHistoryPos();
	if (curr_pos <= 0)
		return OpStatus::ERR;

	int min = 0, max = 0;
	m_window_commander->GetHistoryRange(&min, &max);

	int from = forward ? curr_pos + 1 : curr_pos - 1;
	int to   = forward ? max + 1 : min - 1;
	int step = forward ? 1 : -1;

	for (int index = from; index != to; index += step)
	{
		HistoryInformation history;
		if (!m_window_commander->GetHistoryInformation(index, &history))
			continue;

		RETURN_IF_ERROR(AddToList(history_list, history));
	}

	return history_list.GetCount() ? OpStatus::OK : OpStatus::ERR;
}

OP_STATUS DocumentHistory::GetFastHistoryList(OpAutoVector<DocumentHistoryInformation>& history_list, bool forward) const
{
	if (forward)
	{
		for (UINT32 index = 0; index < m_fast_forward_list.GetCount(); index++)
		{
			DocumentHistoryInformation* info = m_fast_forward_list.Get(index);
			RETURN_IF_ERROR(AddToList(history_list, *info));
		}

		return history_list.GetCount() ? OpStatus::OK : OpStatus::ERR;
	}

	// Below is rewind related handling
	int curr_pos = GetCurrentHistoryPos();
	if (curr_pos <= 0)
		return OpStatus::ERR;

	HistoryInformation current_history;
	curr_pos = GetLastHistoryInfoDifferentDomain(&current_history);
	if (curr_pos == 0)
		return OpStatus::ERR;

	OpStringC server_name(current_history.server_name);

	if (curr_pos != GetCurrentHistoryPos())
		RETURN_IF_ERROR(AddToList(history_list, current_history));

	int min = 0, max = 0;
	m_window_commander->GetHistoryRange(&min, &max);

	for (int index = curr_pos; index >= min; index--)
	{
		if (!m_window_commander->GetHistoryInformation(index, &current_history))
			continue;

		if (server_name.Compare(current_history.server_name) == 0)
			continue;

		server_name = current_history.server_name;
		RETURN_IF_ERROR(AddToList(history_list, current_history));
	}

	return history_list.GetCount() ? OpStatus::OK : OpStatus::ERR;
}

const DocumentHistoryInformation& DocumentHistory::GetAdjacentHistoryInfo(bool is_back) const
{
	int pos = GetCurrentHistoryPos();

	bool is_pos_different;
	if (is_back)
		is_pos_different = m_back_history_info.number != pos - 1;
	else
		is_pos_different = m_forward_history_info.number != pos + 1;

	if (is_pos_different)
		OpStatus::Ignore(UpdateHistoryInfo(is_back));

	return is_back ? m_back_history_info : m_forward_history_info;
}

OP_STATUS DocumentHistory::UpdateHistoryInfo(bool is_back) const
{
	int cur_pos = GetCurrentHistoryPos();

	if (is_back && HasBackHistory())
		cur_pos--;
	else if (!is_back && HasForwardHistory())
		cur_pos++;
	else
		return OpStatus::ERR;

	HistoryInformation history;
	if (!m_window_commander->GetHistoryInformation(cur_pos, &history))
		return OpStatus::ERR;

	if (is_back)
	{
		m_back_history_info.Reset();
		RETURN_IF_ERROR(m_back_history_info.title.Set(history.title));
		RETURN_IF_ERROR(m_back_history_info.url.Set(history.url));
		m_back_history_info.number = history.number;
	}
	else
	{
		m_forward_history_info.Reset();
		RETURN_IF_ERROR(m_forward_history_info.title.Set(history.title));
		RETURN_IF_ERROR(m_forward_history_info.url.Set(history.url));
		m_forward_history_info.number = history.number;
	}

	return OpStatus::OK;
}

OP_STATUS DocumentHistory::UpdateFastForwardRefURL() const
{
	m_fast_forward_ref_url.Reset();

	HistoryInformation history;
	int cur_pos = GetCurrentHistoryPos();
	if (!m_window_commander->GetHistoryInformation(cur_pos, &history))
		return OpStatus::ERR;

	RETURN_IF_ERROR(m_fast_forward_ref_url.title.Set(history.title));
	RETURN_IF_ERROR(m_fast_forward_ref_url.url.Set(history.url));
	m_fast_forward_ref_url.number = history.number;

	return OpStatus::OK;
}

int DocumentHistory::GetLastHistoryInfoDifferentDomain(HistoryInformation* info) const
{
	int found_pos; // first history pos which matches the current domain
	int current_pos;
	found_pos = current_pos = GetCurrentHistoryPos();

	OpString server_name;
	if (!m_window_commander->GetHistoryInformation(current_pos, info))
		return 0;

	if (OpStatus::IsError(server_name.Set(info->server_name)))
		return 0;

	int min = 0, max = 0;
	m_window_commander->GetHistoryRange(&min, &max);
	for (int index = current_pos - 1; index >= min; index--)
	{
		if (!m_window_commander->GetHistoryInformation(index, info))
			continue;

		if (server_name.Compare(info->server_name) != 0)
			break;

		found_pos = index;
	}

	m_window_commander->GetHistoryInformation(found_pos, info);
	return found_pos;
}

int DocumentHistory::GetFastForwardValue(OpStringC which) const
{
	PrefsEntry* entry;
	if (g_setup_manager->GetFastForwardSection())
	{
		entry = g_setup_manager->GetFastForwardSection()->FindEntry(which.CStr());
		if (entry)
			return entry->Get() ? uni_strtol(entry->Get(), NULL, 0) : 100;
	}

	return 0;
}

OP_STATUS DocumentHistory::CreateFastForwardList() const
{
	HistoryInformation current_history;
	int curr_pos = GetCurrentHistoryPos();
	RETURN_IF_ERROR(GetHistoryInformation(DocumentHistory::Position(DocumentHistory::Position::NORMAL, curr_pos), &current_history));

	OpString server_name;
	RETURN_IF_ERROR(server_name.Set(current_history.server_name));

	// Distinct Server: If any distinct server found it's a candidate for fast forward history
	int min = 0, max = 0;
	m_window_commander->GetHistoryRange(&min, &max);
	if (max <= 0)
		return OpStatus::ERR;

	for (int index = curr_pos + 1; index <= max; index++)
	{
		if (!m_window_commander->GetHistoryInformation(index, &current_history))
			continue;

		if (server_name.Compare(current_history.server_name) == 0)
			continue;

		RETURN_IF_ERROR(server_name.Set(current_history.server_name));
		RETURN_IF_ERROR(AddToList(m_fast_forward_list, current_history, -index-1));
	}

	// Link Elements: If any information.rel is Next, then it's a candidate for fast forward history
	LinkElementInfo information;
	UINT32 count = m_window_commander->GetLinkElementCount();
	for (UINT index = 0; index < count; index++)
	{
		if (!m_window_commander->GetLinkElementInfo(index, &information))
			continue;

		if (information.rel && information.href && uni_stricmp(UNI_L("Next"), information.rel) == 0)
		{
			if (!GetFastForwardValue(UNI_L("Link element")))
				break;

			DocumentHistoryInformation info(DocumentHistoryInformation::HISTORY_NAVIGATION);
			RETURN_IF_ERROR(info.title.Set(information.title));
			RETURN_IF_ERROR(info.url.Set(information.href));
			RETURN_IF_ERROR(AddToList(m_fast_forward_list, info));
			break;
		}
	}

	// Usable Wand:
	DocumentHistoryInformation info(DocumentHistoryInformation::HISTORY_WAND_LOGIN);
	if (GetFastForwardValue(UNI_L("Wand login"))
		&& OpStatus::IsSuccess(WindowCommanderProxy::GetWandLink(m_window_commander, info.url)))
	{
		RETURN_IF_ERROR(g_languageManager->GetString(Str::M_FASTFORWARD_WAND_LOGIN, info.title));
		RETURN_IF_ERROR(AddToList(m_fast_forward_list, info));
	}

	// Usable next elements:
	WindowCommanderProxy::GetForwardLinks(m_window_commander, m_fast_forward_list);

	// Split forward links into two lists; image-links list and text-links list
	for (int i = static_cast<int>(m_fast_forward_list.GetCount()) - 1 ; i >= 0 ; i--)
	{
		if (m_fast_forward_list.Get(i)->type == DocumentHistoryInformation::HISTORY_IMAGE)
			RETURN_IF_ERROR(m_fast_forward_img_list.Insert(0, m_fast_forward_list.Remove(i)));
	}

	return OpStatus::OK;
}

OP_STATUS DocumentHistory::CreateFastForwardListFromImageList() const
{
	UINT32 count = m_fast_forward_img_list.GetCount();
	if (!count)
		return OpStatus::ERR;

	if (m_window_commander->GetDocumentType(false) == OpWindowCommander::DOC_OTHER_GRAPHICS)
	{
		const uni_char* url_str = m_window_commander->GetCurrentURL(FALSE);
		for (UINT32 i = 0; i < count; i++)
		{
			if (m_fast_forward_img_list.Get(i)->url == url_str)
			{
				if (m_fast_forward_img_list.Get(i+1))
					return AddToList(m_fast_forward_list, *m_fast_forward_img_list.Get(i+1));
				else
					return AddToList(m_fast_forward_list, m_fast_forward_ref_url);
			}
		}
	}

	return OpStatus::ERR;
}

OP_STATUS DocumentHistory::AddFastForwardItemFromImageList() const
{
	if (m_fast_forward_img_list.GetCount() == 0)
		return OpStatus::ERR;

	return AddToList(m_fast_forward_list, *m_fast_forward_img_list.Get(0));
}

OP_STATUS DocumentHistory::AddToList(OpAutoVector<DocumentHistoryInformation>& history_list, const DocumentHistoryInformation &info, int score)
{
	OpAutoPtr<DocumentHistoryInformation> history_info(OP_NEW(DocumentHistoryInformation, (info.type)));
	RETURN_OOM_IF_NULL(history_info.get());
	RETURN_IF_ERROR(history_info->title.Set(info.title));
	RETURN_IF_ERROR(history_info->url.Set(info.url));
	history_info->number = info.number;
	history_info->score = score;
	RETURN_IF_ERROR(history_list.Add(history_info.get()));
	history_info.release();
	return OpStatus::OK;
}

OP_STATUS DocumentHistory::AddToList(OpAutoVector<DocumentHistoryInformation>& history_list, const HistoryInformation &info, int score)
{
	OpAutoPtr<DocumentHistoryInformation> history_info(OP_NEW(DocumentHistoryInformation, (DocumentHistoryInformation::HISTORY_NAVIGATION)));
	RETURN_OOM_IF_NULL(history_info.get());
	RETURN_IF_ERROR(history_info->title.Set(info.title));
	RETURN_IF_ERROR(history_info->url.Set(info.url));
	history_info->number = info.number;
	history_info->score = score;
	RETURN_IF_ERROR(history_list.Add(history_info.get()));
	history_info.release();
	return OpStatus::OK;
}

void DocumentHistory::OnLoadingFinished(DocumentDesktopWindow* document_window,
	OpLoadingListener::LoadingFinishStatus,
	BOOL was_stopped_by_user)
{
	m_is_fast_forward_list_updated = true;
	m_fast_forward_list.DeleteAll();

	/* If the document is an image, try to populate fast forward list with the
	   link to the next image from the image list. */
	if (OpStatus::IsSuccess(CreateFastForwardListFromImageList()))
		return;

	m_fast_forward_img_list.DeleteAll();
	OpStatus::Ignore(CreateFastForwardList());
	OpStatus::Ignore(UpdateFastForwardRefURL());
	OpStatus::Ignore(AddFastForwardItemFromImageList());
}
