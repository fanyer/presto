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

Header_Parameter_Base::~Header_Parameter_Base()
{
	if(InList())
		Out();
}

Header_Parameter::Header_Parameter()
{
}

Header_Parameter::~Header_Parameter()
{
	parameter_value.Wipe();
}


void Header_Parameter::InitL(const OpStringC8 &p_value)
{
	parameter_name.Empty();
	SetValueL(p_value);
}

void Header_Parameter::InitL(const OpStringC8 &p_name, const OpStringC8 &p_value, BOOL quote_value)
{
	SetNameL(p_name);
	SetValueL(p_value, quote_value);
}

void Header_Parameter::SetNameL(const OpStringC8 &p_name)
{
	int len = KAll;
	
	int pos = p_name.FindFirstOf("\t\r\n ");
	if(pos != KNotFound)
		len = pos;
 
	parameter_name.SetL(p_name.CStr(), len);
}

void Header_Parameter::SetValueL(const OpStringC8 &p_value, BOOL quote_value)
{
	const char *param_start = p_value.CStr();
	const char *param_end = param_start + p_value.Length();
	const char *p;
	
	if (!param_start)
	{
 		parameter_value.Empty(); 
		return;
	}
	parameter_value.Wipe();

	// Strip leading CR or LF, only if it is not part of a continuation
	p = param_start;
	while (*p == '\r' || *p == '\n')
		p++;
	if (p > param_start && (*p == ' ' || *p == '\t'))
	{
		p--; // Back out of the continuation
		if (p > param_start && *(p-1) == '\r' && *p == '\n')
			p--;
	}
	param_start = p;

	// Allow CR or LF only in parameter continuations (i.e. followed by space or tab)
	p = param_start;
	while ((p = op_strpbrk(p, "\r\n")) != 0)
	{
		const char *p2 = p;
		if (*p2 == '\r' && *(p2+1) == '\n')
			p2++;
		p2++;
		if (*p2 != ' ' && *p2 != '\t')
		{
			param_end = p; // Chop the parameter if it is not a valid continuation
			break;
		}
		p = p2;
	}

	// Check if there are line feeds that need to be normalized
	int linefeed_normalizations = 0;
	for (p = param_start; p < param_end; p++)
		if ((*p != '\n' &&  p >  param_start && *(p-1) == '\r') ||
			(*p == '\n' && (p == param_start || *(p-1) != '\r')))
			linefeed_normalizations ++;

	if (quote_value || linefeed_normalizations)
	{
		OpString8 param_escaped;
		int count_escapes = 0;
		const char *quotes = "\"";

		if (quote_value)
		{
			parameter_value.AppendL(quotes);

			// Strip existing quotes
			if (*param_start == '\"')
			{
				param_start ++;
				if (param_end > param_start && *(param_end-1) == '\"' && *param_end == '\0')
					param_end --;
			}
	
			for (p = param_start; p < param_end; p++)
				if (*p == '\\' || *p == '\"')
					count_escapes ++;
		}

		if (count_escapes || linefeed_normalizations)
		{
			char *p2 = param_escaped.ReserveL((int)(param_end-param_start + count_escapes+linefeed_normalizations+1));

			for (p = param_start; p < param_end; p++)
			{
				if (quote_value && (*p == '\\' || *p == '\"'))
					*p2++ = '\\';
				if (*p != '\n' &&  p >  param_start && *(p-1) == '\r')
					*p2++ = '\n';
				if (*p == '\n' && (p == param_start || *(p-1) != '\r'))
					*p2++ = '\r';
				*p2++ = *p;
			}
			*p2++ = '\0';

			parameter_value.AppendL(param_escaped.CStr());
		}
		else
	 		parameter_value.AppendL(param_start, (int)(param_end - param_start));

		if (quote_value)
			parameter_value.AppendL(quotes);
	}
 	else
 		parameter_value.SetL(param_start, (int)(param_end - param_start));

}

uint32 Header_Parameter::CalculateLength() const
{
	uint32 len = 0;

	if(parameter_name.CStr() != NULL)
	{
		len += parameter_name.Length();
		if(parameter_value.CStr() != NULL)
			len += STRINGLENGTH("=");
	}
		
	return len + parameter_value.Length();
}

char *Header_Parameter::OutputParameters(char *target) const
{
	if(parameter_name.CStr() != NULL)
	{
		op_strcpy(target, parameter_name.CStr());
		target += parameter_name.Length();
		if(parameter_value.CStr() != NULL)
		{
			op_strcpy(target, "=");
			target += STRINGLENGTH("=");
		}
	}
		
	if(parameter_value.HasContent())
	{
		op_strcpy(target, parameter_value.CStr());
		target += parameter_value.Length();
	}

	return target;
}

BOOL Header_Parameter::IsEmpty()
{
	return parameter_name.IsEmpty() && parameter_value.IsEmpty();
}

#endif // HTTP_DATA
