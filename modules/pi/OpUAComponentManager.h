/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#ifndef OPUACOMPONENTMANAGER_H
#define OPUACOMPONENTMANAGER_H

class OpWindowCommander;

/**
 * API used to modify the User-Agent string at run-time.
 */
class OpUAComponentManager
{
public:
	/**
	 * Create the OpUAComponentManager object. This object is created
	 * by UAManager during the initialization phase.
	 */
	static OP_STATUS Create(OpUAComponentManager **);

	/** Standard arguments for OpUAComponentManager's methods. */
	struct Args
	{
		/** The kind of User-Agent string that is being created. */
		UA_BaseStringId ua;
		/** Host name to retrieve comment for, may be NULL. */
		const uni_char *host;
		/** WindowCommander context to retrieve comment for, may be NULL. */
		OpWindowCommander *win;
# ifdef IMODE_EXTENSIONS
		/** Insert handset serial number (Imode UTN attribute). */
		BOOL use_ua_utn;
# endif
	};

	/**
	 * Retrieve the operating system string to use for this User-Agent
	 * string. The string returned must be valid as long as the
	 * OpUAComponentManager object is alive, or until the next call to
	 * GetOSString(). The string returned will be inserted at the start of
	 * the comment paranthesis of the User-Agent string, following the
	 * browser identifier.
	 *
	 * Comments must correspond to the RFC 2616 comment syntax, and must
	 * not include any opening or closing parantheses ("(" or ")"), and
	 * should be on the form
	 *
	 *   "[WindowSystem [version]; ]OS [version][; make [model]]"
	 *
	 * It is recommended that for ua == UA_MSIE_Only, a string identifying
	 * Windows is reported, for instance "Windows NT 5.1" (Windows XP).
	 *
	 * @param args Description of the User-Agent string.
	 * @return An OS identifier (e.g. "Windows NT 5.1"). Must not be
	 * NULL or empty.
	 */
	virtual const char *GetOSString(Args &args) = 0;

#ifdef SC_UA_CONTEXT_COMPONENTS
	/**
	 * Retrieve the comment string to use for this User-Agent string. The
	 * string returned must be valid as long as the OpUAComponentManager
	 * object is alive, or until the next call to GetCommentString().
	 * The string returned will be inserted in the comment paranthesis
	 * of the User-Agent string, after the operating system string
	 * returned by OpUAComponentManager::GetOSString().
	 *
	 * Comments must correspond to the RFC 2616 comment syntax, and must
	 * not include any opening or closing parantheses ("(" or ")").
	 *
	 * Comments should be separated by semi-colons (";"), and one of the
	 * comments should contain the two-letter language code.
	 *
	 * If NULL is returned, the language code is inserted as a comment.
	 * If non-NULL is returned, the string may not be empty.
	 *
	 * @param args Description of the User-Agent string.
	 * @param langcode Language code for use in comment string (see above).
	 * @return Pointer to a comment to include or NULL (see above).
	 */
	virtual const char *GetCommentString(Args &args, const char *langcode) = 0;

	/**
	 * Retrieve the prefix string to use for this User-Agent string. The
	 * string returned must be valid as long as the OpUAComponentManager
	 * object is alive, or until the next call to GetPrefixString().
	 * The string will be added before the regular Opera product token.
	 * It should conform to the User-Agent token syntax described in
	 * RFC 2616 section 3.8.
	 *
	 * If NULL is returned, no prefix will be inserted.
	 *
	 * @param args Description of the User-Agent string.
	 * @return Pointer to a prefix to use or NULL (see above).
	 */
	virtual const char *GetPrefixString(Args &args) = 0;

	/**
	 * Retrieve the suffix string to use for this User-Agent string. The
	 * string returned must be valid as long as the OpUAComponentManager
	 * object is alive, or until the next call to GetSuffixString().
	 * The string will be added after the regular Opera product token.
	 * It should conform to the User-Agent token syntax described in
	 * RFC 2616 section 3.8.
	 *
	 * If NULL is returned, no suffix will be inserted.
	 *
	 * @param args Description of the User-Agent string.
	 * @return Pointer to a suffix to use or NULL (see above).
	 */
	virtual const char *GetSuffixString(Args &args) = 0;
#endif

	virtual ~OpUAComponentManager()
	{
	}
};

#endif
