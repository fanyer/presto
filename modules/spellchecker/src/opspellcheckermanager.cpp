/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef INTERNAL_SPELLCHECK_SUPPORT

#include "modules/spellchecker/src/opspellchecker.h"
#include "modules/spellchecker/opspellcheckerapi.h"

#include "modules/pi/system/OpFolderLister.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/zipload.h"
#include "modules/util/opstring.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefsfile/prefsfile.h"

unsigned long GetSCProgressTime(OpSpellCheckerSession::SessionState state, OpSpellCheckerSession::SpellCheckPriority priority)
{
	OP_ASSERT(state != OpSpellCheckerSession::IDLE);
	if (state == OpSpellCheckerSession::LOADING_DICTIONARY)
		return SPC_DICTIONARY_LOADING_SLICE_MS;
	
	OP_ASSERT(state == OpSpellCheckerSession::SPELL_CHECKING);
	if (priority == OpSpellCheckerSession::NORMAL)
		return SPC_SPELL_CHECKING_SLICE_NORMAL_MS;
	OP_ASSERT(priority == OpSpellCheckerSession::BACKGROUND);
	return SPC_SPELL_CHECKING_SLICE_BACKGROUND_MS;
}

unsigned long GetSCDelayTime(OpSpellCheckerSession::SessionState state, OpSpellCheckerSession::SpellCheckPriority priority)
{
	OP_ASSERT(state != OpSpellCheckerSession::IDLE);
	if (state == OpSpellCheckerSession::LOADING_DICTIONARY)
		return SPC_DICTIONARY_LOADING_DELAY_MS;
	
	OP_ASSERT(state == OpSpellCheckerSession::SPELL_CHECKING);
	if (priority == OpSpellCheckerSession::NORMAL)
		return SPC_SPELL_CHECKING_DELAY_NORMAL_MS;
	OP_ASSERT(priority == OpSpellCheckerSession::BACKGROUND);
	return SPC_SPELL_CHECKING_DELAY_BACKGROUND_MS;
}

OP_STATUS ClearLastUsedInPrefs()
{
	OP_STATUS status = OpStatus::OK;
	OpString lang;
	lang.Set(UNI_L(""));
	TRAP(status, g_pccore->WriteStringL(PrefsCollectionCore::LastUsedSpellcheckLanguage, lang));
	return status;
}

/* ===============================OpSpellCheckerManager=============================== */

OpSpellCheckerManager::OpSpellCheckerManager()
{
	m_has_refreshed_languages = FALSE;
	m_session_in_progress_deleted = FALSE;
	m_session_in_progress = NULL;
	m_default_language = NULL;
	m_language_with_no_sessions = NULL;

	int i, *r, *e;
	int separator_ranges[] = {
	0x01, '0'-1,
	'9'+1, '?',
	'Z'+1,'a'-1,
	'z'+1, 0xA9,
	0xAB, 0xB1,
	0xB4, 0xB4,
	0xB6, 0xB8,
	0xBB, 0xBB,
	0xBF, 0xBF,
	0};
	int exceptions[] = {'\'','-',0};
	op_memset(m_word_separators, 0, SPC_WORD_SEPARATOR_LOOKUP_CODEPOINTS/8);
	for (r = separator_ranges; *r; r += 2)
	{
		for (i=r[0];i<=r[1];i++)
			m_word_separators[i>>3] |= 1<<(i&7);
	}
	for (e = exceptions; *e; e++)
		m_word_separators[*e>>3] &= ~(1<<(*e&7));

}

OpSpellCheckerManager::~OpSpellCheckerManager()
{
	OpSpellCheckerLanguage *language = (OpSpellCheckerLanguage*)m_languages.First();
	while (language)
	{
		OpSpellCheckerLanguage *next = (OpSpellCheckerLanguage*)language->Suc();
		OP_DELETE(language);
		language = next;
	}
	if (g_main_message_handler)
		g_main_message_handler->UnsetCallBacks(this);
}

OP_STATUS OpSpellCheckerManager::Init()
{
	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_SPELLCHECK_PROGRESS, 0));
	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_SPELLCHECK_BACKGROUND_UPDATE, 0));
	return OpStatus::OK;
}
	
BOOL OpSpellCheckerManager::FreeCachedData(BOOL toplevel_context)
{
	BOOL freed = FALSE;
	if (m_language_with_no_sessions)
	{
		m_language_with_no_sessions->DeleteSpellChecker();
		m_language_with_no_sessions = NULL;
		freed = TRUE;
	}
	OpSpellCheckerLanguage *language;
	for (language = (OpSpellCheckerLanguage*)m_languages.First(); language; language = (OpSpellCheckerLanguage*)language->Suc())
	{
		if (language->GetSpellChecker())
			freed = freed || language->GetSpellChecker()->FreeCachedData(toplevel_context);
	}
	return freed;
}

void OpSpellCheckerManager::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (msg != MSG_SPELLCHECK_PROGRESS && msg != MSG_SPELLCHECK_BACKGROUND_UPDATE)
	{
		OP_ASSERT(!"Shouldn't be possible!!!");
		return;
	}
	if (msg == MSG_SPELLCHECK_BACKGROUND_UPDATE)
	{
		ProcessBackgroundUpdates();
		return;
	}
	OpSpellCheckerSession *session = (OpSpellCheckerSession*)par1;
	BOOL progress_continue = FALSE;
	double time_out = (double)GetSCProgressTime(session->GetState(), session->GetPriority());
	if (time_out != 0.0)
		time_out += g_op_time_info->GetWallClockMS();
	session->Progress(time_out, progress_continue);
	if (progress_continue)
	{
		unsigned long delay = GetSCDelayTime(session->GetState(), session->GetPriority());
		if (!g_main_message_handler->PostDelayedMessage(MSG_SPELLCHECK_PROGRESS, (MH_PARAM_1)session, 0, delay))
		{
			session->SetState(OpSpellCheckerSession::HAS_FAILED);
			session->GetListener()->OnError(session, OpStatus::ERR, NULL);
		}
	}
}

