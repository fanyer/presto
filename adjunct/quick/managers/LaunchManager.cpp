// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Adam Minchinton
//

#include "core/pch.h"

#include "adjunct/quick/managers/LaunchManager.h"
#include "adjunct/desktop_pi/launch_pi.h"

/////////////////////////////////////////////////////////////////////////
//
// NOTE: You CANNOT use any core functionallity in the LaunchManager as 
//		 it is used outside of core.
//
/////////////////////////////////////////////////////////////////////////

// Static member (a global really :))
LaunchManager *LaunchManager::m_launch_manager = NULL;
 
//////////////////////////////////////////////////////////////////////////////
 
LaunchManager *LaunchManager::GetInstance()
{
	if (!m_launch_manager)
		m_launch_manager = new LaunchManager();
	return m_launch_manager;
}
 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void LaunchManager::Destroy()
{
	delete m_launch_manager;
	m_launch_manager = NULL;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL LaunchManager::SetInstance(LaunchManager *launch_manager) 
{ 
	// Fail if there is nothing to set or it's set already
	if (!launch_manager || m_launch_manager)
		return FALSE;

	m_launch_manager = launch_manager; 
	
	return TRUE;
}
 
//////////////////////////////////////////////////////////////////////////////
 
LaunchManager::LaunchManager() :
	m_launch_pi(NULL),
	m_argc(0),
	m_argv(NULL),
	m_exit_program(0),
	m_update_path(NULL)
{
	m_launch_pi = LaunchPI::Create();
}
 
//////////////////////////////////////////////////////////////////////////////
 
LaunchManager::~LaunchManager()
{
	delete m_launch_pi;
	DeleteArgvArray();
	delete [] m_update_path;
}

//////////////////////////////////////////////////////////////////////////////

BOOL LaunchManager::Launch(const uni_char *exe_name, int argc, const char* const* argv)
{
	if (m_launch_pi)
	{
#ifndef _DEBUG	
		if(m_launch_pi->VerifyExecutable(exe_name))
#endif		
			return m_launch_pi->Launch(exe_name, argc, argv);
	}
		
	return FALSE;
}

//////////////////////////////////////////////////////////////////////////////

BOOL LaunchManager::SetOriginalArgs(int argc, const char* const* argv)
{
	BOOL rtn = TRUE;

	m_argc = argc;

	if (m_argv)
		DeleteArgvArray();
		
	if (m_argc)
	{
		m_argv = new char*[m_argc];
		if (m_argv)
		{
			// Loop the command line args to find out how much memory is required
			for (int i = 0; i < m_argc; i++)
			{
				m_argv[i] = new char[op_strlen(argv[i]) + 1];
				if (m_argv[i])
				{
					op_strcpy(m_argv[i], argv[i]);
				}
			}
		}
	}
	
	// Clean up if there was a problem
	if (!rtn)
		DeleteArgvArray();

	return rtn;
}

//////////////////////////////////////////////////////////////////////////////

void LaunchManager::LaunchAutoupdateOnExit(const uni_char* path) 
{ 
	if (path)
	{
		const uni_char *p = path;
		int			len = 0;

		// Get the length of the string
		while (*p)
		{
			p++;
			len++;
		}
			
		if (len > 0)
		{
			if (m_update_path)
			{
				delete [] m_update_path;
				m_update_path = NULL;
			}

			m_update_path = new uni_char[len + 1];
			if (m_update_path)
			{
				// Save the name in the internal buffer
				op_memcpy(m_update_path, path, (len + 1) * sizeof(uni_char)); 

				m_exit_program |= AutoUpdate; 
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////

BOOL LaunchManager::LaunchOnExitApplication() 
{ 
	// Check if the autoupdate is set
	if ((m_exit_program & AutoUpdate) && m_update_path)
		return Launch(m_update_path, m_argc, (const char **)m_argv); 

	// When more types of relaunchs

	// Nothing to launch so so it's OK!
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////////

BOOL LaunchManager::LaunchOnExitApplication(const uni_char* update_path)
{ 
	// Check if the autoupdate is set
	if ((m_exit_program & AutoUpdate) && update_path)
		return Launch(update_path, m_argc, (const char **)m_argv); 

	// When more types of relaunchs

	// Nothing to launch so so it's OK!
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////////

void LaunchManager::DeleteArgvArray()
{
	for (int i = 0; i < m_argc; i++)
	{
		delete [] m_argv[i];
		m_argv[i] = NULL;
	}
	delete [] m_argv;
	m_argv = NULL;
}

//////////////////////////////////////////////////////////////////////////////

uni_char* LaunchManager::GetUpdatePath()
{
	if(!m_update_path)
		return NULL;

	const uni_char *p = m_update_path;
	int	len = 0;

	// Get the length of the string
	while (*p)
	{
		p++;
		len++;
	}

	uni_char* update_path = new uni_char[len + 1];
	if (update_path)
	{
		op_memcpy(update_path, m_update_path, (len + 1) * sizeof(uni_char)); 
	}
	return update_path;
}

//////////////////////////////////////////////////////////////////////////////
