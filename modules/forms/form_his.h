/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** $Id$
**
** Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _FORM_HIS_H_
#define _FORM_HIS_H_

// This class will probably be replaced by wand later.

/**
 * Remembers things to allow for easy fill in.
 *
 * @author ?
 */
class FormsHistory
{
public:
	/**
	 * Calculate possible matches.
	 *
	 * @param in_MatchString
	 * @param  out_Items A pointer that will be pointing to a
	 * uni_char array if the method succeeds. The array will
	 * be sorted in alphabetically order.
	 * @param out_ItemCount The number of items in the array.
	 *
	 * @returns The status
	 */
	OP_STATUS GetItems(const uni_char* in_MatchString,
					   uni_char*** out_Items,
					   int* out_ItemCount);
};

#endif
