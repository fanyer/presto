/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#if !defined OPSETUPMANAGER_H
#define OPSETUPMANAGER_H

#include "adjunct/quick/managers/DesktopManager.h"
#include "modules/util/adt/opvector.h"
#include "modules/prefsfile/prefssection.h"

class OpFile;
class PrefsFile;
class OpFileDescriptor;

enum OpSetupType
{
	OPSKIN_SETUP,
	OPTOOLBAR_SETUP,
	OPMENU_SETUP,
	OPMOUSE_SETUP,
	OPKEYBOARD_SETUP,
	OPVOICE_SETUP_obsolete,
	OPVOICELIB_BINSETUP_obsolete,
	OPDIALOG_SETUP
};

/** The global OpSetupManager object. */
#define g_setup_manager (OpSetupManager::GetInstance())

class OpSetupManager
{
		enum PrefInitFlag
		{
			COPY_TO_LOCAL = 1,
			FIND_IN_LOCAL = 2
		};

	public:

		enum SetupPatchMode
		{
			NONE = 0,
			MERGE,
			REPLACE
		};

								OpSetupManager();
		virtual					~OpSetupManager();


		static OpSetupManager* GetInstance()
		{
			if (!s_instance)
			{
				s_instance = OP_NEW(OpSetupManager, ());
				if(s_instance)
				{
					TRAPD(err, s_instance->InitL());
				}
			}
			return s_instance;
		}
		static void Destroy()
		{
			OP_DELETE(s_instance);
			s_instance = NULL;
		}

		// DesktopManager interface
		virtual OP_STATUS 		Init();


		void					CommitL();
		void					ScanSetupFoldersL();
		void					ScanFolderL(const OpString& searchtemplate, const OpString& rootfolder, enum OpSetupType type);

		/**
		 * Test all files in list and remove those that do not exist
		 *
		 * @param list A list of files
		 *
		 */
		void					RemoveNonExistingFiles(OpVector<OpString>& list);

		/**
		 * Determine wheter the given setup file can be modified (including deleted)
		 *
		 * @param index The index in file list
		 * @param type Identifies the list
		 *
		 * @return true if the file can be modified, otherwise false
		 */
		bool					IsEditable(UINT32 index, OpSetupType type);

		OP_STATUS				GetSetupName(OpString* filenameonly, INT32 index, enum OpSetupType type);

#ifndef PREFS_NO_WRITE
		void					RenameSetupPrefsFileL(const uni_char* newname, INT32 index, enum OpSetupType type);
		void					DuplicateSetupL(INT32 index, enum OpSetupType type, BOOL copy_contents=TRUE, BOOL index_check=FALSE, BOOL modified=FALSE, UINT32* returnindex=NULL);

		BOOL					DeleteSectionL(const char* name, enum OpSetupType type);

		OP_STATUS				SetKeyL(const OpStringC8 &section, const OpStringC8 &key, int value, enum OpSetupType type);
		OP_STATUS				SetKeyL(const OpStringC8 &section, const OpStringC8 &key, const OpStringC &value, enum OpSetupType type);
#endif

		void					DeleteSetupL(INT32 index, enum OpSetupType type);
		PrefsFile*				GetSetupPrefsFileL(volatile INT32 index, enum OpSetupType type, OpString* actualfilename=NULL, BOOL index_check=FALSE);

		PrefsSection*			GetSectionL(const char* name, enum OpSetupType type, BOOL* was_default = NULL, BOOL master = FALSE);
		/** Get string from section in specified file.
		  * @deprecated This re-reads the section each time, and is inefficient. */
		DEPRECATED(void			GetStringL(OpString &string, const OpStringC8 &sectionname, const OpStringC &key, enum OpSetupType type, BOOL master = FALSE));
		/** Get string from specified section. */
		inline OpStringC		GetStringL(PrefsSection* section, const OpStringC8 &key8)
		{ if (!section) return NULL; OpString key; key.Set(key8); return section->Get(key); }

		/** Get integer from section in specified file.
		  * @deprecated This re-reads the section each time, and is inefficient. */
		DEPRECATED(int			GetIntL(const OpStringC8 &sectionname, const OpStringC &key, enum OpSetupType type, int defval, BOOL master = FALSE));
		/** Get integer from specified section. */
		int						GetIntL(PrefsSection* section, const OpStringC8 &key, int defval);

