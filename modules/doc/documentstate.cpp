/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 2002-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/doc/documentstate.h"
#include "modules/doc/frm_doc.h"
#include "modules/doc/html_doc.h"
#include "modules/dochand/docman.h"
#include "modules/dochand/fdelm.h"
#include "modules/dochand/win.h"
#include "modules/forms/formvalue.h"
#include "modules/forms/formvaluelist.h"
#include "modules/forms/formvalueradiocheck.h"
#include "modules/layout/traverse/traverse.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/util/str.h"

#include "modules/debug/debug.h"

static BOOL
WasInsertedByParser(HTML_Element *element)
{
	return element->GetInserted() == HE_NOT_INSERTED;
}

DocumentElementPath::DocumentElementPath()
	: elements(NULL),
	  elements_count(0)
{
}

DocumentElementPath::PathElement::PathElement()
	: name(0)
{
}

DocumentElementPath::PathElement::~PathElement()
{
	OP_DELETEA(name);
}

OP_STATUS
DocumentElementPath::PathElement::Set(HTML_Element *element, unsigned character_offset)
{
	ns = element->GetNsType();
	type = element->Type();

	if (type == HE_UNKNOWN)
		RETURN_IF_ERROR(UniSetStr(name, element->GetTagName()));
	else if (type == HE_TEXTGROUP)
		type = HE_TEXT;

	index = 0;

	if (type == HE_TEXT)
	{
		offset = character_offset;

		HTML_Element *sibling = element->PredActual();

		while (sibling)
			if (sibling->IsText())
			{
				offset += sibling->GetTextContentLength();
				sibling = sibling->PredActual();
			}
			else
				break;

		BOOL in_text = FALSE;

		while (sibling)
		{
			if (in_text && !sibling->IsText())
				in_text = FALSE;
			else if (!in_text && sibling->IsText())
			{
				in_text = TRUE;
				++index;
			}

			sibling = sibling->PredActual();
		}
	}
	else
		for (HTML_Element *sibling = element->PredActual(); sibling; sibling = sibling->PredActual())
			if (WasInsertedByParser(sibling) && NameMatches(sibling))
				++index;

	return OpStatus::OK;
}

BOOL
DocumentElementPath::PathElement::NameMatches(HTML_Element *element)
{
	if (type == HE_TEXT)
		return element->IsText();
	else
		return element->IsMatchingType(type, ns) && (type != ATTR_XML || uni_str_eq(name, element->GetTagName()));
}

HTML_Element *
DocumentElementPath::PathElement::Find(HTML_Element *element, unsigned *offset_out)
{
	unsigned found = 0;

	element = element->FirstChildActual();

	if (type == HE_TEXT)
	{
		BOOL in_text = FALSE;

		while (element)
		{
			if (in_text && !element->IsText())
				in_text = FALSE;
			else if (!in_text && element->IsText())
			{
				in_text = TRUE;
				if (found++ == index)
					break;
			}

			element = element->SucActual();
		}
	}
	else
		while (element)
		{
			if (WasInsertedByParser(element) && NameMatches(element))
				if (found++ == index)
					break;

			element = element->SucActual();
		}

	if (element && type == HE_TEXT && offset_out)
	{
		HTML_Element *last = element;

		unsigned adjusted_offset = offset;

		while (element && element->IsText())
		{
			last = element;

			unsigned length = element->GetTextContentLength();

			if (adjusted_offset < length)
				break;

			adjusted_offset -= length;
			element = element->SucActual();
		}

		if (element && !element->IsText())
			element = NULL;

		if (!element && adjusted_offset == 0)
		{
			element = last;
			adjusted_offset = element->GetTextContentLength();
		}

		*offset_out = adjusted_offset;
	}

	return element;
}

BOOL
DocumentElementPath::PathElement::Match(HTML_Element *element)
{
	if (NameMatches(element))
	{
		int element_index = 0;

		for (HTML_Element *sibling = element->PredActual(); sibling && element_index <= (int) index; sibling = sibling->PredActual())
			if (NameMatches(sibling))
				++element_index;

		if ((int) index == element_index)
			return TRUE;
	}

	return FALSE;
}

