/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef AUTO_UPDATE_SUPPORT

#include "InstallLanguageDialog.h"

#ifdef INTERNAL_SPELLCHECK_SUPPORT

#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "adjunct/quick_toolkit/widgets/OpQuickFind.h"
#include "modules/util/opfile/opfile.h"
#include "adjunct/quick_toolkit/widgets/OpProgressbar.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick_toolkit/widgets/OpGroup.h"

#include "adjunct/desktop_util/treemodel/optreemodelitem.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/desktop_util/prefs/PrefsCollectionDesktopUI.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/util/opstrlst.h"
#include "modules/spellchecker/spellchecker_module.h"
#include "modules/spellchecker/opspellcheckerapi.h"
#include "modules/xmlutils/xmlfragment.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/prefsfile/accessors/lnglight.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/widgets/OpButton.h"
#include "adjunct/desktop_util/treemodel/simpletreemodel.h"

#define DICTIONARIES_FILENAME UNI_L("dictionaries.xml")

/***************************************************************
 *
 * InstallLanguageDialog
 *
 *
 **************************************************************/
InstallLanguageDialog::InstallLanguageDialog()
: m_download_state(IDLE),
  m_languages_model(3),
  m_tree_view(0),
  m_quickfind(0),
  m_progress(NULL),
  m_label(NULL),
  m_listbox(NULL),
  m_show_update_text(TRUE),
  m_enable_update(TRUE),
  m_is_downloading(FALSE),
  m_cur_page(1),
  m_license_to_show(0),
  m_resolved_count(0),
  m_downloaded_count(0),
  m_total_download_size(0),
  m_total_downloaded_size(0),
  m_all_downloaded_ok(FALSE),
  m_all_installed_ok(FALSE)
{

	// Get the list of installable languages ( code, Description/text, size) from the dictionary.xml file.
	if (OpStatus::IsError(m_handler.GetAvailableDictionaries(m_languages_model, DICTIONARIES_FILENAME, OPFILE_DICTIONARY_FOLDER)))
	{
		OP_STATUS status = m_handler.GetAvailableDictionaries(m_languages_model, DICTIONARIES_FILENAME, OPFILE_INI_FOLDER);
		// If this assert goes off, the file is probably missing from the Opera installer package, or not installed the right way.
		OP_ASSERT(OpStatus::IsSuccess(status));
	}

    // Make the first column (Language name)
	OpString column_header;
	g_languageManager->GetString(Str::D_SPELLCHECK_LANGUAGE_HEADER, column_header);
	m_languages_model.SetColumnData(TEXT_COLUMN, column_header, NULL, TRUE, TRUE);

	// Make the second column (size)
	g_languageManager->GetString(Str::D_SPELLCHECK_SIZE_HEADER, column_header);
	m_languages_model.SetColumnData(SIZE_COLUMN, column_header, NULL, FALSE, TRUE);

	// Make the third colum (language ID)
	g_languageManager->GetString(Str::D_SPELLCHECK_LANGUAGE_ID_HEADER, column_header);
	m_languages_model.SetColumnData(ID_COLUMN, column_header, NULL, TRUE, FALSE);
	
	// Setting the autoupdate listener
	g_autoupdate_manager->AddListener(this);    
}

/***************************************************************
 *
 * ~InstallLanguageDialog
 *
 *
 **************************************************************/
InstallLanguageDialog::~InstallLanguageDialog()
{
	for (INT32 i = 0; i < m_languages_model.GetCount(); i++)
	{
		SimpleTreeModelItem* item = m_languages_model.GetItemByIndex(i);
		OpFileLength* size = (OpFileLength*)item->GetUserData();
		item->SetUserData(0);
		OP_DELETE(size);
	}
	if(m_listbox)
	{
		for(INT32 i=0; i<m_listbox->CountItems(); i++)
		{
			uni_char* user_data = (uni_char*)m_listbox->GetItemUserData(i);
			OP_DELETEA(user_data);
		}
		
	}
	m_new_languages.DeleteAll();
	
	if (g_autoupdate_manager)
 	    g_autoupdate_manager->RemoveListener(this);
}


/***************************************************************
 *
 * Init
 *
 *
 **************************************************************/
void InstallLanguageDialog::Init(DesktopWindow* parent_window, UINT32 start_page)
{
	g_languageManager->GetString(Str::D_SPELLCHECK_UPDATE_LANGUAGES, m_update_text);
	
	Dialog::Init(parent_window, start_page);
}


/***************************************************************
 *
 * Init
 *
 *
 **************************************************************/
void InstallLanguageDialog::OnInit()
{
	// Set up the tree view.
	m_tree_view = (OpTreeView *)GetWidgetByName("Spellcheck_install_treeview");

	if (m_tree_view && !m_tree_view->GetTreeModel())
	{
	    m_tree_view->SetUserSortable(TRUE);
		m_tree_view->SetCheckableColumn(0);
		m_tree_view->SetListener(this);
		m_tree_view->SetAutoMatch(TRUE);

		m_tree_view->SetTreeModel(&m_languages_model, 0, TRUE);
		m_tree_view->SetSelectedItem(0);

		m_tree_view->SetColumnWeight(TEXT_COLUMN, 60);
		m_tree_view->SetColumnWeight(SIZE_COLUMN, 20);
		m_tree_view->SetColumnWeight(ID_COLUMN, 20);

		m_tree_view->Sort( TEXT_COLUMN, TRUE);

	}

}


/*************************************************************
 *
 * GetInstalledLanguages
 *
 *
 **************************************************************/
OP_STATUS InstallLanguageDialog::GetInstalledLanguages(OpString_list& languages)
{
	if (g_internal_spellcheck)
		return g_internal_spellcheck->GetInstalledLanguages(languages);
	else
		return OpStatus::ERR;
}


/***************************************************************
 *
 * GetCheckedLanguages
 *
 *
 **************************************************************/
OP_STATUS InstallLanguageDialog::GetCheckedLanguages(OpVector<OpString>& languages)
{	
	if (m_tree_view)
	{
        OpTreeModelItem* item = NULL;

	    for (INT32 i = 0; i < m_tree_view->GetItemCount(); i++)
	    {
	        item = m_tree_view->GetItemByPosition(i);
	        if (item && m_tree_view->IsItemChecked(i))
	        {
	            const uni_char* language_id = m_languages_model.GetItemStringByID(item->GetID(), ID_COLUMN);
	            if (language_id)
	            {
				    OpString* id = OP_NEW(OpString, ());
				    id->Set(language_id);
				    languages.Add(id);
				}
            }
	    }
	    return OpStatus::OK;
	}

    return OpStatus::ERR;
}

/***************************************************************
 *
 * GetCheckedLanguages
 *
 *
 **************************************************************/
OP_STATUS InstallLanguageDialog::GetChangedDictionaries(OpVector<OpString>& languages)
{	
	if (m_tree_view)
	{
        OpTreeModelItem* item = NULL;

	    for (INT32 i = 0; i < m_tree_view->GetItemCount(); i++)
	    {
	        item = m_tree_view->GetItemByPosition(i);
	        if (item && m_tree_view->HasChanged())
	        {
	            const uni_char* language_id = m_languages_model.GetItemStringByID(item->GetID(), ID_COLUMN);
	            if (language_id)
	            {
				    OpString* id = OP_NEW(OpString, ());
				    id->Set(language_id);
				    languages.Add(id);
				}
            }
	    }
	    return OpStatus::OK;
	}

    return OpStatus::ERR;
}

/***************************************************************
 *
 * GetNewChecked
 *
 *
 **************************************************************/
