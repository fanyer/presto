/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/widgets/OpEditCommon.h"
#include "modules/widgets/OpEdit.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/widgets/OpColorField.h"
#include "modules/widgets/OpWidgetStringDrawer.h"

#include "modules/pi/OpClipboard.h"
#include "modules/pi/OpWindow.h"

#include "modules/inputmanager/inputaction.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/unicode/unicode_stringiterator.h"

# ifdef UNIX
#  include "modules/prefs/prefsmanager/collections/pc_unix.h"
# endif
#include "modules/doc/html_doc.h"
#include "modules/doc/frm_doc.h"
#include "modules/forms/piforms.h"

#ifdef SUPPORT_IME_PASSWORD_INPUT
# include "modules/prefs/prefsmanager/collections/pc_display.h"
#endif // SUPPORT_IME_PASSWORD_INPUT

#ifdef QUICK
# include "adjunct/quick/Application.h"
# include "adjunct/quick/dialogs/SimpleDialog.h"
# include "adjunct/quick/data_types/OpenURLSetting.h"
# include "adjunct/quick/panels/NotesPanel.h"
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
# include "adjunct/desktop_util/search/searchenginemanager.h"
#endif // DESKTOP_UTIL_SEARCH_ENGINES
#endif

#include "modules/dochand/docman.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/display/vis_dev.h"
#include "modules/util/str.h"

#ifdef GRAB_AND_SCROLL
# include "modules/display/VisDevListeners.h"
#endif

#ifdef DRAG_SUPPORT
# include "modules/dragdrop/dragdrop_manager.h"
# include "modules/pi/OpDragObject.h"
# include "modules/dragdrop/dragdrop_data_utils.h"
#endif // DRAG_SUPPORT

#ifdef INTERNAL_SPELLCHECK_SUPPORT

#include "modules/spellchecker/opspellcheckerapi.h"
#include "modules/widgets/optextfragment.h"


class OpEditSpellchecker;

class OpEditWordIterator : public OpSpellCheckerWordIterator
{
public:
	/** used_by_session indicates that Reset() should inform edit_sc's spellcheck session to stop the current spell-checking. */
	OpEditWordIterator(OpEditSpellchecker *edit_sc, BOOL used_by_session);
	virtual ~OpEditWordIterator() {}

	/** Reset to empty, and if m_used_by_session == TRUE, make m_edit_sc's spellcheck session stop current spellchecking */
	void Reset();

	/** Start iteration, GetCurrentWord() will now return first word in the OpEdit */
	BOOL Start();

	/** Sets this iterator to only "iterate" through the word at x-position ofs */
	void SetAtWord(int ofs);

	/** Returns whether this iterator is "active" - GetCurrentWord() will return a word */
	BOOL Active() { return m_active; }

	/** Start offset of current word */
	int CurrentStart() { return m_current_start; }

	/** Stop offset of current word */
	int CurrentStop() { return m_current_stop; }

	// <<<OpSpellCheckerWordIterator>>>
	virtual const uni_char *GetCurrentWord() { return m_current_string.IsEmpty() ? UNI_L("") : m_current_string.CStr(); }
	virtual BOOL ContinueIteration();

private:
	OpEditSpellchecker *m_edit_sc;
	BOOL m_active;
	BOOL m_single_word;
	BOOL m_used_by_session;

	int m_current_start, m_current_stop;
	OpString m_current_string;
};

class OpEditSpellchecker : public SpellUIHandler
{
public:
	OpEditSpellchecker(OpEdit *edit);
	virtual ~OpEditSpellchecker() {	DisableSpellcheckInternal(FALSE /*by_user*/, TRUE /*force*/); }


	/*
	 * Functions from OpSpellCheckerListener (inherited by SpellUIHandler)
	 */
	virtual void OnSessionReady(OpSpellCheckerSession *session) { SpellcheckUpdate(); }
	virtual void OnError(OpSpellCheckerSession *session, OP_STATUS error_status, const uni_char *error_string) { DisableSpellcheckInternal(FALSE /*by_user*/, TRUE /*force*/); }
	virtual void OnMisspellingFound(OpSpellCheckerSession *session, OpSpellCheckerWordIterator *word, const uni_char **replacements);
	virtual void OnCorrectSpellingFound(OpSpellCheckerSession *session, OpSpellCheckerWordIterator *word);
	virtual void OnCheckingComplete(OpSpellCheckerSession *session) {}

	/*
	 * Functions from SpellUIHandler
	 */
	virtual OpSpellCheckerSession* GetSpellCheckerSession() { return m_spell_session; }
	/* Called when user enables spellchecking (through the OpSpellUiSession) - Never call this function directly! */
	virtual BOOL SetSpellCheckLanguage(const uni_char *lang);
	virtual void DisableSpellcheck();
	virtual OpSpellCheckerWordIterator* GetAllContentIterator();
	virtual OP_STATUS ReplaceWord(OpSpellCheckerWordIterator *word_iterator, const uni_char *str);


	/** Enable the spellchecker */
	OP_STATUS EnableSpellcheckInternal(BOOL by_user, const uni_char *lang);
	/** Disables the spellchecker */
	void DisableSpellcheckInternal(BOOL by_user, BOOL force);

	/** Marks textfragment between start_ofs and stop_ofs as misspelled (if misspelled==TRUE) or correct (if misspelled==FALSE) */
	void MarkMisspelling(BOOL misspelled, int start_ofs, int stop_ofs);

	/** Creates a SpellUISession and records the word located at point, so this word could later be e.g. replaced with a correct word (if it was misspelled) */
	OP_STATUS CreateSpellUISession(const OpPoint &point, int &spell_session_id);
	OpEdit *GetOpEdit() { return m_edit; }

	/** Causes a new spellchecking of the entire content of the OpEdit (using messages, won't "lock" Opera) */
	void SpellcheckUpdate();

	BOOL ByUser() { return m_by_user; }

private:
	OP_STATUS CreateSpellUISessionInternal(const OpPoint &point, int &spell_session_id);

private:
	OpEdit *m_edit;
	OpSpellCheckerSession *m_spell_session;

	OpEditWordIterator m_word_iterator; ///< Used by spellchecker to peform inline spellchecking
	OpEditWordIterator m_replace_word; ///< Used to record the word which should possibly be replaced
	OpEditWordIterator m_background_updater; ///< Used for updating correct/misspelled words after words have been added/removed from dictionary

	BOOL m_by_user; ///< The spellchecker has been put in its current state by the user. Once the user has changed the state we only change it at the users request.
};

// == OpEditWordIterator ===========================================================

OpEditWordIterator::OpEditWordIterator(OpEditSpellchecker *edit_sc, BOOL used_by_session)
{
	m_edit_sc = edit_sc;
	m_used_by_session = FALSE;
	Reset();
	m_used_by_session = used_by_session;
}

void OpEditWordIterator::Reset()
{
	m_active = FALSE;
	m_current_start = 0;
	m_current_stop = 0;
	m_single_word = FALSE;
	if(m_current_string.CStr())
		m_current_string.CStr()[0] = '\0';
	if(m_used_by_session)
	{
		OpSpellCheckerSession *session = m_edit_sc->GetSpellCheckerSession();
		if(session)
			session->StopCurrentSpellChecking();
	}
}

BOOL OpEditWordIterator::Start()
{
	Reset();
	m_active = TRUE;
	return ContinueIteration();
}

void OpEditWordIterator::SetAtWord(int ofs)
{
	Reset();
	OpSpellCheckerSession *session = m_edit_sc->GetSpellCheckerSession();
	const uni_char *text = m_edit_sc->GetOpEdit()->string.Get();
	while(ofs && !session->IsWordSeparator(text[ofs-1]))
		ofs--;
	m_current_start = ofs;
	m_current_stop = ofs;
	m_active = TRUE;
	ContinueIteration();
	if(m_active)
		m_single_word = TRUE;
}

BOOL OpEditWordIterator::ContinueIteration()
{
	if(!m_active || m_single_word)
		return FALSE;
	OpSpellCheckerSession *session = m_edit_sc->GetSpellCheckerSession();
	const uni_char *text = m_edit_sc->GetOpEdit()->string.Get();
	if(!text)
		return FALSE;
	int text_len = uni_strlen(text);
	int ofs = m_current_stop;
	int first = ofs;
	while(ofs < text_len && session->IsWordSeparator(text[ofs]))
		ofs++;
	m_edit_sc->MarkMisspelling(FALSE, first, ofs);
	if(ofs >= text_len)
	{
		Reset();
		return FALSE;
	}
	m_current_start = ofs;
	while(ofs < text_len && !session->IsWordSeparator(text[ofs]))
		ofs++;
	m_current_stop = ofs;
	OP_STATUS status = m_current_string.Set(text + m_current_start, m_current_stop - m_current_start);
	if(OpStatus::IsError(status))
		Reset();
	return OpStatus::IsSuccess(status);
}

// == OpEditSpellchecker ===========================================================

OpEditSpellchecker::OpEditSpellchecker(OpEdit *edit)
		: m_edit(edit)
		, m_spell_session(NULL)
		, m_word_iterator(this, TRUE)
		, m_replace_word(this, FALSE)
		, m_background_updater(this, FALSE)
		, m_by_user(FALSE)

{
}

void OpEditSpellchecker::OnMisspellingFound(OpSpellCheckerSession *session, OpSpellCheckerWordIterator *word, const uni_char **replacements)
{
	OpEditWordIterator *w = (OpEditWordIterator*)word;
	MarkMisspelling(TRUE, w->CurrentStart(), w->CurrentStop());
}

void OpEditSpellchecker::OnCorrectSpellingFound(OpSpellCheckerSession *session, OpSpellCheckerWordIterator *word)
{
	OpEditWordIterator *w = (OpEditWordIterator*)word;
	MarkMisspelling(FALSE, w->CurrentStart(), w->CurrentStop());
}

BOOL OpEditSpellchecker::SetSpellCheckLanguage(const uni_char *lang)
{
	return OpStatus::IsSuccess(EnableSpellcheckInternal(TRUE /*by_user*/, lang));
}

/* Called when user disables spellchecking (through the OpSpellUiSession) - Never call this function directly! */
void OpEditSpellchecker::DisableSpellcheck()
{
	DisableSpellcheckInternal(TRUE /*by_user*/, TRUE /*force*/);
}

OpSpellCheckerWordIterator* OpEditSpellchecker::GetAllContentIterator()
{
	return m_background_updater.Start() ? &m_background_updater : NULL;
}

OP_STATUS OpEditSpellchecker::ReplaceWord(OpSpellCheckerWordIterator *word_iterator, const uni_char *str)
{
	OpEditWordIterator *w = (OpEditWordIterator*)word_iterator;
	m_edit->SetCaretOffset(w->CurrentStart(), FALSE);
	m_edit->SetSelection(w->CurrentStart(), w->CurrentStop());
	m_edit->InsertText(str);
	return OpStatus::OK;
}

void OpEditSpellchecker::SpellcheckUpdate()
{
	m_edit->SetDelaySpelling(FALSE);
	if(!m_spell_session || m_spell_session->GetState() == OpSpellCheckerSession::LOADING_DICTIONARY)
		return;
	m_word_iterator.Reset();
	m_background_updater.Reset();
	m_replace_word.Reset();
	m_word_iterator.Start();
	if(!m_word_iterator.Active())
		return;
	if(OpStatus::IsError(m_spell_session->CheckText(&m_word_iterator, FALSE)))
		DisableSpellcheckInternal(FALSE /*by_user*/, TRUE /* force */);
}

OP_STATUS OpEditSpellchecker::EnableSpellcheckInternal(BOOL by_user, const uni_char* lang)
{
	if (!by_user && m_by_user)
		return OpStatus::OK;

	OP_STATUS status = OpStatus::OK;
	if (!m_spell_session)
	{
		/* Creating the spellchecker session eventually leads to OnSessionReady() being called, from there everything is set up and the checking is started */
		status = g_internal_spellcheck->CreateSession(lang, this, m_spell_session, FALSE);
		if (OpStatus::IsError(status))
		{
			OP_ASSERT(m_spell_session == NULL);
			by_user = FALSE; // Remaining disabled is not what the user wanted.
		}
	}

	m_by_user = by_user;

	return status;
}

void OpEditSpellchecker::DisableSpellcheckInternal(BOOL by_user, BOOL force)
{
	if (!force && !by_user && m_by_user)
		return;

	if (m_spell_session || by_user)
		m_by_user = by_user;

	if (!m_spell_session)
		return;

	UINT32 i;
	m_word_iterator.Reset();
	m_replace_word.Reset();
	m_background_updater.Reset();
	OP_DELETE(m_spell_session);
	m_spell_session = NULL;
	OpTextFragmentList *fragments = &m_edit->string.fragments;
	for(i=0;i<fragments->GetNumFragments();i++)
		fragments->Get(i)->wi.SetIsMisspelling(FALSE);
	m_edit->InvalidateAll();
	m_edit->OnSpellcheckDisabled();
}

void OpEditSpellchecker::MarkMisspelling(BOOL misspelled, int start_ofs, int stop_ofs)
{
	UINT32 i;
	if(start_ofs >= stop_ofs)
		return;
	BOOL repaint = FALSE;
	OpTextFragmentList *fragments = &m_edit->string.fragments;
	for(i=0;i<fragments->GetNumFragments();i++)
	{
		OP_TEXT_FRAGMENT *fragment = fragments->Get(i);
		if(fragment->start >= start_ofs && fragment->start < stop_ofs)
		{
			repaint = repaint || !!(fragment->wi.IsMisspelling()) != misspelled;
			fragment->wi.SetIsMisspelling(misspelled);
		}
	}
	if(repaint)
		m_edit->InvalidateAll();
}

OP_STATUS OpEditSpellchecker::CreateSpellUISessionInternal(const OpPoint &point, int &spell_session_id)
{
	spell_session_id = 0;
	if(!m_spell_session || m_spell_session->GetState() == OpSpellCheckerSession::LOADING_DICTIONARY)
		return OpStatus::OK;

	int pos = m_edit->string.GetCaretPos(m_edit->GetTextRect(), OpPoint(point.x, point.y));
	const uni_char *str = m_edit->string.Get();
	if(!str || pos >= (int)uni_strlen(str) || m_spell_session->IsWordSeparator(str[pos]))
		return OpStatus::OK;
	m_replace_word.SetAtWord(pos);
	if(!m_replace_word.Active())
		return OpStatus::OK;

	OpSpellCheckerSession temp_session(g_internal_spellcheck, m_spell_session->GetLanguage(), g_spell_ui_data, TRUE, m_spell_session->GetIgnoreWords());
	if(temp_session.CanSpellCheck(m_replace_word.GetCurrentWord()))
	{
		spell_session_id = g_spell_ui_data->CreateIdFor(this);
		RETURN_IF_ERROR(temp_session.CheckText(&m_replace_word, TRUE));
	}
	return OpStatus::OK;
}

OP_STATUS OpEditSpellchecker::CreateSpellUISession(const OpPoint &point, int &spell_session_id)
{
	OP_STATUS status = CreateSpellUISessionInternal(point, spell_session_id);
	if(OpStatus::IsError(status))
	{
		spell_session_id = 0;
		g_spell_ui_data->InvalidateData();
		return status;
	}
	if(!spell_session_id && (g_internal_spellcheck->HasInstalledLanguages() || m_spell_session))
		spell_session_id = g_spell_ui_data->CreateIdFor(this);
	return OpStatus::OK;
}

#endif // INTERNAL_SPELLCHECK_SUPPORT

// == OpEdit ===========================================================

OpEdit::OpEdit()
	: caret_pos(0)
	, caret_blink(0)
	, is_selecting(0)
	, alt_charcode(0)
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	, m_spellchecker(NULL)
#endif // INTERNAL_SPELLCHECK_SUPPORT
	, x_scroll(0)
#ifdef WIDGETS_IME_SUPPORT
	, imstring(NULL)
#endif
#if defined(WIDGETS_IME_SUPPORT) && defined(SUPPORT_IME_PASSWORD_INPUT)
    , hidePasswordCounter(0)
#endif // defined(WIDGETS_IME_SUPPORT) && defined(SUPPORT_IME_PASSWORD_INPUT)
	, autocomp(this)
	, m_ghost_text_onfocus_justify(JUSTIFY_RIGHT)
	, m_selection_highlight_type(VD_TEXT_HIGHLIGHT_TYPE_SELECTION)
#ifdef IME_SEND_KEY_EVENTS
	, m_fake_keys(0)
#endif // IME_SEND_KEY_EVENTS
	, m_packed_init(0)
#ifdef USE_OP_CLIPBOARD
	, m_autocomplete_after_paste(TRUE)
#endif // USE_OP_CLIPBOARD
	, m_corner(NULL)
{
	m_ghost_text_fcol_when_focused = GetInfo() ? GetInfo()->GetSystemColor(OP_SYSTEM_COLOR_TEXT_DISABLED) : 0;
	m_packed.caret_snap_forward = TRUE;
	m_packed.use_default_autocompletion = TRUE;

	string.Reset(this);
	SetTabStop(TRUE);
#ifdef QUICK
	m_delayed_trigger.SetDelayedTriggerListener(this);
	SetGrowValue(1);
#endif
#ifdef SKIN_SUPPORT
	SetSkinned(TRUE);
	GetBorderSkin()->SetImage("Edit Skin");
#endif

#ifdef _UNIX_DESKTOP_
	m_packed.dblclick_selects_all = !g_pcunix->GetIntegerPref(PrefsCollectionUnix::EnableEditTripleClick);
#endif

#ifdef WIDGETS_IME_SUPPORT
	SetOnMoveWanted(TRUE);
#endif

#ifdef IME_SEND_KEY_EVENTS
	previous_ime_len = 0;
#endif // IME_SEND_KEY_EVENTS

	SetPadding(1, 0, 1, 0);
}

OP_STATUS OpEdit::Init()
{
	m_corner = OP_NEW(OpWidgetResizeCorner, (this));
	RETURN_OOM_IF_NULL(m_corner);

	AddChild(m_corner, TRUE);
	UpdateCorner();

	return OpStatus::OK;
}

OP_STATUS OpEdit::Construct(OpEdit** obj)
{
	OpAutoPtr<OpEdit> edit(OP_NEW(OpEdit, ()));
	RETURN_OOM_IF_NULL(edit.get());
	RETURN_IF_MEMORY_ERROR(edit->init_status);
	RETURN_IF_ERROR(edit->Init());

	*obj = edit.release();
	return OpStatus::OK;
}