BOOL OpSpellCheckerManager::SpellcheckEnabledByDefault()
{
	return !!g_pccore->GetIntegerPref(PrefsCollectionCore::SpellcheckEnabledByDefault);
}

void OpSpellCheckerManager::SetSpellcheckEnabledByDefault(BOOL by_default)
{
	TRAPD(status, g_pccore->WriteIntegerL(PrefsCollectionCore::SpellcheckEnabledByDefault, by_default));
}

void OpSpellCheckerManager::ProcessBackgroundUpdates()
{
	OpSpellCheckerWordIterator *word_iterator;
	SpellUpdateObject *update;
	do
	{
		update = (SpellUpdateObject*)m_background_updates.First();
		if (!update)
			return;
		word_iterator = update->GetWordIterator();
		if (!word_iterator)
		{
			word_iterator = update->GetUIHandler()->GetAllContentIterator();
			if (!word_iterator)
			{
				OP_DELETE(update);
				continue;
			}
			update->SetWordIterator(word_iterator);
		}
	} while (!word_iterator);
	SpellUIHandler *handler = update->GetUIHandler();
	double time_out = g_op_time_info->GetWallClockMS() + (double)SPC_SPELL_UPDATE_MS;
	BOOL continue_iteration;
	do
	{
		const uni_char *current = word_iterator->GetCurrentWord();
		if (current && !uni_strcmp(update->GetWord(), current))
		{
			if (update->MarkOK())
				handler->OnCorrectSpellingFound(NULL, word_iterator);
			else
				handler->OnMisspellingFound(NULL, word_iterator, NULL);
		}
		continue_iteration = word_iterator->ContinueIteration();
	} while (continue_iteration && g_op_time_info->GetWallClockMS() < time_out);
	if (!continue_iteration)
		OP_DELETE(update);
	if (m_background_updates.First())
		g_main_message_handler->PostDelayedMessage(MSG_SPELLCHECK_BACKGROUND_UPDATE, 0, 0, SPC_SPELL_UPDATE_DELAY_MS);
}

OP_STATUS OpSpellCheckerManager::StartBackgroundUpdate(SpellUIHandler *handler, const uni_char *str, BOOL mark_ok)
{
	SpellUpdateObject *update = OP_NEW(SpellUpdateObject, (handler, mark_ok));
	if (!update)
		return OpStatus::ERR_NO_MEMORY;
	OP_STATUS status = update->SetWord(str);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(update);
		return status;
	}
	if (!m_background_updates.First())
	{
		status = g_main_message_handler->PostMessage(MSG_SPELLCHECK_BACKGROUND_UPDATE, 0, 0);
		if (OpStatus::IsError(status))
		{
			OP_DELETE(update);
			return status;
		}
	}
	update->Into(&m_background_updates);
	return OpStatus::OK;
}

OP_STATUS OpSpellCheckerManager::GetInstalledLanguages(OpString_list &language_list)
{
	if (!m_has_refreshed_languages)
		RETURN_IF_ERROR(RefreshInstalledLanguages());
	language_list.Resize(m_valid_languages.GetSize());
	int i;
	for (i=0;i<m_valid_languages.GetSize();i++)
		RETURN_IF_ERROR(language_list.Item(i).Set(m_valid_languages.GetElement(i)->GetLanguageString()));
	return OpStatus::OK;
}

void OpSpellCheckerManager::MakeLanguagesUnavailable()
{
	OpSpellCheckerLanguage* language;
	for (language = (OpSpellCheckerLanguage*)m_languages.First(); language; language = (OpSpellCheckerLanguage*)language->Suc())
		language->SetAvailable(FALSE);
}

OpSpellCheckerLanguage *OpSpellCheckerManager::GetOpSpellCheckerLanguage(const uni_char *lang)
{
	if (!lang || !*lang)
		return NULL;
	OpSpellCheckerLanguage *language;
	for (language = (OpSpellCheckerLanguage*)m_languages.First(); language; language = (OpSpellCheckerLanguage*)language->Suc())
	{
		if (!uni_strcmp(language->GetLanguageString(), lang))
			return language;
	}
	return NULL;
}

OP_STATUS OpSpellCheckerManager::AddOrRefreshLanguage(const uni_char *lang, const uni_char *dic_path, const uni_char *affix_path, const uni_char *own_path, const uni_char* metadata_path, const uni_char* license_path)
{
	OpSpellCheckerLanguage *language = GetOpSpellCheckerLanguage(lang);
	OP_STATUS status;

	if (language)
	{
		if (language->IsAvailable())
		{
			// Language added during this refresh.
			return OpStatus::OK;
		}
		else if (language->SamePaths(dic_path, affix_path, own_path, metadata_path, license_path))
		{
			// Revive previously available language.
			language->SetAvailable(TRUE);
			return OpStatus::OK;
		}
		else
		{
			// Language exists but comes from a different dictionary.
			if (!language->HasSessions())
				OP_DELETE(language);
			if (language == m_default_language)
			{
				if (language->IsLastUsedInPrefs())
					ClearLastUsedInPrefs();
				m_default_language = NULL;
			}
			language = NULL;
		}
	}

	language = OP_NEW(OpSpellCheckerLanguage, ());
	if (!language)
		return OpStatus::ERR_NO_MEMORY;

	status = language->SetLanguage(lang);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(language);
		return status;
	}
  
	status = language->SetPaths(dic_path, affix_path, own_path, metadata_path, license_path);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(language);
		return status;
	}

#ifdef WIC_SPELL_DICTIONARY_FULL_NAME
	status = language->SetLanguageName();
	if (OpStatus::IsError(status))
	{
		OP_DELETE(language);
		return status;
	}
#endif // WIC_SPELL_DICTIONARY_FULL_NAME

	language->SetAvailable(TRUE);

	language->Into(&m_languages);

	return OpStatus::OK;
}