OP_STATUS InstallLanguageDialog::GetNewChecked(OpVector<OpString>& languages)
{	
	if (m_tree_view)
	{
        OpTreeModelItem* item = NULL;

	    for (INT32 i = 0; i < m_tree_view->GetItemCount(); i++)
	    {
	        item = m_tree_view->GetItemByPosition(i);
	        if (item && m_tree_view->HasItemChanged(i) && m_tree_view->IsItemChecked(i))
	        {
	            const uni_char* language_id = m_languages_model.GetItemStringByID(item->GetID(), ID_COLUMN);
	            if (language_id)
	            {
				    OpString* id = OP_NEW(OpString, ());
				    id->Set(language_id);
				    languages.Add(id);
				}
            }
	    }
	    return OpStatus::OK;
	}

    return OpStatus::ERR;
}

/***************************************************************
 *
 * GetNewUnchecked
 *
 *
 **************************************************************/
OP_STATUS InstallLanguageDialog::GetNewUnchecked(OpVector<OpString>& languages)
{	
	if (m_tree_view)
	{
        OpTreeModelItem* item = NULL;

	    for (INT32 i = 0; i < m_tree_view->GetItemCount(); i++)
	    {
	        item = m_tree_view->GetItemByPosition(i);
	        if (item && m_tree_view->HasItemChanged(i) && !m_tree_view->IsItemChecked(i))
	        {
	            const uni_char* language_id = m_languages_model.GetItemStringByID(item->GetID(), ID_COLUMN);
	            if (language_id)
	            {
				    OpString* id = OP_NEW(OpString, ());
				    id->Set(language_id);
				    languages.Add(id);
				}
            }
	    }
	    return OpStatus::OK;
	}

    return OpStatus::ERR;
}

/***************************************************************
 *
 * IsLanguageAlreadyInstalled
 *
 *
 **************************************************************/
BOOL InstallLanguageDialog::IsLanguageAlreadyInstalled(const OpString& language_id)
{

    OP_NEW_DBG("::IsLanguageAlreadyInstalled", "Dictionary");

	OpString_list languages;
	
	// Get the current list of installed dictionaries.
	if(OpStatus::IsError(GetInstalledLanguages(languages)))
		return FALSE;
	
	// Go through them and compare to language_id
	for (unsigned long i = 0; i < languages.Count(); i++)
	{
		if(DictionaryHandler::CompareLanguageId(languages[i], language_id))
		{
	        OP_DBG(("The dictionary %s is installed", language_id.CStr()));
			return TRUE;
		}
	}
	
	OP_DBG(("The dictionary %s is not installed", language_id.CStr()));
		
	return FALSE;
	
}


/***************************************************************
 *
 * OnInitPage
 *
 *
 **************************************************************/
void InstallLanguageDialog::OnInitPage(INT32 page_number, BOOL first_time)
{

    if (IsMissingDictionaryPage(page_number))
        InitMissingDictionaryPage();
	else if (IsInstallDictionaryPage(page_number))
		InitInstallDictionaryPage();
	else if (IsDownloadPage(page_number))
		InitDownloadPage();
	else if (IsLicensePage(page_number))
		InitLicensePage();
	else if (IsDefaultLanguagePage(page_number))
		InitDefaultLanguagePage();
	else
		OP_ASSERT(!"there should be no other page than the above");

	ResetDialog(); // needed for some reason to show/invalidate all texts set

}

/***************************************************************
 *
 * IsLastPage
 *
 *
 **************************************************************/
BOOL InstallLanguageDialog::IsLastPage(INT32 page_number)
{
	OpWidget* page = GetPageByNumber(page_number);
	if (page->GetName().Compare(GetDefaultLanguagePageName()) == 0)
		return TRUE;
	else
		return FALSE;
}

/***************************************************************
 *
 * OnChange
 *
 *
 **************************************************************/
void InstallLanguageDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{

	if (IsLicensePage(GetCurrentPage()))
	{
	    OpButton* license_check_box = (OpButton*)GetWidgetByName("accept_dictionary_license_checkbox");
        // If the 'accept license' check box was clicked
        if (widget && widget == license_check_box)
        {
            if (widget->GetValue() == 1)
	            EnableForwardButton(TRUE);
            else
                EnableForwardButton(FALSE);
        }
    }
	else if(IsMissingDictionaryPage(GetCurrentPage()) && widget->GetType() == OpTypedObject::WIDGET_TYPE_RADIOBUTTON)
	{
	    EnableForwardButton(TRUE);
	}

}


/***************************************************************
 *
 * InitInstallDictionaryPage
 *
 *
 **************************************************************/
void InstallLanguageDialog::InitMissingDictionaryPage()
{

    OP_NEW_DBG("::InitMissingDictionaryPage", "Dictionary");

	// Make the small hint on the bottom italic and a bit greyed out.
    OpMultilineEdit* edit = (OpMultilineEdit*)GetWidgetByName("label_for_changing_dictionary_hint");
    if (edit)
    {
        edit->SetForegroundColor(OP_RGB(64, 64, 64));
        edit->SetFontInfo(NULL, 11, TRUE, 4, JUSTIFY_LEFT);
    }

    OpString prefered_language_id;

    if (g_languageManager && DictionaryHandler::GetBestLanguageMatch(g_languageManager->GetLanguage(), prefered_language_id))
    {
        // Fill out the name of the language and language id for the dictionary we think match the best.
		OpString prefered_language_name;
		RETURN_VOID_IF_ERROR(FindLanguageName(prefered_language_id, prefered_language_name));
		prefered_language_name.AppendFormat(UNI_L(" [%s]"), prefered_language_id.CStr());
		
		OpRadioButton* radio = (OpRadioButton*)GetWidgetByName("new_matching_dictionary");
		
		if (radio && prefered_language_name.HasContent())
		{
		    radio->SetText(prefered_language_name.CStr());
		    // Tick off the checkbox with the language we found as default.
		    radio->SetValue(1);
		}
		
    }
    
	OP_DBG(("CurrentPage %d - LokalPage %d", GetCurrentPage(), m_cur_page));

}

/***************************************************************
 *
 * InitInstallDictionaryPage
 *
 *
 **************************************************************/
void InstallLanguageDialog::InitInstallDictionaryPage()
{

    OP_NEW_DBG("::InitInstallDictionaryPage", "Dictionary");

    // If you have clicked back after installing dictionaries.
    // The forward button should be enabled.
    if (m_new_languages.GetCount())
        EnableForwardButton(TRUE);

	// Set up the Quick find text box
	m_quickfind = (OpQuickFind*) GetWidgetByName("Spellcheck_install_quickfind");
	if (m_quickfind)
	    m_quickfind->SetTarget(m_tree_view);

    // Set the "to download size" label. Should initially be 0
	SetSizeStringFromSize( 0, m_total_size);
	OpLabel* label = (OpLabel*) GetWidgetByName("label_for_spellcheck_download_size");
	if (label) 
		label->SetText(m_total_size.CStr());
		
		
	OP_DBG(("CurrentPage %d - LokalPage %d", GetCurrentPage(), m_cur_page));
	
}


/***************************************************************
 *
 * InitDownloadPage
 *
 *
 **************************************************************/
void InstallLanguageDialog::InitDownloadPage()
{
    OP_NEW_DBG("::InitDownloadPage", "Dictionary");

    ShowWidget("download_failed_icon", FALSE);
    ShowWidget("download_failed_label", FALSE);
        
    if (m_download_state != DOWNLOAD_ERROR)
    {
	    m_progress = (OpProgressBar*)GetWidgetByName("spellcheck_download_progress");
	    if(m_progress)
	    {
		    m_progress->SetVisibility(TRUE);
		    m_progress->SetProgress(0,0);
	    }
	}
	else if (m_download_state == DOWNLOAD_ERROR)
	    ShowError();	    
	    
	OP_DBG(("CurrentPage %d - LokalPage %d", GetCurrentPage(), m_cur_page));
}


