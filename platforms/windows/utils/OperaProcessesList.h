/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2013 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
**
*/

#ifndef OPERA_PROCESS_UTILS_H
#define OPERA_PROCESS_UTILS_H

class OperaProcessesList
{
public:
	void Destroy();
	static OperaProcessesList* GetInstance();
	OP_STATUS GetOperaProcessIds(OpStringC& opera_path, OpINT32Vector& ids);
private:
	OperaProcessesList():m_shared_memory(NULL), m_shared_memory_guard(NULL){}
	~OperaProcessesList();
	OP_STATUS Init();

	OP_STATUS AddThisProcessToList();
	OP_STATUS RemoveClosedProcesses();
	OP_STATUS RemoveThisProcessFromList();
	OP_STATUS RemoveProcessFromList(OpStringC& path, INT32 id);
	
	static OperaProcessesList* s_instance;
	static bool s_read_only;
	HANDLE m_shared_memory;
	HANDLE m_shared_memory_guard;
};

#define g_opera_processes_list OperaProcessesList::GetInstance()
#endif //OPERA_PROCESS_UTILS_H