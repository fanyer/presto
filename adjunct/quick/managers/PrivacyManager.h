// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2006 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Adam Minchinton
//

#ifndef __PRIVACY_MANAGER_H__
#define __PRIVACY_MANAGER_H__

#include "modules/wand/wandmanager.h"

class BrowserDesktopWindow;

// Folder where prefs are stored when in private browsing mode
#define PRIVATE_BROWSING_FOLDER UNI_L("Private")

// Defines for the link and mail entries in Wand
#define WAND_OPERA_PREFIX		"opera:"
#define WAND_OPERA_ACCOUNT		"opera:account"
#define WAND_OPERA_MAIL			"opera:mail"

#define g_privacy_manager (PrivacyManager::GetInstance())

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

class PrivacyManagerCallback
{
public:
	virtual ~PrivacyManagerCallback() {}

	virtual void OnPasswordRetrieved(const OpStringC& password) = 0;
	virtual void OnPasswordFailed() = 0;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

class PrivacyManager
{
public:
	enum PrivacyType
	{
		ALL_COOKIES = 0,
		DOCUMENTS_WITH_PASSWORD,
		TEMPORARY_COOKIES,
		MAIL_PASSWORDS,
		GLOBAL_HISTORY,
		TYPED_HISTORY,
		VISITED_LINKS,
		CACHE,
		DOWNLOAD_HISTORY,
		WAND_PASSWORDS_SYNC, /**< Remove passwords and sync deletion via Link. */
		WAND_PASSWORDS_DONT_SYNC, /**< Remove passwords, but don't sync deletion via Link. */
		BOOKMARK_VISITED_TIME,
		ALL_WINDOWS,
		CERTIFICATES,
		SEARCHFIELD_HISTORY,
		WEBSTORAGE_DATA,
		PLUGIN_DATA,
		GEOLOC_DATA,
		CAMERA_PERMISSIONS,
		EXTENSION_DATA,
		NUM_PRIVACY_ITEMS /**< Not privacy type. Always last item. */
	};

	class Flags
	{
	public:
		Flags():m_flags(0) {};

		void Set(unsigned int flags) { m_flags = flags; }
		void Set(PrivacyType flag, bool value) {m_flags = value ? m_flags | (1<<flag) : m_flags&~(1<<flag); }
		void SetAll() { for (int i=0; i<NUM_PRIVACY_ITEMS; i++) Set((PrivacyType)i, TRUE); }
		bool IsSet(PrivacyType flag) const { return (m_flags & (1 << flag)) != 0; }
		void Reset() { m_flags = 0; }
		PrivacyType IntToFlag(INTPTR candidate) { return candidate<0 || candidate>=NUM_PRIVACY_ITEMS ? NUM_PRIVACY_ITEMS : (PrivacyType)candidate; }
		unsigned int Get() const { return m_flags; }

	private:
		unsigned int m_flags;
	};

public:
	static PrivacyManager *GetInstance();

	/**
		Folder to clean up on exit to remove all private browsing mode preferences.
	*/
	void AddPrivateFolder(const uni_char *folder);

	/**
		This cleans up all the big items and will clean up while interface is still
		running so we can show a progress dialog if needed. The cleanup would be
		similar to what Delete Private Data does.

		@param flags Cleaning actions to perform
		@param write_to_prefs Whether flags should be saved in operaprefs.ini
		@param browser_window Window that will survive ALL_WINDOWS action. NULL means active browser window
	*/
	void ClearPrivateData(Flags flags, BOOL write_to_prefs, BrowserDesktopWindow* browser_window = NULL);

	/**
		This is the final stage done right at the end which effectively just deletes
		all the private browsing mode preferences folders.
	*/
	void DeletePrivateFolders();

	/**
	 * Return TRUE if the flag corresponds to a setting that Opera has set to
	 * delete trough the delete private dialog
	 *
	 * @param flag. One of the CLEAR_... flags 
	 *
	 * @return The state of the flag
	 */
	BOOL GetDeleteState(int flag);

	/**
		Gets a password from the wand, used for mail and link based on a protocol, server and username
	    @param protocol Protocol to use (see WAND_OPERA_* defines)
	    @param server Server for which password should be retrieved
	    @param username Username for which password should be retrieved
	    @param callback Object to call once password has been retrieved
	*/
	OP_STATUS GetPassword(const OpStringC8 &protocol, const OpStringC &server, const OpStringC &username, PrivacyManagerCallback* callback);

	/**
		Sets a password from the wand, used for mail and link based on a protocol, server and username
	*/
	OP_STATUS SetPassword(const OpStringC8 &protocol, const OpStringC &server, const OpStringC &username, const OpStringC &password);

	/**
		Removes a password from the wand, used for mail and link based on a protocol, server and username
	*/
	OP_STATUS DeletePassword(const OpStringC8 &protocol, const OpStringC &server, const OpStringC &username);

	/**
	    Removes a previously setup callback
	*/
	OP_STATUS RemoveCallback(PrivacyManagerCallback* callback);

	/**
	    @return Whether an object is registered as a callback for the PrivacyManager
	  */
	BOOL	  IsCallback(PrivacyManagerCallback* callback);

	/**
	    @return OpStatus::OK when folder location is updated successfully
	  */
	OP_STATUS SetPrivateWindowSaveFolderLocation(OpStringC16 string);

	const uni_char * GetPrivateWindowSaveFolderLocation();

	// Use has been notified Turbo is not working with private tabs
	BOOL TurboNotAvailableInPrivateTabNotified() { return m_turbo_not_available_in_private_tab_notified; }
	void SetTurboNotAvailableInPrivateTabNotified() { m_turbo_not_available_in_private_tab_notified = TRUE; }

private:
	class CallerInfo: public WandLoginCallback
	{
		public:
			CallerInfo(PrivacyManagerCallback *callback) : m_callback(callback) {}
			virtual ~CallerInfo() {}

			PrivacyManagerCallback* GetCallback() const { return m_callback; }
			void ResetCallback()						{ m_callback = 0; }

			// From WandLoginCallback
			virtual void OnPasswordRetrieved(const uni_char* password);
			virtual void OnPasswordRetrievalFailed();

		private:
			PrivacyManagerCallback* m_callback;
	};

	friend class CallerInfo;

private:
	PrivacyManager();
	~PrivacyManager();

	void      ClearPrivateData(Flags flags, BrowserDesktopWindow* browser_window);
	OP_STATUS MakeWandID(const OpStringC8 &protocol, const OpStringC &server, OpString &id);
	void      RemoveCallerInfo(CallerInfo* caller_info);

private:
	OpAutoVector<CallerInfo> m_callers;			// GetPassword() callers
	OpAutoVector<OpString>	 m_private_folders; // List of folders to delete to remove private browsing mode preferences
	OpString				 m_private_window_save_loc;
	BOOL					 m_turbo_not_available_in_private_tab_notified;
	BOOL					 m_sync_password_manager_deletion;
};

#endif // __PRIVACY_MANAGER_H__
