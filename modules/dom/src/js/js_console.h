/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef JS_CONSOLE_H
#define JS_CONSOLE_H

#include "modules/ecmascript_utils/esthread.h"
#include "modules/dom/src/domobj.h"

/**
 * This is a debugging tool coming from Firefox (originally Firebug)
 */
class JS_Console
	: public DOM_Object
{
public:

	/**
	 * List of supported console.log-like functions. The error console will
	 * react to these functions:
	 *
	 * console.log
	 * console.debug
	 * console.info
	 * console.warn
	 * console.error
	 * console.assert
	 *
	 * Dragonfly (or other Scope clients) may additionally react to these
	 * functions. Calling these functions will not cause a message to appear
	 * in the regular error console, however.
	 * 
	 * console.dir
	 * console.dirxml
	 * console.group
	 * console.groupCollapsed
	 * console.groupEnd
	 * console.count
	 * console.table
	 * console.clear
	 * console.exception
	 */
	enum LogType
	{
		LOG_LOG = 1,
		LOG_DEBUG,
		LOG_INFO,
		LOG_WARN,
		LOG_ERROR,
		LOG_ASSERT,
		LOG_DIR,
		LOG_DIRXML,
		LOG_GROUP,
		LOG_GROUP_COLLAPSED,
		LOG_GROUP_END,
		LOG_COUNT,
		LOG_TABLE,
		LOG_CLEAR
	};

	virtual ~JS_Console();

	// Override DOM_Object.
	virtual BOOL IsA(int type) { return type == DOM_TYPE_CONSOLE || DOM_Object::IsA(type); }

	DOM_DECLARE_FUNCTION_WITH_DATA(log);

	DOM_DECLARE_FUNCTION(time);
	DOM_DECLARE_FUNCTION(timeEnd);
	DOM_DECLARE_FUNCTION(trace);
	DOM_DECLARE_FUNCTION(profile);
	DOM_DECLARE_FUNCTION(profileEnd);
	DOM_DECLARE_FUNCTION(exception);

	enum { FUNCTIONS_ARRAY_SIZE = 7 };
	enum { FUNCTIONS_WITH_DATA_ARRAY_SIZE = 17 };

#ifdef OPERA_CONSOLE

	/**
	 * Post a console.log-like message to the error console. Note that not all functions
	 * on the console object will cause a message to appear in the Error Console. See
	 * documentation for JS_Console::LogType for details.
	 *
	 * @param type Indicates which function (console.log, console.info, etc) was called.
	 * @param runtime The originating runtime.
	 * @param argv The list of arguments.
	 * @param argc The number of arguments in the list.
	 * @return OpStatus::OK if a message was posted; OpStatus:ERR if a message could not
	 *         be posted, or if an unsupported LogType was used; or OpStatus::ERR_NO_MEMORY.
	 * @see LogType
	 */
	static OP_STATUS PostConsoleMessage(LogType type, DOM_Runtime *runtime, ES_Value* argv, int argc);

	/**
	 * Format a string according to input arguments, and put the result in 'str'.
	 *
	 * Based on http://getfirebug.com/wiki/index.php/Console_API.
	 *
	 * Any number of arguments of any type is supported. When formatting placeholders
	 * are not used, the arguments will be joined into a space-delimited line:
	 *
	 * > console.log("Hello", "brave", "new", "world");
	 * > "Hello brave new world"
	 *
	 * If the first value is a string, and contains placeholders, the other arguments
	 * will be inserted into the first string:
	 *
	 * > console.log("The %s brown %s", "quick", "fox");
	 * > "The quick brown fox"
	 *
	 * An argument may be of any type:
	 *
	 * > console.log(true, 1337, null, {});
	 * > "true 1337 null [object Object]".
	 * 
	 * @param argv [in] The list of arguments. The first argument may be (but is not
	 *                  required to be) a string containing formatting placeholders.
	 * @param argc [in] The number of arguments in the argument list.
	 * @param str [out] The complete string will be stored in this object.
	 * @return OpStatus::OK if a string was formatted; or OpStatus::ERR_NO_MEMORY.
	 */
	static OP_STATUS FormatString(ES_Value* argv, int argc, OpString &str);

#endif // OPERA_CONSOLE

private:

#ifdef OPERA_CONSOLE

	/**
	 * Append the string representation of an ES_Value to a TempBuffer.
	 *
	 * @param value [in] The ES_Value to convert to a string.
	 * @param buf [out] The buffer to append the string to.
	 * @return OpStatus::OK if successful, otherwise OpStatus::ERR_NO_MEMORY.
	 */
	static OP_STATUS AppendValue(const ES_Value &value, TempBuffer &buf);

	/**
	 * Check whether the pointer 'pos' currently points to a supported
	 * placeholder character.
	 *
	 * The following placeholders are supported. Their intended use is listed
	 * to the right, however, they are all treated equally in the Error Console
	 * backend. (Dragonfly, or other Scope clients, may choose to implement more
	 * advanced representations).
	 *
	 * %s      String
	 * %d, %i  Integer
	 * %f      Floating point number
	 * %o      Object hyperlink
	 *
	 * @param pos The character to be checked. The pointer must point to
	 *            the character immediately following the '%' symbol.
	 * @return TRUE if supported, FALSE if unsupported.
	 */
	static BOOL IsSupportedPlaceholder(const uni_char *pos);

#endif // OPERA_CONSOLE

	/**
	 * Returns FALSE if the ES_Value should trigger console.assert.
	 *
	 * '0', 'false', 'undefined', 'null' and the empty string all trigger
	 * the assert, all other values do not.
	 */
	static BOOL AssertValue(ES_Value &value);
};

#endif // !JS_CONSOLE_H