/***************************************************************
 *
 * InitLicensePage
 *
 *
 **************************************************************/
void InstallLanguageDialog::InitLicensePage()
{

    OP_NEW_DBG("::InitLicensePage", "Dictionary");

    // Untick the check box for ever page load (both back and forth in history)
    OpButton* license_check_box = (OpButton*)GetWidgetByName("accept_dictionary_license_checkbox");
    
    if (license_check_box && license_check_box->GetValue())
        license_check_box->SetValue(0);

    // Find the index of the dictionary we just installed and want to show the dictionary for.
    int index = -1;
    
    // License to show
    m_license_to_show = m_cur_page - GetPageByName(GetLicensePageName());
    OP_DBG(("Show license page %d. Current page is %d.", m_license_to_show, m_cur_page));
    
    if (m_license_to_show < m_new_languages.GetCount())
    {
        // Make a fake spell check session so we can querry the dictionaries.
        OpSpellUiSessionImpl session(0);
        int num = session.GetInstalledLanguageCount();
		OpString language_name;
		language_name.Set(m_new_languages.Get(m_license_to_show)->GetLanguageId());
        
        // Setting the license index       
        for (int i = 0; i < session.GetInstalledLanguageCount(); i++)
        {
            if (DictionaryHandler::CompareLanguageId(language_name.CStr(), session.GetInstalledLanguageStringAt(i)))
                index = i;
        }
        
        OP_DBG(("Cores license index is %d out of %d dictionaries", index, num));

        if (index > -1 && index < num)
        {

	        // License text multilineedit.
	        OpMultilineEdit* multiline = (OpMultilineEdit*)GetWidgetByName("dictionary_license_text_box");
    
		    if (multiline)
		    {
                //  Style the text box
		        WIDGET_COLOR colour;
			    colour.use_default_background_color = FALSE;
			    colour.use_default_foreground_color = TRUE;
			    colour.background_color = OP_RGB(0xff, 0xff, 0xff);
        
			    multiline->SetColor(colour);
			    multiline->SetScrollbarStatus(SCROLLBAR_STATUS_AUTO);

			    if (index != -1)
			    {
                    // Set the license text for this language
			        OpString license_text;
        
				    if (OpStatus::IsSuccess(session.GetInstalledLanguageLicenceAt( index, license_text)))
				    {
				        multiline->SetText(license_text.CStr());
					    multiline->Relayout(TRUE, TRUE);
					    multiline->SetVisibility(TRUE);
			        }
			    }
			}
		}

	    // Header label
        OpLabel* header_label = (OpLabel*)GetWidgetByName("dictionary_license_header");
    
		if (header_label)
		{
		    OpString language_name;
			RETURN_VOID_IF_ERROR(FindLanguageName(session.GetInstalledLanguageStringAt(index), language_name));
		    header_label->SetRelativeSystemFontSize(120);

			OpString lbl_text;
			OpString title;
			g_languageManager->GetString(Str::D_DICTIONARY_ACCEPT_LICENSE_TITLE, title);
			lbl_text.AppendFormat(title, language_name.CStr());
			header_label->SetText(lbl_text.CStr());
			header_label->SetEllipsis(ELLIPSIS_NONE);
        }
        
    }
    else
    {
        // If something went wrong while trying to read the license text,
        // just move to the next page.
        OP_DBG(("Warning: Can't show license page %d", index));
        m_cur_page++;
        OP_DBG(("Dictionary page %d.", m_cur_page));
        SetCurrentPage(GetCurrentPage() + 1);
    }
    
	OP_DBG(("CurrentPage %d - LokalPage %d", GetCurrentPage(), m_cur_page));

}


/***************************************************************
 *
 * InitDefaultLanguagePage
 *
 *
 **************************************************************/
void InstallLanguageDialog::InitDefaultLanguagePage()
{
    OP_NEW_DBG("::InitDefaultLanguagePage", "Dictionary");
    
	m_label = (OpLabel*)GetWidgetByName("default_language_label");

	if(m_label)
		m_label->SetVisibility(TRUE);
	
	m_listbox = (OpListBox*)GetWidgetByName("language_selection_listbox");

	if(m_listbox)
	{
		m_listbox->SetVisibility(TRUE);
		m_listbox->RemoveAll();

		g_internal_spellcheck->RefreshInstalledLanguages();

		OpString_list languages;
		if(OpStatus::IsSuccess(GetInstalledLanguages(languages)))
		{
			const uni_char* default_lang = g_internal_spellcheck->GetDefaultLanguage();
			for(unsigned int i=0; i<languages.Count(); i++)
			{
				OpString language_name;
				RETURN_VOID_IF_ERROR(FindLanguageName(languages[i], language_name));
				uni_char* user_data = OP_NEWA(uni_char, languages[i].Length() + 1);
				if(user_data)
				{
					uni_strcpy(user_data, languages[i].CStr());
					language_name.AppendFormat(UNI_L(" [%s]"), user_data);
					m_listbox->AddItem(language_name, i, NULL, reinterpret_cast<INTPTR>(user_data));

					if(languages[i].Compare(default_lang) == 0)
						m_listbox->SelectItem(i, TRUE);
				}
			}
		}
	}

	OP_DBG(("CurrentPage %d - LokalPage %d", GetCurrentPage(), m_cur_page));

}

/***************************************************************
 *
 * GetOkAction
 *
 *
 **************************************************************/
const uni_char*	InstallLanguageDialog::GetOkText()
{
	if(m_show_update_text)
		return m_update_text.CStr();

	return Dialog::GetOkText();
}


/***************************************************************
 *
 * GetOkAction
 *
 *
 **************************************************************/
OpInputAction* InstallLanguageDialog::GetOkAction()
{
	if(m_show_update_text)
		return OP_NEW(OpInputAction, (OpInputAction::ACTION_UPDATE_SPELLCHECK_LANGUAGES));
 
	return Dialog::GetOkAction();
}


/***************************************************************
 *
 * OnOk
 *
 *
 **************************************************************/
UINT32 InstallLanguageDialog::OnOk()
{
	if(m_listbox)
	{
		INT32 index = m_listbox->GetSelectedItem();
		if(index >= 0)
		{
			const uni_char* default_lang = (uni_char*)m_listbox->GetItemUserData(index);
			if(default_lang)
			{
				g_internal_spellcheck->SetDefaultLanguage(default_lang);
				g_input_manager->InvokeAction(OpInputAction::ACTION_INTERNAL_SPELLCHECK_LANGUAGE, index);
			}
		}
	}
	return 0;
}

/***************************************************************
 *
 * OnItemChanged
 * 
 *
 **************************************************************/
void InstallLanguageDialog::OnItemChanged(OpWidget *widget, INT32 pos)
{

	if (widget && widget == m_tree_view)
    {
        OpTreeView* tree_view = static_cast<OpTreeView*>(widget);
        
        if (tree_view)
        {
            OpFileLength download_size = CalculateTotalSize(tree_view);
            
        	OpLabel* label = (OpLabel*) GetWidgetByName("label_for_spellcheck_download_size");
	        if (label) 
	        {
		        SetSizeStringFromSize(download_size, m_total_size);
		        label->SetText(m_total_size.CStr());
	        }
	        
        	g_input_manager->UpdateAllInputStates();
	    }
	}
		
}

/***************************************************************
 *
 * IsMissingDictionaryPage
 * - Checks if the page number is the page number of the
 *   missing dictionary page
 *
 **************************************************************/
BOOL InstallLanguageDialog::IsMissingDictionaryPage(INT32 page_number) const
{
	return (GetPageByNumber(page_number)->GetName().Compare(GetMissingDictionaryPageName()) == 0);
}

