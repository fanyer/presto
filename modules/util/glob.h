/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef UTIL_GLOB_H
#define UTIL_GLOB_H

#ifdef OP_GLOB_SUPPORT

/** This is the GLOB(7) algorithm as commonly used on Unix systems.
    The exact behavior of the algorithm is specified by a standard
    (namely, POSIX 1003.2, 3.13).  The following procedure implements
    the subset of that standard that corresponds with the basic GLOB
    algorithm.  It does not have any POSIX extensions: locale
    sensitivity, character classes, collating symbols, or equivalence
    classes.  Here is a summary of the behavior:

	\c		matches the character c
	c		matches the character c
	*		matches zero or more characters except '/'
	?		matches any character except '/'
	[...]	matches any character in the set except '/'

			Inside the set, a leading '!' denotes complementation
			(the set matches any character not in the set).  

			A ']' can occur in the set if it comes first (possibly
			following '!').  A range a-b in the	set denotes a character
			range; the set contains all	characters a through b inclusive.
			A '-' can occur	in the set if it comes first (possibly
			following '!') or if it comes last.  '\', '*', '[', and '?'
			have no special meaning inside a set, and '/' may not
			be in the set.

    Unlike the standard globber, this one does not treat a leading '.'
    specially.

    The code is written for compactness and low space usage rather
    than for speed; it precomputes nothing and remembers nothing. 
   
    \param string            The string we're matching against
    \param pattern           The pattern we're matching
    \param slash_is_special  If TRUE then wildcards do not match '/',
							this is the default.
    \param brackets_enabled If TRUE (the default) [...] is treated as
                            matching any of the characters inside the
                            square brackets (see above). If FALSE, the
                            square brackets are not special, and are
                            treated like regular characters.
    \return TRUE if a match was found, otherwise FALSE.
*/
BOOL OpGlob( const uni_char *string, const uni_char *pattern, BOOL slash_is_special=TRUE, BOOL brackets_enabled=TRUE );

#endif // OP_GLOB_SUPPORT

#endif // UTIL_GLOB_H
