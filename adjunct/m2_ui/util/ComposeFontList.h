// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
/** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @file			 ComposeFontList.h
 * @author Owner:    Alexander Remen (alexr)
 */

#ifndef COMPOSEFONTLIST_H
#define COMPOSEFONTLIST_H

#include "modules/util/OpHashTable.h"

class ComposeFontList
{
public:
					/** Add a font to the list
						* @param font_name
						* @param index
						*/
	OP_STATUS		AddFont(const uni_char * font_name, INT32& index);

					/** Get the font index to use for a specified font name
						* @param font_name - the name to search for
						* @return the index in the drop down to use
						*/
	INT32			GetFontIndex(const uni_char * font_name) const;

					/** Returns the number of items in the font list
					 */
	INT32			GetCount() const { return m_font_list.GetCount(); }
					
					/** DeleteAll Items
					 */
	void			DeleteAll() { m_font_list.DeleteAll(); }

private:

	OP_STATUS				StripAndUnquote(const uni_char * input, OpString& string_output) const;
	
	struct FontDropdownInfo
	{
		OpString	font_name;
		INT32		index;
	};

	OpAutoStringHashTable<FontDropdownInfo> m_font_list;

};

#endif //COMPOSEFONTLIST_H
