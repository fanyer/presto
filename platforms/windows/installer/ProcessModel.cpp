// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2010 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Øyvind Østlund
//

#include "core/pch.h"

#include "modules/hardcore/timer/optimer.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"

#include "platforms/windows/installer/HandleInfo.h"
#include "platforms/windows/installer/ProcessModel.h"
#include "platforms/windows/windows_ui/winshell.h"


//
// ProcessModel
//

ProcessModel::ProcessModel() : m_handle_info(NULL)
							  ,m_enumerate_processlist_timer(NULL)
{

}

ProcessModel::~ProcessModel()
{

	if (m_enumerate_processlist_timer)
	{
		m_enumerate_processlist_timer->Stop();
		OP_DELETE(m_enumerate_processlist_timer);
		m_enumerate_processlist_timer = NULL;
	}

	
	OP_DELETE(m_handle_info);
	m_handle_info = NULL;
	DeleteAll();
}

OP_STATUS ProcessModel::Init()
{
	if (!m_handle_info)
	{
		m_handle_info = OP_NEW(HandleInfo, ());
		RETURN_OOM_IF_NULL(m_handle_info);
		RETURN_IF_ERROR(m_handle_info->Init());
	}

	if (!m_enumerate_processlist_timer)
	{
		//Initialize and start process-list enumeration timer
		m_enumerate_processlist_timer = OP_NEW(OpTimer,());
		RETURN_OOM_IF_NULL(m_enumerate_processlist_timer);
		m_enumerate_processlist_timer->SetTimerListener(this);
	}

	return OpStatus::OK;
}

OP_STATUS ProcessModel::FindProcessLocking(const uni_char* file_path)
{
	if (!file_path || !*file_path)
		return OpStatus::ERR;

	RETURN_IF_ERROR(Init());

	if (PathIsDirectory(file_path))
		m_search_type = OPERA_PROCESS;
	else if (PathFileExists(file_path))
		m_search_type = ALL_PROCESSES;
	else
	{
		OP_ASSERT(!"We can't recognize this path.");
		return OpStatus::ERR;
	}

	RETURN_IF_ERROR(m_file_locked.Set(file_path));

	return OpStatus::OK;
}

OP_STATUS ProcessModel::GetColumnData(ColumnData* column_data) 
{ 
	if (column_data->column == 0)
		RETURN_IF_ERROR(g_languageManager->GetString(Str::D_INSTALLER_APPLICATION_NAME, column_data->text)); 

	return OpStatus::OK;
}

OP_STATUS ProcessModel::SetProcesses(OpVector<ProcessItem>& pi_list)
{

	//
	// BeginChange
	//
	BeginChange();

	ProcessItem* pi;
	ProcessItem* pi_new;
	UINT32 i = 0;

	// Go through all the ProcessItems in the model
	while ((pi = GetItemByIndex(i)) != NULL)
	{
		DWORD pID = pi->GetProcessID();
		BOOL found = FALSE;
		INT32 j = 0;

		// Go through all the ProcessItems in the OpAutoVector
		// Delete from model those we can't find in the OpAutoVector
		while (!found && ((pi_new = pi_list.Get(j)) != NULL))
		{
			if (pID == pi_new->GetProcessID())
			{
				pi_list.Delete((UINT32)j);
				found = TRUE;
			}
			else
				++j;
		}

		if (!found)
			Delete(i);
		else
			++i;
	}

	// Go through all the ProcessItems in the OpAutoArray
	// Remove from OpAutoArray and add to the model those we can't find in the model.
	while ((pi_new = pi_list.Get(0)) != NULL)
	{
		if (!HasProcessID(pi_new->GetProcessID()))
		{
			if (AddLast(pi_new) == -1)	// Insert it into the model
				return OpStatus::ERR_NO_MEMORY;

			pi_list.Remove(0);		// Remove it, so we don't have to make a new one for the model
		}
		else
			pi_list.Delete((UINT32)0);		// Delete it, so we can do a check at the end that the list is empty
	}

	EndChange();

	//
	// End Change
	//

	OP_ASSERT(pi_list.GetCount() == 0);
	
	return OpStatus::OK;
}

BOOL ProcessModel::HasProcessID(DWORD process_id)
{
	INT32 count = GetCount();

	for (INT32 i = 0; i < count; ++i)
	{
		if (GetItemByIndex(i)->GetProcessID() == process_id)
			return TRUE;
	}

	return FALSE;
}

void ProcessModel::OnTimeOut(OpTimer* timer)
{
	if(timer == m_enumerate_processlist_timer)
	{
		if (OpStatus::IsError(Update(ENUMERATE_PROCESS_LIST_NOW)) || GetCount() == 0)
		{
			g_main_message_handler->PostMessage(MSG_FILE_HANDLE_NO_LONGER_LOCKED, (MH_PARAM_1)0, (MH_PARAM_2)0);
			m_enumerate_processlist_timer->Stop(); // Stopp the timer
		}
	}
}

OP_STATUS ProcessModel::Update(UINT timeout)
{
	OP_STATUS status = OpStatus::ERR;
    OpAutoVector<ProcessItem> processes;

	if (timeout > 0)
	{
		m_enumerate_processlist_timer->Start(timeout); // Restart the timer
		status = OpStatus::OK;
	}
	else		
	{
		if (m_search_type == ALL_PROCESSES)
			status = m_handle_info->IsFileInUse(m_file_locked.CStr(), processes, TRUE);
		else if (m_search_type == OPERA_PROCESS)
			status = m_handle_info->IsOperaRunningAt(m_file_locked.CStr(), processes, TRUE);

		if (OpStatus::IsSuccess(status) && (processes.GetCount() > 0 || GetCount() > 0))
		{
			SetProcesses(processes);
			m_enumerate_processlist_timer->Start(ENUMERATE_PROCESS_LIST_TIMEOUT); // Restart the timer
		}
	}
	return status;
}
