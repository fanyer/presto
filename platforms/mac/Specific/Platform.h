// Platform.h
// Define the pathnames to platform-specific files.

#ifndef USE_COMMON_RESOURCES

#ifdef PREFS_CAP_3
# define OLD_PREFERENCES_FILENAME UNI_L("Opera Preferences")
# if defined(UNICODE)
#  define PREFERENCES_FILENAME	UNI_L("Opera 9 Preferences")
#  define GLOBAL_PREFERENCES	UNI_L("Opera 9 Preferences")
#  define FIXED_PREFERENCES		UNI_L("Opera 9 Preferences")
# else
#  define PREFERENCES_FILENAME	OLD_PREFERENCES_FILENAME
#  define GLOBAL_PREFERENCES	UNI_L("Opera Default Preferences")
#  define FIXED_PREFERENCES		UNI_L("Opera Fixed Preferences")
# endif
#endif

#endif // USE_COMMON_RESOURCES
