/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/** @file counters.cpp
 *
 * Implementation of Counter class and friends
 *
 */

#include "core/pch.h"

#include "modules/layout/counters.h"
#include "modules/layout/numbers.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/layout/layout_workplace.h"

#define TMPSTRING_SIZE 101

CounterElement* CounterElement::Recalculate(FramesDocument* doc, HTML_Element* root_element, int new_value)
{
	//if (!root_element || root_element->IsAncestorOf(element))
	if (!root_element || !root_element->Parent() || root_element->Parent()->IsAncestorOf(element))
	{
		if (type == COUNTER_USE && value != new_value)
		{
			value = new_value;

			if (doc)
			{
				// Cannot mark elements inserted by layout engine extra dirty because
				// Box::LayoutChildren doesn't handle that.
				// For now mark parent extra dirty. An optimization would be to change Box::LayoutChildren.

				while (element->GetInserted() == HE_INSERTED_BY_LAYOUT)
					element = element->Parent();

				element->MarkExtraDirty(doc);
			}
		}

		if (Suc())
		{
			if (type == COUNTER_RESET)
			{
				CounterElement* next = Suc()->Recalculate(doc, element->GetIsPseudoElement() ? element->Parent() : element, value);
				if (next)
					return next->Recalculate(doc, root_element, new_value);
			}
			else
			{
				if (type == COUNTER_INCREMENT)
					new_value += value;

				return Suc()->Recalculate(doc, root_element, new_value);
			}
		}

		return NULL;
	}
	else
		return this;
}

Counter::Counter(const uni_char* counter_name)
	: name(NULL),
	  need_recalculate(FALSE)
{
	OP_ASSERT(counter_name && *counter_name && "Counter names are non-empty strings");

	name_length = uni_strlen(counter_name);
	if (!(name = UniSetNewStr(counter_name)))
		name_length = 0;
}

CounterElement* Counter::SetCounter(HTML_Element* element, CounterElementType type, int value, BOOL add_only)
{
	CounterElement* counter_element = OP_NEW(CounterElement, (element, type, value));
	if (!counter_element)
		return NULL;

	element->SetReferenced(TRUE);

	if (add_only || !list.First())
		counter_element->Into(&list);
	else
	{
		for (CounterElement* c_element = (CounterElement*) list.Last(); c_element; c_element = c_element->Pred())
		{
			if (c_element->GetElement() == element || c_element->GetElement()->Precedes(element))
			{
				counter_element->Follow(c_element);
				break;
			}
		}

		if (!counter_element->InList())
			counter_element->IntoStart(&list);
	}

	return counter_element;
}

