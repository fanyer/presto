// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2008 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Adam Minchinton
//
 
#ifndef __LAUNCH_MANAGER_H__
#define __LAUNCH_MANAGER_H__

/////////////////////////////////////////////////////////////////////////
//
// NOTE: You CANNOT use any core functionallity in the LaunchManager as 
//		 it is used outside of core.
//
/////////////////////////////////////////////////////////////////////////

// Redefine for a more Opera like name
#define g_launch_manager (LaunchManager::GetInstance())

class LaunchPI;
 
/////////////////////////////////////////////////////////////////////////
 
class LaunchManager
{
public:
	static LaunchManager *GetInstance();
	static void Destroy();

	/**
	 * Allows the setting of the launch manager object. Should only be 
	 * used on platforms which have seperate binary/frameworks so that a
	 * single LaunchManager can be shared
	 *
	 * @param launch_manager	Pointer to a launch manager object
	 *
	 * @return BOOL, TRUE if successful
	 */
	static BOOL SetInstance(LaunchManager *launch_manager);
 
	/**
	 * Executes the program file with the command line passed
	 *
	 * @param exe_name	Full path of file to read in UTF16
	 * @param cmd_line	Command line to pass to the application UTF8
	 *
	 * @return BOOL, TRUE if successful
	 */
	BOOL Launch(const uni_char *exe_name, int argc, const char * const *argv);

	/**
	 * Sets the command line arguments to use when starting another 
	 * application on exit of this application
	 *
	 * This function will ignore the first argument assuming it's the
	 * name of the application
	 *
	 * @param argc	argc, number of arguments
	 * @param argv	argv array from the application in UTF8
	 *
	 * @return BOOL, TRUE if successful
	 */
	BOOL SetOriginalArgs(int argc, const char* const* argv);

	/**
	 * Returns argc set with SetOriginalArgs
	 *
	 * @return int, argc
	 */
	int	GetArgc() { return m_argc; }

	/**
	 * Returns argv set with SetOriginalArgs
	 *
	 * @return const char* const* , argv
	 */
	const char* const* GetArgv() { return m_argv; }

	/**
	 * Returns the update path
	 *
	 * @return uni_char*
	 */
	uni_char* GetUpdatePath();

	/**
	 * Calling this will enter the autoupdate update program to be
	 * run on exit. 
	 *
	 * @param path Full path to update.exe to run UTF16
	 *
	 * @return BOOL, TRUE if successful
	 */
	void LaunchAutoupdateOnExit(const uni_char* path);

	/**
	 * Called as the final step of a progam shutdown and will 
	 * launch another program at this point if one has been set
	 *
	 * @return BOOL, TRUE if successful
	 */
	BOOL LaunchOnExitApplication();

	/**
	 * Called as the final step of a progam shutdown and will 
	 * launch the specified program
	 *
	 * @return BOOL, TRUE if successful
	 */
	BOOL LaunchOnExitApplication(const uni_char* update_path);
private:
	// Flag that is used to define what programs to check on shutdown
	enum LM_ExitProgramType
	{
		AutoUpdate = 0x00000001
	};

	LaunchManager();
	~LaunchManager();

	void DeleteArgvArray();
	
	LaunchPI *m_launch_pi;			// Launch PI for platform launching code

	// Original argc and argv arguments
	int		 m_argc;
	char	 **m_argv;

	INT32    m_exit_program;		// Bit field holding all of the programs that may want to run when Opera exits

	uni_char *m_update_path;		// Path to the autoupdate update.exe to run on exit

	static LaunchManager *m_launch_manager;
};
 
#endif // __LAUNCH_MANAGER_H__