OpSpellCheckerLanguage* OpSpellCheckerManager::GetLastLanguageInPrefs()
{
	const OpStringC string = g_pccore->GetStringPref(PrefsCollectionCore::LastUsedSpellcheckLanguage);
	return GetOpSpellCheckerLanguage(string.CStr());
}


void OpSpellCheckerManager::DeterminePathFromExtension(const uni_char *dic_full_path, const uni_char* ext, OpString& file_path)
{
	OP_STATUS status; 
	BOOL exists;
	OpFile file;
	OpString tmp_path;

	file_path.Empty();

	OP_ASSERT( dic_full_path );

	status = tmp_path.Set(dic_full_path);
	if ( OpStatus::IsError(status) )
		return;

	OP_ASSERT( tmp_path.Length() >= 5 );

	uni_strcpy(tmp_path.DataPtr()+tmp_path.Length()-3, ext);

	status = file.Construct(tmp_path.CStr());
	if ( OpStatus::IsError(status) )
		return;
 
	status = file.Exists(exists);
	if ( OpStatus::IsError(status) )
		return;
		
	if ( !exists )
		return;

	status = file_path.Set(tmp_path);
	OpStatus::Ignore(status);
}

void OpSpellCheckerManager::DetermineLincensePath(const uni_char *dic_full_path, OpString& license_path)
{
	// License file name. The license file should lie next to the actual dictionary (the .dic file)
	const uni_char* license_file_name = UNI_L("license.txt");

	BOOL exists;
	OpFile license_file;
	OpString tmp_path;

	license_path.Empty();
	OP_ASSERT(dic_full_path);
	
	RETURN_VOID_IF_ERROR(tmp_path.Set(dic_full_path));
	if (tmp_path.IsEmpty())
		return;

	// Remove the dictionary file name, and add the license file name instead.
	tmp_path.Delete(tmp_path.FindLastOf(PATHSEPCHAR) + 1);
	RETURN_VOID_IF_ERROR(tmp_path.Append(license_file_name));

	// Construct file object, and check if file exists.
	RETURN_VOID_IF_ERROR(license_file.Construct(tmp_path.CStr()));
	RETURN_VOID_IF_ERROR(license_file.Exists(exists));

	if (!exists)
		return;

	OpStatus::Ignore(license_path.Set(tmp_path.CStr()));
}

extern OP_STATUS OwnFileFromDicPath(const uni_char * dictionary_path, OpFile& own_file);

void OpSpellCheckerManager::DetermineOwnPath(const uni_char *dic_full_path, OpString& own_path)
{
	OP_STATUS status; 
	BOOL exists;
	OpFile own_file;

	own_path.Empty();

	if (OpStatus::IsError(OwnFileFromDicPath(dic_full_path, own_file)))
		return;

	status = own_file.Exists(exists);
	if ( OpStatus::IsError(status) )
		return;
		
	if ( !exists )
		return;

	status = own_path.Set(own_file.GetFullPath());
	OpStatus::Ignore(status);
}


#define SPC_MAX_DICTIONARY_FOLDER_DEPTH 40

OP_STATUS OpSpellCheckerManager::RefreshInstalledLanguages()
{
	OpVector<OpFolderLister> listers;
    listers.SetAllocationStepSize(SPC_MAX_DICTIONARY_FOLDER_DEPTH);
	MakeLanguagesUnavailable();
	m_valid_languages.Reset();

#ifdef FOLDER_MANAGER_LOCALE_FOLDER_SUPPORT
    // Create an OpFolderLister for all available locale folders:
    for (unsigned int i=0; i<g_folder_manager->GetLocaleFoldersCount(); ++i)
    {
        OpString locale_folder_path;
        OP_STATUS err = g_folder_manager->GetLocaleFolderPath(i, locale_folder_path);
        if (OpStatus::IsSuccess(err))
        {
            OpFolderLister* lister = 0;
            if (OpStatus::IsError(OpFolderLister::Create(&lister)) ||
                OpStatus::IsError(lister->Construct(locale_folder_path.CStr(), UNI_L("*"))) ||
                OpStatus::IsError(listers.Add(lister)))
            {
                OP_DELETE(lister);
            }
        }
    }
#endif // FOLDER_MANAGER_LOCALE_FOLDER_SUPPORT

    OpString dic_folder_path;
    OP_STATUS dic_folder_ok = g_folder_manager->GetFolderPath(OPFILE_DICTIONARY_FOLDER, dic_folder_path);
    if (OpStatus::IsSuccess(dic_folder_ok))
    {
        OpFolderLister* lister = 0;
        if (OpStatus::IsError(OpFolderLister::Create(&lister)) ||
            OpStatus::IsError(lister->Construct(dic_folder_path.CStr(), UNI_L("*"))) ||
            OpStatus::IsError(listers.Add(lister)))
        {
            OP_DELETE(lister);
        }
    }

    OP_STATUS status = OpStatus::OK;
	while (listers.GetCount() > 0)
	{
        UINT32 level = listers.GetCount()-1;
        OpFolderLister* lister = listers.Get(level);
		if (!lister->Next())
		{
            listers.Remove(level);
			OP_DELETE(lister);
			continue; // continue with next OpFolderLister
		}
		const uni_char* file_name = lister->GetFileName();
		if (!file_name || !*file_name || !uni_strcmp(file_name, UNI_L("..")) || !uni_strcmp(file_name, UNI_L(".")))
			continue; // continue with next file in same OpFolderLister

		const uni_char* full_path = lister->GetFullPath();
		if (!full_path || !*full_path)
			continue; // continue with next file in same OpFolderLister

		int len = uni_strlen(file_name);

		if (lister->IsFolder())
		{
			if (level == SPC_MAX_DICTIONARY_FOLDER_DEPTH-1)
                // we are "too deep" in the file hierarchy:
                continue; // continue with next file in same OpFolderLister

            OpFolderLister* lister = 0;
            if (OpStatus::IsError(OpFolderLister::Create(&lister)) ||
                OpStatus::IsError(lister->Construct(full_path, UNI_L("*"))) ||
                OpStatus::IsError(listers.Add(lister)))
            {
                OP_DELETE(lister);
            }
			continue; // continue with first file in new OpFolderLister
		}
#ifdef ZIPFILE_DIRECT_ACCESS_SUPPORT
		else if (len > 4 && uni_strnicmp(file_name+len-4, UNI_L(".zip"), 4) == 0)
		{
			if (level == SPC_MAX_DICTIONARY_FOLDER_DEPTH-1)
                // we are "too deep" in the file hierarchy:
                continue; // continue with next file in same OpFolderLister

            OpFolderLister* lister = 0;
            if (OpStatus::IsError(OpZipFolderLister::Create(&lister)) ||
                OpStatus::IsError(lister->Construct(full_path, UNI_L("*"))) ||
                OpStatus::IsError(listers.Add(lister)))
            {
                OP_DELETE(lister);
            }
			continue; // continue with first file in new OpZipFolderLister
		}
#endif // ZIPFILE_DIRECT_ACCESS_SUPPORT
		if (len < 5 || uni_strnicmp(file_name+len-4, UNI_L(".dic"), 4) != 0)
			continue; // continue with next file in same OpFolderLister

		OpString lang;
		if (OpStatus::IsError(lang.Set(file_name, uni_strlen(file_name)-4)))
			break;

        OpString affix_path, own_path, metadata_path, license_path;
		DeterminePathFromExtension(full_path, UNI_L("aff"), affix_path);
        if (!affix_path.CStr())
			continue; // continue with next file in same OpFolderLister
        DetermineOwnPath(full_path, own_path);
		DeterminePathFromExtension(full_path, UNI_L("ini"), metadata_path);
		DetermineLincensePath(full_path, license_path);
		if (OpStatus::IsError(AddOrRefreshLanguage(lang.CStr(), full_path, affix_path.CStr(), own_path.CStr(), metadata_path.CStr(), license_path.CStr())))
			break;
	}

	while (listers.GetCount() > 0)
	{
        OpFolderLister* lister = listers.Remove(listers.GetCount()-1);
        OP_DELETE(lister);
    }

	if (m_default_language && !m_default_language->IsAvailable())
		m_default_language = NULL;

	OpSpellCheckerLanguage *language = (OpSpellCheckerLanguage*)m_languages.First(), *next;
	while (language)
	{
		next = (OpSpellCheckerLanguage*)language->Suc();
		if (language->IsAvailable())
		{
			if (!m_default_language)
				m_default_language = language;
			int idx = m_valid_languages.AddElement(language);
			if (idx < 0)
				status = SPC_ERROR(OpStatus::ERR_NO_MEMORY);
			language->SetIndex(idx);
		}
		else 
		{
			if (!language->HasSessions())
				OP_DELETE(language);
			else
				language->SetIndex(-1);
		}
		language = next;
	}
	OpSpellCheckerLanguage *last_used = GetLastLanguageInPrefs();
	if (last_used)
		m_default_language = last_used;
	m_has_refreshed_languages = OpStatus::IsSuccess(status);
	return status;
}