OP_STATUS Counter::AddCounterValues(OpString& counter_str, HTML_Element* element, const uni_char* delimiter, unsigned int delimiter_length, short style_type, BOOL add_only)
{
	CounterElement* counter_element;
	if (add_only)
	{
		counter_element = (CounterElement*) list.Last();
		if (counter_element && (counter_element->GetElement() != element || counter_element->GetType() != COUNTER_USE))
			counter_element = NULL;
	}
	else
	{
		BOOL found = FALSE;

		for (counter_element = (CounterElement*) list.First(); counter_element; counter_element = counter_element->Suc())
			if (counter_element->GetElement() == element)
			{
				if (counter_element->GetType() == COUNTER_USE)
					break;

				found = TRUE;
			}
			else
				if (found)
				{
					counter_element = NULL;
					break;
				}
	}

	if (!counter_element)
	{
		counter_element = SetCounter(element, COUNTER_USE, 0, add_only);

		if (!counter_element)
			return OpStatus::ERR_NO_MEMORY;

		((CounterElement*)list.First())->Recalculate(NULL, NULL, 0);
	}

	/* Buffer for sending strings into ContentGenerator::AddString(). */
	uni_char tmp_buf[TMPSTRING_SIZE]; /* ARRAY OK 2009-05-11 davve */

	if (delimiter)
	{
		/* If delimiter is set (even if it empty), we should add the
		   complete chain of counters. */

		int prev_value = 0;
		HTML_Element* prev_counter_root = NULL;
		for (CounterElement* c_element = (CounterElement*) list.First(); c_element; c_element = c_element->Suc())
		{
			if (prev_counter_root && c_element->GetType() == COUNTER_RESET && !c_element->GetElement()->IsAncestorOf(element))
			{
				HTML_Element* this_counter_root = c_element->GetElement();

				for (c_element = c_element->Suc(); c_element; c_element = c_element->Suc())
					if (!this_counter_root->IsAncestorOf(c_element->GetElement()))
						break;

				if (!c_element)
					break;
			}

			if (c_element->GetType() == COUNTER_RESET)
			{
				if (prev_counter_root && (!prev_counter_root->Parent() || prev_counter_root->Parent()->IsAncestorOf(element)))
				{
					MakeListNumberStr(prev_value, style_type, NULL, tmp_buf, TMPSTRING_SIZE-1); //FIXME rtl?
					RETURN_IF_ERROR(counter_str.Append(tmp_buf));

					// add delimiter
					if (delimiter_length)
						RETURN_IF_ERROR(counter_str.Append(delimiter, delimiter_length));
				}

				prev_counter_root = c_element->GetElement();

				if (prev_counter_root->GetIsPseudoElement())
					prev_counter_root = prev_counter_root->Parent();
			}
			else
				if (!prev_counter_root)
				{
					prev_counter_root = c_element->GetElement();

					if (prev_counter_root->GetIsPseudoElement())
						prev_counter_root = prev_counter_root->Parent();
				}

			if (c_element->GetType() == COUNTER_INCREMENT)
				prev_value += c_element->GetValue();
			else
				prev_value = c_element->GetValue();
		}
	}

	MakeListNumberStr(counter_element->GetValue(), style_type, NULL, tmp_buf, TMPSTRING_SIZE-1); //FIXME rtl?

	return counter_str.Append(tmp_buf);
}

void Counter::Recalculate(FramesDocument* doc)
{
	if (need_recalculate)
	{
		need_recalculate = FALSE;

		if (list.First())
			((CounterElement*)list.First())->Recalculate(doc, NULL, 0);
	}
}

BOOL Counter::RemoveElement(HTML_Element* element)
{
	BOOL removed = FALSE;
	CounterElement* counter_element = (CounterElement*) list.First();

	while (counter_element)
	{
		CounterElement* next = counter_element->Suc();

		if (counter_element->GetElement() == element)
		{
			removed = TRUE;
			counter_element->Out();
			OP_DELETE(counter_element);
		}
		else
			if (removed)
				break;

		counter_element = next;
	}

	if (removed)
		need_recalculate = TRUE;

	return removed;
}

Counter* Counters::GetCounter(const uni_char* name, unsigned int name_length)
{
	for (Counter* counter = (Counter*) counter_list.First(); counter; counter = (Counter*) counter->Suc())
		if (name_length == counter->GetNameLength() && uni_strncmp(counter->GetName(), name, name_length) == 0)
			return counter;

	return NULL;
}

OP_STATUS Counters::SetCounters(HTML_Element* element, CSS_decl* cp, BOOL reset)
{
	if (cp && cp->GetDeclType() == CSS_DECL_GEN_ARRAY)
	{
		short gen_arr_len = cp->ArrayValueLen();
		const CSS_generic_value* gen_arr = cp->GenArrayValue();

		for (int i = 0; i < gen_arr_len; i++)
		{
			if (gen_arr[i].value_type == CSS_STRING_LITERAL && gen_arr[i].value.string)
			{
				OP_ASSERT(*gen_arr[i].value.string || !"Only non-empty strings (identifiers) should be allowed by the css parser");
				Counter* counter = GetCounter(gen_arr[i].value.string, uni_strlen(gen_arr[i].value.string));

				if (!counter)
				{
					counter = OP_NEW(Counter, (gen_arr[i].value.string));

					if (counter)
						counter->Into(&counter_list);
					else
						return OpStatus::ERR_NO_MEMORY;
				}

				if (!counter->GetName() ||
					!counter->SetCounter(element, reset ? COUNTER_RESET : COUNTER_INCREMENT, i + 1 < gen_arr_len && gen_arr[i + 1].value_type == CSS_INT_NUMBER ? gen_arr[++i].value.number : (reset ? 0 : 1), add_only))
				{
					counter->Out();
					OP_DELETE(counter);
					return OpStatus::ERR_NO_MEMORY;
				}
			}
		}
	}

	return OpStatus::OK;
}