OpEdit::~OpEdit()
{
#ifndef WIDGETS_DISABLE_CURSOR_BLINKING
	StopTimer();
#endif //!WIDGETS_DISABLE_CURSOR_BLINKING
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	OP_DELETE(m_spellchecker);
#endif // INTERNAL_SPELLCHECK_SUPPORT
}

#ifdef WIDGETS_CLONE_SUPPORT

OP_STATUS OpEdit::CreateClone(OpWidget **obj, OpWidget *parent, INT32 font_size, BOOL expanded)
{
	*obj = NULL;
	OpEdit *widget;

	RETURN_IF_ERROR(OpEdit::Construct(&widget));
	parent->AddChild(widget);

	if (OpStatus::IsError(widget->CloneProperties(this, font_size)))
	{
		widget->Remove();
		OP_DELETE(widget);
		return OpStatus::ERR;
	}
	widget->SetReadOnly(IsReadOnly());
	widget->SetMaxLength(string.GetMaxLength());
	widget->SetPasswordMode(string.GetPasswordMode());
	*obj = widget;
	return OpStatus::OK;
}

#endif // WIDGETS_CLONE_SUPPORT

void OpEdit::OnRemoving()
{
#ifdef WIDGETS_IME_SUPPORT
	if (imstring && vis_dev)
		// This should teoretically not be needed since OpWidget::OnKeyboardInputLost will do it. But will keep it to be extra safe.
		vis_dev->view->GetOpView()->AbortInputMethodComposing();
#endif // WIDGETS_IME_SUPPORT
}

void OpEdit::OnDeleted()
{
	if(autocomp.window)
	{
		// This avoids a double deletion problem that depends on deletion order in the unix ports
		// problem: Close desktop or compose window with shortcut when autocompletion is open
		autocomp.ClosePopup(TRUE);
	}
}

INT32 OpEdit::GetCaretOffset()
{
	return caret_pos;
}

void OpEdit::SetCaretOffset(INT32 pos, BOOL reset_snap_forward)
{
	if (pos < 0)
		pos = 0;

	INT32 text_len = GetTextLength();
	if (pos > text_len)
		pos = text_len;

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	// check if spellchecking has been "delayed" (for the word the user was currently writing), and the caret
	// is no longer in that word, then perform spellchecking.
	if(m_spellchecker && m_packed.delayed_spell && m_spellchecker->GetSpellCheckerSession())
	{
		int i;
		OpSpellCheckerSession *session = m_spellchecker->GetSpellCheckerSession();
		int start = MIN(caret_pos, pos);
		int stop = MAX(caret_pos, pos);
		const uni_char *text = string.Get();
		for(i = start; i < stop && !session->IsWordSeparator(text[i]); i++)
			;
		if(i != stop)
		{
			m_packed.delayed_spell = FALSE;
			m_spellchecker->SpellcheckUpdate();
		}
	}
#endif // INTERNAL_SPELLCHECK_SUPPORT

	caret_pos = pos;
	if(reset_snap_forward)
		m_packed.caret_snap_forward = TRUE;
}

INT32 OpEdit::GetTextLength()
{
	return (INT32) uni_strlen(string.Get());
}

void OpEdit::GetText(uni_char *buf, INT32 buflen, INT32 offset)
{
	const uni_char* str = string.Get();
	int copylen = uni_strlen(str) - offset;
	if (buflen < copylen)
		copylen = buflen;
	op_memcpy(buf, &str[offset], copylen * sizeof(uni_char));
	buf[copylen] = 0; // or '\0' but *not* NULL !
}

OP_STATUS OpEdit::GetText(OpString &str)
{
	return str.Set(string.Get());
}

OP_STATUS OpEdit::SetTextInternal(const uni_char* text, BOOL force_no_onchange, BOOL use_undo_stack)
{
	INT32 text_length;
	if (text == NULL)
	{
		text = UNI_L("");
		text_length = 0;
	}
	else
		text_length =  uni_strlen(text);

	OpStringC current_text = string.Get();
	if (current_text.Compare(text) == 0)
		return OpStatus::OK;

#ifdef WIDGETS_UNDO_REDO_SUPPORT
	//don't use undo bufer if user has not input anything
	if (use_undo_stack && !HasReceivedUserInput())
		use_undo_stack = FALSE;

	if (use_undo_stack && IsUndoRedoEnabled())
	{
		if(text_length > 0)
		{
			if(!current_text.IsEmpty())
			{
				INT32 ss, se;
				OpWidget::GetSelection(ss, se);
				RETURN_IF_ERROR(undo_stack.SubmitReplace(this->caret_pos, ss, se, current_text.CStr(), current_text.Length(), text, text_length));
			}
			else
				RETURN_IF_ERROR(undo_stack.SubmitInsert(MIN(this->caret_pos,text_length), text, TRUE, text_length));
		}
		else
		{
			OP_ASSERT(!current_text.IsEmpty());//text_length is 0 so the compare above should have returned
			RETURN_IF_ERROR(undo_stack.SubmitRemove(this->caret_pos, 0, current_text.Length(), current_text.CStr()));
		}
	}
	else if (!use_undo_stack)
	{
		undo_stack.Clear();
	}
#endif

	if (!m_pattern.IsEmpty())
	{
		INT32 pattern_len = static_cast<INT32>(m_pattern.Length());
		OP_ASSERT(text_length == pattern_len);
		if (text_length != pattern_len)
		{
			return OpStatus::ERR;
		}
	}
	string.SelectNone();
	OP_STATUS status = string.Set(text, text_length, this);
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	if(m_spellchecker)
		m_spellchecker->SpellcheckUpdate();
#endif // INTERNAL_SPELLCHECK_SUPPORT
	Invalidate(GetBounds());

	// The caret is expected to be at the end of the set text.
	SetCaretOffset(GetTextLength(), FALSE);
	m_packed.caret_snap_forward = TRUE;
	ResetScroll(); // Make sure the new text is visible inside the edit box

#ifdef WIDGETS_IME_SUPPORT
	if (imstring)
	{
		imstring->Set(text, uni_strlen(text));
		vis_dev->GetOpView()->SetInputTextChanged();
	}
#endif // WIDGETS_IME_SUPPORT

	if (!IsForm() && !m_packed.onchange_on_enter && !force_no_onchange)
		BroadcastOnChange(FALSE);
	return status;
}

#ifdef INTERNAL_SPELLCHECK_SUPPORT

void OpEdit::OnSpellcheckDisabled()
{
	m_packed.delayed_spell = FALSE;
}

BOOL OpEdit::SpellcheckByUser()
{
	return m_spellchecker && m_spellchecker->ByUser();
}

void OpEdit::EnableSpellcheck()
{
	m_packed.enable_spellcheck_later = !CanUseSpellcheck() || !g_internal_spellcheck->SpellcheckEnabledByDefault();

	if(!m_packed.enable_spellcheck_later)
	{
		if(m_spellchecker)
		{
			if(m_spellchecker->GetSpellCheckerSession())  // Already enabled.
				return;
		}
		else
		{
			m_spellchecker = OP_NEW(OpEditSpellchecker, (this));
		}

		m_packed.enable_spellcheck_later = !m_spellchecker || OpStatus::IsError(m_spellchecker->EnableSpellcheckInternal(FALSE /*by_user*/, NULL /*lang*/));
	}
}

void OpEdit::DisableSpellcheck(BOOL force)
{
	if(m_spellchecker)
		m_spellchecker->DisableSpellcheckInternal(FALSE /*by_user*/, force);
	m_packed.enable_spellcheck_later = FALSE;
}

BOOL OpEdit::CanUseSpellcheck()
{
	return m_pattern.IsEmpty() &&
		!m_packed.flatmode && !m_packed.is_readonly && IsEnabled() && !IsDead() && !string.GetPasswordMode();
}

int OpEdit::CreateSpellSessionId(OpPoint* point /* = NULL */)
{
	if (CanUseSpellcheck())
	{
		if (!m_spellchecker)
			m_spellchecker = OP_NEW(OpEditSpellchecker, (this));
		if (m_spellchecker)
		{
			OpPoint p;
			if (point)
			{
				p = *point;
			}
			else
			{
				// Caret pos
				// FIXME
			}
			int spell_session_id;
			m_spellchecker->CreateSpellUISession(p, spell_session_id);
			return spell_session_id;
		}
	}
	return 0;
}


#endif // INTERNAL_SPELLCHECK_SUPPORT

OP_STATUS OpEdit::SetTextAndFireOnChange(const uni_char* text)
{
	OP_STATUS status = SetText(text);
	if (OpStatus::IsSuccess(status) && listener)
	{
		listener->OnChange(this);
	}
	return status;
}

OP_STATUS OpEdit::InsertText(const uni_char* text)
{
	if (text)
	{
		InternalInsertText(text);
		Invalidate(GetBounds());
	}
	return OpStatus::OK; // OOM is handled through ReportOOM.
}

void OpEdit::SetShowGhostTextWhenFocused(BOOL show)
{
	if (m_packed.show_ghost_when_focused != !!show)
	{
		InvalidateAll();
		m_packed.show_ghost_when_focused = !!show;
	}
}

void OpEdit::SetGhostTextJusifyWhenFocused(JUSTIFY justify)
{
	m_ghost_text_onfocus_justify = justify;
}

OP_STATUS OpEdit::SetGhostText(const uni_char* ghost_text)
{
	if (ghost_string.Get() == NULL || uni_strcmp(ghost_string.Get(), ghost_text))
	{
		Invalidate(GetBounds());
		return ghost_string.Set(ghost_text, this);
	}
	return OpStatus::OK;
}

void OpEdit::SetReadOnly(BOOL readonly)
{
	m_packed.is_readonly = !!readonly;
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	DisableSpellcheckIfNotUsable();
#endif // INTERNAL_SPELLCHECK_SUPPORT
}

void OpEdit::SetFlatMode()
{
	SetReadOnly(TRUE);
	m_packed.flatmode = TRUE;
	SetTabStop(FALSE);
#ifdef SKIN_SUPPORT
	GetBorderSkin()->SetImage(NULL);
#else
	SetHasCssBorder(TRUE); // Makes us not draw our border (since CSS borders are drawn by the HTML layout engine)
#endif
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	DisableSpellcheck(TRUE /* force */);
#endif
}

void OpEdit::SetMaxLength(INT32 length)
{
	string.SetMaxLength(length);
}

void OpEdit::SetPasswordMode(BOOL password_mode)
{
	if (password_mode)
		m_packed.dblclick_selects_all = TRUE;

	string.SetPasswordMode(password_mode);
}

void OpEdit::SelectText()
{
	SetSelection(0, GetTextLength());
}

BOOL GetMatchedText(const uni_char* txt, int len, BOOL forward, BOOL match_case, BOOL words,
					int &start_idx, int end_idx, int elm_len, const uni_char* ctxt);

#ifndef HAS_NO_SEARCHTEXT

BOOL OpEdit::SearchText(const uni_char* txt, int len, BOOL forward, BOOL match_case, BOOL words, SEARCH_TYPE type, BOOL select_match, BOOL scroll_to_selected_match, BOOL wrap, BOOL next)
{
	int search_text_len = uni_strlen(txt);
	int elm_len = GetTextLength();
	int end_idx = elm_len - len;

	int start_idx = caret_pos;
	if (type == SEARCH_FROM_BEGINNING)
		start_idx = 0;
	else if (type == SEARCH_FROM_END)
		start_idx = elm_len;
	else if (!next && forward && HasSelectedText())
		start_idx = string.sel_start;

	int idx = start_idx;
	BOOL found = FALSE;
	while(TRUE)
	{
		found = GetMatchedText(txt, len, forward, match_case, words,
									idx, end_idx, elm_len, string.Get());
		if (!forward && idx == start_idx - search_text_len && HasSelectedText())
			idx -= search_text_len;
		else
			break;
	}
	if (found && select_match)
	{
		string.Select(idx, idx + search_text_len);
		SetCaretOffset(idx + search_text_len, FALSE);
		if (scroll_to_selected_match)
			ScrollIfNeeded(TRUE);
		Invalidate(GetBounds());
	}
	return found;
}

#endif // !HAS_NO_SEARCHTEXT

BOOL OpEdit::HasSelectedText()
{
	return string.sel_start != -1 && string.sel_stop != -1 && string.sel_start != string.sel_stop;
}


BOOL OpEdit::IsAllSelected()
{
	if( string.sel_start == -1 || string.sel_stop == -1 )
	{
		return FALSE;
	}
	else if( string.sel_start == 0 &&string.sel_stop == GetTextLength() )
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}


void OpEdit::SelectNothing()
{
	string.SelectNone();
	Invalidate(GetBounds());
	m_selection_highlight_type = VD_TEXT_HIGHLIGHT_TYPE_SELECTION;
}

/* virtual */
void OpEdit::GetSelection(INT32& start_ofs, INT32& stop_ofs, SELECTION_DIRECTION& direction, BOOL)
{
	INT32 start, stop;

	if (HasSelectedText())
	{
		if (string.sel_start < string.sel_stop)
		{
			start = string.sel_start;
			stop = string.sel_stop;
		}
		else
		{
			start = string.sel_stop;
			stop = string.sel_start;
		}

#ifdef RANGESELECT_FROM_EDGE
		if (m_packed.determine_select_direction)
			direction = SELECTION_DIRECTION_NONE;
		else
#endif // RANGESELECT_FROM_EDGE
		if (caret_pos == start)
			direction = SELECTION_DIRECTION_BACKWARD;
		else
			direction = SELECTION_DIRECTION_FORWARD;
	}
	else
	{
		start = stop = 0;
		direction = SELECTION_DIRECTION_DEFAULT;
	}

	start_ofs = start;
	stop_ofs = stop;
}

/* virtual */
void OpEdit::SetSelection(INT32 start_ofs, INT32 stop_ofs, SELECTION_DIRECTION direction, BOOL)
{
// FIX: Should probablt enable this code so we don't reset caret offset when we shouldn't. See selftest "SelectAndSearch_OpEdit".
//	int old_sel_start = string.sel_start;
//	int old_sel_stop = string.sel_stop;
	string.Select(start_ofs, stop_ofs);

#ifdef RANGESELECT_FROM_EDGE
	if (direction == SELECTION_DIRECTION_NONE)
		m_packed.determine_select_direction = TRUE;
	else
		m_packed.determine_select_direction = FALSE;
#endif // RANGESELECT_FROM_EDGE

//	if (old_sel_start != string.sel_start ||
//		old_sel_stop != string.sel_stop)
	{
		if (direction == SELECTION_DIRECTION_BACKWARD)
			SetCaretOffset(string.sel_start == -1 ? start_ofs : string.sel_start, FALSE);
		else
			SetCaretOffset(string.sel_stop == -1 ? stop_ofs : string.sel_stop, FALSE);
		m_packed.caret_snap_forward = TRUE;
		Invalidate(GetBounds());
	}
	m_selection_highlight_type = VD_TEXT_HIGHLIGHT_TYPE_SELECTION;
}

void OpEdit::SelectSearchHit(INT32 start_ofs, INT32 stop_ofs, BOOL is_active_hit)
{
	SetSelection(start_ofs, stop_ofs);
	m_selection_highlight_type = is_active_hit
		? VD_TEXT_HIGHLIGHT_TYPE_ACTIVE_SEARCH_HIT
		: VD_TEXT_HIGHLIGHT_TYPE_SEARCH_HIT;
}

BOOL OpEdit::IsSearchHitSelected()
{
	return (m_selection_highlight_type == VD_TEXT_HIGHLIGHT_TYPE_SEARCH_HIT)
		|| (m_selection_highlight_type == VD_TEXT_HIGHLIGHT_TYPE_ACTIVE_SEARCH_HIT);
}

BOOL OpEdit::IsActiveSearchHitSelected()
{
	return (m_selection_highlight_type == VD_TEXT_HIGHLIGHT_TYPE_ACTIVE_SEARCH_HIT);
}

BOOL OpEdit::IsPatternWriteable(int pos)
{
	return pos < m_pattern.Length() && m_pattern[pos] == ' ';
}

OP_STATUS OpEdit::SetPattern(const uni_char* new_pattern)
{
	const uni_char* old_pattern = m_pattern.CStr();
	if (!new_pattern && old_pattern ||
		new_pattern && (!old_pattern || !uni_str_eq(new_pattern, old_pattern)))
	{
		RETURN_IF_ERROR(m_pattern.Set(new_pattern));
		RETURN_IF_ERROR(SetText(new_pattern ? new_pattern : UNI_L("")));
		SetCaretOffset(0);
	}
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	DisableSpellcheckIfNotUsable();
#endif
	return OpStatus::OK;
}

#ifdef WIDGETS_IME_SUPPORT
BOOL SpawnIME(VisualDevice* vis_dev, const uni_char* istyle, OpView::IME_MODE mode, OpView::IME_CONTEXT context)
{
	if (!vis_dev)
		return FALSE;

	if (g_opera->widgets_module.im_spawning)
	{
		// IME is doing weird things with focus, probably showing a webUI which steals focus.
		// Must move focus to document to work around a lot of problems.
		vis_dev->SetFocus(FOCUS_REASON_OTHER);
		return FALSE;
	}

	g_opera->widgets_module.im_spawning = TRUE;
	// Set the current wanted input method mode.
	vis_dev->GetView()->GetOpView()->SetInputMethodMode(mode, context, istyle);
	g_opera->widgets_module.im_spawning = FALSE;
	return TRUE;
}

OpView::IME_CONTEXT GetIMEContext(FormObject* form_obj)
{
	if (!form_obj)
		return OpView::IME_CONTEXT_DEFAULT;

	FramesDocument* frames_doc = form_obj->GetDocument();
	HTML_Element* form_control_element = form_obj->GetHTML_Element();

	if (FormManager::IsIncludedInSubmit(frames_doc, form_control_element, NULL))
		return form_control_element->GetInputType() == INPUT_SEARCH ?
			   OpView::IME_CONTEXT_SEARCH : OpView::IME_CONTEXT_FORM;

	return OpView::IME_CONTEXT_DEFAULT;
}
#endif // WIDGETS_IME_SUPPORT

INT32 NextCharRegion(const uni_char* str, INT32 start, INT32 max_len)
{
	CharRecognizer *recog = GetCharTypeRecognizer(str[start]);

	INT32 i = start;

	while(str[i] != '\0' && i < max_len && recog->is(str[i]))
		i++;

	return i;
}

INT32 PrevCharRegion(const uni_char* str, INT32 start, BOOL stay_within_region)
{
	CharRecognizer *recog = GetCharTypeRecognizer(str[start]);

	INT32 i = start;

	while (i > 0 && recog->is(str[i]))
		i--;

	if (stay_within_region && !recog->is(str[i]))
		i++;

	return i;
}