void OpSpellCheckerManager::OnLanguageLoadingFinished(OpSpellCheckerLanguage *language, BOOL succeeded)
{
	language->SetHasFailed(!succeeded);
	if (succeeded)
	{
		language->SetLastUsedInPrefs();
		m_default_language = language;
	}
	else if (m_default_language == language)
	{
		int i;
		if (language->IsLastUsedInPrefs())
			ClearLastUsedInPrefs();
		for (i=0;i<m_valid_languages.GetSize();i++)
		{
			if (!m_valid_languages.GetElement(i)->GetHasFailed())
			{
				m_default_language = m_valid_languages.GetElement(i);
				break;
			}
		}
	}
}

BOOL OpSpellCheckerManager::IsWordSeparator(uni_char c)
{
	if (c < SPC_WORD_SEPARATOR_LOOKUP_CODEPOINTS)
		return !!(m_word_separators[c>>3] & (1<<(c&7)));
	if (Unicode::GetWordBreakType(c) == WB_Other && !is_ambiguous_break(c))
		return TRUE;
	return FALSE;
}

const uni_char* OpSpellCheckerManager::GetDefaultLanguage()
{
	if (!m_has_refreshed_languages)
	{
		if (OpStatus::IsError(RefreshInstalledLanguages()))
			return NULL;
	}
	return m_default_language ? m_default_language->GetLanguageString() : NULL;
}

BOOL OpSpellCheckerManager::HasInstalledLanguages()
{ 
	return !!GetDefaultLanguage(); 
}

BOOL OpSpellCheckerManager::SetDefaultLanguage(const uni_char *lang)
{
	if (!m_has_refreshed_languages)
	{
		if (OpStatus::IsError(RefreshInstalledLanguages()))
			return FALSE;
	}
	
	OpSpellCheckerLanguage *language = GetOpSpellCheckerLanguage(lang);
	if (!language || !language->HasValidPaths())
		return FALSE;
	m_default_language = language;
	return TRUE;
}

int OpSpellCheckerManager::GetInstalledLanguageCount()
{
	if (!m_has_refreshed_languages)
	{
		if (OpStatus::IsError(RefreshInstalledLanguages()))
			return FALSE;
	}
	return m_valid_languages.GetSize();
}

const uni_char* OpSpellCheckerManager::GetInstalledLanguageStringAt(int index)
{
	OpSpellCheckerLanguage *language = GetInstalledLanguageAt(index);
	return language ? language->GetLanguageString() : NULL;
}

OpSpellCheckerLanguage *OpSpellCheckerManager::GetInstalledLanguageAt(int index)
{
	if (!m_has_refreshed_languages)
	{
		if (OpStatus::IsError(RefreshInstalledLanguages()))
			return NULL;
	}
	if (index < 0 || index >= m_valid_languages.GetSize())
		return NULL;
	return m_valid_languages.GetElement(index);
}

