// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2010 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Øyvind Østlund
//

#ifndef PROCESSMODEL_H
#define PROCESSMODLE_H

#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "platforms/windows/installer/Processitem.h"

/*
 *	Class ProcessModel
 *
 *	- Container class for ProcessItems.
 *	- Can be used with a TreeView to present a set of process for the user (like in the locked file dialog during installation).
 *	- Can be used with HandleInfo to find out what process has a certain file handle open.
 *	- To get debug info in the output window, set the "Processes" key in the debug.txt file.
 *
 */

class ProcessModel : public TreeModel<ProcessItem>, public OpTimerListener
{
	// The type of search we are doing
	enum SearchType
	{
		OPERA_PROCESS,		// Means we are just checking if Opera is running at a special path.
		ALL_PROCESSES		// Means we are looking for all processes that have a open handle to a specific file[path].
	};

	public:
		ProcessModel();
		~ProcessModel();

		OP_STATUS			FindProcessLocking(const uni_char* file_path);

		// Override baseclass methods
		virtual INT32		GetColumnCount()				{ return  1; }
		virtual INT32		GetItemParent(INT32 position)	{ return -1; }
		virtual OP_STATUS	GetColumnData(ColumnData* column_data);

		OP_STATUS			SetProcesses(OpVector<ProcessItem>& pi_list);
		BOOL				HasProcessID(DWORD process_id);

		// Updates the list of locking files. If the timeout is 0, the update will be done right away.
		OP_STATUS			Update(UINT timeout);

		// Timer event listener
		virtual void		OnTimeOut(OpTimer* timer);

	private:
		OP_STATUS			Init();

		HandleInfo*			m_handle_info;						// The object that can enumerate handles, and find out who is using a handle.
		OpTimer*			m_enumerate_processlist_timer;      // The timer controlling the enumeration of handles.
		OpString			m_file_locked;						// Path to the file that is locked, or folder where we check if opera might be running.
		SearchType			m_search_type;						// Type of search we are doing. 
};

#endif // PROCESSMODEL_H