BOOL IsWhitespaceChar(uni_char c)
{
	return uni_isspace(c);
}

CharRecognizer *GetCharTypeRecognizer(uni_char c)
{
	// The following steps will add a new character class, such that a sequence
	// of such characters will be considered a unit when double-clicking,
	// ctrl-stepping, etc.:
	//
	// 1. Define a new CharRecognizer subclass and bump N_CHAR_RECOGNIZERS
	// 2. Add an instance of it to WIDGET_GLOBALS
	// 3. Add the instance's address to g_widget_globals->char_recognizers in
	//    OpWidget::InitializeL()
	//
	// An older solution used function pointers instead of the CharRecognizer
	// class hierarchy (and was in general much neater), but that causes
	// problems on BREW.

	for (unsigned int i = 0; i < N_CHAR_RECOGNIZERS; i++)
		if (g_widget_globals->char_recognizers[i]->is(c))
			return g_widget_globals->char_recognizers[i];

	// Default recognizer
	return &g_widget_globals->word_char_recognizer;
}

// Returns the delta from start to the beginning of the word in the specified direction
INT32 SeekWordEx(const uni_char* str, INT32 start, INT32 step, BOOL is_passwd, INT32 max_len = 1000000)
{
	if (start == 0 && step < 0)
		return 0;

	if (is_passwd)
	{
		if (step < 0)
			return -start;
		INT32 len = 0;
		while(str[len] != '\0' && len < max_len)
			len++;
		return len - start;
	}

	INT32 i = start;

	if (step > 0)
	{
		// Fix for DSK-322573. Remove correct part of the path name.
		if (str[i] == '/')
			i++;

		i = NextCharRegion(str, i, max_len);

		// Skip over eventual whitespace
		if (IsWhitespaceChar(str[i]))
			i = NextCharRegion(str, i, max_len);
	}
	else
	{
		i--;

		// Skip over eventual whitespace
		if (IsWhitespaceChar(str[i]))
			i = PrevCharRegion(str, i, FALSE);

		// Fix for DSK-322573. Remove correct part of the path name.
		if (str[i] == '/' && i > 0)
			i--;

		i = PrevCharRegion(str, i, TRUE);
	}
	return i - start;
}

INT32 SeekWord(const uni_char* str, INT32 start, INT32 step, INT32 max_len)
{
	return SeekWordEx(str, start, step, FALSE, max_len);
}


void OpEdit::InternalInsertText(const uni_char* instr, BOOL no_append_undo)
{
	OP_STATUS status = OpStatus::OK;
	OpString str;
	status = str.Set(string.Get());
	REPORT_AND_RETURN_IF_ERROR(status);

	int s1, s2;
	OpWidget::GetSelection(s1, s2);
	// Replace the selection with new text
	if (s2)
	{
#ifdef WIDGETS_UNDO_REDO_SUPPORT
		if (IsUndoRedoEnabled())
		{
			status = undo_stack.SubmitRemove(caret_pos, s1, s2, &str.CStr()[s1]);
			REPORT_AND_RETURN_IF_ERROR(status);
		}
#endif
		if (!m_pattern.IsEmpty())
		{
			OP_ASSERT(m_pattern.Length() == str.Length());
			// replace selection with spaces unless it's part of the pattern
			for (int sel_index = s1; sel_index < s2; sel_index++)
			{
				if (IsPatternWriteable(sel_index))
				{
					str[sel_index] = ' ';
				}
			}
		}
		else
		{
			str.Delete(s1, s2 - s1);
		}
		SetCaretOffset(s1, FALSE);
#ifdef WIDGETS_IME_SUPPORT
		if (m_packed.im_is_composing)
			im_pos = s1;
#endif
		string.SelectNone();
	}

	// Don't override max length
	INT32 len = uni_strlen(instr);
	if (str.Length() + len > (INT32)string.GetMaxLength())
		len -= (len + str.Length()) - string.GetMaxLength();

	if (len <= 0) // Nothing left to insert
	{
#ifdef WIDGETS_IME_SUPPORT
		if (m_packed.im_is_composing)
		{
			// We have to create a entry on the undo_stack when composing. Otherwise the next compose will
			// undo an entry it should not.
			status = undo_stack.SubmitEmpty();
			REPORT_AND_RETURN_IF_ERROR(status);
		}
#endif // WIDGETS_IME_SUPPORT
		return;
	}

	// Check for newline characters and stop inserting there.
	INT32 i;
	for(i = 0; i < len; i++)
		if (instr[i] == '\r' || instr[i] == '\n')
		{
			len = i;
			break;
		}

	// Potentially very slow
	OpString tmp_string; // Temp holder of a instr copy, since we want to modify it
	if (!m_allowed_chars.IsEmpty())
	{
		if (OpStatus::IsSuccess(tmp_string.Set(instr)))
		{
			instr = tmp_string.CStr();

			for(i = 0; i < len; i++)
			{
				if (char(tmp_string[i]) != tmp_string[i] ||
					m_allowed_chars.FindFirstOf((char)tmp_string[i]) == -1)
				{
					// This character isn't allowed. But check if it's upper or lower case opposite is.
					uni_char upper = uni_toupper(tmp_string[i]);
					uni_char lower = uni_tolower(tmp_string[i]);
					uni_char othercase = (tmp_string[i] == upper ? lower : upper);
					if (othercase != tmp_string[i] && m_allowed_chars.FindFirstOf((char)othercase) != -1)
					{
						// The other case was allowed, so use that instead!
						tmp_string[i] = othercase;
						continue;
					}
					len = i;
					break;
				}
			}
		}
	}
	if (len <= 0) // Nothing left to insert
		return;

	// Insert new text

#ifdef WIDGETS_UNDO_REDO_SUPPORT
	if (IsUndoRedoEnabled())
	{
		status = undo_stack.SubmitInsert(caret_pos, instr, no_append_undo, len);
		REPORT_AND_RETURN_IF_ERROR(status);
	}
#endif

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	BOOL delay_spell = FALSE;
	UINT32 spell_old_frag_count = 0;
#endif // INTERNAL_SPELLCHECK_SUPPORT

	if (!m_pattern.IsEmpty())
	{
		OP_ASSERT(m_pattern.Length() == str.Length());
		int insert_pos = caret_pos;
		for (int instr_pos = 0; instr_pos < len; instr_pos++)
		{
			while (!IsPatternWriteable(insert_pos))
			{
				insert_pos++;
				if (insert_pos > m_pattern.Length())
				{
					// skip the rest
					return;
				}
			}
			OP_ASSERT(str.Length() > insert_pos);
			str[insert_pos++] = instr[instr_pos];
		}
		len = insert_pos - caret_pos;
	}
	else
	{
#ifdef INTERNAL_SPELLCHECK_SUPPORT
		// Check if we should delay spellchecking, for not marking the word currently being written
		OpSpellCheckerSession *session = m_spellchecker ? m_spellchecker->GetSpellCheckerSession() : NULL;
		if(session && !s2 && len == 1 && !session->IsWordSeparator(*instr))
		{
			BOOL after_blocked = caret_pos < str.Length() && !session->IsWordSeparator(str.CStr()[caret_pos]);
			if(!after_blocked)
			{
				BOOL before_blocked = FALSE;
				if(!m_packed.delayed_spell)
					before_blocked = caret_pos && !session->IsWordSeparator(str.CStr()[caret_pos-1]);
				if(!before_blocked)
				{
					string.Update();
					spell_old_frag_count = string.fragments.GetNumFragments();
					delay_spell = TRUE;
					m_packed.delayed_spell = TRUE;
				}
			}
		}
#endif
		status = str.Insert(caret_pos, instr, len);
		REPORT_AND_RETURN_IF_ERROR(status);
	}

	status = string.Set(str.CStr(), this);

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	if(m_spellchecker)
	{
		if(!delay_spell)
			m_spellchecker->SpellcheckUpdate();
		else // possibly move the spell-flag for the fragments in front of the word beeing written, if a new fragment has been created
		{
			string.Update();
			if(string.fragments.GetNumFragments() > spell_old_frag_count)
				string.fragments.MoveSpellFlagsForwardAfter(caret_pos, string.fragments.GetNumFragments() - spell_old_frag_count);
		}
		m_packed.delayed_spell = !!delay_spell;
	}
#endif // INTERNAL_SPELLCHECK_SUPPORT

	if (OpStatus::IsError(status))
	{
		SetCaretOffset(0, FALSE);
		REPORT_AND_RETURN_IF_ERROR(status);
	}

	//if (!get_bidi_mirrored(instr[0]))
		SetCaretOffset(caret_pos + len, FALSE);

	INT32 text_len = GetTextLength();
	if (caret_pos > text_len) // Maxlen may have truncated the string. Then we won't move the caret.
		SetCaretOffset(text_len, FALSE);

#if defined(SUPPORT_IME_PASSWORD_INPUT) && defined(WIDGETS_IME_SUPPORT)
    else
        if (len && string.GetPasswordMode() == OpView::IME_MODE_PASSWORD)
        {
			if (g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::ShowIMEPassword))
			{
				string.ShowPasswordChar(caret_pos);
				hidePasswordCounter = 2;	// 2 blinks of cursor to hide the character
			}
        }
#endif //defined(SUPPORT_IME_PASSWORD_INPUT) && defined(WIDGETS_IME_SUPPORT)
	m_packed.is_changed = TRUE;
}

OpRect OpEdit::GetTextRect()
{
	OpRect inner_rect = GetBounds();

	GetInfo()->AddBorder(this, &inner_rect);

	int indent = GetTextIndentation();
	inner_rect.x += indent;
	inner_rect.width -= indent;

	AddPadding(inner_rect);

	return inner_rect;
}

int OpEdit::GetTextIndentation()
{
	int indentation = 0;
#ifdef SKIN_SUPPORT
	if (GetForegroundSkin()->HasContent() && !g_widgetpaintermanager->NeedCssPainter(this))
	{
		OpRect inner_rect = GetBounds();

		GetInfo()->AddBorder(this, &inner_rect);

		OpRect rect = GetForegroundSkin()->CalculateScaledRect(inner_rect, FALSE, TRUE);

		// This is the same calculation as in the painter
		UINT32 image_width = rect.width + 4;

		indentation += image_width;
	}
#endif
	return indentation;
}

void OpEdit::ScrollIfNeeded(BOOL include_document, BOOL smooth_scroll)
{
	OP_ASSERT(!smooth_scroll); // smooth_scroll not implemented

	if (!IsForm() && m_packed.is_readonly && !IsFocused())
		return; // We want no scroll to end for UI elements with no editfunction.

	// Scroll the content in the widget if needed

	OpRect inner_rect = GetTextRect();
	if (!inner_rect.IsEmpty())
	{
		int caret_x;
		if (string.sel_start == 0 && string.sel_stop == GetTextLength())
		{
			// When all text are selected we always keep the start of the text visible,
			// otherwise Desktop will have to reset caret to 0 to achive this which will
			// break other things.See DSK-319460,
			caret_x = 0;
		}
		else
			caret_x = string.GetCaretX(inner_rect, caret_pos, m_packed.caret_snap_forward);

#ifdef WIDGETS_IME_SUPPORT
		if (m_packed.im_is_composing)
		{
			// if there is a composition string, try to ensure that
			// the full composition string is visible and not only the
			// current cursor position - and if the full composition
			// string is too wide, try to ensure, that at least the
			// current candidate is visible:
			int inner_rect_right = inner_rect.Right();

			// try to get full composition string into visible area
			int im_pos_x = string.GetCaretX(inner_rect, im_pos, m_packed.caret_snap_forward);
			int im_end_x = string.GetCaretX(inner_rect, im_pos + string.GetIMLen(), m_packed.caret_snap_forward);
			if (im_pos_x < inner_rect.x + x_scroll)
				x_scroll = im_pos_x - inner_rect.x;
			else if (im_end_x > inner_rect_right + x_scroll)
				x_scroll = im_end_x - inner_rect_right;

			// check that at least the current candidate is visible;
			int candidate_pos_x = string.GetCaretX(inner_rect, im_pos + imstring->GetCandidatePos(), m_packed.caret_snap_forward);
			int candidate_end_x = string.GetCaretX(inner_rect, im_pos + imstring->GetCandidatePos() + imstring->GetCandidateLength(), m_packed.caret_snap_forward);
			if (candidate_pos_x < inner_rect.x + x_scroll)
				x_scroll = candidate_pos_x - inner_rect.x;
			if (candidate_end_x > inner_rect_right + x_scroll)
				x_scroll = candidate_end_x - inner_rect_right;
		}
		else
#endif // WIDGETS_IME_SUPPORT
		if (caret_x - x_scroll < inner_rect.x)
			x_scroll -= inner_rect.x - (caret_x - x_scroll);
		else if (caret_x - x_scroll > inner_rect.x + inner_rect.width)
			x_scroll = caret_x - (inner_rect.x + inner_rect.width);

		if (string.GetWidth() < inner_rect.width)
			x_scroll = 0;
		else if (string.GetWidth() - x_scroll < inner_rect.width)
			x_scroll = string.GetWidth() - inner_rect.width;

		// Scroll the document if needed

		if (include_document && IsForm() && vis_dev && vis_dev->GetDocumentManager())
		{
			FramesDocument* doc = vis_dev->GetDocumentManager()->GetCurrentDoc();
			if (doc && doc->GetHtmlDocument() && !doc->IsReflowing())
			{
				OpRect caret_rect(caret_x - x_scroll, 0, 1, rect.height);
				AffinePos doc_pos = GetPosInDocument();
				doc_pos.Apply(caret_rect);

				HTML_Element* html_element = GetFormObject() ? GetFormObject()->GetHTML_Element() : 0;
				doc->ScrollToRect(caret_rect, SCROLL_ALIGN_CENTER, FALSE, VIEWPORT_CHANGE_REASON_FORM_FOCUS, html_element);
			}
		}
	}
}

void OpEdit::ResetScroll()
{
	x_scroll = 0;
}

void OpEdit::MoveCaretToStartOrEnd(BOOL start, BOOL visual_order)
{
	string.Update(); // Make sure fragments is up to date.
	if (visual_order)
	{
		OP_TEXT_FRAGMENT_VISUAL_OFFSET v_ofs;
		if (start)
		{
			v_ofs.idx = 0;
			v_ofs.offset = 0;
		}
		else
		{
			v_ofs.idx = string.fragments.GetNumFragments() - 1;
			v_ofs.offset = GetTextLength();
		}
		OP_TEXT_FRAGMENT_LOGICAL_OFFSET l_ofs = string.fragments.VisualToLogicalOffset(v_ofs);

		SetCaretOffset(l_ofs.offset, FALSE);
		m_packed.caret_snap_forward = !!l_ofs.snap_forward;
	}
	else
	{
		if (start)
		{
			SetCaretOffset(0, FALSE);
		}
		else
		{
			SetCaretOffset(GetTextLength(), FALSE);
		}
		m_packed.caret_snap_forward = !!start;
	}
}

void OpEdit::MoveCaret(BOOL forward, BOOL visual_order)
{
	string.Update(); // Make sure fragments is up to date.
	if (!visual_order)
	{
		UnicodeStringIterator iter(string.Get(), caret_pos, GetTextLength());

		if (forward)
		{

#ifdef SUPPORT_UNICODE_NORMALIZE
			iter.NextBaseCharacter();
#else
			iter.Next();
#endif

			SetCaretOffset(iter.Index(), FALSE);
		}
		else
		{

#ifdef SUPPORT_UNICODE_NORMALIZE
			iter.PreviousBaseCharacter();
#else
			iter.Previous();
#endif

			SetCaretOffset(iter.Index(), FALSE);
		}
		return;
	}

	OP_TEXT_FRAGMENT_LOGICAL_OFFSET log_ofs;
	log_ofs.offset = caret_pos;
	log_ofs.snap_forward = m_packed.caret_snap_forward;

	OP_TEXT_FRAGMENT_VISUAL_OFFSET vis_ofs = string.fragments.LogicalToVisualOffset(log_ofs);

	UnicodeStringIterator iter(string.Get(), vis_ofs.offset, GetTextLength());

	if (forward)
	{

#ifdef SUPPORT_UNICODE_NORMALIZE
		iter.NextBaseCharacter();
#else
		iter.Next();
#endif

		vis_ofs.offset = iter.Index();
	}
	else
	{

#ifdef SUPPORT_UNICODE_NORMALIZE
		iter.PreviousBaseCharacter();
#else
		iter.Previous();
#endif

		vis_ofs.offset = iter.Index();
	}

	log_ofs = string.fragments.VisualToLogicalOffset(vis_ofs);

	m_packed.caret_snap_forward = !!log_ofs.snap_forward;
	SetCaretOffset(log_ofs.offset, FALSE);
}

void OpEdit::ReportCaretPosition()
{
	OpRect inner_rect = GetTextRect();
	if (!inner_rect.IsEmpty())
	{
		GenerateHighlightRectChanged(OpRect(
			string.GetCaretX(inner_rect, caret_pos, m_packed.caret_snap_forward) - x_scroll,
			inner_rect.y,
			1,
			inner_rect.height), TRUE);
	}
}

BOOL OpEdit::IsSelectionStartVisuallyBeforeStop()
{
	OP_ASSERT(HasSelectedText());
	OP_TEXT_FRAGMENT_LOGICAL_OFFSET log_start, log_stop;
	log_start.offset = string.sel_start;
	log_start.snap_forward = TRUE;
	log_stop.offset = string.sel_stop;
	log_stop.snap_forward = FALSE;

	OP_TEXT_FRAGMENT_VISUAL_OFFSET vis_start = string.fragments.LogicalToVisualOffset(log_start);
	OP_TEXT_FRAGMENT_VISUAL_OFFSET vis_stop = string.fragments.LogicalToVisualOffset(log_stop);
	return vis_start.offset <= vis_stop.offset;
}

