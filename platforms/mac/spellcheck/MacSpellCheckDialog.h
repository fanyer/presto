/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef MACSPELLCHECKDIALOG_H
#define MACSPELLCHECKDIALOG_H

#ifdef SPELLCHECK_SUPPORT

#include "adjunct/desktop_util/treemodel/simpletreemodel.h"
#include "adjunct/spellcheck/opsrc/spellcheck.h"
#include "modules/util/opstring.h"

class DesktopWindow;
class OpWidget;

/***********************************************************************************
**
**	SpellCheckDialog
**
***********************************************************************************/

class SpellCheckDialog //: public Dialog
{
	private:
		OpWidget*	m_update_widget;

		OpString	m_text;
		int			m_start_pos;
		int			m_end_pos;
		CFStringRef	m_cf_str;

//		SpellcheckSession* m_session;
		int			m_current_pos;
		BOOL		m_text_is_changed;
		BOOL		m_accept_new_replacements;
		BOOL		m_has_started;
		BOOL		m_failed_on_init;
		CFRange		m_misspelledRange;

	public:
								SpellCheckDialog(const OpStringC& text, int start_pos, int end_pos, OpWidget* update_widget=NULL);
		virtual					~SpellCheckDialog();

		void					Init(DesktopWindow* parent_window);

		void					OnInit();

		static SpellCheckDialog* sSpellCheckDialog;
//		virtual void			OnInitVisibility();
//		virtual void			OnShow(BOOL show);

	private:
		UINT32					OnOk();
		void					OnCancel();
		void					OnChange(OpWidget *widget, BOOL changed_by_mouse);
		BOOL					OnInputAction(OpInputAction* action);
		BOOL FindNextWrongWord(); //Returns FALSE when no more misspelled words are found
		OP_STATUS PopulateReplacementWords(const uni_char* original_word, int original_word_length, const uni_char** replacement_words=NULL);

		static pascal OSStatus SpellEventHandler(EventHandlerCallRef inCaller, EventRef inEvent, void* inRefcon);

		void SpellcheckReplace();
		void SpellcheckIgnore();
		void SpellcheckLearn();
		void SpellcheckDelete();
		void SpellCheckDone();
};
#endif // SPELLCHECK_SUPPORT
#endif // MACSPELLCHECKDIALOG_H
