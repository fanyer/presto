/*
 *  folderdefines.h
 *  Opera
 *
 *  Created by Adam Minchinton on 12/21/05.
 *  Copyright 2005 Opera. All rights reserved.
 *
 */

#ifdef _MAC_DEBUG
	#define OPERA_APP_SUPPORT_FOLDER_NAME 		UNI_L("Opera Debug")
	#define OPERA_CACHE_FOLDER_NAME				UNI_L("Opera Debug")
	#define OPERA_PREFS_FOLDER_NAME 			UNI_L("Opera Debug")
	#define OPERA_SINGLE_PROFILE_FOLDER_NAME 	UNI_L(".operaprofiledebug")
	#define OPERA_SINGLE_PROFILE_AUTOTEST_FOLDER_NAME 	UNI_L(".operaprofiledebug-autotest")
#else
	#define OPERA_APP_SUPPORT_FOLDER_NAME 		UNI_L("Opera")
	#define OPERA_CACHE_FOLDER_NAME				UNI_L("Opera")
	#define OPERA_PREFS_FOLDER_NAME				UNI_L("Opera")
	#define OPERA_SINGLE_PROFILE_FOLDER_NAME 	UNI_L(".operaprofile")
	#define OPERA_SINGLE_PROFILE_AUTOTEST_FOLDER_NAME 	UNI_L(".operaprofile-autotest")
#endif

#define OPERA_CACHE_LOCKFILE_NAME			"CacheLock"
