/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XMLTYPES_H
#define XMLTYPES_H

enum XMLVersion
{
    XMLVERSION_1_0,
    XMLVERSION_1_1
};

enum XMLStandalone
{
	XMLSTANDALONE_NONE,
	XMLSTANDALONE_YES,
	XMLSTANDALONE_NO
};

enum XMLWhitespaceHandling
{
	XMLWHITESPACEHANDLING_DEFAULT,
	XMLWHITESPACEHANDLING_PRESERVE
};

class XMLPoint
{
public:
	XMLPoint() : line(~0u), column(~0u) {}

	BOOL IsValid() const { return line != ~0u; }

	unsigned line;
	/**< Line number.  Counts from zero.  Has the value ~0u if this is
	     not a valid point. */

	unsigned column;
	/**< Column number.  Counts from zero.  Has the value ~0u if this
	     is not a valid point. */
};

class XMLRange
{
public:
	XMLPoint start;
	/**< Beginning of range, inclusive. */

	XMLPoint end;
	/**< End of range, exclusive. */
};

#endif // XMLTYPES_H