void OpEdit::EditAction(OpInputAction* action)
{
	if (m_packed.is_readonly || is_selecting)
		return;

	BOOL local_is_changed = FALSE;
	OP_STATUS status;
	int strlen = GetTextLength();
	int old_caret_pos = caret_pos;
	int old_sel_start = string.sel_start;
	int old_sel_stop = string.sel_stop;
	OpKey::Code key = action->GetActionKeyCode();
	BOOL moving_caret = action->IsMoveAction();

	switch (action->GetAction())
	{
#if defined(WIDGETS_UP_DOWN_MOVES_TO_START_END)
		case OpInputAction::ACTION_PREVIOUS_LINE:
#endif
		case OpInputAction::ACTION_GO_TO_LINE_START:
		case OpInputAction::ACTION_RANGE_GO_TO_LINE_START:
		{
			MoveCaretToStartOrEnd(TRUE, FALSE);
			break;
		}
#if defined(WIDGETS_UP_DOWN_MOVES_TO_START_END)
		case OpInputAction::ACTION_NEXT_LINE:
#endif
		case OpInputAction::ACTION_GO_TO_LINE_END:
		case OpInputAction::ACTION_RANGE_GO_TO_LINE_END:
		{
			MoveCaretToStartOrEnd(FALSE, FALSE);
			break;
		}
		case OpInputAction::ACTION_NEXT_CHARACTER:
		case OpInputAction::ACTION_RANGE_NEXT_CHARACTER:
		{
#ifdef WIDGETS_MOVE_CARET_TO_SELECTION_STARTSTOP
			if (HasSelectedText() && !action->IsRangeAction())
			{
				BOOL start_before_stop = IsSelectionStartVisuallyBeforeStop();
				SetCaretOffset(start_before_stop ? string.sel_stop : string.sel_start, FALSE);
				m_packed.caret_snap_forward = !start_before_stop;
			}
			else
#endif
			{
				// Move action moves in visual order. Range action moves in logical order but reversed if RTL.
				BOOL forward = TRUE;
				if (action->IsRangeAction() && GetRTL() && !GetForceTextLTR())
					forward = !forward;
				MoveCaret(forward, !action->IsRangeAction());
			}
			break;
		}
		case OpInputAction::ACTION_PREVIOUS_CHARACTER:
		case OpInputAction::ACTION_RANGE_PREVIOUS_CHARACTER:
		{
#ifdef WIDGETS_MOVE_CARET_TO_SELECTION_STARTSTOP
			if (HasSelectedText() && !action->IsRangeAction())
			{
				BOOL start_before_stop = IsSelectionStartVisuallyBeforeStop();
				SetCaretOffset(start_before_stop ? string.sel_start : string.sel_stop, FALSE);
				m_packed.caret_snap_forward = !!start_before_stop;
			}
			else
#endif
			{
#ifdef RANGESELECT_FROM_EDGE
				if (m_packed.determine_select_direction)
				{
					SetCaretOffset(string.sel_start, FALSE);
					old_caret_pos = caret_pos;
				}
#endif // RANGESELECT_FROM_EDGE
				// Move action moves in visual order. Range action moves in logical order but reversed if RTL.
				BOOL forward = FALSE;
				if (action->IsRangeAction() && GetRTL() && !GetForceTextLTR())
					forward = !forward;
				MoveCaret(forward, !action->IsRangeAction());
			}
			break;
		}
		case OpInputAction::ACTION_NEXT_WORD:
		case OpInputAction::ACTION_RANGE_NEXT_WORD:
		{
			SetCaretOffset(caret_pos + SeekWordEx(string.Get(), caret_pos, 1, string.GetPasswordMode()), FALSE);
			m_packed.caret_snap_forward = FALSE;
			break;
		}
		case OpInputAction::ACTION_PREVIOUS_WORD:
		case OpInputAction::ACTION_RANGE_PREVIOUS_WORD:
		{
			SetCaretOffset(caret_pos + SeekWordEx(string.Get(), caret_pos, -1, string.GetPasswordMode()), FALSE);
			m_packed.caret_snap_forward = TRUE;
			break;
		}
		case OpInputAction::ACTION_DELETE:
		case OpInputAction::ACTION_DELETE_WORD:
		case OpInputAction::ACTION_DELETE_TO_END_OF_LINE:
		{
			OpString str;
			status = str.Set(string.Get(), strlen);
			REPORT_AND_RETURN_IF_ERROR(status);
			int s1, s2;
			OpWidget::GetSelection(s1, s2);
			if (s2)
			{
				// Remove selection
#ifdef WIDGETS_UNDO_REDO_SUPPORT
				if (IsUndoRedoEnabled())
				{
					status = undo_stack.SubmitRemove(caret_pos, s1, s2, &str.CStr()[s1]);
					REPORT_AND_RETURN_IF_ERROR(status);
				}
#endif
				if (!m_pattern.IsEmpty())
				{
					OP_ASSERT(m_pattern.Length() == str.Length());
					for (int s = s1; s < s2; s++)
					{
						str[s] = m_pattern[s];
					}
				}
				else
				{
					str.Delete(s1, s2 - s1);
				}
				local_is_changed = TRUE;
				SetCaretOffset(s1, FALSE);
				string.SelectNone();
			}
			else if (caret_pos < strlen)
			{
				// Remove character or word
				INT32 len = 1;
				if (action->GetAction() == OpInputAction::ACTION_DELETE_WORD) // Seek one word
					len = SeekWordEx(string.Get(), caret_pos, 1, string.GetPasswordMode());
				else if (action->GetAction() == OpInputAction::ACTION_DELETE_TO_END_OF_LINE)
				{
					len = GetTextLength() - caret_pos;
					if( len < 1 )
					{
						len = 1;
					}
				}
				else
				{
					UnicodeStringIterator iter(string.Get(), caret_pos, GetTextLength());

#ifdef SUPPORT_UNICODE_NORMALIZE
					iter.NextBaseCharacter();
#else
					iter.Next();
#endif

					len = iter.Index() - caret_pos;
				}
#ifdef WIDGETS_UNDO_REDO_SUPPORT
				if (IsUndoRedoEnabled())
				{
					status = undo_stack.SubmitRemove(caret_pos, caret_pos, caret_pos + len, &str.CStr()[caret_pos]);
					REPORT_AND_RETURN_IF_ERROR(status);
				}
#endif
				if (!m_pattern.IsEmpty())
				{
					OP_ASSERT(m_pattern.Length() == str.Length());
					int end_pos = caret_pos+len;
					for (int s = caret_pos; s < end_pos; s++)
					{
						str[s] = m_pattern[s];
					}
				}
				else
				{
					str.Delete(caret_pos, len);
				}
				local_is_changed = TRUE;
			}
			status = string.Set(str.CStr(), this);
			if (OpStatus::IsError(status))
				SetCaretOffset(0);
			REPORT_AND_RETURN_IF_ERROR(status);

#ifdef INTERNAL_SPELLCHECK_SUPPORT
			if(m_spellchecker)
				m_spellchecker->SpellcheckUpdate();
#endif // INTERNAL_SPELLCHECK_SUPPORT

			break;
		}
		case OpInputAction::ACTION_BACKSPACE:
		case OpInputAction::ACTION_BACKSPACE_WORD:
		{
			OpString str;
			status = str.Set(string.Get(), strlen);
			REPORT_AND_RETURN_IF_ERROR(status);
			int s1, s2;
			OpWidget::GetSelection(s1, s2);
			if (s2)
			{
				// Remove selection
#ifdef WIDGETS_UNDO_REDO_SUPPORT
				if (IsUndoRedoEnabled())
				{
					status = undo_stack.SubmitRemove(caret_pos, s1, s2, &str.CStr()[s1]);
					REPORT_AND_RETURN_IF_ERROR(status);
				}
#endif
				if (!m_pattern.IsEmpty())
				{
					OP_ASSERT(str.Length() == m_pattern.Length());
					for (int s = s1; s < s2; s++)
					{
						str[s] = m_pattern[s];
					}
				}
				else
				{
					str.Delete(s1, s2 - s1);
				}
				local_is_changed = TRUE;
				SetCaretOffset(s1, FALSE);
				string.SelectNone();
			}
			else if (caret_pos > 0)
			{
				// Remove character or word
				INT32 len = -1;
				if (action->GetAction() == OpInputAction::ACTION_BACKSPACE_WORD) // Seek one word
					len = SeekWordEx(string.Get(), caret_pos, -1, string.GetPasswordMode());
				else
				{
					UnicodeStringIterator iter(string.Get(), caret_pos, GetTextLength());

#ifdef SUPPORT_UNICODE_NORMALIZE
					iter.PreviousBaseCharacter();
#else
					iter.Previous();
#endif

					len = iter.Index() - caret_pos;
				}
#ifdef WIDGETS_UNDO_REDO_SUPPORT
				if (IsUndoRedoEnabled())
				{
					status = undo_stack.SubmitRemove(caret_pos + len, caret_pos + len, caret_pos, &str.CStr()[caret_pos + len]);
					REPORT_AND_RETURN_IF_ERROR(status);
				}
#endif
				if (!m_pattern.IsEmpty())
				{
					OP_ASSERT(str.Length() == m_pattern.Length());
					for (int pos = caret_pos + len; pos < caret_pos; pos++)
					{
						str[pos] = m_pattern[pos];
					}
				}
				else
				{
					str.Delete(caret_pos + len, -len);
				}
				local_is_changed = TRUE;
				SetCaretOffset(caret_pos + len, FALSE);
			}
#if defined(SUPPORT_IME_PASSWORD_INPUT) && defined(WIDGETS_IME_SUPPORT)
			// need to make sure that we don't uncover any chars when deleting something in a string
			if (string.GetPasswordMode() == OpView::IME_MODE_PASSWORD)
			{
				string.HidePasswordChar();
				hidePasswordCounter=0;
			}
#endif
			status = string.Set(str.CStr(), this);
			if (OpStatus::IsError(status))
				SetCaretOffset(0);
			REPORT_AND_RETURN_IF_ERROR(status);

#ifdef INTERNAL_SPELLCHECK_SUPPORT
			if(m_spellchecker)
				m_spellchecker->SpellcheckUpdate();
#endif // INTERNAL_SPELLCHECK_SUPPORT

			break;
		}
		case OpInputAction::ACTION_LOWLEVEL_KEY_PRESSED:
		{
#ifdef WIDGETS_IME_SUPPORT
			OP_ASSERT(imstring == NULL); // Key should not be sent if inputmethod is active.
#endif
			if (string.GetOverstrike() && !HasSelectedText() && caret_pos < GetTextLength())
			{
				UnicodeStringIterator iter(string.Get(), caret_pos, GetTextLength());

#ifdef SUPPORT_UNICODE_NORMALIZE
				iter.NextBaseCharacter();
#else
				iter.Next();
#endif

				string.Select(caret_pos, iter.Index());
			}

			const uni_char *input_string = action->GetKeyValue();
#ifdef OP_KEY_SPACE_ENABLED
			uni_char space_str[2] = { (uni_char)' ', 0};
			if (!input_string && key == OP_KEY_SPACE)
				input_string = space_str;
#endif // OP_KEY_SPACE_ENABLED
			InternalInsertText(input_string);
			local_is_changed = TRUE;
			break;
		}
		case OpInputAction::ACTION_CONVERT_HEX_TO_UNICODE:
		{
			int hex_start, hex_end, hex_pos;

			OpWidget::GetSelection(hex_start, hex_end);
			if (!hex_end)
			{
				hex_end = caret_pos;
				hex_start = 0;
			}
			if (UnicodePoint charcode = ConvertHexToUnicode(hex_start, hex_end, hex_pos, string.Get()))
			{
				string.Select(hex_pos, hex_end);
				uni_char instr[3] = { 0, 0, 0 }; /* ARRAY OK 2011-11-07 peter */
				Unicode::WriteUnicodePoint(instr, charcode);
				InternalInsertText(instr);
				local_is_changed = TRUE;
			}
			break;
		}
	}

#ifndef HAS_NOTEXTSELECTION
	if (action->IsRangeAction())
	{
		string.SelectFromCaret(old_caret_pos, caret_pos - old_caret_pos);
		m_packed.determine_select_direction = FALSE;
	}
	else
#endif // !HAS_NOTEXTSELECTION
	if (moving_caret)
	{
		string.SelectNone();
		m_packed.determine_select_direction = FALSE;
#if defined(SUPPORT_IME_PASSWORD_INPUT) && defined(WIDGETS_IME_SUPPORT)
        if (string.GetPasswordMode() == OpView::IME_MODE_PASSWORD)
        {
            string.HidePasswordChar();
        }
        hidePasswordCounter=0;
#endif
	}

	ScrollIfNeeded(TRUE);

	// Reset cursor blink
#if defined(_MACINTOSH_)
	if (HasSelectedText())
		caret_blink = 1;
	else
#endif
	caret_blink = 0;
#ifndef WIDGETS_DISABLE_CURSOR_BLINKING
	StopTimer();
	StartTimer(GetInfo()->GetCaretBlinkDelay());
#endif //!WIDGETS_DISABLE_CURSOR_BLINKING

	Invalidate(GetBounds());

	if (old_caret_pos != caret_pos)
		ReportCaretPosition();

	// If the user has changed the text, invoke the listener

	if (local_is_changed)
	{
		SetReceivedUserInput();
		if (!m_packed.onchange_on_enter)
	{
		BroadcastOnChange(FALSE, FALSE, TRUE);
			m_packed.is_changed = TRUE;
		}
	}
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	else
	{
		if (local_is_changed)
		{
			AccessibilitySendEvent(Accessibility::Event(Accessibility::kAccessibilityEventTextChanged));
		}
		else if ((old_caret_pos != caret_pos) ||
			(old_sel_start != string.sel_start) ||
			(old_sel_stop != string.sel_stop))
		{
			AccessibilitySendEvent(Accessibility::Event(Accessibility::kAccessibilityEventSelectedTextRangeChanged));
		}
	}
#endif

	// If user selected text, invoke the listener.
	if ((old_sel_start != string.sel_start || old_sel_stop != string.sel_stop) && string.sel_start != string.sel_stop && listener)
		listener->OnSelect(this);

	autocomp.EditAction(action, local_is_changed);
}


BOOL OpEdit::InvokeAction()
{
	if (GetAction())
	{
		OpInputContext * input_context = 0;

		if (GetAction()->GetAction() == OpInputAction::ACTION_SEARCH)
		{
			input_context = this;
		}
		else
		{
			input_context = GetParentInputContext();
			GetAction()->SetActionDataString(string.Get());
		}

#ifdef NOTIFY_ON_INVOKE_ACTION
		// Notify the listener that the action is about to be invoked
		if(listener)
			listener->OnInvokeAction(this, FALSE);
#endif // NOTIFY_ON_INVOKE_ACTION

		g_input_manager->InvokeAction(GetAction(), input_context);

		return TRUE;
	}
	else
	{
		BroadcastOnChange(FALSE, TRUE);
	}
	return FALSE;
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL OpEdit::OnInputAction(OpInputAction* action)
{
	if (is_selecting)
		return FALSE;

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_LOWLEVEL_KEY_PRESSED:
			m_packed.is_last_user_change_an_insert = TRUE;
			break;
		default:
			m_packed.is_last_user_change_an_insert = FALSE;
			break;
	}

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();
			OpInputAction::Action action_type = child_action->GetAction();
#ifdef USE_OP_CLIPBOARD
			BOOL force_enabling = FALSE;
			if (action_type == OpInputAction::ACTION_COPY ||
			    action_type == OpInputAction::ACTION_CUT ||
				action_type == OpInputAction::ACTION_PASTE)
				if (FormObject* form_obj = GetFormObject())
					force_enabling = g_clipboard_manager->ForceEnablingClipboardAction(action_type, form_obj->GetDocument(), form_obj->GetHTML_Element());
#endif // USE_OP_CLIPBOARD
			switch (action_type)
			{
#ifdef WIDGETS_UNDO_REDO_SUPPORT
				case OpInputAction::ACTION_UNDO:
					if (!IsUndoRedoEnabled())
						return FALSE;
					child_action->SetEnabled(!IsReadOnly() && (undo_stack.CanUndo() || HasReceivedUserInput()));
					return TRUE;

				case OpInputAction::ACTION_REDO:
					if (!IsUndoRedoEnabled())
						return FALSE;
					child_action->SetEnabled(!IsReadOnly() && (undo_stack.CanRedo() || HasReceivedUserInput()));
					return TRUE;
#endif // WIDGETS_UNDO_REDO_SUPPORT

				case OpInputAction::ACTION_DELETE:
					child_action->SetEnabled(!m_packed.is_readonly && GetTextLength() > 0);
					return TRUE;

				case OpInputAction::ACTION_CLEAR:
					child_action->SetEnabled((!m_packed.is_readonly || m_packed.force_allow_clear) && GetTextLength() > 0);
					return TRUE;
#ifdef USE_OP_CLIPBOARD

				case OpInputAction::ACTION_CUT:
					child_action->SetEnabled( force_enabling || (!m_packed.is_readonly && HasSelectedText() && !string.GetPasswordMode()) );
					return TRUE;

				case OpInputAction::ACTION_COPY:
					child_action->SetEnabled( force_enabling || (HasSelectedText() && !string.GetPasswordMode()) );
					return TRUE;

				case OpInputAction::ACTION_COPY_TO_NOTE:
					child_action->SetEnabled( HasSelectedText() && !string.GetPasswordMode() );
					return TRUE;

				case OpInputAction::ACTION_PASTE:
					child_action->SetEnabled( force_enabling || (!m_packed.is_readonly && g_clipboard_manager->HasText()) );
					return TRUE;
#endif // USE_OP_CLIPBOARD
#ifndef HAS_NOTEXTSELECTION
				case OpInputAction::ACTION_SELECT_ALL:
					child_action->SetEnabled(GetTextLength() > 0);
					return TRUE;
				case OpInputAction::ACTION_DESELECT_ALL:
					child_action->SetEnabled(HasSelectedText());
					return TRUE;
#endif // !HAS_NOTEXTSELECTION

				case OpInputAction::ACTION_INSERT:
					child_action->SetEnabled(!m_packed.is_readonly);
					return TRUE;
			}
			return FALSE;
		}

