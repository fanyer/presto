/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 */

#ifndef FORMSUBMITTER_H
#define FORMSUBMITTER_H

#include "modules/ecmascript_utils/esthread.h"
#include "modules/forms/webforms2support.h" // ValidationResult

// Compatibility fix. See bug 174846
#define ALLOW_TOO_LONG_FORM_VALUES

#ifdef FORMS_KEYGEN_SUPPORT
#include "modules/util/adt/opvector.h"
class SSL_Private_Key_Generator;
#endif // FORMS_KEYGEN_SUPPORT

class FormSubmitter
#ifdef FORMS_KEYGEN_SUPPORT
	: private MessageObject
#endif // FORMS_KEYGEN_SUPPORT
{
	FramesDocument* m_frames_doc;
	HTML_Element* m_form_element;
	HTML_Element* m_submit_element;
	int m_offset_x;
	int m_offset_y;
	ES_Thread* m_thread;
	BOOL m_triggered_by_script;
	ShiftKeyState m_modifiers;
	/**
	 * The HTML_Elements have been protected and must be unprotected
	 * to prevent huge memory leaks.
	 */
	BOOL m_has_protected;
#ifdef FORMS_KEYGEN_SUPPORT
	BOOL m_has_started_key_generation;
	FormSubmitter* m_next;
	int m_blocking_keygenerator_count;
	BOOL m_keygenerator_aborted;
	OpVector<SSL_Private_Key_Generator>* m_keygen_generators;
	struct FormSubmitterMessageHandler : public MessageHandler
	{
		FormSubmitterMessageHandler(FormSubmitter* submitter, Window* win) :
			MessageHandler(win),
			m_form_submitter(submitter)
		{}
		FormSubmitter* m_form_submitter;
	};
	friend struct FormSubmitterMessageHandler;
	FormSubmitterMessageHandler *m_mh;
#endif // FORMS_KEYGEN_SUPPORT

	OP_STATUS SubmitFormStage2();

public:
	FormSubmitter(FramesDocument* frames_doc,
	              HTML_Element* form_element,
	              HTML_Element* submit_element,
	              int offset_x, int offset_y,
	              ES_Thread* thread,
	              BOOL is_triggered_by_script,
	              ShiftKeyState modifiers);

	OP_STATUS Submit();

	/**
	 * Returns true if this submit belongs to the specified document.
	 *
	 * @param[in] doc The document to compare.
	 *
	 * @returns TRUE if it matches, FALSE otherwise.
	 */
	BOOL BelongsToDocument(FramesDocument* doc) { return m_frames_doc == doc; }

	/**
	 * Abort the submit so that no references will be made to the document.
	 */
	void Abort();

#ifdef FORMS_KEYGEN_SUPPORT
	/**
	 * @returns TRUE if there were keygen tags and we have to wait for them to be processed.
	 */
	BOOL StartKeyGeneration();
	FormSubmitter* GetNextInList() { return m_next; }
#endif // defined FORMS_KEYGEN_SUPPORT

	// Used by FormManager::Submit when we decide that we need a heap allocated FormSubmitter
	FormSubmitter(FormSubmitter& other);

#ifdef FORMS_KEYGEN_SUPPORT
	/* From MessageObject */
	virtual  void	HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
#endif // FORMS_KEYGEN_SUPPORT

	OP_STATUS ProtectHTMLElements(BOOL protect);
	~FormSubmitter();

#ifdef ALLOW_TOO_LONG_FORM_VALUES
	static BOOL HandleTooLongValue(FramesDocument* frames_doc, ValidationResult val_res);
#endif // ALLOW_TOO_LONG_FORM_VALUES
};

#endif // FORMSUBMITTER_H
