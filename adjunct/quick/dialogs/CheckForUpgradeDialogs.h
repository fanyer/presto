/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file CheckForUpdatesDialog.h
 *
 * Contains classes for the dialogs that are used for keeping the user 
 * informed on new updates, as well as a class used for the actual checking.
 * Beware: nothing in these files is thread safe! Threads WILL (probably...)
 * break the system and cause crashes and all sorts of strange behaviour.
 */

#ifndef AUTO_UPDATE_SUPPORT


#ifndef CHECK_FOR_UPGRADE_DIALOGS
#define CHECK_FOR_UPGRADE_DIALOGS

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "modules/windowcommander/OpTransferManager.h"

#ifdef _XML_SUPPORT_
# include "modules/xmlutils/xmlparser.h"
# include "modules/xmlutils/xmltokenhandler.h"
#endif // _XML_SUPPORT_
#include "modules/prefsloader/prefsloadmanager.h"

class NewUpdatesChecker;

/**
 * This dialog is used to notify the user when new upgrades are available. 
 */
class UpgradeAvailableDialog : public Dialog
{
private:
	/** Member to store the version string displayed in the dialog. */
	OpString m_newVersion;

	BOOL m_do_not_show_again_checkbox;
public:
	/**
	 * Constructor that allows certain information about the change in version 
	 * to be passed to the dialog.
	 *
	 * @param major Set to true if the new version is a major upgrade from 
	 * the current
	 * @param newVersion Pass the version string that was found on the web.
	 */
	UpgradeAvailableDialog(OpString &newVersion, BOOL do_not_show_again_checkbox);
	~UpgradeAvailableDialog();

	Type				GetType()				{return DIALOG_TYPE_UPGRADE_AVAILABLE;}
	const char*			GetWindowName()			{return "Upgrade Available Dialog";}
	DialogImage			GetDialogImageByEnum()	{return IMAGE_INFO;}
	void OnInit();
	DialogType			GetDialogType()			{return TYPE_YES_NO;}
	BOOL				GetDoNotShowAgain()		{return m_do_not_show_again_checkbox;}

	UINT32				OnOk();
	void				OnClose(BOOL user_initiated);
};

/**
 * This dialog is used to notify the user that no new upgrade is available.
 */
class NoUpgradeAvailableDialog : public Dialog
{
private:
public:
	NoUpgradeAvailableDialog();
	~NoUpgradeAvailableDialog();

	Type				GetType()				{return DIALOG_TYPE_NO_UPGRADE_AVAILABLE;}
	const char*			GetWindowName()			{return "No Upgrade Available Dialog";}
	DialogImage			GetDialogImageByEnum()	{return IMAGE_INFO;}
	DialogType			GetDialogType()			{return TYPE_OK;}

	void OnInit();
};

/**
 * This dialog is used to notify the user that the update check has been 
 * interrupted by some unspecified error.
 */
class ErrorCheckingForNewUpgradeDialog : public Dialog
{
public:
	enum ErrorType{
		ERROR_DEFAULT,
		ERROR_PARSING,	
		ERROR_TRANSFER,
		ERROR_LANGUAGE_NOT_FOUND
	};
	ErrorCheckingForNewUpgradeDialog()					{m_type = ERROR_DEFAULT;}
	ErrorCheckingForNewUpgradeDialog(ErrorType type)	{m_type = type;}

	Type				GetType()				{return DIALOG_TYPE_ERROR_CHECKING_FOR_UPGRADE;}
	const char*			GetWindowName()			{return "Error Checking For Upgrade Dialog";}
	DialogImage			GetDialogImageByEnum()	{return IMAGE_ERROR;}
	DialogType			GetDialogType()			{return TYPE_OK;}

	void OnInit();
private:
	ErrorType m_type;
};

