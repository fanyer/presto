/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#ifndef SKIN_UTILS_H
#define SKIN_UTILS_H

namespace SkinUtils
{
    /**
     *
     * @param elem_name of skin element 
     * @return the color or -1 if no color was found
     */
    int GetTextColor(const OpStringC8& elem_name);

	/* Checks if an address points to skin.ini or persona.ini, and if so opens it as such
	 *
   	 * @param address		An unresolved address that might point to a local file/directory
	 * @return				TRUE if the address is a theme
	 */
	BOOL OpenIfThemeFile(OpStringC address);
};

#endif // SKIN_UTILS_H
