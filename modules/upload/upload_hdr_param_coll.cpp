/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef HAS_SET_HTTP_DATA
#include "modules/util/opfile/opfile.h"
#include "modules/viewers/viewers.h"

#include "modules/hardcore/mem/mem_man.h"

#include "modules/upload/upload.h"
#include "modules/formats/encoder.h"

#include "modules/olddebug/tstdump.h"

#include "modules/formats/hdsplit.h"
#include "modules/url/url_man.h"
#include "modules/util/handy.h"
#include "modules/util/cleanse.h"

Header_Parameter_Collection::Header_Parameter_Collection(Header_Parameter_Separator sep)
 : separator(sep)
{
}

void Header_Parameter_Collection::SetSeparator(Header_Parameter_Separator sep)
{
	separator = sep;
}

void Header_Parameter_Collection::AddParameterL(const OpStringC8 &p_value)
{
	OpStackAutoPtr<Header_Parameter> new_parameter(OP_NEW_L(Header_Parameter, ()));

	new_parameter->InitL(p_value);

	new_parameter->Into(this);

	new_parameter.release();
}

void Header_Parameter_Collection::AddParameterL(const OpStringC8 &p_name, const OpStringC8 &p_value, BOOL quote_value)
{
	OpStackAutoPtr<Header_Parameter> new_parameter(OP_NEW_L(Header_Parameter, ()));

	new_parameter->InitL(p_name, p_value, quote_value);

	new_parameter->Into(this);

	new_parameter.release();
}

void Header_Parameter_Collection::AddParameter(Header_Parameter_Base *param)
{
	if(param)
	{
		if(param->InList())
			param->Out();
		param->Into(this);
	}
}


uint32 Header_Parameter_Collection::CalculateLength() const
{
	uint32 len = 0; 
	uint32 sep_len = SeparatorLength();
	Header_Parameter_Base *item = First();

	while(item)
	{
		uint32 temp_len;

		temp_len = item->CalculateLength();

		item = item->Suc();

		if(temp_len && item) 
			temp_len += sep_len;

		len += temp_len;
	}

	return len;
}

char *Header_Parameter_Collection::OutputParameters(char *target) const
{
	Header_Parameter_Base *item = First();

	*target = '\0'; // Null terminate in case of no output

	while(item)
	{
		BOOL display = !item->IsEmpty();
		if(display)
			target = item->OutputParameters(target);

		item = item->Suc();

		if(display && item) 
			target = OutputSeparator(target);
	}

	return target;
}


BOOL Header_Parameter_Collection::IsEmpty()
{
	Header_Parameter_Base *item = First();

	while(item)
	{
		if(!item->IsEmpty())
			return FALSE;

		item = item->Suc();
	}

	return TRUE;
}

uint32 Header_Parameter_Collection::SeparatorLength() const
{
	switch(separator)
	{
	case SEPARATOR_SPACE: // " "
		return STRINGLENGTH(" ");
	case SEPARATOR_COMMA: // ", "
		return STRINGLENGTH(", ");
	case SEPARATOR_SEMICOLON:  // ", "
		return STRINGLENGTH("; ");
	case SEPARATOR_NEWLINE:
		return STRINGLENGTH("\r\n ");
	case SEPARATOR_COMMA_NEWLINE:
		return STRINGLENGTH(",\r\n ");
	case SEPARATOR_SEMICOLON_NEWLINE:
		return STRINGLENGTH(";\r\n ");
	}

	return 0;
}

char *Header_Parameter_Collection::OutputSeparator(char *target) const
{
	const char *sep_string = NULL;
	int sep_len = 0;

	switch(separator)
	{
	case SEPARATOR_SPACE: // " "
		sep_string = " ";
		sep_len = STRINGLENGTH(" ");
		break;
	case SEPARATOR_COMMA: // ", "
		sep_string = ", ";
		sep_len = STRINGLENGTH(", ");
		break;
	case SEPARATOR_SEMICOLON:  // ", "
		sep_string = "; ";
		sep_len = STRINGLENGTH("; ");
		break;
	case SEPARATOR_NEWLINE:
		sep_string = "\r\n ";
		sep_len = STRINGLENGTH("\r\n ");
		break;
	case SEPARATOR_COMMA_NEWLINE:
		sep_string = ",\r\n ";
		sep_len = STRINGLENGTH(",\r\n ");
		break;
	case SEPARATOR_SEMICOLON_NEWLINE:
		sep_string = ";\r\n ";
		sep_len = STRINGLENGTH(";\r\n ");
		break;
	}

	if(sep_string)
	{
		op_strcpy(target, sep_string);
		target += sep_len;
	}

	return target;
}

#endif // HTTP_DATA