/**
 * This class contains functionality to get a remote file, and use the 
 * information in this file to check whether a newer release of Opera has 
 * become available.
 *
 * It implements OpTransferlistener so that it can control the downloading of 
 * the document containing the information through callbacks.
 *
 * IMPORTANT: When you call PerformNewUpdatesCheck on an instance of this 
 * class, you will initiate a few call-backs to this class. If you delete the 
 * instance you called PerformNewUpdatesCheck on, the callbacks will 
 * cause trouble, as they are trying to call functions on deleted functions.
 *
 * There is one exception to this, and that is if something fails in 
 * PerformNewUpdatesCheck and no callbacks can be expected. This will be 
 * signalled by a FALSE return value from that function.
 *
 * To avoid a memory leak, it is necessary to keep a reference to the instance
 * of NewUpdatesChecker, and delete it after it has completed its processing 
 * of the remote document. To test when the instance can safely be 
 * deleted, use the function 
 *
 * BOOL CanSafelyBeDeleted()
 *
 * In a multithreaded situation this may still be dangerous, though...
 */
class NewUpdatesChecker : public OpTransferListener
{
private:
	/**
	 * Class for downloading browser.js-files
	 */
	class BrowserJSDownloader : public OpTransferListener
	{
	private:
		OpString m_local_file_name;
		OpTransferItem* m_transferItem;
	public:
		BrowserJSDownloader();
		~BrowserJSDownloader();
		/**
		 * Start downloading browser.js-.
		 */
		void DownloadBrowserJS(const OpStringC &url, const OpStringC &target_file);
		/**
		 * The method handles progress messages from the transfer that the JS downloader
		 * has initiated.
		 */
		void OnProgress(OpTransferItem* transferItem, TransferStatus status);
		// Required to compile.
		void OnReset(OpTransferItem* transferItem) { }
		void OnRedirect(OpTransferItem* transferItem, URL* redirect_from, URL* redirect_to) { }
	};
	/**
	 * Internal class used to parse and compare versions.
	 *
	 * It holds version info on the format 
	 *               [MAJOR]['.'][MINOR]<"beta"<BETANUMBER>>
	 * where MAJOR, MINOR and BETANUMBER are sequences of digits, '.' is a 
	 * period character, and "beta" is a sequence of characters. The square 
	 * brackets are mandatory parts, the angle brackets are optional. (The 
	 * current implementation actually allows b, be, bet and beta as 
	 * beta-version signifying characters, and is not case-sensitive.)
	 *
	 * If the string is not on this format, the parsing will fail, a corrupt 
	 * default state will be set and no comparisons should be made.
	 * ALWAYS check this, using the Corrupt() function.
	 *
	 * Comparison between versions is accomodated through defined operators. 
	 * The comparisons are done in the following order:
	 *
	 * Major - Minor - beta - betanumber.
	 * 
	 * The document structure of the XML that it currently can parse:
	 * Unknown tags in between will be ignored. versions with a lang variable 
	 * that matches the language of the running browser will be read.
	 * What will NOT work is nesting latestversion tags inside one another. 
	 * The class keeps track of only the last seen language attribute, so
	 * some language version may be lost if another latestversion tag is 
	 * placed below it in the tree.
	 *
     * <?xml version="1.0" encoding="utf-8"?>
	 * <versioncheck>
	 *  <latestversion lang="af">6.01</latestversion>
	 *  <latestversion lang="bg">6.06</latestversion>
	 *  <latestversion lang="br-cy">5.02</latestversion>
	 *  <latestversion lang="ca">7.23</latestversion>
	 *  <latestversion lang="cy">6.05</latestversion>
	 *  <latestversion lang="da">7.54</latestversion>
	 *  <latestversion lang="de">7.54</latestversion>
	 *  <latestversion lang="el">6.05</latestversion>
	 *  <latestversion lang="en">7.54</latestversion>
	 *  <latestversion lang="en-GB">7.23</latestversion>
	 *  <latestversion lang="es-ES">7.54</latestversion>
	 *  <latestversion lang="es-LA">7.23</latestversion>
	 *  <latestversion lang="et">6.01</latestversion>
	 *  <latestversion lang="fi">7.23</latestversion>
	 *  <latestversion lang="fr">7.54</latestversion>
	 *  <latestversion lang="gd">6.01</latestversion>
	 *  <latestversion lang="gl">5.12</latestversion>
	 *  <latestversion lang="hu">6.05</latestversion>
	 *  <latestversion lang="id">6.01</latestversion>
	 *  <latestversion lang="is">6.05</latestversion>
	 *  <latestversion lang="it">7.54</latestversion>
	 *  <latestversion lang="ja">7.53</latestversion>
	 *  <latestversion lang="ko">6.06</latestversion>
	 *  <latestversion lang="nb">7.54</latestversion>
	 *  <latestversion lang="nl">7.54</latestversion>
	 *  <latestversion lang="nn">7.54</latestversion>
	 *  <latestversion lang="pl">7.54</latestversion>
	 *  <latestversion lang="pt">6.04</latestversion>
	 *  <latestversion lang="pt-br">7.21</latestversion>
	 *  <latestversion lang="ru">7.23</latestversion>
	 *  <latestversion lang="se">5.02</latestversion>
	 *  <latestversion lang="sl">6.05</latestversion>
	 *  <latestversion lang="sv">7.54</latestversion>
	 *  <latestversion lang="zh-cn">7.23</latestversion>
	 *  <latestversion lang="zh-tw">7.23</latestversion>
	 *</versioncheck>
	 *
	 * It ignores tags that are not recognized during XML-parsing, but 
	 * requires the latestversion tag to be inside of some versioncheck tag.
	 * It will only try to extract information from the first latestversion 
	 * tag in the file.
	 */
	class Version
	: public XMLTokenHandler
	, public XMLParser::Listener
	{
	private:
		/** 
		 * Helper variable that counts the number of encountered versioncheck 
		 * tags during XML parsing. 
		 */
		int m_versionchecks;
		/**
		 * Variable that stores the language of the current latestversion tag found.
		 * (Helps with XML parsing.)
		 */
		OpString m_currentLanguage;
		OpString m_content_string;
		/** String to hold the major part of the version number. */
		OpString m_major;
		/** String to hold the minor part of the version number. */
		OpString m_minor;
		/** A bool to hold whether the version is beta. */
		BOOL m_beta;
		/** String to hold the beta number. */
		OpString m_betaNumber;
		/** String that expresses the entire version string Ex: "7.60beta */
		OpString m_versionString;
		/** Flag that is set TRUE if the spoof file has been changed on the server */
		BOOL m_spoofChange;
		/** Flag that is set TRUE if a new browser.js is changed on the server and needs to be downloaded. */
		BOOL m_browserJSChange;
		/** Temporary variable that is used to determine whether the local language was found. */
		BOOL m_tmpLanguageFound;
		/** Flag that is set to TRUE if the local language was found in the version information string. */
		BOOL m_languageFound;
		/**
		 * Counter to determine if a tag containing trusted TLDs follows.
		 */
		int m_idns;
		/** 
		 * Internal convenience function that parses version info on uni_char* 
		 * form and stores the results in m_major, m_minor, m_beta and 
		 * m_betaNumber. (The last two only if needed.) Errors in the parsing
		 * implies that the version number becomes corrupt.
		 *
		 * @param versionString The string that contains the version info.
		 * @return ERR, ERR_NULL_POINTER, ERR_PARSING_FAILED or OK. Should be
		 * self-explanatory...
		 */
		OP_STATUS ParseAndStoreVersionString(const uni_char* versionString);
		/**
		 * Helper function that extracts a sequence of digits from a string. 
		 * An offset can be supplied to instruct the search for
		 * digits to start at a specified point in the string. It will find only
		 * characters in the range ['0'..'9']. 
		 *
		 * @param target An OpString that the extracted digits are appended into.
		 * @param source The string that contains the digits.
		 * @param offset An offset into the string where the digit parsing is 
		 * to start. On return it will be set to the index of the first 
		 * character in the rest of the string that is not an integer.
		 *
		 * @return The status of the search. If no digits are found, or if the 
		 * supplied offset is illegal, it will return an error status, 
		 * otherwise it will return an OK status.
		 */
		OP_STATUS ExtractAndAppendDigits(OpString& target, const uni_char* source, unsigned int& offset);
		/**
		 * Helper function that "strips" away a [B|b][E|e][T|t][A|a] text by 
		 * incrementing the offset. On return the offset points to the first
		 * character in the string that is not part of that "beta"-string. 
		 *
		 * Note: The current implementation of this function allows all
		 * sorts of strange "beta"-strings: "7.60BeT", "7.60beTA", "7.60bET"...
		 * If this is undesireable, this is the function to fix!
		 *
		 * @param source The string that is to be inspected. starting from 
		 * the offset position.
		 * @param offset The offset into which the parsing is to start. Will 
		 * modify the offset so that it points to the first character after the
		 * beta part of the string if one exists.
		 * @return OK if the beta string was parsed correctly, ERR if no beta
		 * string could be found.
		 */
		OP_STATUS ReadAndStripBetaString(const uni_char* source, unsigned int& offset);
		/** Function used to clear the content of the version. */ 
		void ClearMembers();
		// XML Parsing callbacks.
//		static void sStartElementHandler(void *userData, const uni_char *name, const uni_char **atts);
//		static void sEndElementHandler(void *userData, const uni_char *name);
//		static void sCharacterDataHandler(void *userData, const uni_char *s, int len);
		void StartElementHandler(const XMLToken &token);
		void EndElementHandler(const XMLToken &token);
		void CharacterDataHandler(const uni_char *s, int len);

		// Implementation of the XMLTokenHandler API
		virtual XMLTokenHandler::Result	HandleToken(XMLToken &token);
		// Implementation of the XMLParser::Listener API
		virtual void					Continue(XMLParser *parser);
		virtual void					Stopped(XMLParser *parser);

	public:
		/**
		 * Constructor that constructs a Version instance that can represent 
		 * the version of the running Opera.
		 * @param blank if set true, the version will be initialized with no 
		 * version content, if false, the running instance's version 
		 * information will be used.
		 */
		Version(BOOL blank);
		/**
		 * Constructor that constructs a Version instance by trying to parse 
		 * the information from the given string.
		 *
		 * @param versionString A string containing the version information 
		 * on the format defined in the class documentation.
		 */
		Version(const uni_char* versionString);
		/**
		 * Constructor that constructs a Version by parsing an XML document. 
		 * The schema defining the format is given in the
		 * class documentation.
		 *
		 * @param XMLDocument The XML document that holds the version information
		 * @param length The length of the XML document.
		 */
		Version(uni_char* XMLDocument, int length);
		/**
		 * Destructor.
		 */
		~Version();
		/**
		 * Setter function corresponding to calling the constructor 
		 * Version(FALSE).
		 */
		void Set();
		/**
		 * Setter function corresponding to calling the constructor 
		 * Version(const uni_char* versionString).
		 */
		void Set(const uni_char* versionString);
		/**
		 * Setter function corresponding to calling the constructor 
		 * Version(char* XMLDocument, int lenght).
		 */
		void Set(uni_char* XMLDocument, int length);
		/**
		 * Informs whether the instance is corrupt. NOT thread safe, 
		 * so it might lie if you are using threads...
		 * 
		 * @return True if the version number stored seems to 
		 * be corrupt, false otherwise.
		 */
		BOOL Corrupt();
		/** 
		 * Returns a pointer to a string representing the complete version 
		 * string.
		 */
		OpString* FullString();
		/** 
		 * Returns a pointer to the member that holds the major number. 
		 */
		OpString* GetMajor();
		/**
		 * Returns a pointer to the member that holds the major number.
		 */
		OpString* GetMinor();
		/**
		 * Returns a bool saying whether this instance is 
		 * a beta version or not. 
		 */
		BOOL GetBeta();
		/**
		 * Returns a pointer to the member that holds the major number. 
		 */
		OpString* GetBetaNumber();
		/** Getter that returns whether this version indicates that a new spoof file should be downloaded. */
		BOOL GetSpoofChange() {return m_spoofChange;}
		/** Getter that returns whether this version indicates that a new browser.js file should be downloaded. */
		BOOL GetBrowserJSChange() {return m_browserJSChange;}
		/** Getter that is used to determine whether the local language was found in the file or not. */
		BOOL LocalLanguageFound() {return m_languageFound;}

		// Operators
		BOOL operator !=(Version &);
		BOOL operator ==(Version &);
		BOOL operator  <(Version &);
		BOOL operator <=(Version &);
		BOOL operator  >(Version &);
		BOOL operator >=(Version &);
	};	

#ifdef PREFS_DOWNLOAD
	/**
	 * Needed for automatic update of UA spoof list (spoof.ini)
	 */
	class EndChecker: public OpEndChecker
	{
	public:
		/**
		 * Always accept spoof lists Opera downloads automatically: return FALSE
		 */
		BOOL IsEnd(OpStringC info) { return FALSE; }
		void Dispose() { OP_DELETE(this); }
	};
#endif // PREFS_DOWNLOAD