#if defined (QUICK)
		case OpInputAction::ACTION_SHOW_EDIT_DROPDOWN:
		{
			if (IsInternalChild())
				return FALSE;

			if (GetFormObject())
			{
				// don't stop dropdowns like Google Suggest from working
				HTML_Element* he = GetFormObject()->GetHTML_Element();
				if (he->GetAutocompleteOff())
					return FALSE;
			}

			OpInputAction new_action(OpInputAction::ACTION_SHOW_POPUP_MENU);
			new_action.SetActionDataString(UNI_L("Edit Dropdown Menu"));
			new_action.SetActionMethod(action->GetActionMethod());

			return g_input_manager->InvokeAction(&new_action, this);
		}

		case OpInputAction::ACTION_SHOW_POPUP_MENU:
		{
			if (action->IsKeyboardInvoked())
			{
				action->SetActionPosition(GetScreenRect());
			}
			return g_input_manager->InvokeAction(action, GetParentInputContext());
		}

		case OpInputAction::ACTION_SHOW_CONTEXT_MENU:
		{
			OpRect text_area = GetTextRect();
			OpPoint caret = GetCaretCoordinates();
			caret.y += text_area.height;

			OnContextMenu(caret, &text_area, action->IsKeyboardInvoked());
			return TRUE;
		}
		case OpInputAction::ACTION_SEARCH:
		{
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
			OpString text;
			GetText(text);

			if (text.IsEmpty())
				return FALSE;

			SearchTemplate search;
			SearchTemplate* search_pointer = &search;

			search.m_url.Set(action->GetActionDataString());
			search.m_query.Set(action->GetActionDataStringParameter());

			if (search.m_url.IsEmpty())
			{
				search_pointer = (SearchTemplate*) g_searchEngineManager->GetItemByPosition(action->GetActionData());
				if (search_pointer == NULL)
				{
					return FALSE;
				}
			}

			OpenURLSetting setting;

			if (!search_pointer->MakeSearchURL(setting.m_url, text.CStr(), FALSE))
				return FALSE;

			return g_application->OpenURL(setting);
#else
			return FALSE;
#endif // DESKTOP_UTIL_SEARCH_ENGINES
		}
#endif // QUICK


#ifdef USE_OP_CLIPBOARD
		case OpInputAction::ACTION_PASTE_AND_GO:
#ifdef ACTION_PASTE_AND_GO_BACKGROUND_ENABLED
		case OpInputAction::ACTION_PASTE_AND_GO_BACKGROUND:
#endif
# if defined(_X11_SELECTION_POLICY_)
			Clear();
# endif
			m_autocomplete_after_paste = FALSE;
			g_clipboard_manager->Paste(this);
			m_autocomplete_after_paste = TRUE;
			// use original action method for go action
			if (GetAction())
			{
				OpInputAction::ActionMethod method = GetAction()->GetActionMethod();
				OpInputAction::Action referrer_action = GetAction()->GetReferrerAction();
				GetAction()->SetActionMethod(action->GetActionMethod());
				// Set referrer so that handler knows this is a paste&go action. Triggers different behavior
				GetAction()->SetReferrerAction(action->GetAction());
				InvokeAction();
				GetAction()->SetActionMethod(method);
				GetAction()->SetReferrerAction(referrer_action);
				return TRUE;
			}
			// fall through
#endif // USE_OP_CLIPBOARD

		case OpInputAction::ACTION_GO:
		{
			InvokeAction();
			return TRUE;
		}
		case OpInputAction::ACTION_LOWLEVEL_KEY_PRESSED:
		{
			OpKey::Code key = action->GetActionKeyCode();
			ShiftKeyState modifiers = action->GetShiftKeys();
#ifdef OP_KEY_ENTER_ENABLED
			// special: in single line edit, pressing enter will invoke an action
			// specified by creator of this edit widget (SetEnterAction)

			if (key == OP_KEY_ENTER && modifiers == SHIFTKEY_NONE)
			{
				if (autocomp.EditAction(action, FALSE))
				{
					return TRUE;
				}
				else
				{
					if (InvokeAction())
						return TRUE;
				}
			}
#endif // OP_KEY_ENTER_ENABLED

			const uni_char *key_value = action->GetKeyValue();
			BOOL is_key_wanted;
			switch (key)
			{
#ifdef OP_KEY_TAB_ENABLED
			case OP_KEY_TAB:
				is_key_wanted = FALSE;
				break;
#endif // OP_KEY_TAB_ENABLED
			default:
#ifdef MSWIN
				/* A keypress involving CTRL or ALT is ignored, but not if it involves
				   both. Some key mappings use those modifier combinations
				   for dead/accented keys (e.g., Ctrl-Alt-Z on a Polish keyboard.) */
				ShiftKeyState ctrl_alt = modifiers & (SHIFTKEY_CTRL | SHIFTKEY_ALT);
				if (ctrl_alt == SHIFTKEY_ALT || ctrl_alt == SHIFTKEY_CTRL)
#else //MSWIN
				/* A keypress involving CTRL is ignored, but not if it involves
				   ALT (or META.) Some key mappings use those modifier combinations
				   for dead/accented keys (e.g., Ctrl-Alt-Z on a Polish keyboard.) */
				if ((modifiers & SHIFTKEY_CTRL) != 0 && (modifiers & (SHIFTKEY_ALT | SHIFTKEY_META)) == 0)
#endif //MSWIN
					is_key_wanted = FALSE;
				else
					is_key_wanted = key_value != NULL && key_value[0] >= 32 && key_value[0] != 0x7f;
			}

			if (is_key_wanted)
			{
				if (listener)
				{
					BOOL accept = TRUE;
#ifdef OP_KEY_SPACE_ENABLED
					if (!key_value && key == OP_KEY_SPACE)
					{
						accept = listener->OnCharacterEntered(UNI_L(" "));
					}
					else
#endif //  OP_KEY_SPACE_ENABLED
					{
						accept = key_value == NULL || listener->OnCharacterEntered(key_value);
					}

					if (!accept)
					{
						return FALSE;
					}
				}

				/* Who generates these input sequences (and how are they range constrained)? */
				if (alt_charcode > 255)
				{
					OP_ASSERT(alt_charcode < 0xffff);
					uni_char str[2]; /* ARRAY OK 2011-12-01 sof */
					str[0] = static_cast<uni_char>(alt_charcode);
					str[1] = 0;
					action->SetActionKeyValue(str);
				}
				alt_charcode = 0;

#ifdef IME_SEND_KEY_EVENTS
				if (m_fake_keys == 0)
					EditAction(action);
				else
					m_fake_keys--;
#else
				EditAction(action);
#endif // IME_SEND_KEY_EVENTS
				return TRUE;
			}
			return FALSE;
		}

#ifdef OP_KEY_ALT_ENABLED
		case OpInputAction::ACTION_LOWLEVEL_KEY_DOWN:
		{
			OpKey::Code key = action->GetActionKeyCode();
			if (key == OP_KEY_ALT)
				alt_charcode = 0;		// reset if previous value was stuck because Windoze didn't issue WM_CHAR
			else if ((action->GetShiftKeys() & SHIFTKEY_ALT) && key >= OP_KEY_0 && key <= OP_KEY_9)
				alt_charcode = alt_charcode * 10 + key - OP_KEY_0;

			return FALSE;
		}
#endif //  OP_KEY_ALT_ENABLED

#ifdef SUPPORT_TEXT_DIRECTION
		case OpInputAction::ACTION_CHANGE_DIRECTION_TO_LTR:
		case OpInputAction::ACTION_CHANGE_DIRECTION_TO_RTL:
			if (!IsInWebForm() || IsReadOnly())
				return FALSE;
			SetRTL(action->GetAction() == OpInputAction::ACTION_CHANGE_DIRECTION_TO_RTL);
			SetJustify(action->GetAction() == OpInputAction::ACTION_CHANGE_DIRECTION_TO_RTL ? JUSTIFY_RIGHT : JUSTIFY_LEFT, TRUE);
			string.NeedUpdate();
			return TRUE;
#endif // SUPPORT_TEXT_DIRECTION

		case OpInputAction::ACTION_TOGGLE_OVERSTRIKE:
		{
			string.ToggleOverstrike();
			return TRUE;
		}

		case OpInputAction::ACTION_GO_TO_LINE_START:
		case OpInputAction::ACTION_GO_TO_LINE_END:
		case OpInputAction::ACTION_NEXT_CHARACTER:
		case OpInputAction::ACTION_PREVIOUS_CHARACTER:
		case OpInputAction::ACTION_NEXT_WORD:
		case OpInputAction::ACTION_PREVIOUS_WORD:
#ifndef HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_RANGE_GO_TO_LINE_START:
		case OpInputAction::ACTION_RANGE_GO_TO_LINE_END:
		case OpInputAction::ACTION_RANGE_NEXT_CHARACTER:
		case OpInputAction::ACTION_RANGE_PREVIOUS_CHARACTER:
		case OpInputAction::ACTION_RANGE_NEXT_WORD:
		case OpInputAction::ACTION_RANGE_PREVIOUS_WORD:
#endif // !HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_CONVERT_HEX_TO_UNICODE:
		case OpInputAction::ACTION_DELETE:
		case OpInputAction::ACTION_DELETE_WORD:
		case OpInputAction::ACTION_DELETE_TO_END_OF_LINE:
		case OpInputAction::ACTION_BACKSPACE:
		case OpInputAction::ACTION_BACKSPACE_WORD:
		{
			int old_caret_pos = caret_pos;

			EditAction(action);

			switch (action->GetAction())
			{
				case OpInputAction::ACTION_GO_TO_LINE_START:
				case OpInputAction::ACTION_GO_TO_LINE_END:
					// We need to return FALSE so that the navigation keys work even if you are in an edit field
#ifndef WIDGETS_UP_DOWN_MOVES_TO_START_END
					if (!IsForm())
#endif
					{
						if (
#ifdef WIDGETS_UP_DOWN_MOVES_TO_START_END
							(m_packed.ignore_go_to_line_start && action->GetAction() == OpInputAction::ACTION_GO_TO_LINE_START) ||
#endif // WIDGETS_UP_DOWN_MOVES_TO_START_END
							old_caret_pos == caret_pos)
							return FALSE;
					}
				default:
					break;
			}

#if defined(_X11_SELECTION_POLICY_) && defined(USE_OP_CLIPBOARD)
			if( action->IsRangeAction() )
			{
				if( HasSelectedText() )
				{
					g_clipboard_manager->SetMouseSelectionMode(TRUE);
					Copy();
					g_clipboard_manager->SetMouseSelectionMode(FALSE);
				}
			}
#endif // _X11_SELECTION_POLICY_ && USE_OP_CLIPBOARD

			return TRUE;
		}

		case OpInputAction::ACTION_PAGE_UP:
		case OpInputAction::ACTION_PAGE_DOWN:
			if( autocomp.window )
			{
				autocomp.EditAction(action, FALSE);
				return TRUE;
			}
			return FALSE;

		case OpInputAction::ACTION_NEXT_LINE:
		case OpInputAction::ACTION_PREVIOUS_LINE:
			if (autocomp.window)
			{
				autocomp.EditAction(action, FALSE);
				return TRUE;
			}
			if (IsForm())
			{
				FormObject* form_obj = GetFormObject();
				if (form_obj && form_obj->GetHTML_Element()->GetAutocompleteOff())
				{
					return TRUE; // We consume this action, since we don't want it to be misinterpreted as a page scroll or something else irrelevant. Applications use up/down arrows in text fields to navigate in their own suggestion lists. Maybe only eat the event if autocomplete=off?
				}
			}

			return FALSE; // A UI using OpEdit may want to handle this someway. See bug 219267

		case OpInputAction::ACTION_CLOSE_DROPDOWN:
			if (autocomp.window)
			{
				autocomp.ClosePopup();
				return TRUE;
			}
			return FALSE;

#ifdef WIDGETS_UNDO_REDO_SUPPORT
		case OpInputAction::ACTION_UNDO:
			if (!IsUndoRedoEnabled() ||
				IsReadOnly() ||
				(!undo_stack.CanUndo() && !HasReceivedUserInput()))
				return FALSE;
			Undo(!!action->GetActionData());
			return TRUE;

		case OpInputAction::ACTION_REDO:
			if (!IsUndoRedoEnabled() ||
				IsReadOnly() ||
				(!undo_stack.CanRedo() && !HasReceivedUserInput()))
				return FALSE;
			Redo();
			return TRUE;
#endif // WIDGETS_UNDO_REDO_SUPPORT
#ifdef USE_OP_CLIPBOARD
		case OpInputAction::ACTION_CUT:
			Cut();
			return TRUE;

		case OpInputAction::ACTION_COPY:
			Copy();
			return TRUE;

		case OpInputAction::ACTION_COPY_TO_NOTE:
			Copy(TRUE);
			return TRUE;

		case OpInputAction::ACTION_PASTE:
			g_clipboard_manager->Paste(this, GetFormObject() ? GetFormObject()->GetDocument() : NULL, GetFormObject() ? GetFormObject()->GetHTML_Element() : NULL);
			return TRUE;

		case OpInputAction::ACTION_PASTE_MOUSE_SELECTION:
		{
# ifdef _X11_SELECTION_POLICY_
			g_clipboard_manager->SetMouseSelectionMode(TRUE);
# endif // _X11_SELECTION_POLICY_
			if( g_clipboard_manager->HasText() )
			{
				if( action->GetActionData() == 1 )
				{
					Clear();
				}
				g_clipboard_manager->Paste(this, GetFormObject() ? GetFormObject()->GetDocument() : NULL, GetFormObject() ? GetFormObject()->GetHTML_Element() : NULL);
			}
# ifdef _X11_SELECTION_POLICY_
			g_clipboard_manager->SetMouseSelectionMode(FALSE);
# endif // _X11_SELECTION_POLICY_
			return TRUE;
		}
#endif // USE_OP_CLIPBOARD
#ifndef HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_SELECT_ALL:
			SelectAll();
			return TRUE;

		case OpInputAction::ACTION_DESELECT_ALL:
		{
			if (HasSelectedText())
			{
				SelectNothing();
				return TRUE;
			}
			return FALSE;
		}
#endif // !HAS_NOTEXTSELECTION

		case OpInputAction::ACTION_CLEAR:
			Clear();
			return TRUE;

		case OpInputAction::ACTION_INSERT:
			if (!IsReadOnly())
				InsertText(action->GetActionDataString());
			return TRUE;

		case OpInputAction::ACTION_LEFT_ADJUST_TEXT:
			if( IsForm() )
			{
				SetJustify(JUSTIFY_LEFT, TRUE);
				return TRUE;
			}
			else
				return FALSE;

		case OpInputAction::ACTION_RIGHT_ADJUST_TEXT:
			if( IsForm() )
			{
				SetJustify(JUSTIFY_RIGHT, TRUE);
				return TRUE;
			}
			else
				return FALSE;

#ifdef _SPAT_NAV_SUPPORT_
		case OpInputAction::ACTION_UNFOCUS_FORM:
			return g_input_manager->InvokeAction(action, GetParentInputContext());
#endif // _SPAT_NAV_SUPPORT_

#ifdef WAND_SUPPORT
		case OpInputAction::ACTION_WAND:
			if (IsInWebForm())
				return g_input_manager->InvokeAction(action, GetParentInputContext());
#endif // WAND_SUPPORT
	}

	return FALSE;
}

#ifdef WIDGETS_UNDO_REDO_SUPPORT
void OpEdit::Undo(BOOL ime_undo)
{
	OP_ASSERT(IsUndoRedoEnabled());
	OP_STATUS status;

	if (!m_packed.is_readonly && undo_stack.CanUndo())
	{
		OpString tmp;
		status = tmp.Set(string.Get());
		REPORT_AND_RETURN_IF_ERROR(status);
		UndoRedoEvent* event = undo_stack.Undo();
		UndoRedoEvent* previous_event = undo_stack.PeekUndo();

		OP_ASSERT(event->GetType() != UndoRedoEvent::EV_TYPE_REPLACE);

		if (previous_event != NULL && previous_event->GetType() == UndoRedoEvent::EV_TYPE_REPLACE)
		{
			OP_ASSERT(event->GetType() == UndoRedoEvent::EV_TYPE_INSERT);

			//need to undo the previous_event as well
			event = undo_stack.Undo();

			REPORT_AND_RETURN_IF_ERROR(tmp.Set(previous_event->str, previous_event->str_length));
		}
		else if (event->GetType() == UndoRedoEvent::EV_TYPE_INSERT)
		{
			tmp.Delete(event->caret_pos, event->str_length);
		}
#ifdef WIDGETS_IME_SUPPORT
		else if (event->GetType() == UndoRedoEvent::EV_TYPE_EMPTY)
		{
			// nothing
		}
#endif // WIDGETS_IME_SUPPORT
		else// if (event->GetType() == UndoRedoEvent::EV_TYPE_REMOVE)
		{
			int s1 = event->sel_start < event->sel_stop ? event->sel_start : event->sel_stop;
//			int s2 = event->sel_start > event->sel_stop ? event->sel_start : event->sel_stop;
			status = tmp.Insert(s1, event->str, event->str_length);
			REPORT_AND_RETURN_IF_ERROR(status);
		}

		string.sel_start = event->sel_start;
		string.sel_stop = event->sel_stop;

		status = string.Set(tmp.CStr(), this);
		SetCaretOffset(event->caret_pos, FALSE);
		REPORT_AND_RETURN_IF_ERROR(status);

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	if(m_spellchecker)
		m_spellchecker->SpellcheckUpdate();
#endif // INTERNAL_SPELLCHECK_SUPPORT

		ScrollIfNeeded();

		if (!ime_undo)
			BroadcastOnChange();
	}
	Invalidate(GetBounds());
}

void OpEdit::Redo()
{
	OP_ASSERT(IsUndoRedoEnabled());
	OP_STATUS status;

	if (!m_packed.is_readonly && undo_stack.CanRedo())
	{
		OpString tmp;
		status = tmp.Set(string.Get());
		REPORT_AND_RETURN_IF_ERROR(status);

		UndoRedoEvent* event = undo_stack.Redo();
		INT32 new_caret_pos;

		if (event->GetType() == UndoRedoEvent::EV_TYPE_REPLACE)
		{
			//replace all text
			OP_ASSERT(undo_stack.PeekRedo() && undo_stack.PeekRedo()->GetType() == UndoRedoEvent::EV_TYPE_INSERT);

			//need to redo the next_event as well
			event = undo_stack.Redo();

			REPORT_AND_RETURN_IF_ERROR(tmp.Set(event->str, event->str_length));
			new_caret_pos = event->str_length;
		}
		else if (event->GetType() == UndoRedoEvent::EV_TYPE_INSERT)
		{
			//insert the text diff at the caret position
			status = tmp.Insert(event->caret_pos, event->str, event->str_length);
			REPORT_AND_RETURN_IF_ERROR(status);
			new_caret_pos = event->caret_pos + event->str_length;
		}
		else// if (event->GetType() == UndoRedoEvent::EV_TYPE_REMOVE)
		{
			//remove text diff again
			tmp.Delete(event->sel_start, event->sel_stop - event->sel_start);
			new_caret_pos = event->sel_start;
		}
		string.SelectNone();

		status = string.Set(tmp.CStr(), this);
		SetCaretOffset(new_caret_pos, FALSE);
		REPORT_AND_RETURN_IF_ERROR(status);

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	if(m_spellchecker)
		m_spellchecker->SpellcheckUpdate();
#endif // INTERNAL_SPELLCHECK_SUPPORT

		ScrollIfNeeded();

		BroadcastOnChange();
	}
	Invalidate(GetBounds());
}
#endif // WIDGETS_UNDO_REDO_SUPPORT

