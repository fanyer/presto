/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef TEXTDATA_H
#define TEXTDATA_H

#include "modules/hardcore/mem/mem_man.h"

#define NEW_TextData(x)			OP_NEW(TextData, x)
#define DELETE_TextData(d)      OP_DELETE(d)

/**
 * A wrapper class that stores a chunk of text in a document. At most
 * SplitTextLen characters (currently ~32KB). It will resolve entities
 * if asked.
 */
class TextData
{
	OP_ALLOC_ACCOUNTED_POOLING
	OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT

private:

	uni_char*		m_text;

	union
	{
		struct
		{
			unsigned int
					text_len:15;
			unsigned int
					cdata:1;
			unsigned int
					unterminated_entity:1;
#if 0 // saved to show how many free bits we have
			unsigned int
					unused:15;
#endif // 0
		} packed; // 32 bits
		unsigned int
					packed_init;
	};

public:

					TextData() : m_text(NULL), packed_init(0) {}
                    ~TextData();

	/** Create a text content object.
	 *	@param[IN] new_text Start of text content. Can be NULL, can contain nul characters, need not be nul terminated.
	 *	@param[IN] new_text_len Length of text buffer.
	 *	@param[IN] resolve_entities If TRUE entity references will be dereferenced.
	 *	@param[IN] is_cdata TRUE if this is cdata rather than ordinary text. Matters in scripts, not much elsewhere.
	 *	@param[IN] mark_as_unfinished If TRUE, this is just a short text piece with an undecoded entity.
	 *	@returns Normal OOM values. */
	OP_STATUS       Construct(const uni_char* new_text, unsigned int new_text_len, BOOL resolve_entities, BOOL is_cdata, BOOL mark_as_unfinished);
    OP_STATUS       Construct(TextData *src_txt);

	/** @returns A pointer to the text content or NULL if none. */
	uni_char*		GetText() { return m_text; }
	/** @returns The length of the text content. */
	short			GetTextLen() { return packed.text_len; }

	BOOL			GetIsCDATA() const { return packed.cdata; }

	BOOL			IsUnterminatedEntity() { return packed.unterminated_entity; }
	const uni_char*	GetUnresolvedEntity() { OP_ASSERT(IsUnterminatedEntity()); return m_text + packed.text_len+1; }
};

#endif // TEXTDATA_H