/* static */ OP_STATUS
DocumentElementPath::Make(DocumentElementPath *&path, HTML_Element *element, unsigned character_offset)
{
	path = OP_NEW(DocumentElementPath, ());
	if (!path)
		return OpStatus::ERR_NO_MEMORY;

	unsigned level = 0;
	HTML_Element *parent = element;
	while (parent && parent->Type() != HE_DOC_ROOT)
	{
		++level;
		parent = parent->ParentActual();
	}

	path->elements = OP_NEWA(PathElement, level);
	if (!path->elements)
	{
		OP_DELETE(path);
		path = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}
	path->elements_count = level;

	for (int index = level - 1; index >= 0; --index, element = element->ParentActual())
		if (OpStatus::IsMemoryError(path->elements[index].Set(element, character_offset)))
		{
			OP_DELETE(path);
			path = NULL;
			return OpStatus::ERR_NO_MEMORY;
		}

	return OpStatus::OK;
}

DocumentElementPath::~DocumentElementPath()
{
	OP_DELETEA(elements);
}

HTML_Element *
DocumentElementPath::Find(HTML_Element *element, unsigned *offset_out)
{
	unsigned index = 0;
	while (element && index < elements_count)
		element = elements[index++].Find(element, offset_out);

	return element;
}

BOOL
DocumentElementPath::Match(HTML_Element *element)
{
	for (int index = elements_count - 1; element && index >= 0; element = element->ParentActual(), --index)
		if (!elements[index].Match(element))
			return FALSE;
	return TRUE;
}

DocumentFormElementState::DocumentFormElementState()
	: path(NULL),
	  value(NULL),
	  options(NULL)
{
	OP_NEW_DBG("DocumentFormElementState::DocumentFormElementState", "doc");
	OP_DBG((UNI_L("this=%p"), this));
}

DocumentFormElementState::~DocumentFormElementState()
{
	OP_NEW_DBG("DocumentFormElementState::~DocumentFormElementState", "doc");
	OP_DBG((UNI_L("this=%p"), this));
	OP_DELETE(path);
	OP_DELETE(value);
	OP_DELETE(options);
}

DocumentFormElementState::Option::Option()
	: value(NULL),
	  text(NULL),
	  next(NULL)
{
}

DocumentFormElementState::Option::~Option()
{
	OP_DELETEA(value);
	OP_DELETEA(text);
	OP_DELETE(next);
}

BOOL
DocumentFormElementState::Option::Matches(HTML_Element *element)
{
	OP_ASSERT(element->IsMatchingType(HE_OPTION, NS_HTML));

	const uni_char *elementvalue = element->GetStringAttr(ATTR_VALUE, NS_IDX_HTML);

	if (value == elementvalue || value && elementvalue && uni_str_eq(value, elementvalue))
	{
		TempBuffer buffer;

		if (OpStatus::IsSuccess(element->GetOptionText(&buffer)))
			if (uni_str_eq(text, buffer.GetStorage()))
				return TRUE;
	}

	return FALSE;
}

DocumentFrameState::DocumentFrameState()
	: path(NULL),
	  fdelm(NULL)
{
	OP_NEW_DBG("DocumentFrameState::DocumentFrameState", "doc");
	OP_DBG((UNI_L("this=%p"), this));
}

DocumentFrameState::~DocumentFrameState()
{
	OP_NEW_DBG("DocumentFrameState::~DocumentFrameState", "doc");
	OP_DBG((UNI_L("this=%p"), this));
	OP_DELETE(path);
	OP_DELETE(fdelm);
}

DocumentState::DocumentState(FramesDocument *doc)
	: doc(doc)
	, focused_element(0)
	, navigation_element(0)
	, highlight_navigation_element(FALSE)
#ifndef HAS_NOTEXTSELECTION
	, selection_anchor(0)
	, selection_focus(0)
#endif // HAS_NOTEXTSELECTION
{
	OP_NEW_DBG("DocumentState::DocumentState", "doc");
	OP_DBG((UNI_L("this=%p"), this));
}

