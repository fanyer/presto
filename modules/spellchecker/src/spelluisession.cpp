/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef INTERNAL_SPELLCHECK_SUPPORT

#include "modules/spellchecker/src/opspellchecker.h"
#include "modules/spellchecker/opspellcheckerapi.h"

//===================================OpSpellUiSession===================================

/** static */
OP_STATUS OpSpellUiSession::Create(int spell_session_id, OpSpellUiSession **session)
{
	if (!session)
	{
		OP_ASSERT(FALSE);
		return OpStatus::ERR;
	}
	*session = OP_NEW(OpSpellUiSessionImpl, (spell_session_id));
	return *session ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

//===================================OpSpellUiSessionImpl===================================

BOOL OpSpellUiSessionImpl::IsValid()
{
	return m_id && g_spell_ui_data->GetCurrentID() == m_id;
}

int OpSpellUiSessionImpl::GetSelectedLanguageIndex()
{
	if (IsValid() && g_spell_ui_data->GetSpellUIHandler()->GetSpellCheckerSession())
		return g_spell_ui_data->GetSpellUIHandler()->GetSpellCheckerSession()->GetLanguage()->GetIndex();
	return -1;
}

const uni_char* OpSpellUiSessionImpl::GetInstalledLanguageStringAt(int index)
{
	OpSpellCheckerLanguage *language = g_internal_spellcheck->GetInstalledLanguageAt(index);
	return language ? language->GetLanguageString() : NULL;
}

#ifdef WIC_SPELL_DICTIONARY_FULL_NAME
const uni_char* OpSpellUiSessionImpl::GetInstalledLanguageNameAt(int index)
{
	OpSpellCheckerLanguage *language = g_internal_spellcheck->GetInstalledLanguageAt(index);
	return language ? language->GetLanguageName() : NULL;
}
#endif // WIC_SPELL_DICTIONARY_FULL_NAME

int OpSpellUiSessionImpl::GetInstalledLanguageVersionAt(int index)
{
	OpSpellCheckerLanguage *language = g_internal_spellcheck->GetInstalledLanguageAt(index);
	return language ? language->GetVersion() : -1;
}

BOOL OpSpellUiSessionImpl::SetLanguage(const uni_char *lang)
{
	if (!IsValid())
		return FALSE;

	OpString preferred_lang;
	SpellUIHandler *handler = g_spell_ui_data->GetSpellUIHandler();
	if (lang && !uni_strcmp(lang, UNI_L("default")))
	{
		BOOL found = FALSE;
		if (handler->GetPreferredLanguage(preferred_lang))
		{
			if (g_internal_spellcheck->GetOpSpellCheckerLanguage(preferred_lang.CStr()))
			{
				lang = preferred_lang.CStr();
				found = TRUE;
			}
		}

		if (!found && g_internal_spellcheck->GetDefaultLanguage())
		{
			lang = g_internal_spellcheck->GetDefaultLanguage();
			found = TRUE;
		}
		if (!found)
			return FALSE;
	}

	if (!lang || !*lang)
	{
		if (handler->GetSpellCheckerSession())
		{
			handler->DisableSpellcheck();
			return TRUE;
		}
		return FALSE;
	}
	if (handler->GetSpellCheckerSession())
	{
		if (!uni_strcmp(handler->GetSpellCheckerSession()->GetLanguageString(), lang))
			return TRUE; // we already use this lanugage...
		SetLanguage(NULL);
	}
	g_spell_ui_data->InvalidateData();
	return handler->SetSpellCheckLanguage(lang);
}

OP_STATUS OpSpellUiSessionImpl::GetReplacementString(int replace_index, OpString &value)
{
	if (IsValid())
	{
		const uni_char *str = g_spell_ui_data->GetReplacement(replace_index);
		if (str)
		{
			RETURN_IF_ERROR(value.Set(str));
			return OpStatus::OK;
		}
	}
	value.Set(UNI_L(""));
	return OpStatus::OK;
}

int OpSpellUiSessionImpl::GetReplacementStringCount()
{
	return IsValid() ? g_spell_ui_data->GetReplacementCount() : 0;
}
	
BOOL OpSpellUiSessionImpl::CanEnableSpellcheck()
{
	return IsValid() && 
		!g_spell_ui_data->GetSpellUIHandler()->GetSpellCheckerSession() &&
		g_internal_spellcheck->HasInstalledLanguages();
}

BOOL OpSpellUiSessionImpl::CanDisableSpellcheck()
{
	return IsValid() && g_spell_ui_data->GetSpellUIHandler()->GetSpellCheckerSession();
}

BOOL OpSpellUiSessionImpl::EnableSpellcheck()
{
	return CanEnableSpellcheck() && SetLanguage(UNI_L("default"));
}

BOOL OpSpellUiSessionImpl::DisableSpellcheck()
{
	return SetLanguage(UNI_L(""));
}

BOOL OpSpellUiSessionImpl::CanAddWord()
{
	if (!IsValid() || !g_spell_ui_data->HasWord() || g_spell_ui_data->IsCorrect())
		return FALSE;
	OpSpellCheckerSession *session = g_spell_ui_data->GetSpellUIHandler() ? g_spell_ui_data->GetSpellUIHandler()->GetSpellCheckerSession() : NULL;
	return session && session->CanAddWord(g_spell_ui_data->GetWord());
}

BOOL OpSpellUiSessionImpl::CanIgnoreWord()
{
	return IsValid() && g_spell_ui_data->HasWord() && !g_spell_ui_data->IsCorrect();
}

BOOL OpSpellUiSessionImpl::CanRemoveWord()
{
	return IsValid() && g_spell_ui_data->HasWord() && g_spell_ui_data->IsCorrect();
}
	
OP_STATUS OpSpellUiSessionImpl::UpdateContent(SpellContentUpdateType type)
{
	if (type == SPC_UPDATE_ADD && !CanAddWord() || type == SPC_UPDATE_IGNORE && !CanIgnoreWord() || type == SPC_UPDATE_REMOVE && !CanRemoveWord())
		return OpStatus::OK;

	OpSpellCheckerWordIterator *word = g_spell_ui_data->GetWordIterator();
	SpellUIHandler *handler = g_spell_ui_data->GetSpellUIHandler();
	g_spell_ui_data->InvalidateData();
	
	OpSpellCheckerSession *session = handler->GetSpellCheckerSession();
	const uni_char *str = word->GetCurrentWord();
	if (type == SPC_UPDATE_ADD)
		RETURN_IF_ERROR(session->AddWord(str));
	else if (type == SPC_UPDATE_IGNORE)
		RETURN_IF_ERROR(session->IgnoreWord(str));
	else // SPC_UPDATE_REMOVE
		RETURN_IF_ERROR(session->DeleteWord(str));

	RETURN_IF_ERROR(g_internal_spellcheck->StartBackgroundUpdate(handler, str, type != SPC_UPDATE_REMOVE));
	if (type == SPC_UPDATE_REMOVE)
		handler->OnMisspellingFound(NULL, word, NULL);
	else
		handler->OnCorrectSpellingFound(NULL, word);

	return OpStatus::OK;
}

OP_STATUS OpSpellUiSessionImpl::ReplaceWord(int replace_index)
{
	if (!IsValid())
		return OpStatus::OK;
	BOOL has_word = g_spell_ui_data->HasWord();
	const uni_char *str = g_spell_ui_data->GetReplacement(replace_index);
	OpSpellCheckerWordIterator *word = g_spell_ui_data->GetWordIterator();
	SpellUIHandler *handler = g_spell_ui_data->GetSpellUIHandler();
	OP_STATUS status = str && *str && has_word ? handler->ReplaceWord(word, str) : OpStatus::OK;
	g_spell_ui_data->InvalidateData();
	return status;
}

BOOL OpSpellUiSessionImpl::HasWord()
{
	return IsValid() && g_spell_ui_data->HasWord();
}

OP_STATUS OpSpellUiSessionImpl::GetCurrentWord(OpString &word)
{
	return word.Set(HasWord() ? g_spell_ui_data->GetWord() : UNI_L(""));
}

OP_STATUS OpSpellUiSessionImpl::GetInstalledLanguageLicenceAt(int index, OpString& license)
{
	OpSpellCheckerLanguage *language = g_internal_spellcheck->GetInstalledLanguageAt(index);
	return language->GetLicense(license);
}

//===================================SpellUIData===================================

void SpellUIData::InvalidateData()
{
	m_current_id++;
	m_handler = NULL;
	m_word_iterator = NULL;
	m_is_correct = FALSE;
	m_is_valid = FALSE;
	m_replacement_count = 0;
}

int SpellUIData::CreateIdFor(SpellUIHandler *handler)
{
	InvalidateData();
	m_handler = handler;
	m_is_valid = !!handler;
	return m_current_id;
}

void SpellUIData::OnMisspellingFound(OpSpellCheckerSession *session, OpSpellCheckerWordIterator *word, const uni_char **replacements)
{
	m_is_valid = TRUE;
	m_word_iterator = word;
	m_is_correct = FALSE;
	int buf_ofs = 0;
	int i;
	for (i = 0; i < SPC_MAX_REPLACEMENTS && replacements[i]; i++)
	{
		int len = uni_strlen(replacements[i]) + 1;
		if (buf_ofs + len > REPLACEMENT_CHAR_BUF_SIZE)
			break;
		m_replacements[i] = m_replacement_char_buf + buf_ofs;
		uni_strcpy(m_replacements[i], replacements[i]);
		buf_ofs += len;
	}
	m_replacement_count = i;
}

void SpellUIData::OnCorrectSpellingFound(OpSpellCheckerSession *session, OpSpellCheckerWordIterator *word)
{
	m_is_valid = TRUE;
	m_word_iterator = word;
	m_is_correct = TRUE;
	m_replacement_count = 0;
}


#endif // INTERNAL_SPELLCHECK_SUPPORT
