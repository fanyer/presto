/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DEVICEAPI_WILDCARDCMP_H
#define DEVICEAPI_WILDCARDCMP_H

/**
 * Checks if a string matches a pattern.
 *
 * @param pattern Pattern to match.
 * @param string String which is matched to the pattern.
 * @param case_sensitive Flag signifying if comparisons should be case sensitive.
 * @param escape_char Character used for escaping special characters (wildcard and itself)
 * @param wildcard Character which is used as a wildcard(it matches any substring)
 * @return
 *	- OpBoolean::IS_TRUE if there is a match,
 *	- OpBoolean::IS_FALSE if there is no match
 *  - other OpStatus errors if any occured (notably ERR_NO_MEMORY is possible for long pattern strings)
 */
OP_BOOLEAN WildcardCmp(const uni_char* pattern, const uni_char* string, BOOL case_sensitive, uni_char escape_char = UNI_L("\\")[0], uni_char wildcard_char = UNI_L("*")[0]);

#ifdef DAPI_JIL_SUPPORT
/**
 * Checks if a string matches a pattern according to the JIL matching rules.
 *
 * @param pattern Pattern to match.
 * @param string String which is matched to the pattern.
 * @return
 *	- OpBoolean::IS_TRUE if there is a match,
 *	- OpBoolean::IS_FALSE if there is no match
 *  - other OpStatus errors if any occured (notably ERR_NO_MEMORY is possible for long pattern strings)
 */
OP_BOOLEAN JILWildcardCmp(const uni_char* pattern, const uni_char* string);
#endif // DAPI_JIL_SUPPORT

#endif // DEVICEAPI_WILDCARDCMP_H