OP_STATUS
DocumentState::AddFormElementState(HTML_Element *iter)
{
	OP_NEW_DBG("DocumentState::AddFormElementState", "doc");
	OP_DBG((UNI_L("this=%p; iter=%p"), this, iter));
	FormValue *formvalue = iter->GetFormValue();

	if (iter->GetInputType() == INPUT_PASSWORD || formvalue->GetType() == FormValue::VALUE_NO_OWN_VALUE)
		return OpStatus::OK;

	if (!formvalue->IsChangedFromOriginal(iter))
		return OpStatus::OK;

	if (iter->Type() == HE_OUTPUT)
		return OpStatus::OK;

#ifdef _WML_SUPPORT_
	if (doc->GetDocManager() && doc->GetDocManager()->WMLHasWML())
		return OpStatus::OK;
#endif // _WML_SUPPORT_

	DocumentFormElementState *state = OP_NEW(DocumentFormElementState, ());

	if (!state)
		return OpStatus::ERR_NO_MEMORY;

	OpAutoPtr<DocumentFormElementState> anchor(state);

	RETURN_IF_ERROR(DocumentElementPath::Make(state->path, iter));

	state->inputtype = iter->GetInputType();

	state->value = formvalue->Clone(iter);

	if (!state->value)
		return OpStatus::ERR_NO_MEMORY;

	state->serialnr = formvalue->GetSerialNr();
	state->signal = formvalue->SignalValueRestored();

	if (iter->IsMatchingType(HE_SELECT, NS_HTML))
	{
		FormValueList *list = FormValueList::GetAs(formvalue);
		HTML_Element *select = iter, *stop = iter->NextSiblingActual();
		int index = 0;
		DocumentFormElementState::Option **ptr = &state->options;

		while (iter != stop)
		{
			if (iter->IsMatchingType(HE_OPTION, NS_HTML))
			{
				if (list->IsSelectedElement(select, iter))
				{
					DocumentFormElementState::Option *option = OP_NEW(DocumentFormElementState::Option, ());

					if (!option)
						return OpStatus::ERR_NO_MEMORY;

					OpAutoPtr<DocumentFormElementState::Option> anchor(option);

					option->index = index;

					const uni_char *value = iter->GetStringAttr(ATTR_VALUE, NS_IDX_HTML);
					if (value)
						RETURN_IF_ERROR(UniSetStr(option->value, value));

					TempBuffer buffer;
					RETURN_IF_ERROR(iter->GetOptionText(&buffer));
					RETURN_IF_ERROR(UniSetStr(option->text, buffer.GetStorage()));

					option->next = NULL;
					*ptr = option;
					ptr = &option->next;

					anchor.release();
				}

				++index;
			}

			iter = iter->NextActual();
		}
	}

	DocumentFormElementState *existing;

	for (existing = (DocumentFormElementState *) form_element_states.First(); existing; existing = (DocumentFormElementState *) existing->Suc())
		if (state->serialnr < existing->serialnr)
			break;

	if (existing)
		state->Precede(existing);
	else
		state->Into(&form_element_states);

	anchor.release();

	return OpStatus::OK;
}

OP_STATUS
DocumentState::AddIFrameState(HTML_Element *iter, FramesDocElm *fde)
{
	OP_NEW_DBG("DocumentState::AddIFrameState", "doc");
	OP_DBG((UNI_L("this=%p; iter=%p"), this, iter));
	/* Note: this function is called for regular frames too. */

	DocumentFrameState *state = OP_NEW(DocumentFrameState, ());

	if (!state)
		return OpStatus::ERR_NO_MEMORY;

	OpAutoPtr<DocumentFrameState> anchor(state);

	RETURN_IF_ERROR(DocumentElementPath::Make(state->path, iter));

	fde->DetachHtmlElement();
	fde->Out();

	state->fdelm = fde;
	state->Into(&iframe_states);

	anchor.release();

	return OpStatus::OK;
}