void OpEdit::Cut()
{
#ifdef USE_OP_CLIPBOARD
	FormObject* fo = GetFormObject();
	OP_STATUS status = g_clipboard_manager->Cut(this, fo ? fo->GetDocument()->GetWindow()->GetUrlContextId() : 0, fo ? fo->GetDocument() : NULL, fo ? fo->GetHTML_Element() : NULL);
	REPORT_AND_RETURN_IF_ERROR(status);
#endif // USE_OP_CLIPBOARD
}

void OpEdit::Copy(BOOL to_note)
{
#ifdef QUICK
	if (to_note)
	{
		OP_STATUS status;

		if (!string.GetPasswordMode())
		{
			int s1, s2;
			OpWidget::GetSelection(s1, s2);
			if (s2)
			{
				OpString str;
				status = str.Set(&string.Get()[s1], s2 - s1);
				REPORT_AND_RETURN_IF_ERROR(status);

				// less code here.. redirect to static helper function ASAP
				NotesPanel::NewNote(str.CStr());
			}
		}
	}
	else
#endif
	{
#ifdef USE_OP_CLIPBOARD
		FormObject* fo = GetFormObject();
		OP_STATUS status = g_clipboard_manager->Copy(this, fo ? fo->GetDocument()->GetWindow()->GetUrlContextId() : 0, fo ? fo->GetDocument() : NULL, fo ? fo->GetHTML_Element() : NULL);
		REPORT_AND_RETURN_IF_ERROR(status);
#endif // USE_OP_CLIPBOARD
	}
}

#ifdef USE_OP_CLIPBOARD
void OpEdit::OnCut(OpClipboard* clipboard)
{
	if (m_packed.is_readonly || string.GetPasswordMode())
		return;

	OP_STATUS status;

	int s1, s2;
	OpWidget::GetSelection(s1, s2);
	if (s2)
	{
		OpString str;
		status = str.Set(string.Get());
		REPORT_AND_RETURN_IF_ERROR(status);

		OpString selected;
		status = selected.Set(str.CStr() + s1, s2 - s1);
		REPORT_AND_RETURN_IF_ERROR(status);

		FormObject* fo = GetFormObject();
		status = clipboard->PlaceText(selected.CStr(), fo ? fo->GetDocument()->GetWindow()->GetUrlContextId() : 0);
		REPORT_AND_RETURN_IF_ERROR(status);

#ifdef WIDGETS_UNDO_REDO_SUPPORT
		if (IsUndoRedoEnabled())
		{
			status = undo_stack.SubmitRemove(caret_pos, s1, s2, &str.CStr()[s1]);
			REPORT_AND_RETURN_IF_ERROR(status);
		}
#endif // WIDGETS_UNDO_REDO_SUPPORT

		str.Delete(s1, s2 - s1);
		SetCaretOffset(s1, FALSE);
		string.SelectNone();
		status = string.Set(str.CStr(), this);
		if (OpStatus::IsError(status))
			SetCaretOffset(0);
		REPORT_AND_RETURN_IF_ERROR(status);

#ifdef INTERNAL_SPELLCHECK_SUPPORT
		if(m_spellchecker)
			m_spellchecker->SpellcheckUpdate();
#endif // INTERNAL_SPELLCHECK_SUPPORT

		m_packed.is_changed = TRUE;
		Invalidate(GetBounds());

		BroadcastOnChange();

		OpInputAction action(OpInputAction::ACTION_CUT);
		autocomp.EditAction(&action, TRUE);
	}
}

void OpEdit::OnCopy(OpClipboard* clipboard)
{
	OP_STATUS status;

	if (!string.GetPasswordMode())
	{
		int s1, s2;
		OpWidget::GetSelection(s1, s2);
		if (s2)
		{
			OpString str;
			status = str.Set(&string.Get()[s1], s2 - s1);
			REPORT_AND_RETURN_IF_ERROR(status);
			FormObject* fo = GetFormObject();
			status = clipboard->PlaceText(str.CStr(), fo ? fo->GetDocument()->GetWindow()->GetUrlContextId() : 0);
			REPORT_AND_RETURN_IF_ERROR(status);
		}
	}
}

void OpEdit::OnPaste(OpClipboard* clipboard)
{
	if(!m_packed.is_readonly && clipboard->HasText())
	{
		OpString text;
		OP_STATUS status = clipboard->GetText(text);
		REPORT_AND_RETURN_IF_ERROR(status);

		// Remove newlines. This is mostly to allow pasting
		// urls that span over multiple lines into the address field.
		if( text.HasContent() )
		{
			uni_char* buffer = text.CStr();

			int length = text.Length();
			for( int i=0; i<length; i++ )
			{
				if( buffer[i] == '\n' || buffer[i] == '\r' )
				{
					buffer[i] = ' ';
				}
			}
		}

		InternalInsertText(text.CStr());
		ScrollIfNeeded(TRUE);
		Invalidate(GetBounds());
		SetReceivedUserInput();

		if (!m_packed.onchange_on_enter)
			BroadcastOnChange();

		if (m_autocomplete_after_paste)
		{
			OpInputAction action(OpInputAction::ACTION_PASTE);
			autocomp.EditAction(&action, TRUE);
		}
	}
}
#endif // USE_OP_CLIPBOARD

void OpEdit::SelectAll()
{
	SelectText();
	if (listener)
		listener->OnSelect(this);

#if defined(_X11_SELECTION_POLICY_) && defined(USE_OP_CLIPBOARD)
	if( HasSelectedText() )
	{
		g_clipboard_manager->SetMouseSelectionMode(TRUE);
		Copy();
		g_clipboard_manager->SetMouseSelectionMode(FALSE);
	}
#endif // _X11_SELECTION_POLICY_ && USE_OP_CLIPBOARD

}


void OpEdit::Clear()
{
	if (m_packed.is_readonly && !m_packed.force_allow_clear)
		return;

#ifdef WIDGETS_UNDO_REDO_SUPPORT
	int s1 = 0;
	int s2 = GetTextLength();
	if( s1 != s2 )
	{
		if (IsUndoRedoEnabled())
		{
			OpString str;
			OP_STATUS status = str.Set(string.Get());
			status = undo_stack.SubmitRemove(caret_pos, s1, s2, &str.CStr()[s1]);
			REPORT_AND_RETURN_IF_ERROR(status);
		}
	}
#endif // WIDGETS_UNDO_REDO_SUPPORT

	SetCaretOffset(0, FALSE);
	string.SelectNone();
	OP_STATUS status = string.Set(0, this);
	Invalidate(GetBounds());

	if (OpStatus::IsError(status))
		ReportOOM();

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	if(m_spellchecker)
		m_spellchecker->SpellcheckUpdate();
#endif // INTERNAL_SPELLCHECK_SUPPORT

	if (autocomp.window)
	{
		autocomp.ClosePopup();
	}
	ScrollIfNeeded();

	if (!m_packed.onchange_on_enter)
	{
		BroadcastOnChange();
	}
}

void OpEdit::PaintContent(OpRect inner_rect, UINT32 fcol, BOOL draw_ghost)
{
	VisualDevice * vd = GetVisualDevice();

	int indent = GetTextIndentation();
	inner_rect.x += indent;
	inner_rect.width -= indent;

	int tmp_caret_pos = caret_pos;
	if( !m_packed.show_drag_caret )
	{
		if (caret_blink || !IsFocused() || m_packed.is_readonly
#ifdef WIDGETS_MOVE_CARET_TO_SELECTION_STARTSTOP
		    || HasSelectedText()
#endif
		   )
			tmp_caret_pos = -1;
	}

#ifdef WIDGETS_IME_KEEPS_FOCUS_ON_COMMIT
	// Never show caret if widgets keep focus after IME commit
	tmp_caret_pos = -1;
#endif

	OpRect r = inner_rect;
	AddPadding(r);

	// Clip and Draw the text
	SetClipRect(inner_rect);

	OpWidgetStringDrawer widget_string_drawer;
	widget_string_drawer.SetCaretPos(tmp_caret_pos);
	widget_string_drawer.SetEllipsisPosition(GetEllipsis());
	widget_string_drawer.SetXScroll(x_scroll);
	widget_string_drawer.SetCaretSnapForward(m_packed.caret_snap_forward);
	widget_string_drawer.SetSelectionHighlightType(m_selection_highlight_type);

	if (draw_ghost)
	{
		if (m_packed.show_ghost_when_focused && IsFocused())
		{
			// Draw the caret. The string itself is empty.
			widget_string_drawer.Draw(&string, r, vd, fcol);

			if (m_ghost_text_onfocus_justify == JUSTIFY_RIGHT)
				r.x += r.width - ghost_string.GetWidth(); // Ghost string to the right
			else
				 r.x += g_op_ui_info->GetCaretWidth(); // Ghost string to the left displaced by caret's width pixel

			widget_string_drawer.SetCaretPos(-1);
			widget_string_drawer.Draw(&ghost_string, r, vd, fcol);

		}
		else
		{
			r.x += g_op_ui_info->GetCaretWidth(); // Ghost string to the left displaced by caret's width pixel
			widget_string_drawer.Draw(&ghost_string, r, vd, fcol);
		}
	}
	else
	{
		widget_string_drawer.Draw(&string, r, vd, fcol);
	}

	RemoveClipRect();
}

void OpEdit::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	if (autocomp.window)
		autocomp.UpdatePopupLookAndPlacement(TRUE);

	widget_painter->DrawEdit(GetBounds());
}

void OpEdit::OnTimer()
{
#if defined(SUPPORT_IME_PASSWORD_INPUT) && defined(WIDGETS_IME_SUPPORT)
    if (hidePasswordCounter)
    {
        hidePasswordCounter--;
        if (hidePasswordCounter==0)
		{
            string.HidePasswordChar();
			Invalidate(GetBounds());
		}
    }
#endif
#if defined(_MACINTOSH_) || defined(WIDGETS_MOVE_CARET_TO_SELECTION_STARTSTOP)
	if (!HasSelectedText())
#endif
	{
		caret_blink = !caret_blink;
		InvalidateCaret();
	}
}

void OpEdit::InvalidateCaret()
{
	OpRect caret = GetTextRect();
	caret.x = string.GetCaretX(caret, caret_pos, m_packed.caret_snap_forward) - x_scroll;
	caret.y = 0;
	caret.height = GetHeight();
	caret.width = MAX(3, g_op_ui_info->GetCaretWidth());
	Invalidate(caret);
}

#ifdef WIDGETS_IME_SUPPORT
BOOL OpEdit::IMEHandleFocusEventInt(BOOL focus, FOCUS_REASON reason)
{
# if defined(SUPPORT_IME_PASSWORD_INPUT)
    if (string.GetPasswordMode() == OpView::IME_MODE_PASSWORD)
    {
        string.HidePasswordChar();
    }
    hidePasswordCounter = 0;
# endif //defined(SUPPORT_IME_PASSWORD_INPUT)

# ifdef _MACINTOSH_
	// this is to draw the focusborder
	Invalidate(GetBounds().InsetBy(-2,-2), FALSE, TRUE);
# endif

# ifdef WIDGETS_IME_KEEPS_FOCUS_ON_COMMIT
	if (!(focus && (reason == FOCUS_REASON_ACTIVATE || reason == FOCUS_REASON_RESTORE)))
# endif // WIDGETS_IME_KEEPS_FOCUS_ON_COMMIT
	{
		OpView::IME_MODE mode = OpView::IME_MODE_UNKNOWN;
		if (focus && !m_packed.is_readonly)
			mode = string.GetPasswordMode() ? OpView::IME_MODE_PASSWORD : OpView::IME_MODE_TEXT;
		if (!SpawnIME(vis_dev, im_style.CStr(), mode, GetIMEContext(GetFormObject())))
			return FALSE;
	}

	return TRUE;
}
#endif // WIDGETS_IME_SUPPORT

void OpEdit::OnFocus(BOOL focus, FOCUS_REASON reason)
{
#ifdef WIDGETS_IME_SUPPORT
# ifdef WIDGETS_IME_KEEPS_FOCUS_ON_COMMIT
	if (focus && reason == FOCUS_REASON_ACTIVATE)
	{
		return;
	}
# endif // WIDGETS_IME_KEEPS_FOCUS_ON_COMMIT

	m_suppress_ime = FALSE;

	if (focus)
	{
		if (FormObject* fo = GetFormObject())
		{
			if (reason != FOCUS_REASON_ACTIVATE && fo->GetHTML_Element()->HasEventHandler(fo->GetDocument(), ONFOCUS))
			{
				m_suppress_ime = TRUE;
				m_suppressed_ime_reason = reason;
			}

			if (!fo->IsDisplayed())
				SetRect(OpRect(0, 0, GetWidth(), GetHeight()), FALSE);
		}
	}

	if (!m_suppress_ime && !m_suppress_ime_mouse_down)
		if (!IMEHandleFocusEventInt(focus, reason))
			return;

	m_suppress_ime_mouse_down = FALSE;
#endif // WIDGETS_IME_SUPPORT

	if (focus)
	{
		caret_blink = 0;
#ifndef WIDGETS_DISABLE_CURSOR_BLINKING
		StopTimer();
		StartTimer(GetInfo()->GetCaretBlinkDelay());
#endif //!WIDGETS_DISABLE_CURSOR_BLINKING
		if (!(IsInWebForm() && reason == FOCUS_REASON_OTHER)) // Selection must NOT be changed if focus_reason is OTHER. (Some scripts goes crazy.)
		{
#if defined(_X11_SELECTION_POLICY_)
			BOOL selected = FALSE;

			// X11 behavior: A click on a widget that results in a focus
			// in shall not select text, a shortcut shall (and copy text to
			// the clipboard while tabbing shall select but not copy to clipboard
			// [espen 2003-03-13]
			if( reason == FOCUS_REASON_ACTION )
			{
				SelectText();
				ScrollIfNeeded(TRUE);
				selected = TRUE;
				is_selecting = 0;
			}
			else if( reason == FOCUS_REASON_KEYBOARD ) // Tabbing
			{
				SelectText();
				ScrollIfNeeded(TRUE);
				selected = TRUE;
				is_selecting = 0;
			}
			else if( reason == FOCUS_REASON_MOUSE )
			{
				// Originally 'is_selecting' was set to 1 here, but eventually
				// that triggerd bug #227078. We now depend on that the flag gets
				// set in OnMouseDown() [espen 2007-01-05]
				//is_selecting = 1;
			}
			else
			{
				is_selecting = 0;
			}

			if (reason != FOCUS_REASON_ACTIVATE || !m_packed.has_been_focused)
			{
				// We can not let the mouse select all text on focus-in. Breaks mouse selections
				if( reason != FOCUS_REASON_MOUSE && !selected )
				{
					SelectText();
					selected = TRUE;

					if( reason == FOCUS_REASON_KEYBOARD ) // Tabbing
					{
						ScrollIfNeeded(TRUE);
					}
				}
			}

			if (selected && listener)
				listener->OnSelect(this);
#else
			if (reason != FOCUS_REASON_RESTORE && (reason != FOCUS_REASON_ACTIVATE || !m_packed.has_been_focused))
			{
				if (!(IsForm() && reason == FOCUS_REASON_MOUSE))
				{
#ifdef PAN_SUPPORT
					// don't select all text if widget looses focus due to panning
					if (!(reason == FOCUS_REASON_RELEASE && vis_dev->GetPanState() != NO))
#endif // PAN_SUPPORT
					SelectAll();
					if (reason == FOCUS_REASON_MOUSE)
						m_packed.this_click_selected_all = TRUE;
				}
				if( reason == FOCUS_REASON_KEYBOARD ) // Tabbing
				{
					ScrollIfNeeded(TRUE);
				}
			}
			is_selecting = 0;
#endif
		}
		if (IsInWebForm())
		{
			// Can not open popup menu with a focus in using the mouse. It will
			// prevent proper operation because X will only send events to the
			// popup window. Firefox opens the dropdown window on mouse release
			// when the window had focus before the mouse press.
#if defined(_X11_SELECTION_POLICY_)
			if( reason != FOCUS_REASON_MOUSE )
#endif
			{
				autocomp.OpenManually();
			}
		}
#ifdef INTERNAL_SPELLCHECK_SUPPORT
		m_packed.is_changed = FALSE;
		if (!m_packed.has_been_focused && m_packed.enable_spellcheck_later && CanUseSpellcheck())
		{
			EnableSpellcheck();
		}
#endif // INTERNAL_SPELLCHECK_SUPPORT

		m_packed.has_been_focused = TRUE;
		/* Record the contents; should it have changed upon losing focus
		   the listener's OnChangeWhenLostFocus is invoked (and then only.) */
		if (FormObject* fo = GetFormObject())
			if (FramesDocument *frames_doc = fo->GetDocument())
			{
				OpString &text = frames_doc->GetFocusInputText();
				if (OpStatus::IsError(GetText(text)))
					text.Empty();
			}

		ReportCaretPosition();
	}
	else
	{
		is_selecting = 0;
#ifndef WIDGETS_DISABLE_CURSOR_BLINKING
		StopTimer();
#endif //!WIDGETS_DISABLE_CURSOR_BLINKING
		caret_blink = 1;
		// Selection must NOT be changed on forms if focus_reason is OTHER or mouse. (Some scripts goes crazy.)
		if (!(IsForm() && (reason == FOCUS_REASON_OTHER || reason == FOCUS_REASON_MOUSE)))
		{
			if( reason != FOCUS_REASON_RELEASE )
			{
				SelectNothing();
			}
		}

#ifdef WIDGETS_AUTOCOMPLETION_GOOGLE
		autocomp.StopGoogle();
#endif
		autocomp.ClosePopup();

		// If the user has changed the text, invoke the listener
		BOOL changed = m_packed.is_changed;
		if (FormObject* fo = GetFormObject())
			if (FramesDocument *frames_doc = fo->GetDocument())
			{
				OpString &focus_text = frames_doc->GetFocusInputText();
				OpString text;
				if (changed && OpStatus::IsSuccess(GetText(text)))
					changed = text.Compare(focus_text) != 0;

				focus_text.Empty();
			}

		if (changed && listener)
			listener->OnChangeWhenLostFocus(this); // Invoke!

		// don't have any selection when hovering if we don't have focus
		m_packed.this_click_selected_all = FALSE;

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	if(m_spellchecker && m_packed.delayed_spell && m_spellchecker->GetSpellCheckerSession())
	{
		m_packed.delayed_spell = FALSE;
		m_spellchecker->SpellcheckUpdate();
	}
#endif // INTERNAL_SPELLCHECK_SUPPORT

	}
}

