OPFILE_MENUSETUP_FOLDER,			///< Location of menu setups.
OPFILE_TOOLBARSETUP_FOLDER,			///< Location of toolbar setups.
OPFILE_MOUSESETUP_FOLDER,			///< Location of mouse configurations.
OPFILE_KEYBOARDSETUP_FOLDER,		///< Location of keyboard configurations.
OPFILE_BUTTON_FOLDER,				// Folder containing buttons.
OPFILE_ICON_FOLDER,					// Folder for storing icons (favicons).
OPFILE_DESKTOP_FOLDER,				// Folder where the user's desktop files are kept.
#ifdef USE_COMMON_RESOURCES
OPFILE_CACHE_HOME_FOLDER,			// Read/Write user prefs for cache/temp data 
OPFILE_LOCALE_FOLDER,				// Parent folder for all the language folders
OPFILE_DOCUMENTS_FOLDER,			// Documents folder on the platform
OPFILE_AUX_FOLDER,					// Package folder for stuff that just doesn't fit anywhere
OPFILE_CUSTOM_FOLDER,				// Custom folder in user prefs for the customised build data
OPFILE_CUSTOM_PACKAGE_FOLDER,		// Custom folder in the package for the customised build data
OPFILE_DEFAULT_LANGUAGE_FOLDER,		// Folder where the default language file is to be found
#ifndef UTIL_CAP_OPFILE_INI_CUSTOM_FOLDER
OPFILE_INI_CUSTOM_FOLDER,			// Custom folder in the package for the default files (e.g. search.ini)
#endif // !UTIL_CAP_OPFILE_INI_CUSTOM_FOLDER
OPFILE_LOCALE_CUSTOM_FOLDER,		// Custom folder in user prefs for customized locale changes
OPFILE_LANGUAGE_CUSTOM_FOLDER,		// Custom folder in the package for the current language (e.g. search.ini)
OPFILE_LANGUAGE_CUSTOM_PACKAGE_FOLDER, // Custom folder in the package for customized locale changes
OPFILE_REGION_ROOT_FOLDER,          //< Read-Only: Parent folder for all region folders
OPFILE_REGION_FOLDER,               //< Read-Only: Folder with customizations for the current region
OPFILE_REGION_LANGUAGE_FOLDER,      //< Read-Only: Folder with customizations for the current region and language
OPFILE_REGION_ROOT_CUSTOM_PACKAGE_FOLDER, //< Read-Only: Parent folder in the package for all custom region folders
OPFILE_REGION_CUSTOM_PACKAGE_FOLDER,  //< Read-Only: Custom folder in the package for the current region (e.g. search.ini)
OPFILE_REGION_LANGUAGE_CUSTOM_PACKAGE_FOLDER, //< Read-Only: Custom folder in the package for the current region and language (e.g. search.ini)
OPFILE_REGION_ROOT_CUSTOM_FOLDER,       //< Read-Write: Custom folder in user prefs for all region folders
OPFILE_REGION_CUSTOM_FOLDER,        //< Read-Write: Custom folder in user prefs for the current region (e.g. search.ini)
OPFILE_REGION_LANGUAGE_CUSTOM_FOLDER, //< Read-Write: Custom folder in user prefs for the current region and language (e.g. search.ini)
OPFILE_DEFAULT_REGION_LANGUAGE_FOLDER,	//< Read-Only: Folder with customizations for current region's default language
#endif // USE_COMMON_RESOURCES
#ifdef WEBSERVER_SUPPORT
OPFILE_UNITE_CUSTOM_PACKAGE_FOLDER,	//< Read-Only: Location of package containing unite services to be installed on first run.
#endif // WEBSERVER_SUPPORT
#ifdef WIDGET_RUNTIME_SUPPORT
OPFILE_GADGETS_CUSTOM_FOLDER,		//< Read-Write: widgets delivered with a custom build and copied to user prefs
#endif // WIDGET_RUNTIME_SUPPORT
OPFILE_EXTENSION_CUSTOM_PACKAGE_FOLDER,    // < Read-Only: Location of package containing extensions to be installed on first run.
OPFILE_EXTENSION_CUSTOM_FOLDER,     // < Read-Write: extensions delivered with a custom build and copied to user prefs
#ifdef USE_COMMON_RESOURCES
OPFILE_PICTURES_FOLDER, 			// Pictures/Photo folder on the platform
OPFILE_VIDEOS_FOLDER, 				// Videos folder on the platform
OPFILE_MUSIC_FOLDER, 				// Music folder on the platform
OPFILE_PUBLIC_FOLDER, 				// Public folder on the platform
OPFILE_DEFAULT_SHARED_FOLDER, 		// Default share folder on the platform
#endif // USE_COMMON_RESOURCES
#ifdef WIDGET_RUNTIME_SUPPORT
OPFILE_DEFAULT_GADGET_FOLDER,
#ifdef _UNIX_DESKTOP_
OPFILE_PACKAGE_FOLDER,
#endif // _UNIX_DESKTOP_
#endif // WIDGET_RUNTIME_SUPPORT
OPFILE_BINARY_RESOURCES_FOLDER,		// Main folder for binary, machine-specific resources (libraries etc)
OPFILE_CRASHLOG_FOLDER,				// Where to store crashlogs
#if defined(MSWIN)
OPFILE_JUMPLIST_CACHE_FOLDER,		// Folder used to cache the icons used in the jump list on Windows 7+
#endif // defined(MSWIN)
