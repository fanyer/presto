/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#ifndef ENCODER_UTILITY_H
#define ENCODER_UTILITY_H

/** Replacement character for undefined code points */
#define NOT_A_CHARACTER_ASCII '?'

#ifdef ENCODINGS_HAVE_TABLE_DRIVEN
void lookup_dbcs_table(const unsigned char *dbcs_table, long tablelen,
                       uni_char character, char out[2]);

/** 
 * Helper function for comparing entries in sorted (unichar, unichar)
 * tables (via bsearch).
 */
int unichar_compare(const void *key, const void *entry);
#endif

#endif // ENCODER_UTILITY_H
