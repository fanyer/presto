// Format is: {section, key, oldpath, oldparent, newpath, newparent}
// if section and /or key is missing (NULL), the relevant path will not be removed from the preferences.
// if newpath is missing (NULL) the file will not be copied to its new location.

// Order is important in this file as the newer moves should be done before the older ones so the newest stuff if
// it exists is copied before the older stuff. If newer stuff is copied first the older stuff won't copy over

// Fix the main folder path prefs
{"User Prefs",	"Opera Directory",					UNI_L(""),						OPFILE_HOME_FOLDER, NULL,							OPFILE_ABSOLUTE_FOLDER},
{"User Prefs",	"Opera Local Directory",			UNI_L(""),						OPFILE_LOCAL_HOME_FOLDER, NULL,						OPFILE_ABSOLUTE_FOLDER},

// All the folders that have moved from the profile location to the folder above
{"User Prefs",	"Keyboard Configuration",			UNI_L("keyboard"),						OPFILE_HOME_FOLDER,			UNI_L("keyboard"),				OPFILE_HOME_FOLDER},
{"User Prefs",	"Menu Configuration",				UNI_L("menu"),							OPFILE_HOME_FOLDER,			UNI_L("menu"),					OPFILE_HOME_FOLDER},
{"User Prefs",	"Mouse Configuration",				UNI_L("mouse"),							OPFILE_HOME_FOLDER,			UNI_L("mouse"),					OPFILE_HOME_FOLDER},
{"User Prefs",	"Automatic Window Storage File",	UNI_L("sessions"),						OPFILE_HOME_FOLDER,			UNI_L("sessions"),				OPFILE_HOME_FOLDER},
{"User Prefs",	"Windows Storage File",				UNI_L("sessions"),						OPFILE_HOME_FOLDER,			UNI_L("sessions"),				OPFILE_HOME_FOLDER},
{"User Prefs",	"Button Set",						UNI_L("skin"),							OPFILE_HOME_FOLDER,			UNI_L("skin"),					OPFILE_HOME_FOLDER},
{NULL,			NULL,								UNI_L("styles"),						OPFILE_HOME_FOLDER,			UNI_L("styles"),				OPFILE_HOME_FOLDER},
{"User Prefs",	"Toolbar Configuration",			UNI_L("toolbar"),						OPFILE_HOME_FOLDER,			UNI_L("toolbar"),				OPFILE_HOME_FOLDER},
{"User Prefs",	"Voice Configuration",				UNI_L("voice"),							OPFILE_HOME_FOLDER,			UNI_L("voice"),					OPFILE_HOME_FOLDER},
{NULL,			NULL,								UNI_L("webfeeds"),						OPFILE_HOME_FOLDER,			UNI_L("webfeeds"),				OPFILE_HOME_FOLDER},

{NULL,			NULL,								UNI_L("bt_metadata"),					OPFILE_LOCAL_HOME_FOLDER,	UNI_L("bt_metadata"),		OPFILE_LOCAL_HOME_FOLDER},
{NULL,			NULL,								UNI_L("images"),						OPFILE_LOCAL_HOME_FOLDER,	UNI_L("icons"),				OPFILE_LOCAL_HOME_FOLDER},
{NULL,			NULL,								UNI_L("voicelib"),						OPFILE_LOCAL_HOME_FOLDER,	UNI_L("voicelib"),			OPFILE_LOCAL_HOME_FOLDER},
{NULL,			NULL,								UNI_L("vps"),							OPFILE_LOCAL_HOME_FOLDER,	UNI_L("vps"),				OPFILE_LOCAL_HOME_FOLDER},

{NULL,			NULL,								UNI_L("thumbnails"),					OPFILE_HOME_FOLDER,			UNI_L("thumbnails"),		OPFILE_CACHE_HOME_FOLDER},

// Move for the old location when everything was in roaming
{NULL,			NULL,								UNI_L("bt_metadata"),					OPFILE_HOME_FOLDER,			UNI_L("bt_metadata"),		OPFILE_LOCAL_HOME_FOLDER},
{NULL,			NULL,								UNI_L("images"),						OPFILE_HOME_FOLDER,			UNI_L("icons"),				OPFILE_LOCAL_HOME_FOLDER},
{NULL,			NULL,								UNI_L("voicelib"),						OPFILE_HOME_FOLDER,			UNI_L("voicelib"),			OPFILE_LOCAL_HOME_FOLDER},
{NULL,			NULL,								UNI_L("vps"),							OPFILE_HOME_FOLDER,			UNI_L("vps"),				OPFILE_LOCAL_HOME_FOLDER},

// The mail folder is a big move but if we can we should move it
{"Mail",		"Mail Root Directory",				UNI_L("mail"),							OPFILE_HOME_FOLDER,			UNI_L("mail"),				OPFILE_LOCAL_HOME_FOLDER},

// Files that may have been created in the intermediate new resources before it was all done
// These need to be done first so that if they exist they are copied otherwise the old files will be copied later
{"User Prefs",	"Global History File",				UNI_L("global_history.dat"),			OPFILE_HOME_FOLDER, UNI_L("global_history.dat"),	OPFILE_HOME_FOLDER},
{"User Prefs",	"Hot List File Ver2",				UNI_L("bookmarks.adr"),					OPFILE_HOME_FOLDER, UNI_L("bookmarks.adr"),			OPFILE_HOME_FOLDER},
{"User Prefs",	"Direct History File",				UNI_L("typed_history.xml"),				OPFILE_HOME_FOLDER, UNI_L("typed_history.xml"),		OPFILE_HOME_FOLDER},
{"User Prefs",	"Overrides File",					UNI_L("override.ini"),					OPFILE_HOME_FOLDER, UNI_L("override.ini"),			OPFILE_HOME_FOLDER},

