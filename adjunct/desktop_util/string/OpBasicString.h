/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Adam Minchinton
 */

/////////////////////////////////////////////////////////////////////////
//
// NOTE: You CANNOT use any core functionallity in the OpBasicString as 
//		 it is used outside of core.
//
// Currently this was made for use in the autoupdate code
//
/////////////////////////////////////////////////////////////////////////

#ifndef OPBASICSTRING_H
#define OPBASICSTRING_H

class OpBasicString8
{
public:
	OpBasicString8(const char *string);
	~OpBasicString8();
	
	const char *CStr() { return m_string; }

	BOOL Append(const char *string);
	BOOL DeleteAfterLastOf(const char chr);
	
private:
	char *m_string;
};

#endif // OPBASICSTRING_H