static BOOL css_is_space(uni_char c)
{
	switch (c)
	{
	case ' ':
	case '\n':
	case '\r':
	case '\t':
	case '\f':
		return TRUE;
	}
	return FALSE;
}

OP_STATUS Counters::AddCounterValues(OpString& counter_str, HTML_Element* element, short counter_type, const uni_char* counter_value)
{
	if (counter_value)
	{
		const uni_char* name = counter_value;
		unsigned int name_length = 0;

		// The css parser should have stripped off the leading whitespaces.
		OP_ASSERT(!css_is_space(*name));

		const uni_char* style = name;
		unsigned int style_length = 0;

		/* 'delimiter' corresponds to the 'string' argument of the
		   counters function in the CSS 2.1. */
		const uni_char* delimiter = NULL;
		unsigned int delimiter_length = 0;

		if (counter_type == CSS_FUNCTION_COUNTERS)
		{
			delimiter = name;

			while (*delimiter && !css_is_space(*delimiter) && *delimiter != ',')
				delimiter++;

			BOOL comma_found = (*delimiter == ',');

			name_length = delimiter++ - name;

			while (css_is_space(*delimiter))
				delimiter++;

			if (!comma_found)
			{
				if (*delimiter && *delimiter != ',')
					return OpStatus::OK;

				while (css_is_space(*++delimiter))
					;
			}

			uni_char quote_char = *delimiter++;
			if (quote_char != '"' && quote_char != '\'')
				return OpStatus::OK;

			style = delimiter;
			while (*style && (*style != quote_char || *(style-1) == '\\'))
				style++;
			if (*style != quote_char)
				return OpStatus::OK;

			delimiter_length = style++ - delimiter;
		}
		else
		{
			while (*style && !css_is_space(*style) && *style != ',')
				style++;

			name_length = style - name;
		}

		while (*style && (css_is_space(*style) || *style == ','))
			style++;

		const uni_char* end = style;

		while (*end && !css_is_space(*end) && *end != ',')
			end++;

		OP_ASSERT(*end == 0);
		style_length = end - style;

		short style_type = CSS_VALUE_decimal;
		if (*style)
		{
			short keyword = CSS_GetKeyword(style, style_length);
			if (CSS_is_list_style_type_val(keyword) || keyword == CSS_VALUE_none)
				style_type = keyword;
		}

		Counter* counter = GetCounter(name, name_length);

		if (counter)
			return counter->AddCounterValues(counter_str, element, delimiter, delimiter_length, style_type, add_only);
		else
		{
			uni_char number_buf[TMPSTRING_SIZE]; /* ARRAY OK 2009-05-11 davve */

			MakeListNumberStr(0, style_type, NULL, number_buf, TMPSTRING_SIZE-1); //FIXME rtl??
			return counter_str.Append(number_buf);
		}
	}

	return OpStatus::OK;
}

void Counters::Recalculate(FramesDocument* doc)
{
	if (need_recalculate)
	{
		need_recalculate = FALSE;

		for (Counter* counter = (Counter*) counter_list.First(); counter; counter = (Counter*) counter->Suc())
			counter->Recalculate(doc);
	}
}

void Counters::RemoveElement(HTML_Element* element)
{
	if (counter_list.First())
		for (Counter* counter = (Counter*) counter_list.First(); counter; counter = (Counter*) counter->Suc())
			if (counter->RemoveElement(element))
				need_recalculate = TRUE;
}
