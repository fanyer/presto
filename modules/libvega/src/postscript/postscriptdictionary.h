/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * @author Anders Karlsson (pugo)
 */

#ifndef POSTSCRIPT_DICTIONARY_H
#define POSTSCRIPT_DICTIONARY_H

#ifdef VEGA_POSTSCRIPT_PRINTING

class PostScriptDictionary
{
public:
	OP_STATUS add(const OpStringC8& key, const OpStringC8& value);
	OP_STATUS add(const OpStringC8& key, long value);

	OpStringC8 getValue(const OpStringC8& key) const;
	unsigned int size() const;

	OP_STATUS generate(OpString8& result);

private:
	struct DictionaryItem
	{
		OpString8 key;
		OpString8 value;
	};

	OpAutoVector<DictionaryItem> dictionary;
};

#endif // VEGA_POSTSCRIPT_PRINTING

#endif // POSTSCRIPT_DICTIONARY_H
