/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/spellcheck/SpellCheckContext.h"

#if defined(AUTO_UPDATE_SUPPORT) && defined(INTERNAL_SPELLCHECK_SUPPORT)
#include "adjunct/quick/dialogs/InstallLanguageDialog.h"
#endif

#ifdef INTERNAL_SPELLCHECK_SUPPORT
#include "modules/spellchecker/opspellcheckerapi.h"
#endif // INTERNAL_SPELLCHECK_SUPPORT

SpellCheckContext::SpellCheckContext()
#ifdef INTERNAL_SPELLCHECK_SUPPORT
: m_spell_session_id(0)
#endif
{
}

SpellCheckContext::~SpellCheckContext()
{
}

BOOL SpellCheckContext::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
	case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
#ifdef INTERNAL_SPELLCHECK_SUPPORT
			case OpInputAction:: ACTION_INSTALL_SPELLCHECK_LANGUAGE:
				{
					child_action->SetEnabled(TRUE);
					return TRUE;
				}
			case OpInputAction::ACTION_INTERNAL_SPELLCHECK_ENABLE:
				{
					OpSpellUiSessionImpl session(m_spell_session_id);
					BOOL is_enabled = g_internal_spellcheck->SpellcheckEnabledByDefault();
  					if (session.CanEnableSpellcheck())
						is_enabled=FALSE;
					else if (session.CanDisableSpellcheck())
						is_enabled=TRUE;

					child_action->SetEnabled(TRUE);
					child_action->SetSelectedByToggleAction(OpInputAction::ACTION_INTERNAL_SPELLCHECK_ENABLE, is_enabled);
					return TRUE;
				}
			case OpInputAction::ACTION_INTERNAL_SPELLCHECK_ADD_WORD:
				{
					OpSpellUiSessionImpl session(m_spell_session_id);
					child_action->SetEnabled(session.CanAddWord());
					return TRUE;
				}
			case OpInputAction::ACTION_INTERNAL_SPELLCHECK_IGNORE_WORD:
				{
					OpSpellUiSessionImpl session(m_spell_session_id);
					child_action->SetEnabled(session.CanIgnoreWord());
					return TRUE;
				}
			case OpInputAction::ACTION_INTERNAL_SPELLCHECK_REMOVE_WORD:
				{
					OpSpellUiSessionImpl session(m_spell_session_id);
					child_action->SetEnabled(session.CanRemoveWord());
					return TRUE;
				}
			case OpInputAction::ACTION_INTERNAL_SPELLCHECK_LANGUAGE:
				{
					OpSpellUiSessionImpl session(m_spell_session_id);
					child_action->SetEnabled(TRUE);
					child_action->SetSelected((int)child_action->GetActionData() == session.GetSelectedLanguageIndex());
					return TRUE;
				}
			case OpInputAction::ACTION_INTERNAL_SPELLCHECK_REPLACEMENT:
				{
					child_action->SetEnabled(TRUE);
					return TRUE;
				}
#endif // INTERNAL_SPELLCHECK_SUPPORT
#ifdef SPELLCHECK_SUPPORT
			case OpInputAction::ACTION_SPELL_CHECK:
				{
					child_action->SetEnabled(FALSE);
					return TRUE;
				}
#endif

			}
			break;
		}
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	case OpInputAction::ACTION_INSTALL_SPELLCHECK_LANGUAGE:
		{
#ifdef AUTO_UPDATE_SUPPORT
			InstallLanguageDialog *dialog = OP_NEW(InstallLanguageDialog, ());
			if (dialog)
				dialog->Init(g_application->GetActiveDesktopWindow());
#endif // AUTO_UPDATE_SUPPORT
			return TRUE;
		}
	case OpInputAction::ACTION_INTERNAL_SPELLCHECK_ENABLE:
		{
			OpSpellUiSessionImpl session(m_spell_session_id);
			if (session.CanDisableSpellcheck())
			{
				session.DisableSpellcheck();
				g_internal_spellcheck->FreeCachedData(TRUE);
			}
			else if (session.CanEnableSpellcheck())
				session.EnableSpellcheck();
			return TRUE;
		}
	case OpInputAction::ACTION_INTERNAL_SPELLCHECK_ADD_WORD:
		{
			OpSpellUiSessionImpl session(m_spell_session_id);
			session.AddWord();
			return TRUE;
		}
	case OpInputAction::ACTION_INTERNAL_SPELLCHECK_IGNORE_WORD:
		{
			OpSpellUiSessionImpl session(m_spell_session_id);
			session.IgnoreWord();
			return TRUE;
		}
	case OpInputAction::ACTION_INTERNAL_SPELLCHECK_REMOVE_WORD:
		{
			OpSpellUiSessionImpl session(m_spell_session_id);
			session.RemoveWord();
			return TRUE;
		}
	case OpInputAction::ACTION_INTERNAL_SPELLCHECK_LANGUAGE:
		{
			OpSpellUiSessionImpl session(m_spell_session_id);
			session.SetLanguage(session.GetInstalledLanguageStringAt((int)action->GetActionData()));
			g_internal_spellcheck->SetDefaultLanguage(session.GetInstalledLanguageStringAt((int)action->GetActionData()));
			return TRUE;
		}
	case OpInputAction::ACTION_INTERNAL_SPELLCHECK_REPLACEMENT:
		{
			OpSpellUiSessionImpl session(m_spell_session_id);
			session.ReplaceWord((int)action->GetActionData());
			return TRUE;
		}
#endif // INTERNAL_SPELLCHECK_SUPPORT
	}
	return FALSE;
}


#ifdef INTERNAL_SPELLCHECK_SUPPORT
void SpellCheckContext::SetSpellSessionID(int id)
{
	m_spell_session_id = id;
#ifdef FIND_BEST_DICTIONARY_MATCH
	DictionaryHandler::ShowInstallDialogIfNeeded(m_spell_session_id);
#endif // FIND_BEST_DICTIONARY_MATCH
}
#endif // INTERNAL_SPELLCHECK_SUPPORT