/***************************************************************
 *
 * IsInstallDictionaryPage
 * - Checks if the page number is the page number of the
 *   install dictionary page
 *
 **************************************************************/
BOOL InstallLanguageDialog::IsInstallDictionaryPage(INT32 page_number) const
{
	return (GetPageByNumber(page_number)->GetName().Compare(GetInstallDictionaryPageName()) == 0);
}

/***************************************************************
 *
 * IsDownloadPage
 * - Checks if the page number is the page number of the
 *   download page.
 *
 **************************************************************/
BOOL InstallLanguageDialog::IsDownloadPage(INT32 page_number) const
{
	return (GetPageByNumber(page_number)->GetName().Compare(GetDownloadPageName()) == 0);
}

/***************************************************************
 *
 * IsLicensePage
 * - Checks if the page number is the page number of the
 *   license page. 
 * 
 **************************************************************/
BOOL InstallLanguageDialog::IsLicensePage(INT32 page_number) const
{
	return (GetPageByNumber(page_number)->GetName().Compare(GetLicensePageName()) == 0);
}

/***************************************************************
 *
 * IsDefaultLanguagePage
 * - Checks if the page number is the page number of the Default
 *   language page.
 *
 **************************************************************/
BOOL InstallLanguageDialog::IsDefaultLanguagePage(INT32 page_number) const
{
	return (GetPageByNumber(page_number)->GetName().Compare(GetDefaultLanguagePageName()) == 0);
}

/***************************************************************
 *
 * GetPageByName
 * - Finds the page index using the page name.
 *
 **************************************************************/
INT32 InstallLanguageDialog::GetPageByName(const OpStringC8 & name)
{
    UINT32 page_count = GetPageCount();
    
	for (UINT32 i = 0; i < page_count; i++)
	{
		OpWidget* page = GetPageByNumber(i);
		if (page->GetName().Compare(name) == 0)
			return i;
	}
	return 0;
}


/***************************************************************
 *
 * OnForward
 * - Returning the page to show when forward is pressed.
 *
 **************************************************************/
UINT32 InstallLanguageDialog::OnForward(UINT32 page_number)
{
	
	if (IsMissingDictionaryPage(page_number))
	{
	    INT32 page = -1;
	    OpRadioButton* radio;
	    
	    // The user chose to install the one we found matching best
	    //   * Go to Default Language page if download was not initiated
	    //   * Go to Download page if download is started
	    radio = (OpRadioButton*)GetWidgetByName("new_matching_dictionary");
	    if (radio->GetValue() == 1)
	    {
	        OpString language_id;
	        if (DictionaryHandler::GetBestLanguageMatch(g_languageManager->GetLanguage(), language_id))
	            AddLanguageToDownloadList(language_id.CStr());
	        else
	            OP_ASSERT(!"We could not find that language id");
	            
	        UpdateLanguages();
	        
	        if (m_download_state == IDLE)
                page = GetPageByName(GetDefaultLanguagePageName());
            else
	            page = GetPageByName(GetDownloadPageName());
	            
	    }
	    
	    // The user just wanted to keep using the default dictionary.
	    //   * Go to OnCancel, there is nothing really to do.
	    radio = (OpRadioButton*)GetWidgetByName("use_default_dictionary");
		if (radio->GetValue() == 1)
		{
		    page = 0;
			OnCancel();
			Close( FALSE, TRUE);
		}
			
		// The user want to see the list and chose him self.
		//   * Go to the install dictionary page
		radio = (OpRadioButton*)GetWidgetByName("choose_from_list_dictionary");
		if (radio->GetValue() == 1)
			page = GetPageByName(GetInstallDictionaryPageName());
			
		OP_ASSERT(page != -1);
		
		return page;
	}
	else if (IsInstallDictionaryPage(page_number))
	{
 		UpdateLanguages();

        if (m_download_state == IDLE && m_new_languages.GetCount() > 0)
            return GetPageByName(GetLicensePageName());
        else if (m_download_state == IDLE)
            return GetPageByName(GetDefaultLanguagePageName());
        else
            return GetPageByName(GetDownloadPageName());

	}
	else if (IsDownloadPage(page_number))
	{
		return GetPageByName(GetLicensePageName());
	}
	else if (IsLicensePage(page_number))
	{
	    int license_page_num = GetPageByName(GetLicensePageName());
	    
	    if (m_cur_page > 0 && m_cur_page < license_page_num + m_new_languages.GetCount())
	        return license_page_num;
	    else
			return GetPageByName(GetDefaultLanguagePageName());
	}
	else
	{
		OP_ASSERT(!"this shouldn't happen, there's no next page");
	}

	return 0; // todo: handle pages that aren't found
}

/***************************************************************
 *
 * CalculateTotalSize
 * - Takes a filled out tree view and calculates how many bytes in
 *   total all the dictionaries that have been checked off takes up.
 *
 **************************************************************/
OpFileLength InstallLanguageDialog::CalculateTotalSize(OpTreeView* tree_view)
{
    OP_NEW_DBG("::CalculateTotalSize", "Dictionary");
    
	OpFileLength total_size = 0;
	OpTreeModelItem* item = NULL;
	
	if (tree_view)
	{
	    for (INT32 i = 0; i < tree_view->GetItemCount(); i++)
	    {
	        item = tree_view->GetItemByPosition(i);
			if (item && tree_view->IsItemChecked(i) && tree_view->HasItemChanged(i))
	        {
				INT32 id = item->GetID();
				total_size += GetDownloadSize(m_languages_model.GetItemByID(id));
            }
	    }
	
	}

	OP_DBG(("Total size is %ld", total_size));
	
	return total_size;
}

/***************************************************************
 *
 * GetDownloadSize
 * - Takes a Tree item, and returns the size in bytes as reported
 *   by the server in the dictionary.xml file
 *
 **************************************************************/
OpFileLength InstallLanguageDialog::GetDownloadSize(SimpleTreeModelItem* item)
{
    OP_NEW_DBG("::GetDownloadSize", "Dictionary");
    
    OpFileLength* size = NULL;
    
	if (item)
	{
		size = (OpFileLength*)item->GetUserData();
	}
	
    OP_DBG(("The download size is %ld", size ? (*size) : 0));

	return size ? (*size) : 0;
}

/***************************************************************
 *
 * UpdateTotalSizeString
 * - Takes file size in bytes, and makes a pretty human readable
 *   string with Kb/Mb appended on the end.
 *
 **************************************************************/
OP_STATUS InstallLanguageDialog::SetSizeStringFromSize(OpFileLength size, OpString& total_size)
{	
	OpString size_string;
	if (size_string.Reserve(256))
		StrFormatByteSize(size_string, size, SFBS_ABBRIVATEBYTES);

	OpString total_size_str;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::D_SPELLCHECK_LANGUAGES_TOTAL_SIZE, total_size_str));

	total_size.Empty();
	RETURN_IF_ERROR(total_size.Set(total_size_str));
	RETURN_IF_ERROR(total_size.Append(UNI_L(" ")));
	RETURN_IF_ERROR(total_size.Append(size_string.CStr()));
	
	return OpStatus::OK;
}

void InstallLanguageDialog::ShowError()
{
    // Show the error icon and text
    ShowWidget("download_failed_icon", TRUE);
    ShowWidget("download_failed_label", TRUE);
    
    // Hide the progress bar as well as the label
    ShowWidget("download_label", FALSE);
    ShowWidget("spellcheck_download_progress", FALSE);

	// Call OP_DELETE on each ILD_DictionaryItem from m_new_languages.
	// Destroying an ILD_DictionaryItem causes stopping its current download,
	// setting its listener to NULL and deleting the file resource downloaded
	// so far, if any.
	m_new_languages.DeleteAll();
}
/***************************************************************
 *
 * OnInputAction
 *
 **************************************************************/
