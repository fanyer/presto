/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#if defined(SPELLCHECK_SUPPORT)

#include "platforms/mac/spellcheck/MacSpellCheckDialog.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/widgets/OpWidgetFactory.h"
#include "modules/locale/oplanguagemanager.h"

extern "C" {
// in MacSpellCheckDialog.mm
// Didn't want it in the header, since these functions should only be used from here.
	void		InitSpellCheck();
	CFRange		CheckSpellingOfStringAndDisplay(CFStringRef stringToCheck, int startingOffset);
	void		SetSpellingPanelPosition(int x, int y, int width, int height);

// in SpellCheck.m
	int			UniqueSpellDocumentTag();
	void		CloseSpellDocumentWithTag(int tag);
	CFRange		CheckSpellingOfString(CFStringRef stringToCheck, int startingOffset);
	CFRange		CheckSpellingOfStringWithOptions(CFStringRef stringToCheck, int startingOffset, CFStringRef language, Boolean wrapFlag, int tag, int wordCount);
	void		IgnoreWord(CFStringRef wordToIgnore, int tag);
	void		SetIgnoredWords(CFArrayRef inWords, int tag);
	CFArrayRef	CopyIgnoredWordsInSpellDocumentWithTag(int tag);
	CFArrayRef	GuessesForWord(CFStringRef word);
}

// A static instance for cocoa events...
SpellCheckDialog* SpellCheckDialog::sSpellCheckDialog = NULL;
int gSpellTag = -1;

pascal OSStatus SpellCheckDialog::SpellEventHandler( EventHandlerCallRef inCaller, EventRef inEvent, void* inRefcon )
{
	OSStatus	result = eventNotHandledErr;

	switch ( GetEventClass( inEvent ) )
	{
		case kEventClassCommand:
		{
			HICommandExtended cmd;
			verify_noerr( GetEventParameter( inEvent, kEventParamDirectObject, typeHICommand, NULL, sizeof( cmd ), NULL, &cmd ) );

			switch ( GetEventKind( inEvent ) )
			{
				case kEventCommandProcess:
					switch ( cmd.commandID )
					{
						case 'chsp':	// kHICommandChangeSpelling
							result = noErr;
							break;

						case 'igsp':	// kHICommandIgnoreSpelling
							result = noErr;
							break;

						default:
							break;
					}
					break;
			}
			break;
		}

		default:
			break;
	}

	return result;
}

SpellCheckDialog::SpellCheckDialog(const OpStringC& text, int start_pos, int end_pos, OpWidget* update_widget)
: m_update_widget(update_widget),
  m_start_pos(start_pos),
  m_end_pos(end_pos),
  m_current_pos(start_pos),
  m_text_is_changed(FALSE),
  m_accept_new_replacements(TRUE),
  m_has_started(FALSE),
  m_failed_on_init(FALSE)
{
	InitSpellCheck();
	if (gSpellTag == -1)
	{
		static const EventTypeSpec	kSpellEvents[] = {
			{ kEventClassCommand, kEventCommandProcess }
		};
		InstallApplicationEventHandler(NewEventHandlerUPP(SpellEventHandler), GetEventTypeCount(kSpellEvents), kSpellEvents, 0, NULL);
		gSpellTag = UniqueSpellDocumentTag();
	}

	m_text.Set(text);
	m_cf_str = CFStringCreateWithCharacters(kCFAllocatorDefault,(const UniChar*)text.CStr(),text.Length());
	sSpellCheckDialog = this;
	FindNextWrongWord();
}

SpellCheckDialog::~SpellCheckDialog()
{
	if (m_cf_str)
		CFRelease(m_cf_str);
	if (sSpellCheckDialog == this)
		sSpellCheckDialog = NULL;
}

void SpellCheckDialog::Init(DesktopWindow* parent_window)
{

}

void SpellCheckDialog::OnInit()
{
}

UINT32 SpellCheckDialog::OnOk()
{
	if (m_text_is_changed && m_update_widget)
	{
		m_update_widget->SetText(m_text.CStr());
		m_update_widget->SetSelection(m_start_pos, m_end_pos);
	}

	return 0;
}


void SpellCheckDialog::OnCancel()
{
	//abort
}


void SpellCheckDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
}


BOOL SpellCheckDialog::OnInputAction(OpInputAction* action)
{
}

BOOL SpellCheckDialog::FindNextWrongWord()
{
	CFRange range = CheckSpellingOfStringAndDisplay(m_cf_str, m_current_pos);
	if (range.length && m_update_widget)
	{
		m_update_widget->SetSelection(range.location, range.location + range.length);
	}
	return (range.length > 0);
}

OP_STATUS SpellCheckDialog::PopulateReplacementWords(const uni_char* original_word, int original_word_length, const uni_char** replacement_words)
{
	return OpStatus::OK;
}

void SpellCheckDialog::SpellcheckReplace()
{
}

void SpellCheckDialog::SpellcheckIgnore()
{
}

void SpellCheckDialog::SpellcheckLearn()
{
}

void SpellCheckDialog::SpellcheckDelete()
{
}

void SpellCheckDialog::SpellCheckDone()
{
	// close dialog
}

#endif // SPELLCHECK_SUPPORT
