/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef HAS_SET_HTTP_DATA
#include "modules/upload/upload.h"
#include "modules/formats/encoder.h"
#include "modules/formats/base64_decode.h"
#include "modules/libcrypto/include/OpRandomGenerator.h"
#include "modules/selftest/selftest_module.h"

#include "modules/olddebug/tstdump.h"

Boundary_Item::~Boundary_Item()
{
	if(InList())
		Out();
}

void Boundary_Item::GenerateL(Boundary_List *base, int len)
{
	const char *generator_template = "----------";
	int generator_template_len = STRINGLENGTH("----------");
	int generate_len = 24;

	if(base)
	{
		OpStringC8 base_boundary = base->GetBoundaryL();
		int len2 = base_boundary.Length();
		if(len2> 0)
		{
			generate_len = len2 + 8;
			generator_template = base_boundary.CStr();
			generator_template_len = len2;
		}
	}
	else if(len> generate_len)
		generate_len = len;

	char *boundary_target = boundary_string.ReserveL(generate_len+1);

	OP_ASSERT(generator_template_len < generate_len);

	op_strlcpy(boundary_target, generator_template, generate_len+1);
	boundary_target += generator_template_len;
	generate_len -= generator_template_len;

	while(generate_len > 0)
	{
		UINT32 random;
#ifdef SELFTEST
		if (g_selftest.running)
			random = (UINT32)g_selftest.deterministic_rand();
		else
#endif
		random = g_libcrypto_random_generator->GetUint32();
		*(boundary_target++) = Base64_Encoding_chars[random % 62];
		generate_len --;
	}
	*boundary_target = '\0';

	boundary_string_length = boundary_string.Length();

	SignalActionL(BOUNDARY_GENERATE_ACTION);
}

Boundary_Compare_Status Boundary_Item::ScanForBoundary(const unsigned char *buffer, uint32 buffer_len, uint32 &committed)
{
	committed = 0;

	if (buffer == NULL || buffer_len == 0 || boundary_string.IsEmpty())
		return Boundary_Not_Found;

	const char *boundary_start = boundary_string.CStr();
	const char *boundary_pos = boundary_start; 
	const unsigned char *buffer_pos = buffer;
	uint32 pos = 0;

	char boundary_char;

	boundary_char= *boundary_pos;
	while(pos < buffer_len)
	{
		pos++;
		if(boundary_char == *(buffer_pos++))
		{
			boundary_char = *(++boundary_pos);

			if(boundary_char == '\0')
				return Boundary_Match;
		}
		else
		{
			// Assumes that no substrings within the boundary matches the start of the boundary
			if(boundary_pos != boundary_start)
			{
				boundary_pos = boundary_start; 
				boundary_char= *boundary_pos;
			}
			committed = pos;
		}
	}

	return (boundary_pos != boundary_start ? Boundary_Partial_Match : Boundary_Not_Found);
}

uint32 Boundary_Item::Length(Boundary_Kind boundary_kind)
{
	switch (boundary_kind)
	{
		case Boundary_First:
			return boundary_string_length + 4; /* "--"+boundary_string+"\r\n" */
		case Boundary_Internal:
			return boundary_string_length + 6; /* "\r\n"+"--"+boundary_string+"\r\n" */
		case Boundary_Last:
			return boundary_string_length + 8; /* "\r\n"+"--"+boundary_string+"--"+"\r\n" */
		case Boundary_First_Last:
			return boundary_string_length + 6; /* "--"+boundary_string+"--"+"\r\n" */
	}
	OP_ASSERT(!"Invalid call to Boundary_Item::Length()");
	return 0;
}

unsigned char *Boundary_Item::WriteBoundary(unsigned char *target, uint32 &remaining_len, Boundary_Kind boundary_kind)
{
	if(target == NULL)
		return NULL;

	OP_ASSERT(remaining_len >= Length(boundary_kind));

	if (boundary_kind != Boundary_First && boundary_kind != Boundary_First_Last)
	{
		*(target++) = '\r';
		*(target++) = '\n';
		remaining_len -= 2;
	}
	*(target++) = '-';
	*(target++) = '-';
	remaining_len -= 2;

	if (boundary_string_length)
	{
		op_memcpy(target, boundary_string.CStr(), boundary_string_length);
		target += boundary_string_length;
		remaining_len -= boundary_string_length;
	}

	if (boundary_kind == Boundary_Last || boundary_kind == Boundary_First_Last)
	{
		*(target++) = '-';
		*(target++) = '-';
		remaining_len -= 2;
	}
	*(target++) = '\r';
	*(target++) = '\n';
	remaining_len -= 2;

	return target;
}

Boundary_List::~Boundary_List()
{
	RemoveAll();
}

void Boundary_List::GenerateL()
{
	Boundary_Item *item, *current;

	current = First();
	while(current)
	{
		current->GenerateL(this);

		item = current->Pred();
		while(item)
		{
			if(current->Boundary().Compare(item->Boundary()) == 0) break;

			item = item->Pred();
		}
		if(!item)
			current = current->Suc();
	}
}


Header_Boundary_Parameter::Header_Boundary_Parameter()
	: boundary(this), quote_boundary(FALSE) 
{
}

Header_Boundary_Parameter::~Header_Boundary_Parameter()
{
}

void Header_Boundary_Parameter::InitL(const OpStringC8 &p_name, Boundary_Item *p_boundary, BOOL quote_value)
{
	Header_Parameter::InitL(p_name, NULL, FALSE);

	boundary = p_boundary;
	quote_boundary = quote_value;
}

void Header_Boundary_Parameter::TwoWayPointerActionL(
	TwoWayPointer_Target *action_source,
	uint32 action_val)
{
	OP_ASSERT((Boundary_Item*)action_source == (Boundary_Item*)boundary);
	if(action_val == Boundary_Item::BOUNDARY_GENERATE_ACTION)
	{
		OP_ASSERT(boundary.get() != NULL);
		if(boundary.get())
		{
			SetValueL(boundary->Boundary(), quote_boundary);
		}
	}
}


#endif // HTTP_data
