/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 */

#ifndef I18N_H
#define I18N_H

#include "modules/locale/locale-enum.h"

/** @brief Utilities for formatting translated strings
 * In general our strings can have specifiers in them in the form %1, %2, %3 that
 * allow us to replace these strings with alternative content at runtime.
 * %1, %2, %3 etc. stand for first argument, second argument, third argument
 * but can occur in any order in the actual string.
 * 
 * As an example, an English string might say
 *   "Are you sure you want to delete %1 items from %2?", 5, "trash"
 *
 * In other languages %1 and %2 might be reversed, but they still would need
 * to be replaced by the correct string.
 */
class I18n
{
public:
	// Maximum number of arguments for the formatting functions
	static const size_t MaxArguments = 5;

	/** Format a string
	 * @param formatted On success, contains the formatted string
	 *    @note if there is existing content, the formatted string will be appended
	 * @param format A format string with substitutions like %1, %2 etc up to MaxArguments
	 * @param arg1, ... Replacements for placeholders, where arg1 replaces '%1', arg2 replaces '%2', etc.
	 * @return See OpStatus
	 *
	 * @example usage
	 *   OpString formatted;
	 *   I18n::Format(formatted, UNI_L("Delete %1 items from %2"), 5, UNI_L("Trash"));
	 * Results in the string "Delete 5 items from Trash" in formatted
	 *
	 * @note These functions are type-safe. If your argument is of a type that is not supported by this
	 * implementation, you will get a compile error stating that
	 *   I18n::FormatArgument(const FormatArgument* next, X arg) does not exist for type X.
	 */
	template<class T1, class T2, class T3, class T4, class T5>
		static OP_STATUS Format(OpString& formatted, OpStringC format, const T1& arg1, const T2& arg2, const T3& arg3, const T4& arg4, const T5& arg5)
		{ return Format(NULL, formatted, format.CStr(), arg1, arg2, arg3, arg4, arg5); }

	template<class T1, class T2, class T3, class T4>
		static OP_STATUS Format(OpString& formatted, OpStringC format, const T1& arg1, const T2& arg2, const T3& arg3, const T4& arg4)
		{ return Format(NULL, formatted, format.CStr(), arg1, arg2, arg3, arg4); }

	template<class T1, class T2, class T3>
		static OP_STATUS Format(OpString& formatted, OpStringC format, const T1& arg1, const T2& arg2, const T3& arg3)
		{ return Format(NULL, formatted, format.CStr(), arg1, arg2, arg3); }

	template<class T1, class T2>
		static OP_STATUS Format(OpString& formatted, OpStringC format, const T1& arg1, const T2& arg2)
		{ return Format(NULL, formatted, format.CStr(), arg1, arg2); }

	template<class T1>
		static OP_STATUS Format(OpString& formatted, OpStringC format, const T1& arg1)
		{ return Format(NULL, formatted, format.CStr(), arg1); }

	static OP_STATUS Format(OpString& formatted, OpStringC format)
		{ return Format(NULL, formatted, format.CStr()); }


	/** @overload Overloads for use with string IDs instead of strings as input format
	 */
	template<class T1, class T2, class T3, class T4, class T5>
		static OP_STATUS Format(OpString& formatted, Str::LocaleString format, const T1& arg1, const T2& arg2, const T3& arg3, const T4& arg4, const T5& arg5)
		{ return Format(NULL, formatted, format, arg1, arg2, arg3, arg4, arg5); }

	template<class T1, class T2, class T3, class T4>
		static OP_STATUS Format(OpString& formatted, Str::LocaleString format, const T1& arg1, const T2& arg2, const T3& arg3, const T4& arg4)
		{ return Format(NULL, formatted, format, arg1, arg2, arg3, arg4); }

	template<class T1, class T2, class T3>
		static  OP_STATUS Format(OpString& formatted, Str::LocaleString format, const T1& arg1, const T2& arg2, const T3& arg3)
		{ return Format(NULL, formatted, format, arg1, arg2, arg3); }

	template<class T1, class T2>
		static OP_STATUS Format(OpString& formatted, Str::LocaleString format, const T1& arg1, const T2& arg2)
		{ return Format(NULL, formatted, format, arg1, arg2); }

	template<class T1>
		static OP_STATUS Format(OpString& formatted, Str::LocaleString format, const T1& arg1)
		{ return Format(NULL, formatted, format, arg1); }

	static OP_STATUS Format(OpString& formatted, Str::LocaleString format)
		{ return Format(NULL, formatted, format); }

private:
	enum FormattingType
	{
		FORMAT_INT32,
		FORMAT_UINT32,
		FORMAT_STRING,
		FORMAT_STRING_ID
	};

