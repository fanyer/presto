/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/util/PatternMatch.h"

BOOL UniWildCompare(const uni_char *inPattern, const uni_char *inString, Boolean inCaseSensitive)
{
	register const uni_char	*p;
	register const uni_char	*s;
	register uni_char		c;
	uni_char				sc;
	if (!inPattern || !inString)
	{
		return(FALSE);
	}
	p = inPattern;
	s = inString;
	c = *p++;
	while('\0' != c)
	{
		if('*' == c)
		{
			while(TRUE)
			{
				if(UniWildCompare(p, s, inCaseSensitive))
				{
					return(TRUE);
				}
				if('\0' == *s++)
				{
					return(FALSE);
				}
			}
		}
		else if('?' == c)
		{
			if('\0' == *s++)
			{
				return(FALSE);
			}
		}
		else
		{
			sc = *s++;
			if (inCaseSensitive && (sc != c))
			{
				return(FALSE);
			}
			if (uni_tolower(sc) != uni_tolower(c))
			{
				return(FALSE);
			}
		}
		c = *p++;
	}
	return('\0' == *s);
}