	/**
	 * A reference to the object responsible for downloading and 
	 * storing browser.js-files.
	 */
	BrowserJSDownloader* m_browserJSDownloader;
	/**
	 * A reference to the TransferItem that represents the transfer 
	 * initiated by this class.
	 */
	OpTransferItem* m_transferItem;
	/**
	 * A flag that signals whether it is safe to delete this instance
	 * of NewUpdatesChecker.
	 */
	BOOL m_canSafelyBeDeleted;
	/**
	 * If this flag is set, a dialog must be displayed even if there is 
	 * no new update. It is set when a user initiates the check and must 
	 * get feedback so that the user knows that the check is working.
	 */
	BOOL m_mustGiveFeedback;
	/** 
	 * Flag that is set true if the version check is being run for the 
	 * first time.If that is the case, we are not to inform the users 
	 * of new updates, and not to store the newest seen version. This
	 * allows IDNA whitelists and ua.ini-entries to be downloaded,
	 * even without disturbing the user.
	 */
	BOOL m_firstRun;
public:
	/**
	 * Constructor.
	 * @param mustGiveFeedback Boolean that signals whether a dialog to 
	 * inform the user that no update is available should be displayed. 
	 * Can be used in situations where the user has requested the check,
	 * and some intelligent feedback is required.
	 */
    NewUpdatesChecker(BOOL mustGiveFeedback);
	/** Destructor */
	~NewUpdatesChecker();

