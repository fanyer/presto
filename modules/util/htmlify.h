/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MODULES_UTIL_HTMLIFY_H
#define MODULES_UTIL_HTMLIFY_H

/**
 * HTMLify_string converts ', ", <, > and & to a sequence on the form \&x;
 * for example < becomes &lt; and & becomes &amp;. Non-printable
 * characters are converted to %xx where xx is the charcter
 * represented as a hexadecimal value.
 *
 * @param content The string that is to be converted.
 *
 * @param len The length of the input string.
 *
 * @param no_link If this is set to TRUE then links on the form
 * http://hostname/path/file will not be converted. When FALSE they
 * will be converted to:
 * <a href="http://host/path/file">http://host/path/file</a>.
 *
 * @returns A pointer to the converted string which must be freed by
 * the caller. NULL if OOM or if content equals NULL.
 */
char *HTMLify_string(const char *content, int len, BOOL no_link= FALSE);
char *XMLify_string(const char *content, int len, BOOL no_link, BOOL flowed_mail=FALSE);
char *XHTMLify_string(const char *content, int len, BOOL no_link, BOOL use_xml, BOOL flowed_mail=FALSE);

uni_char *HTMLify_string(const uni_char *content);
uni_char *HTMLify_string(const uni_char *content, int len, BOOL no_link= FALSE);
uni_char *HTMLify_string_link(const uni_char *content);
uni_char *XMLify_string(const uni_char *content);
uni_char *XMLify_string(const uni_char *content, int len, BOOL no_link, BOOL flowed_mail=FALSE);
uni_char *XHTMLify_string(const uni_char *content, int len, BOOL no_link, BOOL use_xml, BOOL flowed_mail=FALSE);

#endif // !MODULES_UTIL_HTMLIFY_H
