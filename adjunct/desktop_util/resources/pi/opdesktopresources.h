#ifndef OPDESKTOPRESOURCES_H
#define OPDESKTOPRESOURCES_H

class OpFolderLister;

class OpDesktopResources
{
public:
	virtual ~OpDesktopResources() {}
	
	static OP_STATUS Create(OpDesktopResources **newObj);

	/*
	 * GetResourceFolder
	 *
	 * Gets the path to the resource root folder inside the package
	 *
	 * @param folder  returns retrieved folder
	 *
	 * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	 */
	virtual OP_STATUS GetResourceFolder(OpString &folder) = 0;

	/*
	 * GetBinaryResourceFolder
	 * 
	 * Gets the path to the binary resource root folder inside the package
	 * eg. the folder where libraries etc. are stored
	 * (can be the same as GetResourceFolder)
	 *
	 * @param folder  returns retrieved folder
	 *
	 * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	 */
	virtual OP_STATUS GetBinaryResourceFolder(OpString &folder) = 0;

	/*
	 * GetFixedPrefFolder
	 *
	 * Fixed preferences folder
	 *
	 * @param folder  returns retrieved folder
	 *
	 * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	 */
	virtual OP_STATUS GetFixedPrefFolder(OpString &folder) = 0;
	/*
	 * GetGlobalPrefFolder
	 *
	 * Global Fixed preferences folder
	 *
	 * @param folder  returns retrieved folder
	 *
	 * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	 */
	virtual OP_STATUS GetGlobalPrefFolder(OpString &folder) = 0;
	/*
	 * GetLargePrefFolder
	 *
	 * Gets the path to the user preferences that holds large preference data
	 * e.g. Mail, Widgets, Unite Services
	 *
	 * @param folder  returns retrieved folder
	 * @param profile_name  pointer to profile folder to use instead of the
	 *						standard "Opera" one
	 *
	 * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	 */
	virtual OP_STATUS GetLargePrefFolder(OpString &folder, const uni_char *profile_name = NULL) = 0;
	
	/*
	 * GetSmallPrefFolder
	 *
	 * Gets the path to the user preferences that holds small preference data
	 * e.g. Ini files, skins, history, etc
	 *
	 * @param folder  returns retrieved folder
	 * @param profile_name  pointer to profile folder to use instead of the
	 *						standard "Opera" one
	 *
	 * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	 */
	virtual OP_STATUS GetSmallPrefFolder(OpString &folder, const uni_char *profile_name = NULL) = 0;

	/*
	 * GetTempPrefFolder
	 *
	 * Gets the path to the user preferences that holds the temp preference data
	 * e.g. Cache, Temporary Downloads
	 *
	 * @param folder  returns retrieved folder
	 * @param profile_name  pointer to profile folder to use instead of the
	 *						standard "Opera" one
	 *
	 * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	 */
	virtual OP_STATUS GetTempPrefFolder(OpString &folder, const uni_char *profile_name = NULL) = 0;

	/*
	 * GetTempFolder
	 *
	 * Gets the path to the temp folder you want to use for Opera
	 *
	 * @param folder  returns retrieved folder
	 *
	 * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	 */
	virtual OP_STATUS GetTempFolder(OpString &folder) = 0;

	/*
	 * GetDesktopFolder
	 *
	 * Gets the path to the desktop folder on the platform
	 *
	 * @param folder  returns retrieved folder
	 *
	 * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	 */
	virtual OP_STATUS GetDesktopFolder(OpString &folder) = 0;

	/*
	 * GetDocumentsFolder
	 *
	 * Gets the path to the documents folder you want to use for Opera
	 * This is used for the start location of Open/Save dialogs
	 *
	 * @param folder  returns retrieved folder
	 *
	 * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	 */
	virtual OP_STATUS GetDocumentsFolder(OpString &folder) = 0;

	/*
	 * GetDownloadsFolder
	 *
	 * Gets the path to the self downloads folder you want to use for Opera
	 * Used for the download default location 
	 *
	 * @param folder  returns retrieved folder
	 *
	 * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	 */
	virtual OP_STATUS GetDownloadsFolder(OpString &folder) = 0;
	
	/*
	 * GetPicturesFolder
	 *
	 * Gets the path to the pictures/photos folder on the platform
	 *
	 * @param folder  returns retrieved folder
	 *
	 * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	 */
	virtual OP_STATUS GetPicturesFolder(OpString &folder) = 0;
	
	/*
	 * GetVideosFolder
	 *
	 * Gets the path to the videos folder on the platform
	 *
	 * @param folder  returns retrieved folder
	 *
	 * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	 */
	virtual OP_STATUS GetVideosFolder(OpString &folder) = 0;
	
	/*
	 * GetMusicFolder
	 *
	 * Gets the path to the music folder on the platform
	 *
	 * @param folder  returns retrieved folder
	 *
	 * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	 */
	virtual OP_STATUS GetMusicFolder(OpString &folder) = 0;

	/*
	 * GetPublicFolder
	 *
	 * Gets the path to the public folder on the platform
	 *
	 * @param folder  returns retrieved folder
	 *
	 * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	 */
	virtual OP_STATUS GetPublicFolder(OpString &folder) = 0;

	/*
	 * GetDefaultShareFolder
	 *
	 * Gets the path to the default share folder on the platform
	 *
	 * @param folder  returns retrieved folder
	 *
	 * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	 */
	virtual OP_STATUS GetDefaultShareFolder(OpString &folder) = 0;
	