BOOL InstallLanguageDialog::OnInputAction(OpInputAction* action)
{
    
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();
			
			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_UPDATE_SPELLCHECK_LANGUAGES:
				{
					return TRUE;
				}
				case OpInputAction::ACTION_CANCEL:
				{
					child_action->SetEnabled(TRUE);
					return TRUE;
				}
				case OpInputAction::ACTION_OK:
				{
					child_action->SetEnabled(TRUE);
					return TRUE;
				}
				case OpInputAction::ACTION_BACK:
				{
					if(IsDownloadPage(GetCurrentPage()))
					{
						child_action->SetEnabled(m_download_state == IDLE);
						return TRUE;
					}
					break;
				}
				case OpInputAction::ACTION_FORWARD:
				{
					if(IsInstallDictionaryPage(GetCurrentPage()))
					{
						child_action->SetEnabled(m_tree_view && m_tree_view->HasChanged());
					}
					else if(IsDownloadPage(GetCurrentPage()))
					{
					    child_action->SetEnabled(m_download_state == IDLE);		    
						return TRUE;
					}
					else if(IsLicensePage(GetCurrentPage()))
					{
                        // If the "accept license" check box is checked, then the forward button should be enabled,
                        // otherwise disabled.
						OpButton* license_check_box = (OpButton*)GetWidgetByName("accept_dictionary_license_checkbox");
						BOOL checked = license_check_box && license_check_box->GetValue();
                        child_action->SetEnabled(checked);
					    return TRUE;
					}
					else
					{
						child_action->SetEnabled(TRUE);						
					}
					return TRUE;
				}
			}
			break;
			
 	    //****************************************************
 	    //  - End of OpInputAction::ACTION_GET_ACTION_STATE
 	    //****************************************************
 	    
		}
		case OpInputAction::ACTION_UPDATE_SPELLCHECK_LANGUAGES:
		{
			m_enable_update = FALSE;
			m_tree_view->SetEnabled(FALSE);
			
			return TRUE;
		}
		case OpInputAction::ACTION_FORWARD:
		{
		    m_cur_page++;
		    break;
		}
		case OpInputAction::ACTION_BACK:
		{
		    m_cur_page--;
		    break;
		}
		case OpInputAction::ACTION_CANCEL:
		{
		    // If cancel is clicked then delete all the dictionaries. 
		    // They have to accept all the licenses and finish the wizard to install them.
		    for (UINT32 i=0; i<m_new_languages.GetCount(); i++)
			{
				ILD_DictionaryItem* item = m_new_languages.Get(i);
				OP_ASSERT(item);
				OpString language_id;

				if (OpStatus::IsSuccess(language_id.Set(item->GetLanguageId())))
					m_handler.DeleteDictionary(language_id);
			}

			// Stop transfers, set listeners to NULL, delete resources, we're not needing these any more
			m_new_languages.DeleteAll();
		        
		    g_internal_spellcheck->RefreshInstalledLanguages();
		    break;
		}
	}
	
	return Dialog::OnInputAction(action);
}



/***************************************************************
 *
 * UpdateLanguages
 * - Installs new dictionaries checked in the tree view, and 
 *   uninstalls/deletes unchecked dictionaries.
 *
 **************************************************************/
OP_STATUS InstallLanguageDialog::UpdateLanguages()
{
    OP_STATUS status = OpStatus::OK;

	// Get a list of the dictionaries which have been checked
	OpVector<OpString> new_checked;
	RETURN_IF_ERROR(GetNewChecked(new_checked));
	
	// Get a list of the dictionaries which have been unchecked
	OpVector<OpString> new_unchecked;
	RETURN_IF_ERROR(GetNewUnchecked(new_unchecked));


    // Uninstall dictionaries which have been deselected
    for (UINT32 i = 0; i < new_unchecked.GetCount() ; i++)
    {
        OpString file_name;
        file_name.Set(new_unchecked.Get(i)->CStr());
        if(OpStatus::IsError(m_handler.DeleteDictionary(file_name)))
            status = OpStatus::ERR;
    }

    g_internal_spellcheck->RefreshInstalledLanguages();
    
    // Removed dictionaries from the new_checked list that has already been
	// installed in this Dialog session (happens when people have clicked back after installing)
	OpString_list installed_dict;
	g_internal_spellcheck->GetInstalledLanguages(installed_dict);
	
	for (UINT32 i = 0; i < installed_dict.Count(); i++)
	{
	    for (UINT32 j = 0; j < new_checked.GetCount(); j++)
	    {
			if (m_handler.CompareLanguageId(installed_dict[i].CStr(), new_checked.Get(j)->CStr()))
	        {
	            new_checked.Delete(j);
	            break;
	        }
	    }
	}
	
	// Install dictionaries which has been selected and not already installed
	if(new_checked.GetCount() > 0)
	{
        // Store the languages, so we know what licenses to show.
		m_new_languages.DeleteAll();
		m_downloaded_count = 0;
		m_resolved_count = 0;
		m_download_state = DOWNLOAD_ERROR;

        for (UINT32 i = 0; i < new_checked.GetCount(); i++)
        {
			OpAutoPtr<ILD_DictionaryItem> new_item(OP_NEW(ILD_DictionaryItem, ()));
			RETURN_OOM_IF_NULL(new_item.get());

			RETURN_IF_ERROR(new_item->SetLanguageId(*new_checked.Get(i)));
            m_new_languages.Add(new_item.get());
			RETURN_IF_ERROR(g_autoupdate_manager->CheckForAddition(UpdatableResource::RTDictionary, new_item->GetLanguageId()));
			new_item.release();
        }
		m_download_state = DOWNLOADING;
	}
	else
        m_download_state = IDLE;

    new_checked.DeleteAll();
    new_unchecked.DeleteAll();

	return status;
}

ILD_DictionaryItem* InstallLanguageDialog::GetItemByLanguage(const OpStringC& language_id)
{
	for (UINT32 i=0; i<m_new_languages.GetCount(); i++)
	{
		ILD_DictionaryItem* item = m_new_languages.Get(i);
		OP_ASSERT(item);
		OpString item_language;
		RETURN_VALUE_IF_ERROR(item_language.Set(item->GetLanguageId()), NULL);
		if (item_language.CompareI(language_id) == 0)
			return item;
	}
	return NULL;
}

ILD_DictionaryItem* InstallLanguageDialog::GetItemByDownloader(FileDownloader* dictionary)
{
	for (UINT32 i=0; i<m_new_languages.GetCount(); i++)
	{
		ILD_DictionaryItem* item = m_new_languages.Get(i);
		OP_ASSERT(item);
		if (dictionary == item->GetDictionary())
			return item;
	}
	return NULL;
}

OP_STATUS InstallLanguageDialog::OnAllDictionariesDownloaded()
{
	if (m_all_downloaded_ok && m_all_installed_ok)
	{
		m_cur_page++;
		m_download_state = IDLE;
		EnableBackButton(TRUE);
		SetCurrentPage(GetCurrentPage() + 1);
	}
	else
		ShowError();

	return OpStatus::OK;
}

OP_STATUS InstallLanguageDialog::UpdateDownloadProgress()
{
	m_progress->SetVisibility(TRUE);
		
	OpString total_size_string;
	if( !total_size_string.Reserve(128))
		return OpStatus::ERR_NO_MEMORY;
		
	if (!StrFormatByteSize(total_size_string, m_total_download_size, SFBS_ABBRIVATEBYTES))
		return OpStatus::ERR_NO_MEMORY;
		    
	OpString downloaded_size_string;
	if (!downloaded_size_string.Reserve(128))
		return OpStatus::ERR_NO_MEMORY;
		    
	if (!StrFormatByteSize(downloaded_size_string, m_total_downloaded_size, SFBS_ABBRIVATEBYTES))
		return OpStatus::ERR_NO_MEMORY;
		    
	OpString label;
	RETURN_IF_ERROR(label.AppendFormat(UNI_L("%s / %s"), downloaded_size_string.CStr(), total_size_string.CStr()));
		
	RETURN_IF_ERROR(m_progress->SetLabel(label.CStr(), TRUE));
		
	m_progress->SetProgress(m_total_downloaded_size, m_total_download_size);
	m_progress->SyncLayout();
	m_progress->Sync();

	return OpStatus::OK;
}

