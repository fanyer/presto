// Format is: {section, key, oldpath, oldparent, newpath, newparent}
// if section and /or key is missing (NULL), the relevant path will not be removed from the preferences.
// if newpath is missing (NULL) the file will not be copied to its new location.

{"User Prefs",	"Direct History File",		UNI_L("Opera Direct History"),			OPFILE_HOME_FOLDER, UNI_L("typed_history.xml"),			OPFILE_HOME_FOLDER},
{"User Prefs",	"Global History File",		UNI_L("Opera Global History"),			OPFILE_HOME_FOLDER, UNI_L("global_history.dat"),		OPFILE_HOME_FOLDER},
{"User Prefs",	"Overrides File", 			UNI_L("Site Preferences"),				OPFILE_HOME_FOLDER, UNI_L("override.ini"),				OPFILE_HOME_FOLDER},
{"User Prefs",	"Downloaded Overrides File",UNI_L("Site Preferences Downloaded"),	OPFILE_HOME_FOLDER, NULL,								OPFILE_ABSOLUTE_FOLDER},
{"User Prefs",	"Hot List File Ver2",		UNI_L("Bookmarks"),						OPFILE_HOME_FOLDER, UNI_L("bookmarks.adr"),				OPFILE_HOME_FOLDER},

{"User Prefs",	"Toolbar Configuration",	UNI_L("Toolbars"),						OPFILE_HOME_FOLDER, UNI_L("toolbar"),					OPFILE_HOME_FOLDER},
