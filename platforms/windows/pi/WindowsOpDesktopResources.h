#ifndef WINDOWSOPDESKTOPRESOURCES_H
#define WINDOWSOPDESKTOPRESOURCES_H

#include "adjunct/desktop_util/resources/pi/opdesktopresources.h"

class OpFolderLister;

class WindowsOpDesktopResources: public OpDesktopResources
{
public:

	WindowsOpDesktopResources();

	virtual ~WindowsOpDesktopResources() {}
	
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
	virtual OP_STATUS GetResourceFolder(OpString &folder);
	virtual OP_STATUS GetBinaryResourceFolder(OpString &folder) { return GetResourceFolder(folder); }

	/*
	 * GetFixedPrefFolder
	 *
	 * Fixed preferences folder
	 *
	 * @param folder  returns retrieved folder
	 *
	 * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	 */
	virtual OP_STATUS GetFixedPrefFolder(OpString &folder);
	/*
	 * GetGlobalPrefFolder
	 *
	 * Global Fixed preferences folder
	 *
	 * @param folder  returns retrieved folder
	 *
	 * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	 */
	virtual OP_STATUS GetGlobalPrefFolder(OpString &folder);
	/*
	 * GetLargePrefFolder
	 *
	 * Gets the path to the user preferences that holds large preference data
	 * e.g. Mail, Widgets, Unite Services
	 *
	 * @param folder  returns retrieved folder
	 *
	 * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	 */
	virtual OP_STATUS GetLargePrefFolder(OpString &folder, const uni_char *profile_name = NULL);
	
	/*
	 * GetSmallPrefFolder
	 *
	 * Gets the path to the user preferences that holds small preference data
	 * e.g. Ini files, skins, history, etc
	 *
	 * @param folder  returns retrieved folder
	 *
	 * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	 */
	virtual OP_STATUS GetSmallPrefFolder(OpString &folder, const uni_char *profile_name = NULL);

	/*
	 * GetTempPrefFolder
	 *
	 * Gets the path to the user preferences that holds the temp preference data
	 * e.g. Cache, Temporary Downloads
	 *
	 * @param folder  returns retrieved folder
	 *
	 * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	 */
	virtual OP_STATUS GetTempPrefFolder(OpString &folder, const uni_char *profile_name = NULL);

	/*
	 * GetTempFolder
	 *
	 * Gets the path to the temp folder you want to use for Opera
	 *
	 * @param folder  returns retrieved folder
	 *
	 * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	 */
	virtual OP_STATUS GetTempFolder(OpString &folder);

	/*
	 * GetDesktopFolder
	 *
	 * Gets the path to the desktop folder on the platform
	 *
	 * @param folder  returns retrieved folder
	 *
	 * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	 */
	virtual OP_STATUS GetDesktopFolder(OpString &folder);

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
	virtual OP_STATUS GetDocumentsFolder(OpString &folder);

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
	virtual OP_STATUS GetDownloadsFolder(OpString &folder);
	
	/*
	* GetPicturesFolder
	*
	* Gets the path to the pictures/photos folder on the platform
	*
	* @param folder  returns retrieved folder
	*
	* @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	*/
	virtual OP_STATUS GetPicturesFolder(OpString &folder);

	/*
	* GetVideosFolder
	*
	* Gets the path to the videos folder on the platform
	*
	* @param folder  returns retrieved folder
	*
	* @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	*/
	virtual OP_STATUS GetVideosFolder(OpString &folder);

	/*
	* GetMusicFolder
	*
	* Gets the path to the music folder on the platform
	*
	* @param folder  returns retrieved folder
	*
	* @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	*/
	virtual OP_STATUS GetMusicFolder(OpString &folder);


	/*
	* GetPublicFolder
	*
	* Gets the path to the public folder on the platform
	*
	* @param folder  returns retrieved folder
	*
	* @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	*/
	virtual OP_STATUS GetPublicFolder(OpString &folder);

	/*
	* GetDefaultShareFolder
	*
	* Gets the path to the default share folder on the platform
	*
	* @param folder  returns retrieved folder
	*
	* @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	*/
	virtual OP_STATUS GetDefaultShareFolder(OpString &folder);

#ifdef WIDGET_RUNTIME_SUPPORT
	virtual OP_STATUS GetGadgetsFolder(OpString &folder);
#endif // WIDGET_RUNTIME_SUPPORT

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
	virtual OP_STATUS GetLocaleFolder(OpString &folder);
	
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
    virtual OpFolderLister* GetLocaleFolders(const uni_char* locale_root);

	/*
	 * GetLanguageFolderName
	 *
	 * Gets the name of the folder for the current language set
	 *
	 * @param folder_name  returns the name of the language folder
	 * @param type         type of requested language folder
	 * @param lang_code    language code to build the folder with, if
	 *                     excluded then we need follow the above logic
	 *
	 * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	 */
	virtual OP_STATUS GetLanguageFolderName(OpString &folder_name, LanguageFolderType type, OpStringC lang_code = UNI_L(""));

	/*
	 * GetLanguageFileName
	 *
	 * Gets the name of the language file for the current language set
	 *
	 * @param file_name  returns the name of the language file
	 * @param lang_code  language code to build the name with, if
	 *                   excluded then we need follow the above logic
	 */
	virtual OP_STATUS GetLanguageFileName(OpString& file_name, OpStringC lang_code = UNI_L(""));

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
	virtual OP_STATUS GetCountryCode(OpString& country_code);

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
	virtual OP_STATUS EnsureCacheFolderIsAvailable(OpString &folder);
	
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
	virtual OP_STATUS ExpandSystemVariablesInString(const uni_char* in, uni_char* out, INT32 out_max_len);

	virtual OP_STATUS SerializeFileName(const uni_char* in, uni_char* out, INT32 out_max_len);

	/*
	* Returns whether the paths point to the same volume. This is used to avoid doing potentially time-consuming copies.
	*/
	virtual BOOL IsSameVolume(const uni_char* path1, const uni_char* path2);

	/*
	 * Gets a subfolder path pointing to the previous profile location, relatively to the
	 * new profile location.
	 */
	virtual OP_STATUS GetOldProfileLocation(OpString& old_profile_path);

	/*
	 * This is a wrapper for both SHGetKnownFolderPath and SHGetFolderPath
	 * The right method will be chosen based on the operating system
	 */
	static OP_STATUS WindowsOpDesktopResources::GetFolderPath(REFKNOWNFOLDERID Vista_known_folder, int XP_CLSID_folder, OpString& folder, BOOL create = TRUE);

	/*
	 * Gets the path to the user's home folder
	 */
	virtual OP_STATUS GetHomeFolder(OpString &folder);

	virtual OP_STATUS GetUpdateCheckerPath(OpString &checker_path) { return checker_path.Set(m_checker_path); }

private:

	void MergeVirtualStoreProfile();

	OpString m_opera_directory;
	OpString m_checker_path;
	OpString m_opera_last_path_component;
	OpString m_language_directory;
	OpString m_language_code;

	BOOL m_usermultiuserprofiles;
	
};

#endif // WINDOWSOPDESKTOPRESOURCES_H