void InstallLanguageDialog::OnAdditionResolved(UpdatableResource::UpdatableResourceType type, const OpStringC& key, UpdatableResource* resource)
{
	if (UpdatableResource::RTDictionary != type)
		return;

	OP_ASSERT(resource);
	OP_ASSERT(key.HasContent());
	OP_ASSERT(resource->GetType() == UpdatableResource::RTDictionary);

	ILD_DictionaryItem* item = GetItemByLanguage(key);
	OP_ASSERT(item);

	UpdatableDictionary* dict = static_cast<UpdatableDictionary*>(resource);
	item->SetDictionary(dict);
	m_resolved_count++;

	if (m_resolved_count == m_new_languages.GetCount())
		OnAllDictionariesResolved();
}

OP_STATUS InstallLanguageDialog::ProcessDictionaryDownloaded(FileDownloader* downloader, DownloadResult result)
{
	ILD_DictionaryItem* item = GetItemByDownloader(downloader);
	if (!item)
		return OpStatus::ERR;

	OP_ASSERT(item->GetDictionary());
	m_downloaded_count++;
	switch (result)
	{
	case Downloaded:
		if (OpStatus::IsError(item->GetDictionary()->UpdateResource()))
			m_all_installed_ok = FALSE;
		break;
	case Failed:
	case Aborted:
		m_all_downloaded_ok = FALSE;
		break;
	default:
		OP_ASSERT(!"Unknown download result");
		break;
	}

	if (m_downloaded_count == m_new_languages.GetCount())
		RETURN_IF_ERROR(OnAllDictionariesDownloaded());

	return OpStatus::OK;
}

void InstallLanguageDialog::OnFileDownloadDone(FileDownloader* file_downloader, OpFileLength total_size)
{
	if (OpStatus::IsError(ProcessDictionaryDownloaded(file_downloader, Downloaded)))
		ShowError();
}

void InstallLanguageDialog::OnFileDownloadFailed(FileDownloader* file_downloader)
{
	if (OpStatus::IsError(ProcessDictionaryDownloaded(file_downloader, Failed)))
		ShowError();
}

void InstallLanguageDialog::OnFileDownloadAborted(FileDownloader* file_downloader)
{
	if (OpStatus::IsError(ProcessDictionaryDownloaded(file_downloader, Aborted)))
		ShowError();
}

void InstallLanguageDialog::OnFileDownloadProgress(FileDownloader* file_downloader, OpFileLength total_size, OpFileLength downloaded_size, double kbps, unsigned long time_estimate)
{
	m_total_downloaded_size = 0;
	for (UINT32 i=0; i<m_new_languages.GetCount(); i++)
	{
		OP_ASSERT(m_new_languages.Get(i));
		OP_ASSERT(m_new_languages.Get(i)->GetDictionary());
		m_total_downloaded_size += m_new_languages.Get(i)->GetDictionary()->GetHaveSize();
	}

	OpStatus::Ignore(UpdateDownloadProgress());
}

void InstallLanguageDialog::OnAllDictionariesResolved()
{
	BOOL all_resolved_ok = TRUE;
	for (UINT32 i=0; i<m_new_languages.GetCount(); i++)
		if (NULL == m_new_languages.Get(i)->GetDictionary())
			all_resolved_ok = FALSE;

	if (all_resolved_ok)
	{
		// Start downloads
		m_total_download_size = 0;
		m_total_downloaded_size = 0;
		m_all_downloaded_ok = TRUE;
		m_all_installed_ok = TRUE;
		for (UINT32 i=0; i<m_new_languages.GetCount(); i++)
		{
			ILD_DictionaryItem* item = m_new_languages.Get(i);
			OP_ASSERT(item);
			int dictionary_size = 0;
			UpdatableDictionary* dictionary = item->GetDictionary();
			OP_ASSERT(dictionary);
			if (OpStatus::IsError(dictionary->GetAttrValue(URA_SIZE, dictionary_size)))
				ShowError();

			m_total_download_size += dictionary_size;
			if (OpStatus::IsError(dictionary->StartDownloading(this)))
				ShowError();
		}
		if (OpStatus::IsError(UpdateDownloadProgress()))
			ShowError();
	}
	else
	{
		m_new_languages.DeleteAll();
		m_resolved_count = 0;
		m_downloaded_count = 0;
		ShowError();
	}
}

void InstallLanguageDialog::OnAdditionResolveFailed(UpdatableResource::UpdatableResourceType type, const OpStringC& key)
{
	if (UpdatableResource::RTDictionary != type)
		return;

	OP_ASSERT(key.HasContent());
	m_resolved_count++;

	if (m_resolved_count == m_new_languages.GetCount())
		OnAllDictionariesResolved();
}

/***************************************************************
 *
 * FindLanguageName
 * - Send in a language_id and get the language name in return 
 *   if found. Otherwise an empthy string will be returned.
 *
 **************************************************************/
OP_STATUS InstallLanguageDialog::FindLanguageName(OpStringC language_id, OpString& language_name)
{
	UINT32 count = m_languages_model.GetCount();
	language_name.Empty();

	// First search through the dictionaries in the list view
	for (UINT32 i = 0; i < count; i++)
	{
		const uni_char* item_string = m_languages_model.GetItemStringByID(i, ID_COLUMN);

		if (DictionaryHandler::CompareLanguageId(item_string, language_id))
		{
		    RETURN_IF_ERROR(language_name.Set(m_languages_model.GetItemStringByID(i, TEXT_COLUMN)));
		    break;
		}
	}

#ifdef WIC_SPELL_DICTIONARY_FULL_NAME
	// Then search through the already installed dictionaries
	if (language_name.IsEmpty())
	{
        OpSpellUiSessionImpl session(0);

        // Setting the license index       
        for (int i = 0; i < session.GetInstalledLanguageCount(); i++)
        {
            if (DictionaryHandler::CompareLanguageId(language_id, session.GetInstalledLanguageStringAt(i)))
                RETURN_IF_ERROR(language_name.Set(session.GetInstalledLanguageNameAt(i)));
        }
	}
#endif // WIC_SPELL_DICTIONARY_FULL_NAME

	return OpStatus::OK;
}


/***************************************************************
 *
 * AddLanguageToDownloadList
 * - Adds a dictionary to the download list, to be downloaded later.
 *   (it checks the right dictionary in the tree view)
 *
 **************************************************************/
OP_STATUS InstallLanguageDialog::AddLanguageToDownloadList(const uni_char* language_id)
{
	SimpleTreeModelItem* item = GetModelItem(language_id);
	
	if (!item)
	    return OpStatus::ERR;
	    
    INT32 index = m_tree_view->GetItemByID(item->GetID());
    
    if (index == -1)
        return OpStatus::ERR;
    m_tree_view->SetCheckboxValue(index, TRUE);
    
	return OpStatus::OK;
}


/***************************************************************
 *
 * GetModelItem
 * - Returns the item which has that language_id
 *
 **************************************************************/