	struct FormatArgument
	{
		FormatArgument(const FormatArgument* next, INT8 arg) : next(next), format(FORMAT_INT32), argument_int(arg) {}
		FormatArgument(const FormatArgument* next, INT16 arg) : next(next), format(FORMAT_INT32), argument_int(arg) {}
		FormatArgument(const FormatArgument* next, INT32 arg) : next(next), format(FORMAT_INT32), argument_int(arg) {}
		FormatArgument(const FormatArgument* next, UINT8 arg) : next(next), format(FORMAT_UINT32), argument_uint(arg) {}
		FormatArgument(const FormatArgument* next, UINT16 arg) : next(next), format(FORMAT_UINT32), argument_uint(arg) {}
		FormatArgument(const FormatArgument* next, UINT32 arg) : next(next), format(FORMAT_UINT32), argument_uint(arg) {}
		FormatArgument(const FormatArgument* next, const uni_char* arg) : next(next), format(FORMAT_STRING), argument_string(arg) {}
		FormatArgument(const FormatArgument* next, OpStringC arg) : next(next), format(FORMAT_STRING), argument_string(arg.CStr()) {}
		FormatArgument(const FormatArgument* next, Str::LocaleString arg) : next(next), format(FORMAT_STRING_ID), argument_string_id(arg) {}

		const FormatArgument* next;
		FormattingType format;
		union
		{
			INT32 argument_int;
			UINT32 argument_uint;
			const uni_char* argument_string;
			int argument_string_id;
		};

	private:
		// Private constructors with no implementation: we can't handle these types. Use (smaller) integers instead.
		FormatArgument(const FormatArgument* next, bool arg);
		FormatArgument(const FormatArgument* next, double arg);
		FormatArgument(const FormatArgument* next, INT64 arg);
		FormatArgument(const FormatArgument* next, UINT64 arg);
	};

	// boilerplate code in the form of inline template functions to handle the different arguments and argument types
	template<class T1, class T2, class T3, class T4, class T5>
		static OP_STATUS Format(const FormatArgument* argument, OpString& formatted, const uni_char* format, const T1& arg1, const T2& arg2, const T3& arg3, const T4& arg4, const T5& arg5)
		{ FormatArgument arg(argument, arg5); return Format(&arg, formatted, format, arg1, arg2, arg3, arg4); }

	template<class T1, class T2, class T3, class T4>
		static OP_STATUS Format(const FormatArgument* argument, OpString& formatted, const uni_char* format, const T1& arg1, const T2& arg2, const T3& arg3, const T4& arg4)
		{ FormatArgument arg(argument, arg4); return Format(&arg, formatted, format, arg1, arg2, arg3); }

	template<class T1, class T2, class T3>
		static OP_STATUS Format(const FormatArgument* argument, OpString& formatted, const uni_char* format, const T1& arg1, const T2& arg2, const T3& arg3)
		{ FormatArgument arg(argument, arg3); return Format(&arg, formatted, format, arg1, arg2); }

	template<class T1, class T2>
		static OP_STATUS Format(const FormatArgument* argument, OpString& formatted, const uni_char* format, const T1& arg1, const T2& arg2)
		{ FormatArgument arg(argument, arg2); return Format(&arg, formatted, format, arg1); }

	template<class T1>
		static OP_STATUS Format(const FormatArgument* argument, OpString& formatted, const uni_char* format, const T1& arg1)
		{ FormatArgument arg(argument, arg1); return Format(&arg, formatted, format); }

	template<class T1, class T2, class T3, class T4, class T5>
		static OP_STATUS Format(const FormatArgument* argument, OpString& formatted, Str::LocaleString format, const T1& arg1, const T2& arg2, const T3& arg3, const T4& arg4, const T5& arg5)
		{ FormatArgument arg(argument, arg5); return Format(&arg, formatted, format, arg1, arg2, arg3, arg4); }

	template<class T1, class T2, class T3, class T4>
		static OP_STATUS Format(const FormatArgument* argument, OpString& formatted, Str::LocaleString format, const T1& arg1, const T2& arg2, const T3& arg3, const T4& arg4)
		{ FormatArgument arg(argument, arg4); return Format(&arg, formatted, format, arg1, arg2, arg3); }

	template<class T1, class T2, class T3>
		static OP_STATUS Format(const FormatArgument* argument, OpString& formatted, Str::LocaleString format, const T1& arg1, const T2& arg2, const T3& arg3)
		{ FormatArgument arg(argument, arg3); return Format(&arg, formatted, format, arg1, arg2); }

	template<class T1, class T2>
		static OP_STATUS Format(const FormatArgument* argument, OpString& formatted, Str::LocaleString format, const T1& arg1, const T2& arg2)
		{ FormatArgument arg(argument, arg2); return Format(&arg, formatted, format, arg1); }

	template<class T1>
		static OP_STATUS Format(const FormatArgument* argument, OpString& formatted, Str::LocaleString format, const T1& arg1)
		{ FormatArgument arg(argument, arg1); return Format(&arg, formatted, format); }

	// Actual functions that do the formatting
	static OP_STATUS Format(const FormatArgument* argument, OpString& formatted, const uni_char* format);
	static OP_STATUS Format(const FormatArgument* argument, OpString& formatted, Str::LocaleString format);
};

#endif // I18N_H
