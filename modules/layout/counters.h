/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

/** @file counters.h
 *
 * class prototypes for CSS Counters
 *
 */

#ifndef _COUNTER_H_
#define _COUNTER_H_

class ContentGenerator;

enum CounterElementType {
	COUNTER_RESET,
	COUNTER_INCREMENT,
	COUNTER_USE
};

class CounterElement
  : public Link
{
private:
	HTML_Element*		element;
	CounterElementType	type;
	int					value;

public:
						CounterElement(HTML_Element* html_element, CounterElementType counter_type, int counter_value)
						  : element(html_element),
							type(counter_type),
							value(counter_value) {}

	HTML_Element*		GetElement() const { return element; }
	CounterElementType	GetType() const { return type; }
	int					GetValue() const { return value; }

	CounterElement*		Recalculate(FramesDocument* doc, HTML_Element* root_element, int new_value);

	CounterElement*		Suc() { return (CounterElement*) Link::Suc(); }
	CounterElement*		Pred() { return (CounterElement*) Link::Pred(); }
};

class Counter
  : public Link
{
private:
	uni_char*	name;
	unsigned int
				name_length;

	Head		list; // list of CounterElement objects
	BOOL		need_recalculate;

public:
				Counter(const uni_char* counter_name);
				~Counter() { list.Clear(); OP_DELETEA(name); }

	const uni_char*	GetName() const { return name; }
	unsigned int GetNameLength() const { return name_length; }

	CounterElement*	SetCounter(HTML_Element* element, CounterElementType type, int value, BOOL add_only);

	OP_STATUS	AddCounterValues(OpString& counter_str, HTML_Element* element, const uni_char* delimiter, unsigned int delimiter_length, short style_type, BOOL add_only);

	BOOL		RemoveElement(HTML_Element* element);

	void		Recalculate(FramesDocument* doc);
};

class Counters
{
private:
	Head		counter_list; // list of Counter objects
	BOOL		need_recalculate;
	BOOL		add_only;

	Counter*	GetCounter(const uni_char* name, unsigned int name_length);

public:
				Counters()
				  : need_recalculate(FALSE),
					add_only(FALSE) {}
				~Counters() { Clear(); }

	void		Clear() { counter_list.Clear(); }

	void		SetAddOnly(BOOL value) { add_only = value; }

	OP_STATUS	SetCounters(HTML_Element* element, CSS_decl* cp, BOOL reset);

	OP_STATUS	AddCounterValues(OpString& counter_str, HTML_Element* element, short counter_type, const uni_char* counter_value);

	void		RemoveElement(HTML_Element* element);

	void		Recalculate(FramesDocument* doc);
};

#endif // _COUNTER_H_