#ifndef MOUSELESS

// ======================================

void OpEdit::OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
#ifdef DISPLAY_NO_MULTICLICK_LOOP
	if (m_packed.dblclick_selects_all)
		nclicks = nclicks <= 2 ? nclicks : 2;
	else
		nclicks = nclicks <= 3 ? nclicks : 3;
#else
	if (m_packed.dblclick_selects_all)
		nclicks = (nclicks-1)%2 + 1;
	else
		nclicks = (nclicks-1)%3 + 1;
#endif

#if defined(WIDGETS_IME_SUPPORT) && defined(WIDGETS_IME_SUPPRESS_ON_MOUSE_DOWN)
	SuppressIMEOnMouseDown();
#endif

	if (listener)
	{
#ifndef MOUSELESS
		if (listener->OnMouseEventConsumable(this, -1, point.x, point.y, button, TRUE, nclicks))
			return;
		listener->OnMouseEvent(this, -1, point.x, point.y, button, TRUE, nclicks);
#endif // MOUSELESS
	}

#if defined(SUPPORT_IME_PASSWORD_INPUT) && defined(WIDGETS_IME_SUPPORT)
    if (string.GetPasswordMode() == OpView::IME_MODE_PASSWORD)
    {
        string.HidePasswordChar();
    }
    hidePasswordCounter=0;
#endif

	if (IsDead())
		return;

	m_packed.mousedown_focus_status = !!IsFocused();

	caret_blink = 0;
#ifndef WIDGETS_DISABLE_CURSOR_BLINKING
	StopTimer();
	StartTimer(GetInfo()->GetCaretBlinkDelay());
#endif //!WIDGETS_DISABLE_CURSOR_BLINKING

#if defined(_X11_SELECTION_POLICY_) && defined(USE_OP_CLIPBOARD)
	if( button == MOUSE_BUTTON_3)
	{
		// Paste into the mouse position
		OpRect r = GetTextRect();
		caret_pos=string.GetCaretPos(r, OpPoint(point.x + x_scroll, point.y));

		// We want to paste from the mouse selection buffer but if that
		// is not possible we will fallaback to the regular clipboard in
		// order to be compatible with program not using the mouse selection
		// properly [espen 2003-01-21]

		g_clipboard_manager->SetMouseSelectionMode(TRUE);
		if( g_clipboard_manager->HasText() )
		{
			SelectNothing(); // So that we can copy selected text inside the widget
			g_clipboard_manager->Paste(this, GetFormObject() ? GetFormObject()->GetDocument() : NULL, GetFormObject() ? GetFormObject()->GetHTML_Element() : NULL);
			g_clipboard_manager->SetMouseSelectionMode(FALSE);
		}
		else
		{
			g_clipboard_manager->SetMouseSelectionMode(FALSE);
			if( g_clipboard_manager->HasText() )
			{
				SelectNothing();
				g_clipboard_manager->Paste(this, GetFormObject() ? GetFormObject()->GetDocument() : NULL, GetFormObject() ? GetFormObject()->GetHTML_Element() : NULL);
			}
		}
		SetFocus(FOCUS_REASON_MOUSE);
		is_selecting = 0;
		return;
	}
#endif

	if (button == MOUSE_BUTTON_1)
	{
		OpRect r = GetTextRect();
		BOOL shift = (vis_dev->view->GetShiftKeys() & SHIFTKEY_SHIFT) != 0;
		BOOL snap_forward = FALSE;
		selecting_start = string.GetCaretPos(r, OpPoint(point.x + x_scroll, point.y), &snap_forward);

		if (nclicks == 3)
		{
			if (m_packed.dblclick_selects_all)
			{
				SelectNothing();
				is_selecting = 0;
			}
			else
			{
				SelectAll();
				is_selecting = 3;
			}
		}
		else if (nclicks == 2 && m_packed.dblclick_selects_all)
		{
			SelectText();
			is_selecting = 3;
		}
		else if (nclicks == 2) // Select a word
		{
			is_selecting = 2;
			const uni_char* str = string.Get();

			string.sel_start = PrevCharRegion(str, selecting_start, TRUE);
			string.sel_stop  = NextCharRegion(str, selecting_start);

			SetCaretOffset(string.sel_stop, FALSE);
		}
		else
		{
			m_packed.caret_snap_forward = !!snap_forward;
			if (shift) // Select from current caretpos to new caretpos
			{
				if (!HasSelectedText())
				{
					string.sel_start = caret_pos;
					string.sel_stop = selecting_start;
				}
				else if (string.sel_start == caret_pos)
					string.sel_start = selecting_start;
				else
					string.sel_stop = selecting_start;

				SetCaretOffset(selecting_start, FALSE);

				if (string.sel_start == string.sel_stop)
					SelectNothing();
			}
			else if (!IsDraggable(point) )
			{
				is_selecting = 1;
				SetCaretOffset(selecting_start, FALSE);
				string.sel_start = caret_pos;
				string.sel_stop = caret_pos;
			}
		}

		Invalidate(GetBounds());
	}

	ReportCaretPosition();

	if (!IsInWebForm())
	{
		SetFocus(FOCUS_REASON_MOUSE);
	}

#ifdef PAN_SUPPORT
	if (vis_dev->GetPanState() != NO)
		is_selecting = 0;
#elif defined GRAB_AND_SCROLL
	if (MouseListener::GetGrabScrollState() != NO)
		is_selecting = 0;
#endif
}

void OpEdit::OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	if (listener)
	{
#ifndef MOUSELESS
		if (listener->OnMouseEventConsumable(this, -1, point.x, point.y, button, FALSE, nclicks))
			return;
		listener->OnMouseEvent(this, -1, point.x, point.y, button, FALSE, nclicks);
#endif // MOUSELESS
	}

	if (button == MOUSE_BUTTON_1)
	{
		BOOL shift = (vis_dev->view->GetShiftKeys() & SHIFTKEY_SHIFT) != 0;
#if defined(_X11_SELECTION_POLICY_) && defined(USE_OP_CLIPBOARD)
		if( is_selecting && HasSelectedText() )
		{
			g_clipboard_manager->SetMouseSelectionMode(TRUE);
			Copy();
			g_clipboard_manager->SetMouseSelectionMode(FALSE);
		}
#endif

		if (is_selecting && listener && HasSelectedText())
		{
			listener->OnSelect(this);
			m_packed.determine_select_direction = TRUE;
		}
		else if (!is_selecting && HasSelectedText() && !shift && m_packed.mousedown_focus_status
#ifdef DRAG_SUPPORT
			&& !g_drag_manager->IsDragging() && GetClickableBounds().Contains(point)
#endif // DRAG_SUPPORT
			)
		{
			OpRect r = GetTextRect();
			BOOL caret_snap_forward_temp;
			SetCaretOffset(string.GetCaretPos(r, OpPoint(point.x + x_scroll, point.y), &caret_snap_forward_temp), FALSE);
			m_packed.caret_snap_forward = !!caret_snap_forward_temp;
			SelectNothing();
		}

		is_selecting = 0;

		if (GetBounds().Contains(point) && listener && !IsDead())
			listener->OnClick(this); // Invoke!
	}

#if defined(_X11_SELECTION_POLICY_)
	// Fix for bug #203902, but I think all platforms can use this.
	is_selecting = 0;
#endif

	m_packed.this_click_selected_all = FALSE;

#if defined(WIDGETS_IME_SUPPORT) && defined(WIDGETS_IME_KEEPS_FOCUS_ON_COMMIT)
	if ((NULL == imstring) && !m_suppress_ime)
	{
		OpView::IME_MODE mode = OpView::IME_MODE_UNKNOWN;
		if (!m_packed.is_readonly)
			mode = string.GetPasswordMode() ? OpView::IME_MODE_PASSWORD : OpView::IME_MODE_TEXT;
		SpawnIME(vis_dev, im_style.CStr(), mode, GetIMEContext(GetFormObject()));
	}
#endif // defined(WIDGETS_IME_SUPPORT) && defined(WIDGETS_IME_KEEPS_FOCUS_ON_COMMIT)
}

BOOL OpEdit::OnContextMenu(const OpPoint &menu_point, const OpRect *avoid_rect, BOOL keyboard_invoked)
{
	if (listener && listener->OnContextMenu(this, -1, menu_point, avoid_rect, keyboard_invoked))
		return TRUE;

	return TRUE;
}

void OpEdit::OnMouseMove(const OpPoint &point)
{
	if (m_packed.this_click_selected_all && !is_selecting)
	{
		// If we just clicked this input to focus it and move the mouse enough, we assume
		// the user wanted to select something.
		OpRect r = GetTextRect();
		int pos = string.GetCaretPos(r, OpPoint(point.x + x_scroll, point.y));
		if (pos != selecting_start && op_abs(point.x - g_widget_globals->start_point.x) > 5)
		{
			is_selecting = 1;
			SetCaretOffset(selecting_start, FALSE);
			string.sel_start = caret_pos;
			string.sel_stop = caret_pos;
		}
	}
	if(is_selecting == 1 || is_selecting == 2)
	{
		OpRect r = GetTextRect();
		BOOL caret_snap_forward_temp;
		SetCaretOffset(string.GetCaretPos(r, OpPoint(point.x + x_scroll, point.y), &caret_snap_forward_temp), FALSE);
		m_packed.caret_snap_forward = !!caret_snap_forward_temp;

		if (is_selecting == 2) // Word-by-word selecting
		{
			int step = (caret_pos > selecting_start ? 1 : -1);
			const uni_char* str = string.Get();

			// Select from the region boundary before where selection started
			// up to the region boundary after the caret position.

			if (step == 1)
			{
				string.sel_start = PrevCharRegion(str, selecting_start, TRUE);
				string.sel_stop  = NextCharRegion(str, caret_pos);
			}
			else
			{
				string.sel_start = NextCharRegion(str, selecting_start);
				string.sel_stop  = PrevCharRegion(str, caret_pos, TRUE);
			}

			SetCaretOffset(string.sel_stop, FALSE);
		}
		else // Normal selection
		{
			string.sel_stop = caret_pos;
		}

		// Scroll if we are selecting and moving outside the edges
		if (point.x < r.x)
			x_scroll -= 5;
		if (point.x > r.x + r.width)
			x_scroll += 5;
		if (string.GetWidth() < r.width)
			x_scroll = 0;
		else if(x_scroll < 0)
			x_scroll = 0;
		else if (x_scroll > string.GetWidth() - r.width)
			x_scroll = string.GetWidth() - r.width;

		Invalidate(GetBounds());
	}
}

BOOL OpEdit::IsDraggable(const OpPoint& point)
{
#ifdef WIDGETS_DRAGNDROP_TEXT
	if (!string.GetPasswordMode())
		return IsWithinSelection(point.x, point.y);
#endif
	return FALSE;
}

void OpEdit::OnSetCursor(const OpPoint &point)
{
	OpRect inner_rect = GetTextRect();

	if (!inner_rect.Contains(point) || IsDead()) // We are over the border
		OpWidget::OnSetCursor(point);
	else
	{
#ifdef WIDGETS_DRAGNDROP_TEXT
		if (!is_selecting && IsDraggable(point))
		{
			OpWidget::OnSetCursor(point);
			return;
		}
#endif
		SetCursor(CURSOR_TEXT);
	}
}

#endif // !MOUSELESS

#ifdef WIDGETS_IME_SUPPORT

IM_WIDGETINFO OpEdit::GetIMInfo()
{
	IM_WIDGETINFO info;

	if (GetVisualDevice() == NULL)
		return info;

	if (GetFormObject())
		GetFormObject()->UpdatePosition();

	OpRect inner_rect = GetTextRect();

	int charpos = im_pos;
	if(imstring && imstring->GetCandidatePos() != -1)
		charpos += imstring->GetCandidatePos();
	int xpos = string.GetCaretX(inner_rect, charpos, m_packed.caret_snap_forward) - x_scroll;

	OpRect r = GetBounds();
	info.rect.Set(xpos, 0, inner_rect.width - xpos, r.height);
	info.rect.OffsetBy(GetDocumentRect(TRUE).TopLeft());
	info.rect.OffsetBy(-vis_dev->GetRenderingViewX(), -vis_dev->GetRenderingViewY());
	info.rect = GetVisualDevice()->ScaleToScreen(info.rect);

	OpPoint screenpos(0, 0);
	screenpos = GetVisualDevice()->GetView()->ConvertToScreen(screenpos);
	info.rect.OffsetBy(screenpos);

	info.widget_rect = GetDocumentRect(TRUE);
	info.widget_rect.OffsetBy(-vis_dev->GetRenderingViewX(), -vis_dev->GetRenderingViewY());
	info.widget_rect = GetVisualDevice()->ScaleToScreen(info.widget_rect);
	info.widget_rect.OffsetBy(screenpos);

	info.font = NULL;
	info.has_font_info = TRUE;
	info.font_height = font_info.size;
	info.font_style = 0;
	if (font_info.italic) info.font_style |= WIDGET_FONT_STYLE_ITALIC;
	if (font_info.weight >= 6) info.font_style |= WIDGET_FONT_STYLE_BOLD; // See OpFontManager::CreateFont() in OpFont.h for the explanation
	if (font_info.font_info->Monospace()) info.font_style |= WIDGET_FONT_STYLE_MONOSPACE;
	info.is_multiline = FALSE;
	return info;
}

IM_WIDGETINFO OpEdit::OnStartComposing(OpInputMethodString* imstring, IM_COMPOSE compose)
{
	if (compose == IM_COMPOSE_WORD)
	{
		OP_ASSERT(!"Implement!");
	}
	else if (compose == IM_COMPOSE_ALL)
	{
		OpString text;
		GetText(text);
		if (OpStatus::IsMemoryError(imstring->Set(text.CStr(), text.Length())))
			ReportOOM();
	}

	if (m_packed.is_readonly)
		return GetIMInfo();

	im_pos = caret_pos;
	if (compose == IM_COMPOSE_ALL)
		im_pos = 0; // The whole string is edited.

#ifdef IME_SEND_KEY_EVENTS
	previous_ime_len = uni_strlen(imstring->Get());
#endif // IME_SEND_KEY_EVENTS

	im_compose_method = compose;
	this->imstring = (OpInputMethodString*) imstring;
	string.SetIMPos(im_pos, 0);
	string.SetIMCandidatePos(im_pos, 0);
	string.SetIMString(imstring);
	InvalidateAll();
	return GetIMInfo();
}

IM_WIDGETINFO OpEdit::GetWidgetInfo()
{
	return GetIMInfo();
}

IM_WIDGETINFO OpEdit::OnCompose()
{
	if (imstring == NULL)
	{
		IM_WIDGETINFO info;
		info.font = NULL;
		info.is_multiline = FALSE;
		return info;
	}

	if (m_packed.im_is_composing)
		Undo(TRUE);

	m_packed.im_is_composing = TRUE;
	m_packed.is_changed = TRUE;

	if (im_compose_method == IM_COMPOSE_ALL)
	{
		OpInputMethodString *real_imstring = imstring;
		imstring = NULL;
		SetText(UNI_L(""));
		imstring = real_imstring;
	}

	int space_left = string.GetMaxLength() - GetTextLength();
	if (space_left < 0)
		space_left = 0;
	int ime_string_len = MIN(uni_strlen(imstring->Get()), (unsigned int)space_left);
	int ime_caret_pos = MIN(imstring->GetCaretPos(), ime_string_len);
	int ime_cand_pos = MIN(imstring->GetCandidatePos(), ime_string_len);
	int ime_cand_length = MIN(imstring->GetCandidateLength(), ime_cand_pos + ime_string_len);

#ifdef IME_SEND_KEY_EVENTS
	const uni_char* str = imstring->Get();
	BOOL ends_with_enter = FALSE; // enter is actually \r\n, so we need to send OP_KEY_ENTER
	if (ime_string_len > 1 && str[ime_string_len - 1] == '\n')
	{
		ends_with_enter = TRUE;
	}
#endif // IME_SEND_KEY_EVENTS

	InternalInsertText(imstring->Get(), TRUE); // TRUE: We must NOT let the undostack append this event.
	string.SetIMPos(im_pos, ime_string_len);
	string.SetIMCandidatePos(im_pos + ime_cand_pos, ime_cand_length);
	string.SetIMString(imstring);
	SetCaretOffset(im_pos + ime_caret_pos, FALSE);

#ifdef IME_SEND_KEY_EVENTS
	if (ime_string_len != previous_ime_len)
	{
		uni_char key;
		if (previous_ime_len > ime_string_len)
			key = OP_KEY_BACKSPACE;
		else if (ends_with_enter)
			key = OP_KEY_ENTER;
		else
			key = str[ime_string_len - 1];

		m_fake_keys++;
		g_input_manager->InvokeKeyDown(OpKey::FromString(&key), 0);
		g_input_manager->InvokeKeyPressed(OpKey::FromString(&key), 0);
		g_input_manager->InvokeKeyUp(OpKey::FromString(&key), 0);
	}
	previous_ime_len = ime_string_len;
#endif // IME_SEND_KEY_EVENTS

	// Reset cursor blink
	caret_blink = 0;
#ifndef WIDGETS_DISABLE_CURSOR_BLINKING
	StopTimer();
	StartTimer(GetInfo()->GetCaretBlinkDelay());
#endif //!WIDGETS_DISABLE_CURSOR_BLINKING

	// Let the form object know that its value has changed.
	if (!m_packed.onchange_on_enter)
		BroadcastOnChange(FALSE, FALSE, TRUE);

	ScrollIfNeeded(TRUE);
	Invalidate(GetBounds());

	return GetIMInfo();
}

void OpEdit::OnCommitResult()
{
	if (imstring == NULL)
		return;

	if (im_compose_method == IM_COMPOSE_ALL)
	{
		SelectAll();
		OpInputAction delete_action(OpInputAction::ACTION_DELETE);
		EditAction(&delete_action);
	}

	OnCompose();
	SetCaretOffset(im_pos + uni_strlen(imstring->Get()));

	if (caret_pos > (int) string.GetMaxLength())
		SetCaretOffset(string.GetMaxLength());
	im_pos = caret_pos;
	m_packed.im_is_composing = FALSE;

	SetReceivedUserInput();

	if (!m_packed.onchange_on_enter)
		BroadcastOnChange(FALSE);
	InvalidateAll();
}