/* static */ OP_STATUS
DocumentState::Make(DocumentState *&state, FramesDocument *doc)
{
	OP_NEW_DBG("DocumentState::Make", "doc");
	state = OP_NEW(DocumentState, (doc));
	if (!state)
		return OpStatus::ERR_NO_MEMORY;
	OP_DBG((UNI_L("state=%p; doc=%p"), state, doc));

	OpAutoPtr<DocumentState> anchor(state);

	if (HTML_Document *htmldoc = doc->GetHtmlDocument())
	{
		/*
		 * + Prefer element with focus over text selection
		 * + Prefer text selection over navigation element
		 * + Might have both focused_element and navigation_element (highlight),
		 *   if both exist they will usually (always?) be the same, so might
		 *   be able to optimize a little by just storing one of them, and using
		 *   flags to tell which it is
		 */

		HTML_Element *focused_element = htmldoc->GetFocusedElement();
		if (focused_element)
			RETURN_IF_ERROR(DocumentElementPath::Make(state->focused_element, focused_element));
#ifndef HAS_NOTEXTSELECTION
		if (!focused_element)
		{
			SelectionBoundaryPoint anchor, focus;
			if (htmldoc->GetSelection(anchor, focus))
			{
				RETURN_IF_ERROR(DocumentElementPath::Make(state->selection_anchor, anchor.GetElement(), anchor.GetElementCharacterOffset()));
				RETURN_IF_ERROR(DocumentElementPath::Make(state->selection_focus, focus.GetElement(), focus.GetElementCharacterOffset()));
			}
		}
#endif // HAS_NOTEXTSELECTION
		else if (HTML_Element *navigation_element = htmldoc->GetNavigationElement())
		{
			RETURN_IF_ERROR(DocumentElementPath::Make(state->navigation_element, navigation_element));
			state->highlight_navigation_element = htmldoc->IsNavigationElementHighlighted();
		}
	}

	HTML_Element *iter = doc->GetDocRoot();

	while (iter)
	{
		if (iter->IsFormElement())
			RETURN_IF_ERROR(state->AddFormElementState(iter));

		iter = iter->NextActual();
	}

	anchor.release();
	return OpStatus::OK;
}

OP_STATUS
DocumentState::StoreIFrames()
{
	OP_NEW_DBG("DocumentState::StoreIFrames", "doc");
	OP_DBG((UNI_L("this=%p"), this));
	if (FramesDocElm *root = doc->GetIFrmRoot())
	{
		FramesDocElm *iter = root->FirstChild();
		while (iter)
		{
			HTML_Element *elm = iter->GetHtmlElement();
			if (elm && WasInsertedByParser(elm))
				RETURN_IF_ERROR(AddIFrameState(elm, iter));
			iter = iter->Suc();
		}
	}

	return OpStatus::OK;
}

DocumentState::~DocumentState()
{
	OP_NEW_DBG("DocumentState::~DocumentState", "doc");
	OP_DBG((UNI_L("this=%p"), this));
	OP_DELETE(focused_element);
	OP_DELETE(navigation_element);
#ifndef HAS_NOTEXTSELECTION
	OP_DELETE(selection_anchor);
	OP_DELETE(selection_focus);
#endif // !HAS_NOTEXTSELECTION

	form_element_states.Clear();
	iframe_states.Clear();
}

OP_STATUS
DocumentState::Restore(FramesDocument *doc)
{
	OP_NEW_DBG("DocumentState::Restore", "doc");
	OP_DBG((UNI_L("this=%p; doc=%p"), this, doc));
	HTML_Element *root = doc->GetDocRoot();

	if (HTML_Document *htmldoc = doc->GetHtmlDocument())
	{
		HTML_Element *element = NULL;
		HTML_Element *current_focused = htmldoc->GetFocusedElement();

		if (!current_focused && focused_element && (element = focused_element->Find(root)) != NULL)
			htmldoc->FocusElement(element, HTML_Document::FOCUS_ORIGIN_STORED_STATE, TRUE, FALSE, FALSE);

#ifndef HAS_NOTEXTSELECTION
		if (!element && selection_anchor && selection_focus)
		{
			unsigned anchor_offset, focus_offset;
			HTML_Element *anchor_element = selection_anchor->Find(root, &anchor_offset), *focus_element = selection_focus->Find(root, &focus_offset);

			if (anchor_element && focus_element)
			{
				SelectionBoundaryPoint anchor, focus;

				anchor.SetLogicalPosition(anchor_element, anchor_offset);

				focus.SetLogicalPosition(focus_element, focus_offset);

				RETURN_IF_ERROR(htmldoc->SetSelection(&anchor, &focus));
			}
		}
#endif // HAS_NOTEXTSELECTION
		else if (!current_focused && navigation_element && (element = navigation_element->Find(root)) != NULL)
			if (highlight_navigation_element)
				htmldoc->HighlightElement(element, HTML_Document::FOCUS_ORIGIN_STORED_STATE);
			else
				htmldoc->SetNavigationElement(element, FALSE);
	}

	return OpStatus::OK;
}

