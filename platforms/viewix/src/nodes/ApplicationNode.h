/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#ifndef __FILEHANDLER_APPLICATION_NODE_H__
#define __FILEHANDLER_APPLICATION_NODE_H__

#include "platforms/viewix/src/nodes/FileHandlerNode.h"

class ApplicationNode : public FileHandlerNode
{
public:
	ApplicationNode(const OpStringC & desktop_file_name,
					const OpStringC & path,
					const OpStringC & command,
					const OpStringC & icon);

	~ApplicationNode();

	NodeType GetType() { return APPLICATION_NODE_TYPE; }

	void SetCommand(const OpStringC & command);

	const OpStringC & GetCommand() { return m_command; }

	const OpStringC & GetCleanCommand();

	OP_STATUS GuessIconName(OpString & word);

	BOOL GetTakesUrls();

	void SetTryExec(const OpStringC & try_exec) { m_try_exec.Set(try_exec); }

 	const OpStringC & GetTryExec() { return m_try_exec; }

	void SetInTerminal(BOOL in_terminal){ m_in_terminal = in_terminal;}

	BOOL GetInTerminal() { return m_in_terminal; }

	void SetPath(const OpStringC & path) { m_path.Set(path.CStr()); }

	const OpStringC & GetPath();

	const uni_char * GetKey()
	{
		return GetDesktopFileName().HasContent() ?
			uni_stripdup(GetDesktopFileName().CStr()) :
			uni_stripdup(m_command.CStr());
	}

	BOOL HasCommand(const OpStringC & comm)
	{
		if(comm.IsEmpty() || m_command.HasContent())
			return FALSE;
		return (uni_strncmp(comm.CStr(), m_command.CStr(), uni_strlen(comm.CStr())) == 0);
	}

	BOOL IsExecutable();

    virtual void SetName(const OpStringC & name);

private:
	//Invariant : (m_desktop_file_name || m_command) - ie. both cannot be null
	OpString m_path;              //Can be null if read from mailcap
	OpString m_command;           //Can be null if read from mimeinfo.cache or defaults.list
	OpString m_clean_command;     //Can be null if read from mimeinfo.cache or defaults.list
	OpString m_try_exec;          //Can be null if read from mimeinfo.cache or defaults.list or might not be present at all

	OpString m_default_path;

	BOOL              m_in_terminal;
	BOOL3             m_takes_urls;
	BOOL3             m_is_executable;
};

#endif //__FILEHANDLER_APPLICATION_NODE_H__
