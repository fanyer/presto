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

#include "modules/olddebug/tstdump.h"

Header_Item::Header_Item(Header_Parameter_Separator sep, BOOL is_external)
:  name_separator(SEPARATOR_COLON), enabled(TRUE), temp_disabled(TEMP_DISABLED_NO), parameters(sep), is_external(is_external)
{
}

Header_Item::~Header_Item()
{
	if(InList())
		Out();
}

void Header_Item::SetSeparator(Header_Parameter_Separator sep)
{
	parameters.SetSeparator(sep);
}

void Header_Item::AddParameterL(const OpStringC8 &p_value)
{
	parameters.AddParameterL(p_value);
}

void Header_Item::AddParameterL(const OpStringC8 &p_name, const OpStringC8 &p_value, BOOL quote_value)
{
	parameters.AddParameterL(p_name, p_value, quote_value);
}

void Header_Item::AddParameter(Header_Parameter_Base *param)
{
	parameters.AddParameter(param);
}

void Header_Item::InitL(const OpStringC8 &h_name)
{
	header_name.SetL(h_name);
	parameters.Clear();
}

void Header_Item::InitL(const OpStringC8 &h_name, const OpStringC8 &p_value)
{
	header_name.SetL(h_name);
	parameters.Clear();
	AddParameterL(p_value);
}

uint32 Header_Item::CalculateLength()
{
	if ((!IsEnabled() && temp_disabled != TEMP_DISABLED_SIGNAL_REMOVE) || temp_disabled == TEMP_DISABLED_YES)
		return 0;
 
	uint32 len = 0;
	uint32 templen;
	uint32 add_crlf = 0;

	if (header_name.HasContent() && (!parameters.Empty() || name_separator == SEPARATOR_SPACE ||
		temp_disabled == TEMP_DISABLED_SIGNAL_REMOVE))
	{
		len = header_name.Length();
		if (!parameters.Empty() || temp_disabled == TEMP_DISABLED_SIGNAL_REMOVE)
		{
			len += name_separator != SEPARATOR_SPACE ? STRINGLENGTH(": ") : STRINGLENGTH(" ");
			if(name_separator != SEPARATOR_SPACE)
				add_crlf = 2;
		}
	}
	if( temp_disabled == TEMP_DISABLED_SIGNAL_REMOVE )
		templen = STRINGLENGTH("-");
	else
		templen = parameters.CalculateLength();

	if(templen > 0 && name_separator != SEPARATOR_SPACE)
		add_crlf = 2; 

	return len + templen + add_crlf;
}

char *Header_Item::OutputHeader(char *target)
{
	BOOL add_crlf = FALSE;

	*target = '\0'; // Null terminate in case of no output

	if ((!IsEnabled() && temp_disabled != TEMP_DISABLED_SIGNAL_REMOVE) || temp_disabled == TEMP_DISABLED_YES)
	{
		temp_disabled = TEMP_DISABLED_NO;
		return target;
	}

	if (header_name.HasContent() && (!parameters.Empty() || name_separator == SEPARATOR_SPACE ||
		temp_disabled == TEMP_DISABLED_SIGNAL_REMOVE))
	{
		op_strcpy(target, header_name.CStr());
		target += header_name.Length();
		if (!parameters.Empty() || temp_disabled == TEMP_DISABLED_SIGNAL_REMOVE)
		{
			op_strcpy(target, (name_separator != SEPARATOR_SPACE ? ": " : " "));
			target += (name_separator != SEPARATOR_SPACE ? STRINGLENGTH(": ") : STRINGLENGTH(" "));
			add_crlf = (name_separator != SEPARATOR_SPACE);
		}
	}
	
	return GetValue(target, add_crlf);
}

char *Header_Item::GetValue(char *target, BOOL add_crlf)
{
	*target = '\0'; // Null terminate in case of no output

	if ((!IsEnabled() && temp_disabled != TEMP_DISABLED_SIGNAL_REMOVE) || temp_disabled == TEMP_DISABLED_YES)
	{
		temp_disabled = TEMP_DISABLED_NO;
		return target;
	}

	if (!parameters.Empty() || temp_disabled == TEMP_DISABLED_SIGNAL_REMOVE)
	{
		if( temp_disabled == TEMP_DISABLED_SIGNAL_REMOVE )
		{
			op_strcpy(target,"-");
			target += STRINGLENGTH("-");
			temp_disabled = TEMP_DISABLED_NO;
		}
		else
			target = parameters.OutputParameters(target);

		add_crlf = (name_separator != SEPARATOR_SPACE);
	}

	if(add_crlf)
	{
		*(target++)= '\r';
		*(target++)= '\n';
		*target    = '\0';
	}

	return target;
}

#endif // HTTP_data