void OpEdit::OnStopComposing(BOOL canceled)
{
	if (imstring == NULL)
		return;

	if(canceled && m_packed.im_is_composing)
	{
		Undo(TRUE);
		SetCaretOffset(im_pos);
	}
	string.SetIMNone();
	m_packed.im_is_composing = FALSE;
	imstring = NULL;
	if (!canceled)
		SetReceivedUserInput();

	if( !canceled && vis_dev)
	{
		OpInputAction action(OpInputAction::ACTION_UNKNOWN);
		autocomp.EditAction(&action, TRUE); // To open the window if suitable
	}
}

#ifdef WIDGETS_IME_SUPPORT
void OpEdit::OnCandidateShow(BOOL visible)
{
	if (autocomp.window)
		autocomp.window->Show(!visible);
}
#endif

OP_STATUS OpEdit::SetIMStyle(const uni_char* style)
{
	if (im_style.Compare(style) != 0)
	{
		RETURN_IF_ERROR(im_style.Set(style));
		if (IsFocused())
		{
			// Set the current wanted input method mode.
			OpView::IME_MODE mode = OpView::IME_MODE_UNKNOWN;
			if (!m_packed.is_readonly)
				mode = string.GetPasswordMode() ? OpView::IME_MODE_PASSWORD : OpView::IME_MODE_TEXT;
			vis_dev->GetView()->GetOpView()->SetInputMethodMode(mode, GetIMEContext(GetFormObject()), im_style);
		}
	}

	return OpStatus::OK;
}

#ifdef IME_RECONVERT_SUPPORT
void OpEdit::OnPrepareReconvert(const uni_char*& str, int& psel_start, int& psel_stop)
{
	str = string.Get();
	if(HasSelectedText())
		OpWidget::GetSelection(psel_start,psel_stop);
	else
		psel_stop = psel_start = caret_pos;
}

void OpEdit::OnReconvertRange(int sel_start, int sel_stop)
{
	SetSelection(sel_start,sel_stop);
	SetCaretOffset(sel_start);
}
#endif // IME_RECONVERT_SUPPORT
#endif // WIDGETS_IME_SUPPORT

void OpEdit::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	string.NeedUpdate();
	// This method is in quick used for many types of edit fields but we want the normal
	// edit size for all of them (unless they have a GetPreferedSize of their own).
	GetInfo()->GetPreferedSize(this, OpTypedObject::WIDGET_TYPE_EDIT, w, h, cols, rows);
}

void OpEdit::OnResize(INT32* new_w, INT32* new_h)
{
#ifdef WIDGETS_IME_SUPPORT
	if (imstring)
		vis_dev->GetOpView()->SetInputMoved();
#endif // WIDGETS_IME_SUPPORT

	ScrollIfNeeded();
	UpdateCorner();
}

void OpEdit::OnMove()
{
#ifdef WIDGETS_IME_SUPPORT
	if (imstring)
		vis_dev->GetOpView()->SetInputMoved();
#endif // WIDGETS_IME_SUPPORT
}

void OpEdit::UpdateCorner()
{
	if (!m_corner)
		return;

	if (IsHorizontallyResizable())
	{
		if (GetResizability() != WIDGET_RESIZABILITY_HORIZONTAL)
		{
			// Make sure that only horizontal resize can happen. Calling this
			// function will trigger OnResizabilityChanged(), which will
			// recurse into the current function (once) ...
			SetResizability(WIDGET_RESIZABILITY_HORIZONTAL);
			return; // ... so return here. Don't do things twice.
		}

		INT32 pref_w, pref_h;
		m_corner->GetPreferedSize(&pref_w, &pref_h, 0, 0);

		OpRect border_rect(0, 0, rect.width, rect.height);
		GetInfo()->AddBorder(this, &border_rect);

		INT32 cx = border_rect.x + border_rect.width - pref_w;
		INT32 cy = border_rect.y + border_rect.height - pref_h;
		m_corner->SetRect(OpRect(cx, cy, pref_w, pref_h));
	}

	m_corner->SetVisibility(IsResizable());
	m_corner->InvalidateAll();
}

void OpEdit::OnResizabilityChanged()
{
	UpdateCorner();
}

void OpEdit::BroadcastOnChange(BOOL changed_by_mouse, BOOL force_now, BOOL changed_by_keyboard)
{
#ifdef QUICK
	if (!force_now && m_delayed_trigger.GetInitialDelay())
	{
		m_delayed_trigger.InvokeTrigger(OpDelayedTrigger::INVOKE_REDELAY);
		return;
	}
#endif
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	if (!changed_by_keyboard)
		AccessibilitySendEvent(Accessibility::Event(Accessibility::kAccessibilityEventTextChanged));
	else
		AccessibilitySendEvent(Accessibility::Event(Accessibility::kAccessibilityEventTextChangedBykeyboard));
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

	if (listener)
		listener->OnChange(this, changed_by_mouse);
}

void OpEdit::OnScaleChanged()
{
	string.NeedUpdate();
	ghost_string.NeedUpdate();
}

#ifdef OP_KEY_CONTEXT_MENU_ENABLED
OpPoint OpEdit::GetCaretCoordinates()
{
	OpRect text_area = GetTextRect();
	OpPoint caret;
	caret.x = string.GetCaretX(text_area, caret_pos, m_packed.caret_snap_forward) - x_scroll;
	caret.y = text_area.y;
	return caret;
}
#endif // OP_KEY_CONTEXT_MENU_ENABLED


/***********************************************************************************
**
**	OnDragMove/OnDragDrop
**
***********************************************************************************/

#ifdef DRAG_SUPPORT

void OpEdit::OnDragStart(const OpPoint& point)
{
#ifdef QUICK
	if (g_application->IsDragCustomizingAllowed())
	{
		OpWidget::OnDragStart(point);
		return;
	}
#endif

	// we should support dragging of selected text, but not right now..
	// anyway, only let parent get drag start events if user clicked
	// in the image area

	OpRect inner_rect = GetBounds();

	GetInfo()->AddBorder(this, &inner_rect);

#if defined SKIN_SUPPORT
	if (GetForegroundSkin()->HasContent())
	{
		inner_rect = GetForegroundSkin()->CalculateScaledRect(inner_rect, FALSE, TRUE);

		if (inner_rect.Contains(point))
			OpWidget::OnDragStart(point);
	}
#endif

#ifdef WIDGETS_DRAGNDROP_TEXT
	if (IsDraggable(point) && !is_selecting && !m_packed.this_click_selected_all
#ifdef DRAG_SUPPORT
		&& !g_drag_manager->IsDragging()
#endif // DRAG_SUPPORT
		)
	{
		int s1, s2;
		OpWidget::GetSelection(s1, s2);
		if (s2)
		{
			OpString str;
			str.Set(&string.Get()[s1], s2 - s1);

			OpDragObject* drag_object = NULL;
			RETURN_VOID_IF_ERROR(OpDragObject::Create(drag_object, OpTypedObject::DRAG_TYPE_TEXT));
			OP_ASSERT(drag_object);
			OpAutoVector<OpRect> list;
			OpRect union_rect;
			OpBitmap* bmp = NULL;
			if (OpStatus::IsSuccess(GetSelectionRects(&list, union_rect, TRUE)))
			{
				if (OpStatus::IsSuccess(OpBitmap::Create(&bmp, union_rect.width, inner_rect.height, TRUE, TRUE, 0, 0, TRUE)) && bmp)
				{
					UINT8 color[4] = {0};
					bmp->SetColor(color, TRUE, NULL);
					if (OpPainter* painter = bmp->GetPainter())
					{
						WIDGET_COLOR widget_color = GetColor();
						UINT32 fcol = widget_color.use_default_foreground_color ?
										GetInfo()->GetSystemColor(OP_SYSTEM_COLOR_TEXT) :
										widget_color.foreground_color;
						VisualDevice* vd = GetVisualDevice();
						OpRect doc_rect = GetDocumentRect(TRUE);
						AffinePos pos;
						vd->GetPosition(pos);
						OpPoint point = pos.GetTranslation();
						OpRect win_rect(point.x, point.y, vd->WinWidth(), vd->WinHeight());
						vd->BeginPaint(painter, win_rect, doc_rect);
						int offset_x = 0;
						OpWidgetStringDrawer string_drawer;
						string_drawer.SetCaretSnapForward(m_packed.caret_snap_forward);
						for (UINT32 iter = 0; iter < list.GetCount(); ++iter)
						{
							OpRect* rect = list.Get(iter);
							rect->x -= GetTextRect().x;
							rect->x += x_scroll;
							offset_x = (iter == 0) ? rect->x : offset_x;
							OpRect dest_rect(rect->x - offset_x, rect->y, rect->width, rect->height);
							string_drawer.Draw(&string, dest_rect, vd, fcol, GetEllipsis(), rect->x);
						}
						vd->EndPaint();
						bmp->ReleasePainter();
					}
				}
			}
			drag_object->SetDropType(DROP_COPY);
			drag_object->SetVisualDropType(DROP_COPY);
			drag_object->SetEffectsAllowed(DROP_MOVE | DROP_COPY);
			DragDrop_Data_Utils::SetText(drag_object, str);
			drag_object->SetSource(this);
			drag_object->SetBitmap(bmp);
			OpPoint bmp_point(point.x - union_rect.x , point.y);
			drag_object->SetBitmapPoint(bmp_point);
			g_drag_manager->StartDrag(drag_object, GetWidgetContainer(), TRUE);
		}
	}
#endif // WIDGETS_DRAGNDROP_TEXT
}

void OpEdit::OnDragMove(OpDragObject* drag_object, const OpPoint& point)
{
#ifdef QUICK
	if (g_application->IsDragCustomizingAllowed())
	{
		OpWidget::OnDragMove(drag_object, point);
		return;
	}
#endif // QUICK

	if (m_packed.is_readonly)
	{
		drag_object->SetDropType(DROP_NOT_AVAILABLE);
		drag_object->SetVisualDropType(DROP_NOT_AVAILABLE);
		return;
	}

	DropType drop_type = drag_object->GetDropType();
	BOOL suggested_drop_type = drag_object->GetSuggestedDropType() != DROP_UNINITIALIZED;
	if (!suggested_drop_type) // Modify only when the suggested drop type is not used.
	{
		// Default to move withing the same widget.
		if (drag_object->GetSource() == this)
		{
			drag_object->SetDropType(DROP_MOVE);
			drag_object->SetVisualDropType(DROP_MOVE);
		}
		else if (drop_type == DROP_NOT_AVAILABLE || drop_type == DROP_UNINITIALIZED)
		{
			drag_object->SetDropType(DROP_COPY);
			drag_object->SetVisualDropType(DROP_COPY);
		}
	}

	if (drag_object->GetDropType() == DROP_NOT_AVAILABLE)
	{
		OnDragLeave(drag_object);
		return;
	}

	if (!m_packed.is_readonly && (DragDrop_Data_Utils::HasText(drag_object) || DragDrop_Data_Utils::HasURL(drag_object)))
	{
		if ((GetParent() && GetParent()->GetType() == WIDGET_TYPE_FILECHOOSER_EDIT) ||
			(GetFormObject() && GetFormObject()->GetHTML_Element()->GetInputType() == INPUT_FILE))
		//   Be sure it is the element we check. ^^^^^^^^^^^^^ The formobject might not have been recreated yet.
		{
			// link drop on file chooser is potential security risk
			return;
		}

		BOOL carret_sf_temp;
		//INT32 old_caret_pos = caret_pos;
		SetCaretOffset(string.GetCaretPos(GetTextRect(), OpPoint(point.x + x_scroll, point.y), &carret_sf_temp), FALSE);
		m_packed.caret_snap_forward = !!carret_sf_temp;

		int s1, s2;
		OpWidget::GetSelection(s1, s2);
		int caret_offset = GetCaretOffset();
		BOOL caret_in_selection = HasSelectedText() && caret_offset >= s1 && caret_offset <= s2;
		m_packed.show_drag_caret = caret_in_selection ? false : true;

		//if( caret_pos != old_caret_pos )
		{
			InvalidateAll();
		}
	}
#ifdef QUICK
	else
	{
		OpWidget::OnDragMove(drag_object, point);
	}
#endif // QUICK
}

void OpEdit::OnDragDrop(OpDragObject* drag_object, const OpPoint& point)
{
	m_packed.show_drag_caret = FALSE;

#ifdef QUICK
	if (g_application->IsDragCustomizingAllowed())
	{
		OpWidget::OnDragDrop(drag_object, point);
		return;
	}
#endif // QUICK

	if (drag_object->GetDropType() == DROP_NOT_AVAILABLE)
	{
		OnDragLeave(drag_object);
		return;
	}

	/**
	 * If the drag object is a window (tab) or a link, the url should be chosen
	 * in preference of any text
	 */
	const uni_char* dragged_text = DragDrop_Data_Utils::GetText(drag_object);
	TempBuffer buff;
	OpStatus::Ignore(DragDrop_Data_Utils::GetURL(drag_object, &buff));
	const uni_char* url = buff.GetStorage();
	if (dragged_text && drag_object->GetType() != OpTypedObject::DRAG_TYPE_WINDOW && drag_object->GetType() != OpTypedObject::DRAG_TYPE_LINK)
	{
		if( !*dragged_text )
		{
			drag_object->SetDropType(DROP_NOT_AVAILABLE);
			drag_object->SetVisualDropType(DROP_NOT_AVAILABLE);
			return;
		}

		BOOL move_text = FALSE;
#ifdef WIDGETS_DRAGNDROP_TEXT
		if (drag_object->GetSource() == this && HasSelectedText())
		{
			int insert_caret_pos = caret_pos;
			int s1, s2;
			OpWidget::GetSelection(s1, s2);
			if (insert_caret_pos >= s1 && insert_caret_pos <= s2)
				return;

			if (drag_object->GetDropType() == DROP_MOVE)
			{
				move_text = TRUE;
				if (insert_caret_pos > s2)
					insert_caret_pos -= s2 - s1;
				OpInputAction action(OpInputAction::ACTION_DELETE);
				EditAction(&action);
			}

			SetCaretOffset(insert_caret_pos);
		}

		SelectNothing();
#endif // WIDGETS_DRAGNDROP_TEXT

		if ( !move_text && m_packed.replace_all_on_drop)
		{
			SetFocus(FOCUS_REASON_MOUSE);
			SetText(dragged_text);
			SelectNothing();
		}
		else
		{
			SetFocus(FOCUS_REASON_MOUSE);
			InsertText(dragged_text);
		}
	}
	else if ( url )
	{
		if( !*url )
		{
			drag_object->SetDropType(DROP_NOT_AVAILABLE);
			drag_object->SetVisualDropType(DROP_NOT_AVAILABLE);
			return;
		}

		if (m_packed.replace_all_on_drop)
		{
			SetFocus(FOCUS_REASON_MOUSE);
			SetText(url);
			SelectNothing();
		}
		else
		{
			SetFocus(FOCUS_REASON_MOUSE);
			InsertText(url);
		}
	}
#ifdef QUICK
	else
	{
		OpWidget::OnDragDrop(drag_object, point);
		return;
	}
#endif // QUICK

	if (listener)
		listener->OnChange(this, TRUE);

	// The OnFocus have reset m_packed.is_changed, and we don't want to move it to before InsertText so we have to set it here.
	if (IsForm())
		m_packed.is_changed = TRUE;
	// The OnFocus have set m_packed.this_click_selected_all, but we don't want to have selection ongoing after dropping.
	m_packed.this_click_selected_all = FALSE;
}


void OpEdit::OnDragLeave(OpDragObject* drag_object)
{
	if (m_packed.show_drag_caret)
	{
		m_packed.show_drag_caret = FALSE;
		Invalidate(GetBounds());
	}
	drag_object->SetDropType(DROP_NOT_AVAILABLE);
	drag_object->SetVisualDropType(DROP_NOT_AVAILABLE);
	SetCursor(CURSOR_NO_DROP);
}

#endif // DRAG_SUPPORT

BOOL OpEdit::IsWithinSelection(int x, int y)
{
	if (!HasSelectedText())
		return FALSE;

	OpPoint point(x, y);
	return string.IsWithinSelection(GetVisualDevice(), point, x_scroll, GetTextRect());
}

#ifdef QUICK

/***********************************************************************************
**
**	GetLayout
**
***********************************************************************************/

BOOL OpEdit::GetLayout(OpWidgetLayout& layout)
{
	INT32 width, height;
	GetPreferedSize(&width, &height, 0, 0);

	layout.SetPreferedWidth(width);
	layout.SetFixedHeight(height);
	return TRUE;
}

/***********************************************************************************
**
**	LayoutToAvailableRect
**
***********************************************************************************/

OpRect OpEdit::LayoutToAvailableRect(const OpRect& rect)
{
	OpRect new_rect = rect;
	OpRect edit_rect = rect;

	new_rect.y += 23;
	new_rect.height -= 23;

	edit_rect.height = 23;

	SetRect(edit_rect);

	return new_rect;
}

void OpEdit::SetAction(OpInputAction* action)
{
	OpWidget::SetAction(action);

	if (action)
	{
		if (action->HasActionText())
		{
			SetGhostText(action->GetActionText());
		}

		switch (action->GetAction())
		{
#ifdef M2_SUPPORT
			case OpInputAction::ACTION_START_SEARCH:
				autocomp.SetType(AUTOCOMPLETION_M2_MESSAGES);
				GetBorderSkin()->SetImage("Edit Search Skin", "Edit Skin");
				GetForegroundSkin()->SetImage("Search");
				break;
#endif
			case OpInputAction::ACTION_SEARCH:
				GetBorderSkin()->SetImage("Edit Search Skin", "Edit Skin");
				GetForegroundSkin()->SetImage("Search Web");
				break;

			case OpInputAction::ACTION_GO_TO_PAGE:
				if (m_packed.use_default_autocompletion)
				{
					autocomp.SetType(AUTOCOMPLETION_HISTORY);
				}
				break;
		}
	}
}

#endif // QUICK

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

OP_STATUS OpEdit::AccessibilitySetSelectedTextRange(int start, int end)
{
	SetSelection(start, end);
	return OpStatus::OK;
}

OP_STATUS OpEdit::AccessibilityGetSelectedTextRange(int &start, int &end)
{
	OpWidget::GetSelection(start, end);
	if (((start == 0) && (end == 0)))
		start = end = GetCaretOffset();
	return OpStatus::OK;
}

OP_STATUS OpEdit::AccessibilityGetDescription(OpString& str)
{
	return str.Set(ghost_string.Get());
}

Accessibility::State OpEdit::AccessibilityGetState()
{
	Accessibility::State state = OpWidget::AccessibilityGetState();
	if (m_packed.is_readonly)
		state |= Accessibility::kAccessibilityStateReadOnly;
	return state;
}

#endif // ACCESSIBILITY_EXTENSION_SUPPORT
