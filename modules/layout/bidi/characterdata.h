/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef DOC_I18N_CHARACTERDATA_H
#define DOC_I18N_CHARACTERDATA_H

/**
 * Returns true if the character has a baseline set in the font, and
 * false if it is a character that is positioned within a grid.
 */
#  if defined(MSWIN)
extern BOOL g_far_eastern_win;
#   define has_baseline(ch) (g_far_eastern_win || (ch < 0x3040 || (ch > 0xD7A3 && ch < 0x20000) || ch > 0x2FA06))
#  else
#   define has_baseline(ch) (ch < 0x3040 || (ch > 0xD7A3 && ch < 0x20000) || ch > 0x2FA06)
#  endif

#endif //DOC_I18N_CHARACTERDATA_H