OP_STATUS OpSpellCheckerManager::CreateSession(const uni_char *lang, OpSpellCheckerListener *listener, OpSpellCheckerSession *&session, BOOL synchronous)
{
	session = NULL;
	if (!listener)
	{
		OP_ASSERT(!"Can't be NULL!!!");
		return OpStatus::ERR;
	}
	if (!m_has_refreshed_languages)
		RETURN_IF_ERROR(RefreshInstalledLanguages());
	OpSpellCheckerLanguage *language = !lang || !*lang ? m_default_language : GetOpSpellCheckerLanguage(lang);
	if (!language)
		return OpStatus::ERR_NO_SUCH_RESOURCE;
	session = OP_NEW(OpSpellCheckerSession, (this, language, listener, synchronous, NULL));
	if (!session)
		return OpStatus::ERR_NO_MEMORY;
	session->SetState(OpSpellCheckerSession::LOADING_DICTIONARY);
	language->AddSession(session);
	if (m_language_with_no_sessions)
	{
		if (m_language_with_no_sessions != language)
			m_language_with_no_sessions->DeleteSpellChecker();
		m_language_with_no_sessions = NULL;
	}
	if (synchronous)
	{
		BOOL progress_continue = FALSE;
		session->Progress(0, progress_continue);
		OP_ASSERT(!progress_continue);
	}
	else
	{
		if (!g_main_message_handler->PostMessage(MSG_SPELLCHECK_PROGRESS, (MH_PARAM_1)session, 0))
		{
			OP_DELETE(session);
			session = NULL;
			return OpStatus::ERR;
		}
	}
	return OpStatus::OK;
}

void OpSpellCheckerManager::OnSpellCheckerListenerDeleted(OpSpellCheckerListener *listener)
{
	SpellUpdateObject *update_object;
	for (update_object = (SpellUpdateObject*)m_background_updates.First(); update_object; update_object = (SpellUpdateObject*)update_object->Suc())
	{
		if (update_object->GetUIHandler() == listener)
		{
			OP_DELETE(update_object);
			break;
		}
	}

	OpSpellCheckerLanguage *language;
	for (language = (OpSpellCheckerLanguage*)m_languages.First(); language; language = (OpSpellCheckerLanguage*)language->Suc())
	{
		OpSpellCheckerSession *session = (OpSpellCheckerSession*)language->GetSessions()->First();
		while (session)
		{
			OpSpellCheckerSession *next = (OpSpellCheckerSession*)session->Suc();
			if (session->GetListener() == listener)
				OP_DELETE(session);
			session = next;
		}
	}
}

void OpSpellCheckerManager::OnWordIteratorDeleted(OpSpellCheckerWordIterator *word_iterator)
{
	if (g_spell_ui_data && g_spell_ui_data->GetWordIterator() == word_iterator)
		g_spell_ui_data->InvalidateData();
	SpellUpdateObject *update_object;
	for (update_object = (SpellUpdateObject*)m_background_updates.First(); update_object; update_object = (SpellUpdateObject*)update_object->Suc())
	{
		if (update_object->GetWordIterator() == word_iterator)
		{
			OP_DELETE(update_object);
			break;
		}
	}
	OpSpellCheckerLanguage *language;
	for (language = (OpSpellCheckerLanguage*)m_languages.First(); language; language = (OpSpellCheckerLanguage*)language->Suc())
	{
		OpSpellCheckerSession *session = (OpSpellCheckerSession*)language->GetSessions()->First();
		while (session)
		{
			OpSpellCheckerSession *next = (OpSpellCheckerSession*)session->Suc();
			if (session->GetWordIterator() == word_iterator)
				OP_DELETE(session);
			session = next;
		}
	}
}

void OpSpellCheckerManager::OnLanguageHasNoSessions(OpSpellCheckerLanguage *language)
{
	if (m_language_with_no_sessions)
		m_language_with_no_sessions->DeleteSpellChecker();
	m_language_with_no_sessions = language;
}

void OpSpellCheckerManager::OnLanguageDeleted(OpSpellCheckerLanguage *language)
{
	language->Out();
	if (language == m_language_with_no_sessions)
		m_language_with_no_sessions = NULL;
	if (language == m_default_language)
		m_default_language = NULL;
}

/* ===============================OpSpellCheckerLanguage=============================== */

OpSpellCheckerSession::OpSpellCheckerSession(OpSpellCheckerManager *manager, OpSpellCheckerLanguage *language, OpSpellCheckerListener *listener, BOOL synchronous, SortedStringCollection *ignore_words)
{
	m_language = language;
	m_manager = manager;
	m_listener = listener;
	m_synchronous = synchronous;
	m_state = IDLE;
	m_priority = NORMAL;
	m_find_replacements = FALSE;
	m_stopped = FALSE;
	m_ignore_words = ignore_words;
	m_replacement_search_max_ms = SPC_REPLACEMENT_SEARCH_MAX_DEFAULT_MS;
	m_replacement_timeout = 0.0;
	if (ignore_words)
		ignore_words->IncRef();
}

OpSpellCheckerSession::~OpSpellCheckerSession()
{
	if (g_main_message_handler)
		g_main_message_handler->RemoveDelayedMessage(MSG_SPELLCHECK_PROGRESS, (MH_PARAM_1)this, 0);
	if (g_internal_spellcheck)
	{
		m_language->RemoveSession(this);
		if (g_internal_spellcheck->GetSessionInProgress() == this)
			g_internal_spellcheck->SetSessionInProgressDeleted(TRUE);
	}
	if (m_ignore_words)
	{
		if (m_ignore_words->DecRef() <= 0)
			OP_DELETE(m_ignore_words);
	}
}

OP_STATUS OpSpellCheckerSession::CheckText(OpSpellCheckerWordIterator *start, BOOL find_replacements)
{
	if (m_state != IDLE || !m_language->HasDictionary())
	{
		OP_ASSERT(!"This is NOT the right time to call this function!");
		return OpStatus::ERR;
	}
	m_find_replacements = find_replacements;
	m_state = SPELL_CHECKING;
	m_lookup_state.word_iterator = start;
	if (m_synchronous)
	{
		BOOL progress_continue = FALSE;
		Progress(0, progress_continue);
		OP_ASSERT(!progress_continue);
	}
	else
	{
		if (!g_main_message_handler->PostMessage(MSG_SPELLCHECK_PROGRESS, (MH_PARAM_1)this, 0))
		{
			m_state = IDLE;
			return OpStatus::ERR;
		}
	}
	return OpStatus::OK;
}