OP_BOOLEAN
DocumentState::RestoreForms(FramesDocument *doc)
{
	OP_NEW_DBG("DocumentState::RestoreForms", "doc");
	OP_DBG((UNI_L("this=%p; doc=%p"), this, doc));
	HTML_Element *root = doc->GetDocRoot();

	while (DocumentFormElementState *state = (DocumentFormElementState *) form_element_states.First())
	{
		OP_BOOLEAN pause = OpBoolean::IS_FALSE;

		/* If we tried to restore form values before, and ran out of
		   memory, state->value might be NULL.  In that case, just
		   skip this element and continue restoring the rest.  Not
		   quite perfect, but we might manage to restore most of the
		   form values (and possibly all) and that is better than
		   just giving up in a controlled manner. */
		if (state->value)
			if (HTML_Element *element = state->path->Find(root))
				if (element->IsFormElement() && element->GetInputType() == state->inputtype)
				{
					FormValue *formvalue = element->GetFormValue();

					BOOL is_select = element->IsMatchingType(HE_SELECT, NS_HTML);
					BOOL is_checkbox_or_radio = formvalue->GetType() == FormValue::VALUE_RADIOCHECK;

					OpString oldvalue, newvalue;
					BOOL oldischecked = FALSE, newischecked = FALSE;

					if (is_checkbox_or_radio)
						oldischecked = FormValueRadioCheck::GetAs(formvalue)->IsChecked(element);
					else if (!is_select)
						RETURN_IF_ERROR(element->GetFormValue()->GetValueAsText(element, oldvalue));

					element->ReplaceFormValue(formvalue = state->value);
					state->value = NULL;

					if (is_checkbox_or_radio)
						newischecked = FormValueRadioCheck::GetAs(formvalue)->IsChecked(element);
					else if (!is_select)
						RETURN_IF_ERROR(element->GetFormValue()->GetValueAsText(element, newvalue));

					BOOL changed = FALSE;

					if (is_select)
					{
						FormValueList *list = FormValueList::GetAs(element->GetFormValue());
						HTML_Element *select = element, *iter = element->NextActual(), *stop = element->NextSiblingActual();
						DocumentFormElementState::Option *option = state->options;
						int index = 0;

						while (iter != stop)
						{
							if (iter->IsMatchingType(HE_OPTION, NS_HTML))
							{
								BOOL selected = list->IsSelectedElement(select, iter);

								if (!selected == (option && option->index == index && option->Matches(iter)))
								{
									list->SelectValue(select, index, !selected);
									changed = TRUE;
								}

								if (option && option->index == index)
									option = option->next;

								++index;
							}

							iter = iter->NextActual();
						}
					}
					else if (is_checkbox_or_radio)
						changed = !newischecked != !oldischecked;
					else
						changed = oldvalue != newvalue;

					// We hadn't saved it if it hadn't been changed
					formvalue->SetIsChangedFromOriginal(element);

					if (state->signal && changed)
						RETURN_IF_ERROR(pause = doc->SignalFormValueRestored(element));
					else
						pause = OpBoolean::IS_FALSE;
				}

		state->Out();
		OP_DELETE(state);

		if (pause == OpBoolean::IS_TRUE)
			return pause;
	}

	return OpBoolean::IS_FALSE;
}

FramesDocElm *
DocumentState::FindIFrame(HTML_Element *element)
{
	OP_NEW_DBG("DocumentState::FindIFrame", "doc");
	OP_DBG((UNI_L("this=%p; element=%p"), this, element));
	for (DocumentFrameState *state = (DocumentFrameState *) iframe_states.First(); state; state = (DocumentFrameState *) state->Suc())
		if (state->path->Match(element))
		{
			FramesDocElm *fdelm = state->fdelm;
			state->fdelm = NULL;

			state->Out();
			OP_DELETE(state);

			fdelm->Under(doc->GetIFrmRoot());

			fdelm->SetHtmlElement(element);

			return fdelm;
		}

	return NULL;
}