SimpleTreeModelItem* InstallLanguageDialog::GetModelItem(const uni_char* language_id)
{
	UINT32 count = m_languages_model.GetCount();
	const uni_char* item_string = NULL;
	
	for (UINT32 id = 0; id < count; id++)
	{
		item_string = m_languages_model.GetItemStringByID(id, ID_COLUMN);
		
		if (DictionaryHandler::CompareLanguageId(item_string, language_id))
		    return m_languages_model.GetItemByID(id);
	}
	
	return NULL;
}


/////////////////////////////////////////////////////////////////////////////
//                       DictionaryHandler
/////////////////////////////////////////////////////////////////////////////


/***************************************************************
 *
 * InstallDictionaries
 * - N/A
 *
 **************************************************************/
void DictionaryHandler::InstallDictionaries()
{
	// TODO: Get function to do the install
	// Update the installed languages in the spellchecker module
}


/******************************************************************
 *
 * GetAvailableDictionaries
 *
 * Reads the dictionaries file dictionaries.xml in the profile folder
 * and adds the languages available for download to languages_model
 * (as language-name, dictionary-file-size, language-id)
 *
 *
 ******************************************************************/
OP_STATUS DictionaryHandler::GetAvailableDictionaries(SimpleTreeModel& languages_model, const uni_char* path, OpFileFolder folder)
{
	OpFile languages_file;

	RETURN_IF_ERROR(languages_file.Construct(path, folder));

	RETURN_IF_ERROR(languages_file.Open(OPFILE_READ));

	// Parse XML file
	XMLFragment fragment;
	RETURN_IF_ERROR(fragment.Parse(&languages_file));

	RETURN_IF_ERROR(languages_file.Close());

	return GetAvailableLanguages(fragment, languages_model);
}

/***************************************************************
 *
 * GetAvailableLanguages
 * - Read the XML to find out what dicionaries are available.
 *
 **************************************************************/
OP_STATUS DictionaryHandler::GetAvailableLanguages(XMLFragment& fragment, SimpleTreeModel& languages_model)
{

    if (!g_internal_spellcheck)
        return OpStatus::ERR;

    // First get a list of the languages that are installed, so we can initally check them in the model
    OpString_list initial_languages;
    RETURN_IF_ERROR(g_internal_spellcheck->RefreshInstalledLanguages());
	RETURN_IF_ERROR(g_internal_spellcheck->GetInstalledLanguages(initial_languages));

	UINT32 id = 0;
	languages_model.RemoveAll();

	if (fragment.EnterElement(UNI_L("dictionaries")))
	{
		OpString name;
		OpString size;
		OpString language_id;
		while (fragment.EnterElement(UNI_L("dictionary")))
		{
			OpStringC lang_id = fragment.GetAttribute(UNI_L("id"));
			language_id.Set(fragment.GetAttribute(UNI_L("id")));

			if (fragment.EnterElement(UNI_L("name")))
			{
				name.Set(fragment.GetText());
				fragment.LeaveElement();
			}
			
			if (fragment.EnterElement(UNI_L("size")))
			{
				size.Set(fragment.GetText());
				fragment.LeaveElement();
			}

			fragment.LeaveElement();
			
			// Test to see if it is a language that is already installed.
			BOOL checked = FALSE;
			for (UINT32 i = 0; i < initial_languages.Count(); i++)
			{
			    if (language_id.CompareI(initial_languages.Item(i)) == 0)
                    checked = TRUE;
            }
            
            // If the language is already installed, check if if it is possible to uninstall it.
            BOOL should_be_listed = TRUE;
            if (checked)
            {
                OpString file_name;
                file_name.Set(lang_id.CStr());
                // GetActualFileName only checks the folder where you have installed extra dictionaries
                if (OpStatus::IsError(GetActualFileName(file_name)))
                    should_be_listed = FALSE;
                
            }
            
    
			// Add it to the model, if it is not a preinstalled dictionary.
            if (should_be_listed)            
			{
	            // Prepare the user data
                OpFileLength* dict_size = OP_NEW(OpFileLength, ());
                OpFileLength bytes = 0;
            
		        if (!dict_size)
				    dict_size = NULL;
			    else
			    {
			        *dict_size = uni_atoi(size.CStr());
			        bytes = *dict_size;
		        }
			
                //
                // Add item to model, and add data (Language)
                //
			    INT32 index = languages_model.AddItem(name,   //string
			                                          NULL,   // image
			                                             0,   // sort_order
			                                            -1,   // parent
			                              (void*)dict_size,   // user_data
			                                            id,   // id
			                   OpTypedObject::UNKNOWN_TYPE,   // type
							                       checked); // initially_checked
			                                
			    
			    //
			    // Add data to column 2 (Size)
			    // 
			    OpString size_string;
							            			    
				if (dict_size && size_string.Reserve(256))
                    StrFormatByteSize(size_string, bytes, SFBS_ABBRIVATEBYTES);
                
			    languages_model.SetItemData(index, InstallLanguageDialog::SIZE_COLUMN, size_string.CStr(), NULL, (INT32)*dict_size);


                //
                // Add data to column 3 (Language ID)
                //
			    languages_model.SetItemData(index, InstallLanguageDialog::ID_COLUMN, language_id.CStr());

				id++;
				
			}
		}

		fragment.LeaveElement();
	}
	
	return OpStatus::OK;
}


/***************************************************************
 *
 * GetPositionFromID
 * - Send in language id (ie. en-GB) and will return the index
 *   of the language in the tree_model
 *
 **************************************************************/
INT32 DictionaryHandler::GetPositionFromID(SimpleTreeModel& tree_model, const uni_char* language_id)
{
	for (INT32 i = 0; i < tree_model.GetCount(); i++)
	{
		OpTreeModelItem::ItemData item_data(OpTreeModelItem::COLUMN_QUERY);
		item_data.column_query_data.column = InstallLanguageDialog::ID_COLUMN;
		item_data.column_query_data.column_text = OP_NEW(OpString, ());
		SimpleTreeModelItem* item = tree_model.GetItemByIndex(i);
		
		if (OpStatus::IsSuccess(item->GetItemData(&item_data)))
		{
			if (CompareLanguageId(*item_data.column_query_data.column_text, language_id))
			{
				OP_DELETE(item_data.column_query_data.column_text);
				return i;
			}
		}

		OP_DELETE(item_data.column_query_data.column_text);
	}
	return -1;
}


/***************************************************************
 *
 * CompareLanguageId
 * Case insensitive, ignore '-'/'_' difference
 *
 **************************************************************/
BOOL DictionaryHandler::CompareLanguageId(const uni_char* lang_id_1, const uni_char* lang_id_2)
{
	OpString id_1, id_2;
	id_1.Set(lang_id_1);
	id_2.Set(lang_id_2);
	
	int len = id_1.Length();
	int i;
	for(i=0; i<len; i++)
	{
		if(id_1[i] == '_')
		{
			id_1[i] = '-';
		}
	}
	
	len = id_2.Length();
	for(i=0; i<len; i++)
	{
		if(id_2[i] == '_')
		{
			id_2[i] = '-';
		}
	}
	
	return id_1.CompareI(id_2) == 0 ? TRUE : FALSE;
}

#ifdef FIND_BEST_DICTIONARY_MATCH
/***************************************************************
 *
 * ShowInstallDialogIfNeeded
 * - Show Install Language dialog, if user haven't got the dictionary for this language
 *
 **************************************************************/
