/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef HTMLIFY_MORE_H
#define HTMLIFY_MORE_H

/**
 * @param content     -
 * @param len         -
 * @param no_link     -
 * @param use_xml     -
 * @param flowed_mail - 0:Not flowed, 1:Flowed, 2:Flowed,DelSp=Yes
 * @param smilies     -
 * @return
 */
uni_char * XHTMLify_string(const uni_char * content,
						   int len,
						   BOOL no_link,
						   BOOL use_xml,
						   int flowed_mail,
						   BOOL smilies = FALSE);


/**
 * @param out_str     -
 * @param content     -
 * @param len         -
 * @param no_link     -
 * @param use_xml     -
 * @param flowed_mail - 0:Not flowed, 1:Flowed, 2:Flowed,DelSp=Yes
 * @param smilies     -
 * @return
 */
OP_STATUS HTMLify_string(OpString &out_str,
						 const uni_char *content,
						 int len,
						 BOOL no_link,
						 BOOL use_xml = FALSE,
						 int flowed_mail = FALSE,
						 BOOL smilies = FALSE);


/**
 * This function was made to be used for conversion of text emails to HTML.
 * It assumes the the text received contains line breaks in the CRLF form (\r\n)
 *
 * @param content     -
 * @param len         -
 * @param no_link     -
 * @param use_xml     -
 * @param flowed_mail - 0:Not flowed, 1:Flowed, 2:Flowed,DelSp=Yes
 * @param smilies     -
 * @return
 */
OpString XHTMLify_string_with_BR(const uni_char *content,
								 int len,
								 BOOL no_link,
								 BOOL use_xml,
								 int flowed_mail,
								 BOOL smilies = FALSE);

/**
 * Convert input string to a safe HTMLified string and append it
 * to the target string
 *
 * @param target
 * @param s
 *
 * @return OpStatus::OK on success, otherwise an error code describing the problem
 */
OP_STATUS AppendHTMLified(OpString& target, const uni_char* s);


#endif
