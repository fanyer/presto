/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef LINK_TYPE_H
#define LINK_TYPE_H

enum LinkType
{
	LINK_TYPE_ALTERNATE   = 1 <<  0,
	LINK_TYPE_STYLESHEET  = 1 <<  1,
	LINK_TYPE_START       = 1 <<  2,
	LINK_TYPE_NEXT        = 1 <<  3,
	LINK_TYPE_PREV        = 1 <<  4,
	LINK_TYPE_CONTENTS    = 1 <<  5,
	LINK_TYPE_INDEX       = 1 <<  6,
	LINK_TYPE_GLOSSARY    = 1 <<  7,
	LINK_TYPE_COPYRIGHT   = 1 <<  8,
	LINK_TYPE_CHAPTER     = 1 <<  9,
	LINK_TYPE_SECTION     = 1 << 10,
	LINK_TYPE_SUBSECTION  = 1 << 11,
	LINK_TYPE_APPENDIX    = 1 << 12,
	LINK_TYPE_HELP        = 1 << 13,
	LINK_TYPE_BOOKMARK    = 1 << 14,
	LINK_TYPE_ICON        = 1 << 15,
	LINK_TYPE_END         = 1 << 16,
	LINK_TYPE_FIRST       = 1 << 17,
	LINK_TYPE_LAST        = 1 << 18,
	LINK_TYPE_UP          = 1 << 19,
	LINK_TYPE_FIND        = 1 << 20,
	LINK_TYPE_AUTHOR      = 1 << 21,
	LINK_TYPE_MADE        = 1 << 22,
	LINK_TYPE_ARCHIVES    = 1 << 23,
	LINK_TYPE_NOFOLLOW    = 1 << 24,
	LINK_TYPE_NOREFERRER  = 1 << 25,
	LINK_TYPE_PINGBACK    = 1 << 26,
	LINK_TYPE_PREFETCH    = 1 << 27,
	LINK_TYPE_IMAGE_SRC   = 1 << 28,
	LINK_TYPE_APPLE_TOUCH_ICON = 1 << 29,
	LINK_TYPE_APPLE_TOUCH_ICON_PRECOMPOSED = 1 << 30,
	LINK_TYPE_OTHER       = 1u << 31 // unknown
};

#define LINK_GROUP_ALT_STYLESHEET (LINK_TYPE_ALTERNATE | LINK_TYPE_STYLESHEET)

struct LinkMapEntry
{
	LinkMapEntry(const uni_char* n, LinkType k) : name(n), kind(k) {}
	LinkMapEntry() {}

	const uni_char* name;
	LinkType    kind;
};

#endif // LINK_TYPE_H