OP_STATUS OpSpellCheckerSession::AddWord(const uni_char* new_word)
{
	if (m_state == LOADING_DICTIONARY || m_state == HAS_FAILED)
	{
		OP_ASSERT(!"This function should not be called now!");
		return OpStatus::ERR;
	}
	if (m_ignore_words && m_ignore_words->HasString(new_word))
		return OpStatus::OK;
	RETURN_IF_ERROR(m_language->GetSpellChecker()->AddWord(new_word, TRUE));
	return OpStatus::OK;
}

OP_STATUS OpSpellCheckerSession::IgnoreWord(const uni_char* new_word)
{
	if (m_state == LOADING_DICTIONARY || m_state == HAS_FAILED)
	{
		OP_ASSERT(!"This function should not be called now!");
		return OpStatus::ERR;
	}
	if (!m_ignore_words)
	{
		m_ignore_words = OP_NEW(SortedStringCollection, ());
		if (!m_ignore_words)
			return OpStatus::ERR_NO_MEMORY;
		m_ignore_words->IncRef();
	}
	if (!m_ignore_words->HasString(new_word))
		RETURN_IF_ERROR(m_ignore_words->AddString(new_word));
	return OpStatus::OK;
}

OP_STATUS OpSpellCheckerSession::DeleteWord(const uni_char* new_word)
{
	if (m_state == LOADING_DICTIONARY || m_state == HAS_FAILED)
	{
		OP_ASSERT(!"This function should not be called now!");
		return OpStatus::ERR;
	}
	if (m_ignore_words && m_ignore_words->HasString(new_word))
	{
		m_ignore_words->RemoveString(new_word);
		return OpStatus::OK;
	}
	RETURN_IF_ERROR(m_language->GetSpellChecker()->RemoveWord(new_word, TRUE));
	return OpStatus::OK;
}

void OpSpellCheckerSession::StopCurrentSpellChecking()
{
	if (m_state != SPELL_CHECKING)
		return;
	g_main_message_handler->RemoveDelayedMessage(MSG_SPELLCHECK_PROGRESS, (MH_PARAM_1)this, 0);
	if (g_internal_spellcheck->GetSessionInProgress() == this)
		m_stopped = TRUE; // use this instead of setting m_state = IDLE, for preventing CheckText() to "succeed"
	else
		m_state = IDLE;
}

BOOL OpSpellCheckerSession::HandleAbortInProgress()
{
	if (g_internal_spellcheck->GetSessionInProgressDeleted()) // the listener deleted us...
		return TRUE;
	if (m_stopped) // StopCurrentSpellChecking was called
	{
		m_state = IDLE;
		m_stopped = FALSE;
		m_lookup_state.Reset();
		return TRUE;
	}
	return FALSE;
}

OP_STATUS InformListenerOfLookupResult(OpSpellCheckerSession *session, OpSpellCheckerLookupState *lookup_state, OpSpellCheckerListener *listener)
{
	if (!lookup_state || !listener)
		return OpStatus::OK;
	OP_ASSERT(lookup_state->correct != MAYBE && !lookup_state->in_progress);
	if (lookup_state->correct == NO) // misspelling found
	{
		const uni_char *null_replacements[] = { NULL };
		const uni_char** replacements;
		if (lookup_state->replacements.GetSize())
		{
			if (lookup_state->replacements.AddElement(NULL) < 0)
				return OpStatus::ERR_NO_MEMORY;
			replacements = const_cast<const uni_char**>(lookup_state->replacements.GetElementPtr(0));
		}
		else
			replacements = null_replacements;
		listener->OnMisspellingFound(session, lookup_state->word_iterator, replacements);
	}
	else
		listener->OnCorrectSpellingFound(session, lookup_state->word_iterator);
	return OpStatus::OK;
}

void OpSpellCheckerSession::Progress(double time_out, BOOL &progress_continue)
{
	progress_continue = FALSE;
	if (m_state == LOADING_DICTIONARY)
	{
		if (m_language->HasDictionary())
		{
			m_state = IDLE;
			m_listener->OnSessionReady(this);
		}
		else // will set m_state == IDLE if loading is finished
			m_language->LoadDictionary(time_out, progress_continue);
		return;
	}
	g_internal_spellcheck->SetSessionInProgress(this);
	OpSpellChecker *spell_checker = m_language->GetSpellChecker();
	OP_ASSERT(spell_checker && m_state == SPELL_CHECKING && m_lookup_state.word_iterator && m_lookup_state.correct == MAYBE);
	do
	{
		OP_STATUS status = OpStatus::OK;
		if (m_ignore_words && m_ignore_words->HasString(m_lookup_state.word_iterator->GetCurrentWord()))
			m_lookup_state.correct = YES;
		else
		{
			if (!m_lookup_state.in_progress)
			{
				if (m_find_replacements && m_replacement_search_max_ms)
					m_replacement_timeout = g_op_time_info->GetWallClockMS() + (double)m_replacement_search_max_ms;
				else
					m_replacement_timeout = 0.0;
			}
			status = spell_checker->CheckSpelling(&m_lookup_state, time_out, m_find_replacements, m_replacement_timeout);
		}
		if (OpStatus::IsError(status))
		{
			m_lookup_state.Reset();
			SetState(HAS_FAILED);
			m_listener->OnError(this, status, NULL);
			progress_continue = FALSE;
			break;
		}
		else if (m_lookup_state.in_progress) // time-out
		{		
			progress_continue = TRUE;
			break;
		}
		else
		{
			status = InformListenerOfLookupResult(this, &m_lookup_state, m_listener);
			if (OpStatus::IsError(status) || HandleAbortInProgress())
			{
				progress_continue = FALSE;
				break;
			}
			BOOL continue_iterate = m_lookup_state.word_iterator->ContinueIteration();
			if (HandleAbortInProgress())
			{
				progress_continue = FALSE;
				break;
			}
			m_lookup_state.Reset();
			progress_continue = continue_iterate;
			if (!continue_iterate)
			{
				m_state = IDLE;
				m_listener->OnCheckingComplete(this);
				break;
			}
		}
	} while (time_out == 0.0  || g_op_time_info->GetWallClockMS() < time_out);
	if (progress_continue)
		m_listener->OnCheckingTakesABreak(this);
	g_internal_spellcheck->SetSessionInProgress(NULL);
	g_internal_spellcheck->SetSessionInProgressDeleted(FALSE);
}