// Files in the profile folder moving (they all need to be listed as the widgets folder is NOT moving)
{NULL,			NULL,								UNI_L("widgets\\widgets.dat"),			OPFILE_HOME_FOLDER, UNI_L("widgets\\widgets.dat"),			OPFILE_LOCAL_HOME_FOLDER},
{"MailBox",		"ContactsFile",						UNI_L("browser.js"),					OPFILE_HOME_FOLDER, UNI_L("browser.js"),					OPFILE_HOME_FOLDER},
{"MailBox",		"ContactsFile",						UNI_L("contacts.adr"),					OPFILE_HOME_FOLDER, UNI_L("contacts.adr"),					OPFILE_HOME_FOLDER},
{NULL,			NULL,								UNI_L("cookies4.dat"),					OPFILE_HOME_FOLDER, UNI_L("cookies4.dat"),					OPFILE_HOME_FOLDER},
{NULL,			NULL,								UNI_L("download.dat"),					OPFILE_HOME_FOLDER, UNI_L("download.dat"),					OPFILE_HOME_FOLDER},
{"User Prefs",	"Global History File",				UNI_L("global.dat"),					OPFILE_HOME_FOLDER, UNI_L("global_history.dat"),			OPFILE_HOME_FOLDER},
{NULL,			NULL,								UNI_L("link_queue_myopera.dat"),		OPFILE_HOME_FOLDER, UNI_L("link_queue_myopera.dat"),		OPFILE_HOME_FOLDER},
{NULL,			NULL,								UNI_L("link_queue_out_myopera.dat"),	OPFILE_HOME_FOLDER, UNI_L("link_queue_out_myopera.dat"),	OPFILE_HOME_FOLDER},
{"MailBox",		"NotesFile",						UNI_L("notes.adr"),						OPFILE_HOME_FOLDER, UNI_L("notes.adr"),						OPFILE_HOME_FOLDER},
{NULL,			NULL,								UNI_L("opcacrt6.dat"),					OPFILE_HOME_FOLDER, UNI_L("opcacrt6.dat"),					OPFILE_HOME_FOLDER},
{NULL,			NULL,								UNI_L("opcert6.dat"),					OPFILE_HOME_FOLDER, UNI_L("opcert6.dat"),					OPFILE_HOME_FOLDER},
{"User Prefs",	"Hot List File Ver2",				UNI_L("opera6.adr"),					OPFILE_HOME_FOLDER, UNI_L("bookmarks.adr"),					OPFILE_HOME_FOLDER},
{"User Prefs",	"Direct History File",				UNI_L("opera.dir"),						OPFILE_HOME_FOLDER, UNI_L("typed_history.xml"),				OPFILE_HOME_FOLDER},
{NULL,			NULL,								UNI_L("opicacrt6.dat"),					OPFILE_HOME_FOLDER, UNI_L("opicacrt6.dat"),					OPFILE_HOME_FOLDER},
{NULL,			NULL,								UNI_L("oprand.dat"),					OPFILE_HOME_FOLDER, UNI_L("oprand.dat"),					OPFILE_HOME_FOLDER},
{NULL,			NULL,								UNI_L("opssl6.dat"),					OPFILE_HOME_FOLDER, UNI_L("opssl6.dat"),					OPFILE_HOME_FOLDER},
{NULL,			NULL,								UNI_L("optrust.dat"),					OPFILE_HOME_FOLDER, UNI_L("optrust.dat"),					OPFILE_HOME_FOLDER},
{NULL,			NULL,								UNI_L("opuntrust.dat"),					OPFILE_HOME_FOLDER, UNI_L("opuntrust.dat"),					OPFILE_HOME_FOLDER},
{"User Prefs",	"Overrides File",					UNI_L("override.ini"),					OPFILE_HOME_FOLDER, UNI_L("override.ini"),					OPFILE_HOME_FOLDER},
{"User Prefs",	"Downloaded Overrides File",		UNI_L("override_downloaded.ini"),		OPFILE_HOME_FOLDER, NULL,									OPFILE_ABSOLUTE_FOLDER},
{NULL,			NULL,								UNI_L("search.ini"),					OPFILE_HOME_FOLDER, UNI_L("search.ini"),					OPFILE_HOME_FOLDER},
{"User Prefs",	"Speed Dial File",					UNI_L("speeddial.ini"),					OPFILE_HOME_FOLDER, UNI_L("speeddial.ini"),					OPFILE_HOME_FOLDER},
{"Network",		"URL Filter File",					UNI_L("urlfilter.ini"),					OPFILE_HOME_FOLDER, UNI_L("urlfilter.ini"),					OPFILE_HOME_FOLDER},
{NULL,			NULL,								UNI_L("vlink4.dat"),					OPFILE_HOME_FOLDER, UNI_L("vlink4.dat"),					OPFILE_HOME_FOLDER},
{"User Prefs",	"Wand Storage File",				UNI_L("wand.dat"),						OPFILE_HOME_FOLDER, UNI_L("wand.dat"),						OPFILE_HOME_FOLDER},
{"Widgets",		"Widgets File",						UNI_L("widgets.adr"),					OPFILE_HOME_FOLDER, UNI_L("widgets.adr"),					OPFILE_HOME_FOLDER},
{NULL,			NULL,								UNI_L("unite.adr"),						OPFILE_HOME_FOLDER, UNI_L("unite.adr"),						OPFILE_HOME_FOLDER},
{NULL,			NULL,								UNI_L("opthumb.dat"),					OPFILE_HOME_FOLDER, UNI_L("opthumb.dat"),					OPFILE_HOME_FOLDER},

