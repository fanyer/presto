/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/domglobaldata.h"
#include "modules/dom/src/opatom.h"

OpAtom DOM_StringToAtom(const uni_char *string)
{
	int lo = 0, hi = OP_ATOM_ABSOLUTELY_LAST_ENUM - 1;
	const char *const *names = g_DOM_atomNames;

	while (hi >= lo)
	{
		int index = (lo + hi) >> 1;

		const char *n1 = names[index];
		const uni_char *n2 = string;

		while (*n1 && *n2 && *n1 == *n2)
			++n1, ++n2;

		int r = *n1 - *n2;
		if (r == 0)
			return (OpAtom) index;
		else if (r < 0)
			lo = index + 1;
		else
			hi = index - 1;
	}

	return OP_ATOM_UNASSIGNED;
}

Markup::AttrType
DOM_AtomToHtmlAttribute(OpAtom atom)
{
	unsigned value = (g_DOM_atomData[atom] & 0xffff0000) >> 16;

	if (value == USHRT_MAX)
		return Markup::HA_NULL;
	else
		return static_cast<Markup::AttrType>(value);
}

#ifdef SVG_DOM
Markup::AttrType
DOM_AtomToSvgAttribute(OpAtom atom)
{
	unsigned short value = g_DOM_SVG_atomData[atom];

	if (value == USHRT_MAX)
		return Markup::HA_NULL;
	else
		return static_cast<Markup::AttrType>(value);
}
#endif // SVG_DOM

int
DOM_AtomToCssProperty(OpAtom atom)
{
	unsigned value = g_DOM_atomData[atom] & 0xffff;

	if (value == USHRT_MAX)
		return -1;
	else
		return (int) value;
}

const char *DOM_AtomToString(OpAtom atom)
{
	return g_DOM_atomNames[atom];
}

#include "modules/dom/src/opatomdata.h"
#include "modules/dom/src/opatom.cpp.inc"