void DictionaryHandler::ShowInstallDialogIfNeeded(int spell_session_id)
{
    
    // Check if the user has chosen to not show this dialog anymore
	// abd if it is too early to show it.
    if (!g_pcui->GetIntegerPref(PrefsCollectionUI::AskAboutMissingDictionary) || spell_session_id < MISSING_DICT_TRIGGER_LIMIT)
        return;

    // Find the best matching dictionary, and show the dialog if it is not installed.
    OpString best_match;
	if (DictionaryHandler::GetBestLanguageMatch(g_languageManager->GetLanguage(), best_match))
	{
	
	    if (!DictionaryHandler::IsDictionaryInstalled(best_match))
	    {
		    InstallLanguageDialog* dialog = OP_NEW(InstallLanguageDialog, ());

		    if (dialog)
		        dialog->Init(g_application->GetActiveDesktopWindow(), 0);    
		        
        }
        else
        {
			// If the best match is already installed, just set the pref to never ask again.
            TRAPD(stat, g_pcui->WriteIntegerL(PrefsCollectionUI::AskAboutMissingDictionary, FALSE));
        }
        
	}
}
#endif // FIND_BEST_DICTIONARY_MATCH

/***************************************************************
 *
 * GetBestLanguageMatch
 * - Used to find the language id of the dictionary that is either
 *   equal to the prefered language, or a best match if there is one.
 * @ param prefered_language_id [in]  Id of the language we want a dictionary for
 * @ param matching_language_id [out] Language id of the dictionary we found best
 *   suitable. Note: there might not be one, and you will get an empthy string.
 * @ return Will return TRUE if we found a perfect or close match (ie we found
 *   "en", although we wanted en-GB), but will return FALSE if no dictionary was found suitable.
 *
 **************************************************************/
BOOL DictionaryHandler::GetBestLanguageMatch(const OpStringC ui_language_id, OpString& matching_language_id)
{
    matching_language_id.Empty();

	// Check if there is a dictionary with that language ID avaliable for download
	if (DictionaryHandler::IsDictionaryAvailable(ui_language_id.CStr()))
 	    matching_language_id.Set(ui_language_id.CStr());
 	    
	// If there is not one matching the whole language id, then check if it
	// is four char language id, if only the two first is matching, that is good enough.
	if (matching_language_id.IsEmpty() && ui_language_id.Length() > 3)
	{
		matching_language_id.Set(ui_language_id.CStr(), 2);
		if (!DictionaryHandler::IsDictionaryAvailable(matching_language_id))
 	        matching_language_id.Empty();
 	}
	    
	return matching_language_id.HasContent();
	
}

/***************************************************************
 *
 * IsDictionaryInstalled
 * - Send in a language_id and the function will return TRUE if there
 *   is a matching dictinoary already installed, otherwise return FALSE
 *
 **************************************************************/
BOOL DictionaryHandler::IsDictionaryInstalled(OpString& language_id)
{

    BOOL is_installed = FALSE;
    
    OpSpellUiSessionImpl session(0);
    int dict_count = session.GetInstalledLanguageCount();
    int dict_index = 0;
    
    while (!is_installed && dict_index < dict_count)
    {
        const uni_char* dict_id = session.GetInstalledLanguageStringAt(dict_index);
		if (DictionaryHandler::CompareLanguageId(dict_id, language_id.CStr()))
			is_installed =TRUE;
    
        dict_index++;
    }
 
    return is_installed;   
}

/***************************************************************
 *
 * IsDictionaryAvailable
 * - Checks if dictionary is available for download
 *
 **************************************************************/
BOOL DictionaryHandler::IsDictionaryAvailable(const uni_char* lang_id)
{
	DictionaryHandler handler;
	SimpleTreeModel languages_model(3);
	handler.GetAvailableDictionaries(languages_model, DICTIONARIES_FILENAME, OPFILE_DICTIONARY_FOLDER);

	for(INT32 i = 0; i < languages_model.GetCount(); i++)
	{
		OpTreeModelItem::ItemData item_data(OpTreeModelItem::COLUMN_QUERY);
		item_data.column_query_data.column = InstallLanguageDialog::ID_COLUMN;
		item_data.column_query_data.column_text = OP_NEW(OpString, ());
		SimpleTreeModelItem* item = languages_model.GetItemByIndex(i);
		
		if (OpStatus::IsSuccess(item->GetItemData(&item_data)))
		{
			if (DictionaryHandler::CompareLanguageId(item_data.column_query_data.column_text->CStr(), lang_id))
		    {
		        OP_DELETE(item_data.column_query_data.column_text);
		        return TRUE;
	        }
		}
		
		OP_DELETE(item_data.column_query_data.column_text);
	}
	
	return FALSE;
}

/***************************************************************
 *
 * GetActualFileName
 * - This is a work around since the server gives us the dictionaries
 *   with slightly different names than in the spec. The different variants
 *   can be summed up like this: en_GB, en_gb, en-gb, en-GB.
 *
 **************************************************************/
OP_STATUS DictionaryHandler::GetActualFileName(OpString& file_name)
{

    // Add .zip at the end if it is not already there
    if (file_name.Find(UNI_L(".zip")) == KNotFound)
        file_name.Append(UNI_L(".zip"));

    // Test the name as is    
    if (OpStatus::IsSuccess(VerifyFileName(file_name)))
         return OpStatus::OK;

	OpString tmp;
	tmp.Set(file_name);

    // The file name must be 7 chars long, if not, it should be corect already.
    // It is only those long file names with a _ or - which might be wrong from the server side.
	if (tmp.Length() >= 7)
	{	
	
		// Test with big letters on the end in stead of small (ie en-GB instead of en-gb)
	    tmp.MakeUpper();
	    file_name.CStr()[3] = tmp.CStr()[3];
	    file_name.CStr()[4] = tmp.CStr()[4];
	
	    if (OpStatus::IsSuccess(VerifyFileName(file_name)))
	         return OpStatus::OK;
   
   
	    // Test with _ instead of - (ie en_GB instead of en_GB)
	    int index = file_name.Find(UNI_L("-"));
	
		if (index != KNotFound)
		{
		    file_name.DataPtr()[index] = '_';
	
			if (OpStatus::IsSuccess(VerifyFileName(file_name)))
				return OpStatus::OK;
		}

	
	    // Test with all small case, and - instead of _
	    if (index != KNotFound)
		{
	        tmp.MakeLower();
	        file_name.Set(tmp);
	        file_name.DataPtr()[index] = '_';
	    
	        if (OpStatus::IsSuccess(VerifyFileName(file_name)))
	            return OpStatus::OK;
        }
	         
	}
	
	// We really can't find this file, give up.
	return OpStatus::ERR;

}

/***************************************************************
 *
 * VerifyFileName
 *
 **************************************************************/
OP_STATUS DictionaryHandler::VerifyFileName(OpString& file_name)
{

    OpFile file;
    OP_STATUS status = OpStatus::OK;
    BOOL exists;
    
	if(OpStatus::IsSuccess(file.Construct(file_name, OPFILE_DICTIONARY_FOLDER)))
	{
        if(OpStatus::IsSuccess(file.Exists(exists)))
        {
            if (exists)
			    status = OpStatus::OK;
			else
				status = OpStatus::ERR;
        }

		file.Close();
		
	}
	
    return status;
}

/***************************************************************
 *
 * DeleteDictionary
 *
 **************************************************************/
OP_STATUS DictionaryHandler::DeleteDictionary(OpString& file_name)
{
    
    OP_STATUS status = OpStatus::ERR;
    
    if(OpStatus::IsSuccess(GetActualFileName(file_name)))
	{
	    OpFile file;
	    
	    if(OpStatus::IsSuccess(file.Construct(file_name, OPFILE_DICTIONARY_FOLDER)))
	    {
		    if(OpStatus::IsSuccess(file.Delete(FALSE)))
		        status = OpStatus::OK;
	        else
	        {
		        // This assert should never go off, it should always be possible to
		        // Uninstall/delete a dictionary. Usually it is sharing violation
		        // that makes it fail.
			    OP_ASSERT(!"Can't delete dictionary");
		    }
		}
	}

    return status;
}

#endif // AUTO_UPDATE_SUPPORT
#endif // INTERNAL_SPELLCHECK_SUPPORT