/* ===============================OpSpellCheckerLanguage=============================== */

OpSpellCheckerLanguage::OpSpellCheckerLanguage()
{
	m_has_failed = FALSE;
	m_affix_path = NULL;
	m_dic_path = NULL;
	m_own_path = NULL;
	m_metadata_path = NULL;
	m_license_path = NULL;
	m_language = NULL;
	m_language_name = NULL;
	m_version = -2;
	m_spell_checker = NULL;
	m_index = -1;
	m_available = FALSE;
}

OpSpellCheckerLanguage::~OpSpellCheckerLanguage()
{
	g_internal_spellcheck->OnLanguageDeleted(this);
	OP_DELETE(m_spell_checker);
	OP_DELETEA(m_affix_path);
	OP_DELETEA(m_dic_path);
	OP_DELETEA(m_own_path);
	OP_DELETEA(m_metadata_path);
	OP_DELETEA(m_language);
	OP_DELETEA(m_language_name);
	OP_DELETEA(m_license_path);
}

void OpSpellCheckerLanguage::LoadDictionary(double time_out, BOOL &progress_continue)
{
	if (HasDictionary() || !HasValidPaths())
	{
		OP_ASSERT(FALSE);
		return;
	}
	OP_STATUS status = OpStatus::OK;
	uni_char *error_str = NULL;
	BOOL finished = FALSE;
	OpSpellCheckerSession *session;
	if (!m_spell_checker)
	{
		m_spell_checker = OP_NEW(OpSpellChecker, (this));
		if (m_spell_checker)
			status = m_spell_checker->LoadDictionary(m_dic_path, m_affix_path, m_own_path, time_out, finished, &error_str);
		else
			status = OpStatus::ERR_NO_MEMORY;
	}
	else
	{
		status = m_spell_checker->ContinueLoadingDictionary(time_out, finished, &error_str);
	}
	if (OpStatus::IsError(status))
	{
		session = (OpSpellCheckerSession*)m_sessions.First();
		while (session)
		{
			OpSpellCheckerSession *next = (OpSpellCheckerSession*)session->Suc();
			g_main_message_handler->RemoveDelayedMessage(MSG_SPELLCHECK_PROGRESS, (MH_PARAM_1)session, 0);
			session->SetState(OpSpellCheckerSession::HAS_FAILED);
			session->GetListener()->OnError(session, status, (const uni_char*)error_str);
			session = next;
		}
		OP_DELETEA(error_str);
		finished = TRUE;
		g_internal_spellcheck->OnLanguageLoadingFinished(this, FALSE);
	}
	else if (finished)
	{
		session = (OpSpellCheckerSession*)m_sessions.First();
		while (session)
		{
			OpSpellCheckerSession *next = (OpSpellCheckerSession*)session->Suc();
			g_main_message_handler->RemoveDelayedMessage(MSG_SPELLCHECK_PROGRESS, (MH_PARAM_1)session, 0);
			session->SetState(OpSpellCheckerSession::IDLE);
			session->GetListener()->OnSessionReady(session);
			session = next;
		}
		g_internal_spellcheck->OnLanguageLoadingFinished(this, TRUE);
	}
	progress_continue = !finished;
}


void OpSpellCheckerLanguage::SetAvailable(BOOL available)
{
	m_available = available;
}

BOOL OpSpellCheckerLanguage::IsAvailable()
{
	return m_available;
}


static BOOL uni_strnulleq(const uni_char* str1, const uni_char* str2)
{
	if ( str1 == str2 )
		return TRUE;
	if ( str1 && str2 )
		return uni_strcmp(str1, str2) == 0;
	return FALSE;
}

BOOL OpSpellCheckerLanguage::SamePaths(const uni_char *dictionary_file_path, const uni_char *affix_file_path, const uni_char *own_file_path, const uni_char *metadata_file_path, const uni_char *license_file_path)
{
	return uni_strnulleq(m_dic_path, dictionary_file_path)
		&& uni_strnulleq(m_affix_path, affix_file_path)
		&& uni_strnulleq(m_own_path, own_file_path)
		&& uni_strnulleq(m_metadata_path, metadata_file_path)
		&& uni_strnulleq(m_license_path, license_file_path);
}

OP_STATUS OpSpellCheckerLanguage::SetPaths(const uni_char *dictionary_file_path, const uni_char *affix_file_path, const uni_char *own_file_path, const uni_char *metadata_file_path, const uni_char *license_file_path)
{
	RETURN_IF_ERROR(UniSetStr(m_dic_path, dictionary_file_path));
	RETURN_IF_ERROR(UniSetStr(m_affix_path, affix_file_path));
	RETURN_IF_ERROR(UniSetStr(m_own_path, own_file_path));
	RETURN_IF_ERROR(UniSetStr(m_metadata_path, metadata_file_path));
	RETURN_IF_ERROR(UniSetStr(m_license_path, license_file_path));
	return OpStatus::OK;
}

OP_STATUS OpSpellCheckerLanguage::SetOwnFilePath(const uni_char *own_file_path)
{
	RETURN_IF_ERROR(UniSetStr(m_own_path, own_file_path));
	return OpStatus::OK;
}

