/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef ACCESSIBILITY_UTILS_H
#define ACCESSIBILITY_UTILS_H

// Copy string, skipping extraneous whitespace, tabs, linebreaks, etc.
void CopyStringClean(OpString& dest, const uni_char* src, int len = -1);

#endif // ACCESSIBILITY_UTILS_H
