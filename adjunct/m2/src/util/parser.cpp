/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2/src/util/parser.h"


/***********************************************************************************
 ** Create a new StringParseElement based on an existing string to copy
 **
 ** StringParseElement::CopyText
 ** @param tocopy String to copy
 ** @param target Where to put pointer to copied string
 ** @param parser Parser that should hold parse element
 ***********************************************************************************/
void StringParseElement::CopyText(const char* tocopy, int length, const char*& target, GenericIncomingParser* parser)
{
	StringParseElement* holder = parser->AddElement(OP_NEW(StringParseElement, ()));
	if (holder)
	{
		holder->m_string.Set(tocopy, length);
		target = holder->m_string.CStr();
	}
	else
	{
		target = 0;
	}
}


/***********************************************************************************
 ** Constructor
 **
 ** GenericIncomingParser::GenericIncomingParser
 ***********************************************************************************/
GenericIncomingParser::GenericIncomingParser()
  : m_parse_stack_first(NULL)
  , m_parse_stack_last(NULL)
  , m_needs_reset(FALSE)
  , m_required_state(0)
  , m_readpos(0)
  , m_current_item(0)
  , m_last_eol(0)
  , m_last_eol_item(0)
{
}


/***********************************************************************************
 ** Destructor
 **
 ** GenericIncomingParser::~GenericIncomingParser
 **
 ***********************************************************************************/
GenericIncomingParser::~GenericIncomingParser()
{
	// Cleanup parse stack
	while (m_parse_stack_first)
	{
		ParseItem* next = m_parse_stack_first->m_next;
		OP_DELETE(m_parse_stack_first);
		m_parse_stack_first = next;
	}
}


/***********************************************************************************
 ** Add a string to the strings to-be-parsed, takes ownership of the string
 **
 ** GenericIncomingParser::AddParseItem
 ** @param data String to be parsed
 ** @param length Length of string data
 **
 ***********************************************************************************/
OP_STATUS GenericIncomingParser::AddParseItem(char* data, unsigned length)
{
	// Replace NULL-bytes in string
	char* data_scanner = op_strchr(data, '\0');
	while (data_scanner && (data_scanner - data < (int)length))
	{
		*data_scanner = ' ';
		data_scanner = op_strchr(data_scanner + 1, '\0');
	}

	// Create new item
	ParseItem* item = OP_NEW(ParseItem, (data, length));
	if (!item)
	{
		OP_DELETEA(data);
		return OpStatus::ERR_NO_MEMORY;
	}

	// Put on parse stack
	if (!m_parse_stack_last)
	{
		m_parse_stack_first = item;
	}
	else
	{
		m_parse_stack_last->m_next = item;
		item->m_prev = m_parse_stack_last;
	}

	m_parse_stack_last = item;

	// Make sure invariants for readpos and m_last_eol are maintained
	UpdateReader();

	return OpStatus::OK;
}


/***********************************************************************************
 ** Remove parse stack items that are already processed
 **
 ** GenericIncomingParser::Reset
 **
 ***********************************************************************************/
void GenericIncomingParser::Reset()
{
	// Cleanup parse stack
	while (m_parse_stack_first)
	{
		ParseItem* next = m_parse_stack_first->m_next;
		OP_DELETE(m_parse_stack_first);
		m_parse_stack_first = next;
	}
	m_parse_stack_first = NULL;
	m_parse_stack_last  = NULL;

	// Clean reader
	UpdateReader();

	// Reset tokenizer state
	SetNeedsReset(TRUE);

	// Reset temporary buffer
	m_temp_buffer.Reset();
}


/***********************************************************************************
 ** Add an element for this command, takes ownership of element
 **
 ** GenericIncomingParser::AddElementInternal
 ** @param element The element to add, will be deleted if failed
 ** @return The added element, or NULL if failed
 **
 ***********************************************************************************/
ParseElement* GenericIncomingParser::AddElementInternal(ParseElement* element)
{
	if (element && OpStatus::IsError(m_elements.Add(element)))
	{
		OP_DELETE(element);
		element = NULL;
	}
	return element;
}


/***********************************************************************************
 ** Read data from the parse stack into a buffer
 **
 ** GenericIncomingParser::GetParseInput
 ** @param buf Where to place data
 ** @param result Number of bytes placed in buf after function completed
 ** @param max_size Maximum number of bytes to be placed in buf
 ***********************************************************************************/
void GenericIncomingParser::GetParseInput(char* buf, int& result, unsigned max_size)
{
	result = 0;

	while (max_size > 0 && HasInput())
	{
		// Calculate how much to copy
		size_t readpos_length;
		size_t tocopy;

		if (m_current_item == m_last_eol_item)
			readpos_length = (m_last_eol + 1) - m_readpos;
		else
			readpos_length = m_current_item->m_length - (m_readpos - m_current_item->m_data);

		tocopy = min(max_size, readpos_length);

		// Do the copy
		op_memcpy(buf + result, m_readpos, tocopy);

		// Update reader position
		if (tocopy == m_current_item->m_length - (m_readpos - m_current_item->m_data))
			SetReadPos(m_current_item->m_next);
		else
			SetReadPos(m_current_item, m_readpos + tocopy);

		// Update result
		result	 += tocopy;
		max_size -= tocopy;
	}
}


/***********************************************************************************
 ** Remove parse stack items that are already processed
 **
 ** GenericIncomingParser::CleanupParseStack
 **
 ***********************************************************************************/
void GenericIncomingParser::CleanupParseStack()
{
	ParseItem* parse_item = m_parse_stack_first;

	// All items that are read by the parser can be safely deleted
	while (parse_item && (!m_current_item || parse_item != m_current_item))
	{
		ParseItem* item_to_delete = parse_item;

		parse_item = parse_item->m_next;
		OP_DELETE(item_to_delete);
	}

	// Bookkeeping
	m_parse_stack_first = parse_item;
	if (parse_item)
		parse_item->m_prev = NULL;
	else
		m_parse_stack_last = NULL;
}


/***********************************************************************************
 ** Set read position of reader
 **
 ** GenericIncomingParser::Next
 ** @param item Item where read position is in
 ** @param readpos New read position, inside item
 ***********************************************************************************/
void GenericIncomingParser::SetReadPos(ParseItem* item, const char* readpos)
{
	if (!readpos && item)
		readpos = item->m_data;

	if (m_current_item == m_last_eol_item && (m_current_item != item || readpos > m_last_eol))
		SetLastEOL(0, 0);

	m_current_item = item;
	m_readpos      = readpos;

	CleanupParseStack();
}


/***********************************************************************************
 ** Update reader variables when parse stack has changed
 **
 ** GenericIncomingParser::UpdateReader
 ***********************************************************************************/
void GenericIncomingParser::UpdateReader()
{
	// Update read position
	if (!m_parse_stack_first || !m_readpos)
		SetReadPos(m_parse_stack_first);

	// Update EOL
	SetLastEOL(0, 0);
	ParseItem* item = m_parse_stack_last;

	while (!m_last_eol && item)
	{
		const char* search = item->m_data + (item->m_length - 1);
		const char* limit  = item == m_current_item ? m_readpos : item->m_data;

		while (search >= limit && *search != '\n')
			search--;

		if (search >= limit)
			SetLastEOL(item, search);

		item = item == m_current_item ? 0 : item->m_prev;
	}
}


#endif // M2_SUPPORT