OP_STATUS OpSpellCheckerLanguage::SetLanguage(const uni_char *language)
{
	return UniSetStr(m_language, language);
}

void OpSpellCheckerLanguage::DeleteSpellChecker()
{
	if (m_sessions.Empty())
	{
		OP_DELETE(m_spell_checker);
		m_spell_checker = NULL;
	}
	else
		OP_ASSERT(!"Can't delete spellchecker when there are sessions!");
}

void OpSpellCheckerLanguage::RemoveSession(OpSpellCheckerSession *session)
{
	session->Out();
	if (m_sessions.Empty())
	{
		if (!HasValidPaths())
			OP_DELETE(this);
		else
		{ 
			if (!HasDictionary())
			{
				OP_DELETE(m_spell_checker);
				m_spell_checker = NULL;
			}
			else
				g_internal_spellcheck->OnLanguageHasNoSessions(this);
		}
	}
}

void OpSpellCheckerLanguage::AddSession(OpSpellCheckerSession *session)
{
	session->Out();
	session->Into(&m_sessions);
}

OP_STATUS OpSpellCheckerLanguage::SetLastUsedInPrefs()
{
	OP_STATUS status = OpStatus::OK;
	OpString lang;
	lang.Set(m_language);
	TRAP(status, g_pccore->WriteStringL(PrefsCollectionCore::LastUsedSpellcheckLanguage, lang));
	return status;
}

BOOL OpSpellCheckerLanguage::IsLastUsedInPrefs()
{
	const OpStringC string = g_pccore->GetStringPref(PrefsCollectionCore::LastUsedSpellcheckLanguage);
	return !string.IsEmpty() && !uni_strcmp(string.CStr(), m_language);
}

#ifdef WIC_SPELL_DICTIONARY_FULL_NAME
OP_STATUS OpSpellCheckerLanguage::SetLanguageName()
{
    OP_STATUS status;
 
    if (!m_language_name)
    {
	    if (!m_metadata_path)
		{
		    status = UniSetStr(m_language_name, m_language);
			if (OpStatus::IsError(status))
			    m_language_name = NULL;
				
			return status;
		}

		OpFile file;	   

		status = file.Construct(m_metadata_path);
		if ( OpStatus::IsError(status) )
			return status;

		PrefsFile prefs(PREFS_INI);
		TRAP(status, prefs.ConstructL());
		if ( OpStatus::IsError(status) )
			return status;
		
		TRAP(status, prefs.SetFileL(&file));
		if ( OpStatus::IsError(status) )
			return status;

		OpStringC tmp;
		TRAP(status, tmp = prefs.ReadStringL("Dictionary", "Name", NULL));
		if (OpStatus::IsError(status) || tmp.IsEmpty())
		{
		    status = UniSetStr(m_language_name, m_language);
		    
		    if (OpStatus::IsError(status))
				m_language_name = NULL;
				
			return status;
		}
		else
		{
		    m_language_name = OP_NEWA(uni_char, tmp.Length() + 1);
		    uni_strcpy(m_language_name, tmp.CStr());
	    }
		
	}

	return OpStatus::OK;
}
#endif // WIC_SPELL_DICTIONARY_FULL_NAME

int OpSpellCheckerLanguage::GetVersion()
{
	if ( m_version < -1 )
	{	
		if ( !m_metadata_path )
		{
			m_version = 0;
			return 0;
		}

		OP_STATUS status;
		OpFile file;
	   
		m_version = -1;

		status = file.Construct(m_metadata_path);
		if ( OpStatus::IsError(status) )
			return -1;

		PrefsFile prefs(PREFS_INI);
		TRAP(status, prefs.ConstructL());
		if ( OpStatus::IsError(status) )
			return -1;
		
		TRAP(status, prefs.SetFileL(&file));
		if ( OpStatus::IsError(status) )
			return -1;

		TRAP(status, m_version = prefs.ReadIntL("Dictionary", "Version", -1));		
		if ( m_version < -1 )
			m_version = -1;
	}
	return m_version;
}

OP_STATUS OpSpellCheckerLanguage::GetLicense(OpString& license)
{
	if ( !m_license_path )
		return OpStatus::ERR_FILE_NOT_FOUND;

	OP_STATUS status;
	OpString8 temp_text;
	OpFile file;
	OpFileLength file_len;
	OpFileLength bytes_read;
	BOOL exists = FALSE;

	// Make the file object
	RETURN_IF_ERROR(file.Construct(m_license_path));

	// Check if file exists
	RETURN_IF_ERROR(file.Exists(exists) && exists);

	// Get the file length, so we can reserve enough space.
	RETURN_IF_ERROR(file.GetFileLength(file_len));

	// Reserve enough memory to hold the whole text file
	OP_ASSERT(file_len < static_cast<OpFileLength>(INT_MAX - 1));
	if (!temp_text.Reserve(static_cast<int>(file_len + 1)))
		return OpStatus::ERR_NO_MEMORY;

	// Open the file for reading.
	RETURN_IF_ERROR(file.Open(OPFILE_READ));

	// Read the whole file
	if (OpStatus::IsSuccess(file.Read(temp_text.DataPtr(), file_len, &bytes_read)))
	{
		// Null terminate the string and copy the text as unicode to the input string
		temp_text.CStr()[bytes_read] = '\0';
		status = license.SetFromUTF8(temp_text.CStr());
	}
	else
		status = OpStatus::ERR;

	// Make sure to close the file
	OpStatus::Ignore(file.Close());

	return status;
}

/* ===============================OpSpellCheckerWordIterator=============================== */

OpSpellCheckerWordIterator::~OpSpellCheckerWordIterator()
{
	if (g_internal_spellcheck)
		g_internal_spellcheck->OnWordIteratorDeleted(this);
}

OpSpellCheckerListener::~OpSpellCheckerListener()
{
	if (g_internal_spellcheck)
		g_internal_spellcheck->OnSpellCheckerListenerDeleted(this);
}


#endif // INTERNAL_SPELLCHECK_SUPPORT
