/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef INSTALL_LANGUAGE_DIALOG_H
#define INSTALL_LANGUAGE_DIALOG_H

#ifdef AUTO_UPDATE_SUPPORT

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/desktop_util/treemodel/simpletreemodel.h"
#include "adjunct/autoupdate/updatablefile.h"
#include "adjunct/quick/managers/AutoUpdateManager.h"
#include "adjunct/desktop_util/treemodel/simpletreemodel.h"

#define MISSING_DICT_TRIGGER_LIMIT 5

class XMLFragment;
class OpQuickFind;
class OpString_list;
class OpProgressBar;
class OpLabel;

class DictionaryHandler
{
public:
	DictionaryHandler() {}
	~DictionaryHandler() {}
  
	void InstallDictionaries();

	OP_STATUS GetAvailableDictionaries(SimpleTreeModel& languages_model, const uni_char* path, OpFileFolder folder = OPFILE_ABSOLUTE_FOLDER);
	INT32 GetPositionFromID(SimpleTreeModel& tree_model, const uni_char* language_id);
	
	static BOOL CompareLanguageId(const uni_char* lang_id_1, const uni_char* lang_id_2);
#ifdef FIND_BEST_DICTIONARY_MATCH
	static void ShowInstallDialogIfNeeded(int spell_session_id);
#endif // FIND_BEST_DICTIONARY_MATCH
	static BOOL GetBestLanguageMatch(const OpStringC prefered_language_id, OpString& matching_language_id);
	static BOOL IsDictionaryAvailable(const uni_char* lang_id);
    static OP_STATUS GetActualFileName(OpString& file_name);
    static OP_STATUS DeleteDictionary(OpString& file_name);

private:
    static OP_STATUS GetUILanguageID(const OpStatus& language_id);
    static BOOL IsDictionaryInstalled(OpString& language_id);
	OP_STATUS GetAvailableLanguages(XMLFragment& fragment, SimpleTreeModel& languages_model);
    static OP_STATUS VerifyFileName(OpString& file_name);    
};

class ILD_DictionaryItem
{
public:
	ILD_DictionaryItem(): m_dictionary(NULL) {}
	~ILD_DictionaryItem()
	{
		if (m_dictionary)
		{
			m_dictionary->SetFileDownloadListener(NULL);
			m_dictionary->StopDownload();
			m_dictionary->Cleanup();
		}

		OP_DELETE(m_dictionary);
	}

	OP_STATUS SetLanguageId(OpStringC& language_id) { return m_language_id.Set(language_id); }
	OpString& GetLanguageId() { return m_language_id; }

	void SetDictionary(UpdatableDictionary* dictionary) { m_dictionary = dictionary; }
	UpdatableDictionary* GetDictionary() { return m_dictionary; }
protected:
	OpString				m_language_id;
	UpdatableDictionary*	m_dictionary;
};

