/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef HTML5ATTR_H
#define HTML5ATTR_H

#include "modules/logdoc/src/html5/html5buffer.h"
#include "modules/logdoc/src/html5/html5tokenbuffer.h"
#include "modules/logdoc/src/html5/attrhashbase.h" // HTML5_ATTR_HASH_BASE

/**
 * This class contains information about an attribute in a token during parsing.
 * These will be reused over and over, so they have been optimized for speed rather
 * than freeing memory between each use.
 */
class HTML5Attr
{
public:
	HTML5Attr() : m_name(1024) { m_name.SetHashBase(HTML5_ATTR_HASH_BASE); }

	HTML5TokenBuffer*	GetName() { return &m_name; }
	const uni_char*		GetNameStr() const { return m_name.GetBuffer(); }
	unsigned			GetNameLength() const { return m_name.Length(); }
	const 		HTML5StringBuffer&	GetValue() const { return m_value; }

	/// Clears content, does not free any memory
	void		Clear() { m_name.Clear(); m_value.Clear(); }
	
	void		AppendToNameL(uni_char character) { m_name.AppendAndHashL(character); }
	
	void		AppendToValueL(const HTML5Buffer* buffer, const uni_char* position_in_buffer, unsigned length = 1) { m_value.AppendL(buffer, position_in_buffer, length); }

	/// Takes over the memory of the other attribute (and sets its buffers to NULL)
	/// Used when resizing attribute array and we just want to transfer the attributes
	/// from one array to the other
	void		TakeOver(HTML5Attr& take_over_from) { m_name.TakeOver(take_over_from.m_name); m_value = take_over_from.m_value; }
private:
	HTML5TokenBuffer	m_name;
	HTML5StringBuffer	m_value;
	
	static const unsigned		MIN_BUFFER_LENGTH = 16;
};

#endif // HTML5ATTR_H
