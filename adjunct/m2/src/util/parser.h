/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef PARSER_H
#define PARSER_H

#include "adjunct/desktop_util/adt/opqueue.h"
#include "adjunct/desktop_util/datastructures/StreamBuffer.h"
#include "adjunct/desktop_util/mempool/mempool.h"

class GenericIncomingParser;

/** @brief Derive temporary elements you need during parsing from this class
  */
class ParseElement
{
public:
	virtual ~ParseElement() {}
};

/** @brief Temporary parse element for 8-bit strings
  */
class StringParseElement : public ParseElement
{
public:
	/** Create a new StringParseElement based on an existing string to copy
	  * @param tocopy String to copy
      * @param length Length of string to copy
	  * @param target Where to put pointer to copied string
	  * @param parser Parser that should hold parse element
	  */
	static void CopyText(const char* tocopy, int length, const char*& target, GenericIncomingParser* parser);

private:
	OpString8 m_string;
};


/** @brief Abstract class to implement a parser
  *
  * Implement this class to have a parse stack that can be read using readers
  */
class GenericIncomingParser
{
public:
	/** Constructor
	  * @param parent IMAP4 protocol object this processor reports to
	  */
	GenericIncomingParser();

	/** Destructor
	  */
	virtual ~GenericIncomingParser();

	/** Initializer - run before using functions
	  */
	virtual OP_STATUS Init() { return OpStatus::OK; }

	/** Add a string to the strings to-be-parsed, takes ownership of the string
	  * @param data String to be parsed
	  * @param length Length of string data
	  */
	OP_STATUS AddParseItem(char* data, unsigned length);

	/** Parse and process as many received commands as possible
	 */
	virtual OP_STATUS Process() = 0;

	/** Reset the processor (removes all items in parse stack)
	  */
	virtual void Reset();

	/** Add an element for this command, takes ownership of element
	  * @param element The element to add, will be deleted if failed - should derive from ParseElement
	  * @return The added element, or NULL if failed
	  */
	template<class T> T* AddElement(T* element)
		{ return static_cast<T*>(AddElementInternal(element)); }

	/** Read data from the parse stack into a buffer
	  * @param buf Where to place data
	  * @param result Number of bytes placed in buf after function completed
	  * @param max_size Maximum number of bytes to be placed in buf
	  * @param respect_eol Whether to stop at a linebreak
	  */
	void GetParseInput(char* buf, int& result, unsigned max_size);

	/** Do we have more input
	  */
	BOOL HasInput() { return m_last_eol != 0; }

	/** Make parser reset
	  */
	void SetNeedsReset(BOOL reset) { m_needs_reset = reset; }

	/** Check if parser needs to be reset
	  */
	BOOL NeedsReset() const { return m_needs_reset; }

	/** Make parser/tokenizer enter specified state
	  */
	void SetRequiredState(int req_state) { m_needs_reset = TRUE; m_required_state = req_state; }

	/** Get the required state for the parser/tokenizer
	  */
	int  GetRequiredState() { return m_required_state; }

	StreamBuffer<char>& GetTempBuffer() { return m_temp_buffer; }

protected:
	struct ParseItem
	{
		ParseItem(char* data, size_t length) : m_data(data), m_length(length), m_next(0), m_prev(0) {}
		~ParseItem() { OP_DELETEA(m_data); }

		char*		m_data;
		size_t		m_length; 	///< length of string in m_data
		ParseItem*	m_next; 	///< next item to parse
		ParseItem*  m_prev;     ///< previous item in parse list
	};

	ParseElement* AddElementInternal(ParseElement* element);

	/** Remove parse stack items that are already processed
	  */
	void CleanupParseStack();

	/** Get the last EOL for this reader
	  * @return Position of last EOL
	  */
	const char* GetLastEOL() { return m_last_eol; }

	/** Set the last EOL for this reader
	  * @param item Item where last EOL is in, or 0 to reset
	  * @param last_eol Position of last EOL, inside item, or 0 to reset
	  */
	void SetLastEOL(ParseItem* item, const char* last_eol) { m_last_eol_item = item; m_last_eol = last_eol; }

	/** Set read position of reader
	  * @param item Item where read position is in
	  * @param readpos New read position, inside item, or 0 for start of item
	  */
	void SetReadPos(ParseItem* item, const char* readpos = 0);

	/** Update reader variables when parse stack has changed
	  */
	void UpdateReader();


	ParseItem*  m_parse_stack_first;		///< First item on the parse stack
	ParseItem*  m_parse_stack_last;			///< Last item on the parse stack
	OpAutoVector<ParseElement> m_elements;	///< Objects used while parsing, will be cleared after call to Parse()
	StreamBuffer<char> m_temp_buffer;		///< Temporary buffer can be used by external parsers
	BOOL		m_needs_reset;				///< Whether parser needs to be reset
	int			m_required_state;			///< If parser needs to be reset, go to this state

	const char* m_readpos;					///< Read position inside m_current_item
	ParseItem*	m_current_item;				///< Current item to parse
	const char* m_last_eol;					///< Last end-of-line found in parse stack
	ParseItem*	m_last_eol_item;			///< Item that contains m_last_eol
};

template<typename T>
class IncomingParser : public GenericIncomingParser
{
public:
	IncomingParser() : m_valid_queue(FALSE) {}

	virtual OP_STATUS Process()
	{
		while (TRUE)
		{
			TokenQueueItem* item = new (&m_token_queue_pool) TokenQueueItem;
			if (!item)
				return OpStatus::ERR_NO_MEMORY;

			// Get token
			item->token = GetNextToken(&item->yylval);
			if (!item->token)
			{
				m_token_queue_pool.Delete(item);
				break;
			}

			// Put token in queue
			m_token_queue.Enqueue(item);

			// Parse if possible
			if (m_valid_queue)
			{
				Parse();
				m_valid_queue = FALSE;
				m_elements.DeleteAll();
			}
		}

		return OpStatus::OK;
	}

	virtual void Reset()
	{
		GenericIncomingParser::Reset();

		TokenQueueItem* item = m_token_queue.Dequeue();
		while (item)
		{
			m_token_queue_pool.Delete(item);
			item = m_token_queue.Dequeue();
		}
	}

	/** Queue is valid (contains parseable commands)
	  */
	void MarkValidQueue() { m_valid_queue = TRUE; }

	/** Get preprocessed token (for parser)
	  */
	int GetNextProcessedToken(T* yylval, void* scanner)
	{
		TokenQueueItem* item = m_token_queue.Dequeue();
		if (!item)
			return 0;

		*yylval   = item->yylval;
		int token = item->token;

		m_token_queue_pool.Delete(item);

		return token;
	}


protected:
	/** Get next token (by using external tokenizer)
	  */
	virtual int		  GetNextToken(T* yylval) = 0;

	/** Starts parsing (by using external parser)
	  */
	virtual int		  Parse() = 0;

	class TokenQueueItem : public OpQueueItem<TokenQueueItem>
	{
	public:
		int			  token;
		T			  yylval;
	};

	MemPool<TokenQueueItem> m_token_queue_pool;
	OpQueue<TokenQueueItem> m_token_queue;
	BOOL					m_valid_queue;
};

#endif // PARSER_H
