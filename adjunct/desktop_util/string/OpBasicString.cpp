/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Adam Minchinton
 */

#include "core/pch.h"

/////////////////////////////////////////////////////////////////////////
//
// NOTE: You CANNOT use any core functionallity in the OpBasicString as 
//		 it is used outside of core.
//
// Currently this was made for use in the autoupdate code
//
/////////////////////////////////////////////////////////////////////////

#include "adjunct/desktop_util/string/OpBasicString.h"

//////////////////////////////////////////////////////////////////////////////////////////////////
	
OpBasicString8::OpBasicString8(const char *string)
	: m_string(NULL)
{
	if (string)
	{
		int len = strlen(string) + 1;
		if (len > 0)
		{
			m_string = OP_NEWA(char, len);
			if (m_string)
				strcpy(m_string, string);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////

OpBasicString8::~OpBasicString8()
{
	OP_DELETEA(m_string);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

BOOL OpBasicString8::Append(const char *string)
{
	char *temp;

	int len = strlen(string) + strlen(m_string) + 1;
	temp = OP_NEWA(char, len);
	if (temp && len > 0)
	{
		strcpy(temp, m_string);
		strcat(temp, string);

		OP_DELETEA(m_string);
		m_string = temp;
		
		return TRUE;
	}
	
	return FALSE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

BOOL OpBasicString8::DeleteAfterLastOf(const char chr)
{
	if (m_string)
	{
		char *p = NULL;
		if ((p = strrchr(m_string, chr)) != NULL)
		{
			*(p + 1) = '\0';
			return TRUE;
		}
	}
	return FALSE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