		OP_STATUS				SelectSetupByFile(PrefsFile* setupfile, enum OpSetupType type, BOOL broadcast_change=TRUE);
		OP_STATUS				SelectSetupByFile(const uni_char*, enum OpSetupType type, BOOL broadcast_change=TRUE);
		OP_STATUS				SelectSetupFile(INT32 index, enum OpSetupType type, BOOL broadcast_change=TRUE);
		OP_STATUS				SelectSetupByFile(OpFile* file, enum OpSetupType type, BOOL broadcast_change=TRUE);

		OP_STATUS				ReloadSetupFile(enum OpSetupType type);

		UINT32					GetMouseConfigurationCount() { return m_mouse_setup_list.GetCount(); }
		UINT32					GetKeyboardConfigurationCount() { return m_keyboard_setup_list.GetCount(); }
		INT32					GetIndexOfMouseSetup();		///< returns vector index
		INT32					GetIndexOfKeyboardSetup();	///< returns vector index
		UINT32					GetTypeCount(enum OpSetupType type);

		PrefsFile*				GetDefaultToolbarSetupFile()	{ return m_default_toolbar_prefsfile; }

		PrefsFile*				GetOverrideDialogSetupFile()	{ return m_override_dialog_prefsfile; }
		PrefsFile*				GetDialogSetupFile()			{ return m_dialog_prefsfile; }

		UINT32					GetToolbarConfigurationCount()	{ return m_toolbar_setup_list.GetCount(); }
		UINT32					GetMenuConfigurationCount()		{ return m_menu_setup_list.GetCount(); }
		INT32					GetIndexOfMenuSetup();			///< returns vector index
		INT32					GetIndexOfToolbarSetup();		///< returns vector index

		OP_STATUS				MergeSetupIntoExisting(PrefsFile* file, enum OpSetupType type, SetupPatchMode appendmode, OpFile*& workfile);

#ifdef PREFS_HAVE_FASTFORWARD
		PrefsSection*			GetFastForwardSection()			{ return m_fastforward_section; }
#endif

		/**
		 * Returns the current prefsfile for the specified type
		 * and creates a local copy if needed
		 * @param type The type of setup
		 * @param check_for_readonly Check and copy file to local area if the current is a readonly
		 * @return The current prefsfile
		 */
		PrefsFile*				GetSetupFile(OpSetupType, BOOL check_for_readonly=FALSE, BOOL copy_contents=FALSE);

		BOOL					IsFallback(const uni_char* fullpath, OpSetupType type);

	private:

		static OpSetupManager* s_instance;

		void 					InitL();

		INT32					GetCurrentSetupRevision(enum OpSetupType type);

		const PrefsFile*		GetDefaultSetupFile(OpSetupType type);

		PrefsFile*				InitializePrefsFileL(OpFile* defaultfile, const char* fallbackfile, INT32 flag);
		PrefsFile*				InitializeCustomPrefsFileL(OpFile* defaultfile, const char* pref_filename, OpFileFolder op_folder);

		PrefsFile*				InitializePrefsFileL(const char* pref_filename, OpFileFolder op_folder);

		PrefsFile*				m_mouse_prefsfile;
		PrefsFile*				m_default_mouse_prefsfile;

		PrefsFile*				m_keyboard_prefsfile;
		PrefsFile*				m_default_keyboard_prefsfile;

		OpAutoVector<OpString>	m_mouse_setup_list;
		OpAutoVector<OpString>	m_keyboard_setup_list;

		PrefsFile*				m_menu_prefsfile;
		PrefsFile*				m_custom_menu_prefsfile;
		PrefsFile*				m_default_menu_prefsfile;

		PrefsFile*				m_toolbar_prefsfile;
		PrefsFile*				m_custom_toolbar_prefsfile;
		PrefsFile*				m_default_toolbar_prefsfile;

		PrefsFile*				m_override_dialog_prefsfile;	// file which will override sections in the default file
		PrefsFile*				m_custom_dialog_prefsfile;		// file which will override sections in the default file
		PrefsFile*				m_dialog_prefsfile;

		OpAutoVector<OpString>	m_toolbar_setup_list;
		OpAutoVector<OpString>	m_menu_setup_list;

#ifdef PREFS_HAVE_FASTFORWARD
		PrefsSection*			m_fastforward_section;
#endif
		void					BroadcastChange(OpSetupType type);

};

#endif // !OPSETUPMANAGER_H