	/**
	 * Checks whether new updates are available by means of a MessageHandler. 
	 * The function never actually receives an answer, but returns after 
	 * initiating the check. The actual response is handled in OnProgress, 
	 * which will pop up a dialog if necessary. 
	 *
	 * @return A boolean that signals success or failure. If the setup of the 
	 * check failed, it can safely be deleted. If it succeeded it must 
	 * not be deleted, as it will receive callbacks from an ongoing
	 * transfer. In that case it will delete itself after receiving and 
	 * parsing the incoming version information.
	 */
    BOOL PerformNewUpdatesCheck();

	/**
	 * The method handles progress messages from the transfer that the updates 
	 * checker has initiated. It will call NewUpdateAvailable if a new update 
	 * is found, otherwise do nothing.
	 */
    void OnProgress(OpTransferItem* transferItem, TransferStatus status);
	// Required to compile.
	void OnReset(OpTransferItem* transferItem) { }
	void OnRedirect(OpTransferItem* transferItem, URL* redirect_from, URL* redirect_to) { }
	/**
	 * Function that returns true if it is safe to delete the instance of 
	 * NewUpdatesChecker, and false if deletion can cause problems for 
	 * callbacks.
	 *
	 * @return Whether the instance of NewUpdatesChecker has received 
	 * all its callbacks
	 */
	BOOL CanSafelyBeDeleted();
	/**
	 * Function to handle failed transfers. Reports error if the 
	 * version check is user initiated.
	 */
	void HandleTransferFailure();
	/**
	 * Function to handle the received documents. This is where 
	 * the version comparisons are taking place.
	 */
	void HandleReceivedData(URL_DataDescriptor* dd);
	/**
	 * Function that stores values in preferences that will cause 
	 * the automatic update check to be reperformed in one week.
	 *
	 * @param latestSeenVersion The newest version that the user 
	 * has been made aware of.
	 */
	void CheckInOneWeek(Version &newestSeenVersion);
	/**
	 * Function that performs the user agent spoof list download
	 * Just a helper since it this action is performed from more than one location.
	 */
	void DownloadUASpoofList();
#ifdef DOM_BROWSERJS_SUPPORT
	/**
	 * Function that updates the browser.js file, but only if the user haven't turned browser.js download off.
	 * Just a helper since this action is performed from more than one location.
	 */
	void DownloadBrowserJSIfNeededOrRequired(BOOL needed);
#endif // DOM_BROWSERJS_SUPPORT
};

#endif // CHECK_FOR_UPGRADE_DIALOGS

#endif // !AUTO_UPDATE_SUPPORT