	/*
	 * GetLocaleFolder
	 *
	 * Gets the path to the locale folder (i.e. root of where all the language
	 * folders are)
	 *
	 * @param folder  returns retrieved folder
	 *
	 * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	 */
	virtual OP_STATUS GetLocaleFolder(OpString &folder) = 0;
	
	/*
	 * GetLocaleFolders
	 *
	 * Get a OpFolderLister with all the actual locale folders inside the locale root folder
	 * Caller is responsible for deleting the folder lister
	 *
	 * @param Full path to the locale root folder
	 *
	 * @return A constructed list of folders with localization files or NULL on failure
	 */
	virtual OpFolderLister* GetLocaleFolders(const uni_char* locale_root) = 0;

	enum LanguageFolderType
	{
		LOCALE,        //< language folder inside locale folder
		CUSTOM_LOCALE, //< language folder inside custom/locale folder
		REGION         //< language folder inside region or custom/region folder
	};

	/*
	 * GetLanguageFolderName
	 *
	 * Gets the name of the folder for the current language set. This should find
	 * the setting based on the preference first (as set by the installer) and if
	 * this doesn't exist take the system language
	 *
	 * @param folder_name  returns the name of the language folder
	 * @param type         type of requested language folder
	 * @param lang_code    language code to build the folder with, if
	 *                     excluded then we need follow the above logic
	 *
	 * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	 */
	virtual OP_STATUS GetLanguageFolderName(OpString &folder_name, LanguageFolderType type, OpStringC lang_code = UNI_L("")) = 0;

	/*
	 * GetLanguageFileName
	 *
	 * Gets the name of the language file for the current language set. This should find
	 * the setting based on the preference first (as set by the installer) and if
	 * this doesn't exist take the system language
	 *
	 * @param file_name  returns the name of the language file
	 * @param lang_code  language code to build the name with, if
	 *                   excluded then we need follow the above logic
	 */
	virtual OP_STATUS GetLanguageFileName(OpString& file_name, OpStringC lang_code = UNI_L("")) = 0;

	/*
	 * GetCountryCode
	 *
	 * Gets user's country code in 2-letter ISO 3166-1 format. Returned value is
	 * used to construct path to folder with default regional customization files
	 * (speed dials, bookmarks, etc.). It can be the same as value returned by
	 * OpSystemInfo::GetUserCountry
	 *
	 * @param country_code returns the country code
	 *
	 * @return OK on success, ERR_NO_MEMORY on OOM, ERR on other errors
	 */
	virtual OP_STATUS GetCountryCode(OpString& country_code) = 0;

	/*
	 * EnsureCacheFolderIsAvailable
	 *
	 * On some platforms we need to have multiple cache folders and if one Opera,
	 * installation is using a a cache folder then a new unique folder should be generated
	 * Only implement this function if your platform requires this functionallity
	 *
	 * @param folder  pass in current cache folder returns a new cache folder if needed
	 *
	 * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	 */
	virtual OP_STATUS EnsureCacheFolderIsAvailable(OpString &folder) = 0;

	
	/** 
	 * Expand system variables (also known as "environment variables").
	 *
	 * @param in (input) A string potentially containing one or more
	 * system variables (e.g. "$TERM -e $EDITOR /tmp/foo")
	 * @param out (output) Pre-allocated buffer which will be set to the string
	 * with all the system variables resolved (e.g. "xterm -e vi /tmp/foo"). If
	 * OpStatus::OK is returned, this will be a NUL-terminated string. The
	 * string may be empty. For any other return values, the contents of this
	 * buffer will be undefined.
	 * @param out_max_len 'out' buffer capacity - maximum number of bytes to
	 * write, including NUL terminator
	 *
	 * @return OK if expanding succeeded, ERR_NO_MEMORY on OOM, ERR if other
	 * errors occurred, such as parse errors.
	 */
	virtual OP_STATUS ExpandSystemVariablesInString(const uni_char* in, uni_char* out, INT32 out_max_len) = 0;

	/**
	 * Get a serialized version of the filename, to put into the preferences. It should be compatible with 
	 * ExpandSystemVariablesInString() (see above).
	 *
	 * @param in	full path.
	 * @param out	Preallocated buffer for the serialized filename.
	 * @param out_max_len	The lenght of the out buffer, including nul-termination.
	 */
	virtual OP_STATUS SerializeFileName(const uni_char* in, uni_char* out, INT32 out_max_len) = 0;

	/*
	 * Returns whether the paths point to the same volume. This is used to avoid doing potentially time-consuming copies.
	 */
	virtual BOOL IsSameVolume(const uni_char* path1, const uni_char* path2) = 0;

	/*
	 * Gets a subfolder path pointing to the previous profile location, relatively to the
	 * new profile location.
	 */
	virtual OP_STATUS GetOldProfileLocation(OpString& old_profile_path) = 0;

#ifdef WIDGET_RUNTIME_SUPPORT
	/*
	 * GetGadgetsResourceFolder
	 *
	 * Gets the path to default location for gadgets to be installed into
	 *
	 * @param folder  returns retrieved folder
	 *
	 * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	 */
	virtual OP_STATUS GetGadgetsFolder(OpString &folder) = 0;
#endif // WIDGET_RUNTIME_SUPPORT

	/**
	 * Gets localization of the update checker executable.
	 *
	 * @param checker_path on return contains the path to the checker.
	 *
	 * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	 */
	virtual OP_STATUS GetUpdateCheckerPath(OpString &checker_path) = 0;
};

#endif // OPDESKTOPRESOURCES_H
