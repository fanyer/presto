/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "modules/util/linkobjectstore.h"

#include "modules/formats/uri_escape.h"
#include "modules/util/simset.h"

#ifdef FORMATS_URI_ESCAPE_SUPPORT

unsigned short
LinkObjectStore::GetHashIdx(const char* str, unsigned short max_idx, unsigned int &full_index)
{
	if (!*str)
	{
		full_index = 0;
		return 0;
	}

	// Calculate a hashing index based on the input string, unescaping any URL escapes as we go along
	UriUnescapeIterator it(str, UriUnescape::All);
	unsigned int val = 0;
	while (it.More())
	{
		register unsigned char a, b, c, d;
		register unsigned int val1;

		a = it.Next();
		b = (it.More() ? it.Next() : 0);
		c = (it.More() ? it.Next() : 0);
		d = (it.More() ? it.Next() : 0);

		val1 = (((unsigned int) a) << 24) | (((unsigned int) b) << 16) | (((unsigned int) c) << 8) | d;
		val = 33*val ^ val1;
	}

	full_index = val;
	return (unsigned short)(val % max_idx);
}

static int simset_strcmp_esc(const char *s1, const char *s2)
{
	return UriUnescape::strcmp(s1, s2, UriUnescape::Safe);
}

#else // FORMATS_URI_ESCAPE_SUPPORT

unsigned short
LinkObjectStore::GetHashIdx(const char* str, unsigned short max_idx, unsigned int &full_index)
{
	unsigned int val = 0;
	if (*str)
	{
		// Calculate a hashing index based on the input string, unescaping
		// any URL escapes as we go along
		register unsigned int val1;
		register const unsigned char* tmp = (const unsigned char*) str;
		while (*tmp)
		{
			register unsigned char val_a, val_b, val_c, val_d;
			val1 = 0;

			val_a = tmp[0];
			val_b = tmp[1];
			val_c = (val_b ? tmp[2] : 0);
			val_d = (val_c ? tmp[3] : 0);

			if(val_a != '%' && val_b != '%' && val_c != '%' && val_d != '%')
			{
				// Simple hashing if there are no escaped characters in
				// this chunk.
				val1 = (((unsigned int) val_a) << 24) | (((unsigned int) val_b) << 16) | (((unsigned int) val_c) << 8) | val_d;
				if(val_d)
				{
					tmp+=4;
				}
				else
				{
					val = 33*val ^ val1;
					break;
				}
			}
			else
			{
				// Hash each character by itself when there are escaped
				// characters.
				int len1;
				register unsigned char val2;
				for(len1 = 4; len1 > 0 && *tmp;len1--,tmp++)
				{
					val1 <<= 8;
					val2 = *tmp;
					if(val2 =='%' && tmp[1] && tmp[2])
					{
						// Unescape only if it is a proper hexadecimal escape.
						if (uni_isxdigit(tmp[1]) && uni_isxdigit(tmp[2]))
						{
							val2 = GetEscapedChar(tmp[1], tmp[2]);
							tmp += 2;
						}
					}
					val1 |= val2;
				}
				if(len1)
				{
					val1 <<= (len1 << 3); // multiply by 8
				}
			}
			val = 33*val ^ val1;
		}

	}

	full_index = val;
	return (unsigned short)(val % max_idx);
}

#endif // FORMATS_URI_ESCAPE_SUPPORT

LinkObjectStore::LinkObjectStore(unsigned int size, int (*cmpr)(const char *, const char *)) :
	store_array(NULL),
	store_size(size),
	last_idx(0),
	last_link_obj(0),
	count(0),
	compare(cmpr ? cmpr :
#ifdef FORMATS_URI_ESCAPE_SUPPORT
		simset_strcmp_esc
#else
		NULL
#endif // FORMATS_URI_ESCAPE_SUPPORT
	)
{
}

OP_STATUS
LinkObjectStore::Construct()
{
	store_array = OP_NEWA(Head, store_size);
	return store_array ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

LinkObjectStore::~LinkObjectStore()
{
	OP_DELETEA(store_array);
}

HashedLink*
LinkObjectStore::GetLinkObject(const char* id, unsigned int *ret_index)
{
	if (!id)
		return NULL;

	unsigned int full_index = 0;
	unsigned int hash_idx = GetHashIdx(id, store_size, full_index);

	if(ret_index)
		*ret_index = full_index;

	HashedLink* obj = (HashedLink *) store_array[hash_idx].First();
	while (obj)
	{
		if(obj->GetIndex() == full_index)
		{
			const char *label = obj->LinkId();
			if (label && (*compare)(label, id) == 0)
				return obj;
		}
		obj = obj->Suc();
	}

	return NULL;
}

HashedLink*
LinkObjectStore::GetFirstLinkObject()
{
  	last_idx = 0;
  	last_link_obj = 0;

	return count ? GetNextLinkObject() : NULL;
}

HashedLink*
LinkObjectStore::GetNextLinkObject()
{
	while (last_idx < store_size)
	{
		if (!last_link_obj)
			last_link_obj = (HashedLink *) store_array[last_idx].First();
		else
			last_link_obj = last_link_obj->Suc();

		if (last_link_obj)
			return last_link_obj;
		else
			last_idx++;
	}

	return NULL;
}

void
LinkObjectStore::RemoveLinkObject(HashedLink* obj)
{
	if (!obj)
		return;

	if (last_link_obj == obj)
		last_link_obj = last_link_obj->Pred();

	if (obj->InList())
	{
		obj->Out();
	}

	if (count)
		count--;
}

void
LinkObjectStore::AddLinkObject(HashedLink* obj, unsigned int *pre_index)
{
	if (!obj || !obj->LinkId())
		return;

	unsigned int full_index = 0;
	unsigned int hash_idx = 0;

	if(pre_index)
	{
		full_index = *pre_index;
		hash_idx = full_index % store_size;
	}
	else
		hash_idx = GetHashIdx(obj->LinkId(), store_size, full_index);

	obj->SetIndex(full_index);
	obj->Into(&(store_array[hash_idx]));
	count++;
}