class InstallLanguageDialog
	:public Dialog
	,public AdditionCheckerListener
	,public FileDownloadListener
{
 public:

	enum {
		TEXT_COLUMN,
		SIZE_COLUMN,
		ID_COLUMN
	} COLUMN_TYPE;
	
	enum DownloadState{
		IDLE,
		DOWNLOADING,
		DOWNLOAD_ERROR
	} m_download_state;

	InstallLanguageDialog();		
	~InstallLanguageDialog();

    const char*	GetMissingDictionaryPageName()  const { return "missing_dictionary"; };
	const char*	GetInstallDictionaryPageName()  const { return "install_dictionary"; };
	const char*	GetDownloadPageName()			const { return "download_page"; };
	const char*	GetLicensePageName()			const { return "dictionary_license"; };
	const char*	GetDefaultLanguagePageName()	const { return "default_language";   }; 
	void InstallDictionaries();
	
    //===== Dialog / DesktopWindow
	virtual void		    Init(DesktopWindow* parent_window, UINT32 start_page = 1);
	virtual void		    OnInit();
	virtual Type		    GetType()	    { return DIALOG_TYPE_INSTALL_LANGUAGE; }
	virtual DialogType		GetDialogType() { return TYPE_WIZARD; }
	virtual const char*	    GetWindowName()	{ return "Spell Check Dictionary Install Dialog";  }
	
	virtual	const uni_char*	GetOkText();
	virtual OpInputAction*	GetOkAction();
	virtual UINT32			OnForward(UINT32 page_number);
	virtual void			OnInitPage(INT32 page_number, BOOL first_time);
	virtual BOOL			IsLastPage(INT32 page_number);

	virtual UINT32	        OnOk();
	virtual BOOL	        OnInputAction(OpInputAction* action);	
	virtual void	        OnItemChanged(OpWidget *widget, INT32 pos);
	
	// == OpWidgetListener ======================
	virtual void			OnChange(OpWidget *widget, BOOL changed_by_mouse);
	virtual void            OnSortingChanged(OpWidget *widget) {};
	
	
protected:
	const OpStringC&	 GetTotalSizeString() { return m_total_size; }

	OP_STATUS			 GetInstalledLanguages(OpString_list& languages);
	OP_STATUS            GetCheckedLanguages(OpVector<OpString>& languages);
	OP_STATUS            GetChangedDictionaries(OpVector<OpString>& languages);
	OP_STATUS            GetNewChecked(OpVector<OpString>& languages);
	OP_STATUS            GetNewUnchecked(OpVector<OpString>& languages);
	BOOL				 IsLanguageAlreadyInstalled(const OpString& language_id);
	OpFileLength		 CalculateTotalSize(OpTreeView* tree_view);
	OpFileLength		 GetDownloadSize(SimpleTreeModelItem* item);
	OP_STATUS  		     SetSizeStringFromSize(OpFileLength size, OpString& m_total_size);

	// AUM_AdditionListener implementation
	void OnAdditionResolved(UpdatableResource::UpdatableResourceType type, const OpStringC& key, UpdatableResource* resource);
	void OnAdditionResolveFailed(UpdatableResource::UpdatableResourceType type, const OpStringC& key);

	// FileDownloadListener
	void OnFileDownloadDone(FileDownloader* file_downloader, OpFileLength total_size);
	void OnFileDownloadFailed(FileDownloader* file_downloader);
	void OnFileDownloadAborted(FileDownloader* file_downloader);
	void OnFileDownloadProgress(FileDownloader* file_downloader, OpFileLength total_size, OpFileLength downloaded_size, double kbps, unsigned long time_estimate);

	// Functions to determain what page it is.
	BOOL                 IsMissingDictionaryPage(INT32 page_number) const;
	BOOL				 IsInstallDictionaryPage(INT32 page_number) const;
	BOOL				 IsDownloadPage(INT32 page_number) const;
	BOOL				 IsLicensePage(INT32 page_number) const;
	BOOL				 IsDefaultLanguagePage(INT32 page_number) const;

	// Page inits
	void                 InitMissingDictionaryPage();
	void		         InitInstallDictionaryPage();
	void		         InitDownloadPage();
	void		         InitLicensePage();
	void		         InitDefaultLanguagePage();

	void				ShowError();
	
	OP_STATUS			 UpdateLanguages();
	OP_STATUS			 FindLanguageName(OpStringC language_id, OpString& language_name);

private:
	enum DownloadResult
	{
		Downloaded,
		Failed,
		Aborted
	};

    OP_STATUS            AddLanguageToDownloadList(const uni_char*);
    SimpleTreeModelItem* GetModelItem(const uni_char* language_id);
    
    INT32                LicenseToShow();
	INT32				 GetPageByName(const OpStringC8 & name);

	ILD_DictionaryItem*	GetItemByLanguage(const OpStringC& language_id);
	ILD_DictionaryItem* GetItemByDownloader(FileDownloader* dictionary);
	void 				OnAllDictionariesResolved();
	OP_STATUS			OnAllDictionariesDownloaded();
	OP_STATUS			ProcessDictionaryDownloaded(FileDownloader* dictionary, DownloadResult result);
	OP_STATUS			UpdateDownloadProgress();

	SimpleTreeModel		    m_languages_model;
	OpTreeView*				m_tree_view;
	OpQuickFind*			m_quickfind;
	OpString				m_total_size;
	DictionaryHandler	    m_handler;
	OpProgressBar*			m_progress;
	OpLabel*				m_label;
	OpListBox*				m_listbox;
	BOOL					m_show_update_text;
	BOOL					m_enable_update;
	OpString				m_update_text;
	BOOL					m_is_downloading;
	OpVector<ILD_DictionaryItem>      m_new_languages;     // A list of the new languages since the dialog was opened.
	UINT32                  m_cur_page;
	UINT32                  m_license_to_show;

	UINT32					m_resolved_count;
	UINT32					m_downloaded_count;
	OpFileLength			m_total_download_size;
	OpFileLength			m_total_downloaded_size;
	BOOL					m_all_downloaded_ok;
	BOOL					m_all_installed_ok;
};

#endif // AUTO_UPDATE_SUPPORT
#endif //this file
