/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/js/js_console.h"

#include "modules/dom/src/domglobaldata.h"
#include "modules/console/opconsoleengine.h"
#include "modules/ecmascript_utils/esdebugger.h"

#include "modules/scope/scope_jsconsole_listener.h"

JS_Console::~JS_Console()
{
}

#ifdef SCOPE_ECMASCRIPT_DEBUGGER

static OpScopeJSConsoleListener::LogType ConvertEnum(JS_Console::LogType logType)
{
	switch (logType)
	{
	default:
		OP_ASSERT(!"Unknown log type");
		return OpScopeJSConsoleListener::LOG_INVALID;
	case JS_Console::LOG_LOG:
		return OpScopeJSConsoleListener::LOG_LOG;
	case JS_Console::LOG_DEBUG:
		return OpScopeJSConsoleListener::LOG_DEBUG;
	case JS_Console::LOG_INFO:
		return OpScopeJSConsoleListener::LOG_INFO;
	case JS_Console::LOG_WARN:
		return OpScopeJSConsoleListener::LOG_WARN;
	case JS_Console::LOG_ERROR:
		return OpScopeJSConsoleListener::LOG_ERROR;
	case JS_Console::LOG_ASSERT:
		return OpScopeJSConsoleListener::LOG_ASSERT;
	case JS_Console::LOG_DIR:
		return OpScopeJSConsoleListener::LOG_DIR;
	case JS_Console::LOG_DIRXML:
		return OpScopeJSConsoleListener::LOG_DIRXML;
	case JS_Console::LOG_GROUP:
		return OpScopeJSConsoleListener::LOG_GROUP;
	case JS_Console::LOG_GROUP_COLLAPSED:
		return OpScopeJSConsoleListener::LOG_GROUP_COLLAPSED;
	case JS_Console::LOG_GROUP_END:
		return OpScopeJSConsoleListener::LOG_GROUP_END;
	case JS_Console::LOG_COUNT:
		return OpScopeJSConsoleListener::LOG_COUNT;
	case JS_Console::LOG_TABLE:
		return OpScopeJSConsoleListener::LOG_TABLE;
	case JS_Console::LOG_CLEAR:
		return OpScopeJSConsoleListener::LOG_CLEAR;
	}
}

#endif // SCOPE_ECMASCRIPT_DEBUGGER

#ifdef OPERA_CONSOLE
static const char* GetMessageContext(JS_Console::LogType logType)
{
	switch (logType)
	{
	case JS_Console::LOG_LOG:
		return "console.log";
	case JS_Console::LOG_DEBUG:
		return "console.debug";
	case JS_Console::LOG_INFO:
		return "console.info";
	case JS_Console::LOG_WARN:
		return "console.warn";
	case JS_Console::LOG_ASSERT:
		return "console.assert";
	case JS_Console::LOG_ERROR:
		return "console.error";
	default:
		OP_ASSERT(!"Please add a new case for the unknown LOG-type.");
		return "console (unknown)"; // Unsupported.
	}
}
#endif // OPERA_CONSOLE

int
JS_Console::log(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	ES_Host_Call_Status status = ES_FAILED;

	JS_Console::LogType log_type = static_cast<JS_Console::LogType>(data);

	if (log_type == JS_Console::LOG_ASSERT && (argc > 0) && AssertValue(argv[0]))
		return status; // Assert OK. Do nothing.

#ifdef OPERA_CONSOLE
	if (OpStatus::IsMemoryError(PostConsoleMessage(log_type, origining_runtime, argv, argc)))
		return ES_NO_MEMORY;
#endif // OPERA_CONSOLE

#ifdef SCOPE_ECMASCRIPT_DEBUGGER
	OpScopeJSConsoleListener::OnConsoleLog(origining_runtime, ConvertEnum(log_type), argv, argc);
#endif // SCOPE_ECMASCRIPT_DEBUGGER

	return status;
}

int
JS_Console::time(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	// We need at least one string argument.
	DOM_CHECK_ARGUMENTS_SILENT("s");

#ifdef SCOPE_ECMASCRIPT_DEBUGGER
	OpScopeJSConsoleListener::OnConsoleTime(origining_runtime, argv[0].value.string);
#endif // SCOPE_ECMASCRIPT_DEBUGGER

	return ES_FAILED;
}

int
JS_Console::timeEnd(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	// We need at least one string argument.
	DOM_CHECK_ARGUMENTS_SILENT("s");

#ifdef SCOPE_ECMASCRIPT_DEBUGGER
	OpScopeJSConsoleListener::OnConsoleTimeEnd(origining_runtime, argv[0].value.string);
#endif // SCOPE_ECMASCRIPT_DEBUGGER

	return ES_FAILED;
}


int
JS_Console::trace(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
#ifdef SCOPE_ECMASCRIPT_DEBUGGER
	OpScopeJSConsoleListener::OnConsoleTrace(origining_runtime);
#endif // SCOPE_ECMASCRIPT_DEBUGGER

	return ES_FAILED;
}

int
JS_Console::profile(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	// The argument is optional, but if present, it must be a string.
	if (argc > 0)
		DOM_CHECK_ARGUMENTS_SILENT("s");

#ifdef SCOPE_ECMASCRIPT_DEBUGGER

	const uni_char *title = (argc > 0) ? argv[0].value.string : NULL;

	OpScopeJSConsoleListener::OnConsoleProfile(origining_runtime, title);

#endif // SCOPE_ECMASCRIPT_DEBUGGER

	return ES_FAILED;
}

int
JS_Console::profileEnd(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
#ifdef SCOPE_ECMASCRIPT_DEBUGGER
	OpScopeJSConsoleListener::OnConsoleProfileEnd(origining_runtime);
#endif // SCOPE_ECMASCRIPT_DEBUGGER

	return ES_FAILED;
}

int
JS_Console::exception(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
#ifdef SCOPE_ECMASCRIPT_DEBUGGER
	OpScopeJSConsoleListener::OnConsoleException(origining_runtime, argv, argc);
#endif // SCOPE_ECMASCRIPT_DEBUGGER

	return ES_FAILED;
}

#ifdef OPERA_CONSOLE

/* static */ OP_STATUS
JS_Console::PostConsoleMessage(LogType type, DOM_Runtime *runtime, ES_Value* argv, int argc)
{
	OpConsoleEngine::Severity severity;

	switch (type)
	{
	case LOG_LOG:
	case LOG_DEBUG:
	case LOG_INFO:
	case LOG_WARN:
		severity = OpConsoleEngine::Information;
		break;
	case LOG_ASSERT:
		if (argc > 0)
		{
			// Skip the first argument to console.assert.
			--argc;
			++argv;
		}
	case LOG_ERROR:
		severity = OpConsoleEngine::Error;
		break;
	default:
		return OpStatus::ERR; // Unsupported.
	}

	OpConsoleEngine::Message cmessage(OpConsoleEngine::EcmaScript, severity);

	FramesDocument *frm_doc = runtime->GetFramesDocument();
	cmessage.window = (frm_doc ? frm_doc->GetWindow()->Id() : 0);

	RETURN_IF_ERROR(runtime->GetDisplayURL(cmessage.url));

	RETURN_IF_ERROR(cmessage.context.Set(GetMessageContext(type)));

	RETURN_IF_ERROR(JS_Console::FormatString(argv, argc, cmessage.message));

	RETURN_IF_LEAVE(g_console->PostMessageL(&cmessage));

	return OpStatus::OK;
}

/* static */ OP_STATUS
JS_Console::FormatString(ES_Value* argv, int argc, OpString &str)
{
	if (argc == 0)
		return OpStatus::OK;

	// Holds the formatted string.
	TempBuffer buf;

	// The argument which will be stringified next.
	int argument = 0;

	// If the first argument is a string, check if it contains placeholders.
	if (argv[0].type == VALUE_STRING)
	{
		// The first argument is the format string. It may or may not contain
		// formatting placeholders (%).
		const uni_char *placeholder = argv[0].value.string;
		const uni_char *stored = placeholder;

		// Skip the formatting string.
		++argument;

		while ((placeholder = uni_strchr(placeholder, '%')) != NULL)
		{
			// Calculate the length until the '%'.
			int length = (placeholder - stored);

			// Skip the '%', and check that 'placeholder' now points to
			// a supported character.
			if (!JS_Console::IsSupportedPlaceholder(++placeholder))
				continue;

			// Append everything up until the '%'.
			if (length > 0)
				RETURN_IF_ERROR(buf.Append(stored, length));

			// If the character after the first '%' is another '%', then output
			// a single '%'.
			if (*placeholder == '%')
				RETURN_IF_ERROR(buf.Append("%"));
			// Otherwise, append the string representation of the ES_Value.
			else if (argument < argc)
				RETURN_IF_ERROR(JS_Console::AppendValue(argv[argument++], buf));
			// Or, if we don't have more arguments, output the original placeholder.
			else
				RETURN_IF_ERROR(buf.Append(placeholder - 1, 2));

			// Skip the character following the %, but only if not at the
			// end of string.
			if (*placeholder != '\0')
				++placeholder;

			stored = placeholder;
		}

		// Append the rest of the formatting string, if any.
		if (*stored != '\0')
			RETURN_IF_ERROR(buf.Append(stored));
	}

	// If we have more arguments, append them in a space delimited list.
	while (argument < argc)
	{
		// Never start a string with a space.
		if (argument > 0)
			RETURN_IF_ERROR(buf.Append(" "));

		RETURN_IF_ERROR(AppendValue(argv[argument++], buf));
	}

	return str.Set(buf.GetStorage());
}

/* static */ OP_STATUS
JS_Console::AppendValue(const ES_Value &value, TempBuffer &buf)
{
	switch (value.type)
	{
	case VALUE_UNDEFINED:
		return buf.Append("undefined");
	case VALUE_NULL:
		return buf.Append("null");
	case VALUE_BOOLEAN:
		return buf.Append(value.value.boolean ? "true" : "false");
	case VALUE_NUMBER:
		return buf.AppendDouble(value.value.number);
	case VALUE_STRING:
		return buf.Append(value.value.string);
	case VALUE_OBJECT:
		RETURN_IF_ERROR(buf.Append("[object "));
		RETURN_IF_ERROR(buf.Append(ES_Runtime::GetClass(value.value.object)));
		return buf.Append("]");
	case VALUE_STRING_WITH_LENGTH:
		return buf.Append(value.value.string_with_length->string, value.value.string_with_length->length);
	}

	return OpStatus::OK;
}

/* static */ BOOL
JS_Console::IsSupportedPlaceholder(const uni_char *p)
{
	return (*p == 's' || *p == 'd' || *p == 'i' || *p == 'f' || *p == 'o' || *p == '%');
}

#endif // OPERA_CONSOLE

/* static */ BOOL
JS_Console::AssertValue(ES_Value &value)
{
	switch (value.type)
	{
	case VALUE_BOOLEAN:
		return value.value.boolean;
	case VALUE_NUMBER:
		return value.value.number != 0;
	case VALUE_STRING:
		return *value.value.string != '\0';
	case VALUE_STRING_WITH_LENGTH:
		return value.value.string_with_length->length > 0;
	case VALUE_OBJECT:
		return TRUE;
	default: // NULL, UNDEFINED
		return FALSE;
	}
}

DOM_FUNCTIONS_START(JS_Console)
DOM_FUNCTIONS_FUNCTION(JS_Console, JS_Console::time, "time", "s")
DOM_FUNCTIONS_FUNCTION(JS_Console, JS_Console::timeEnd, "timeEnd", 0)
DOM_FUNCTIONS_FUNCTION(JS_Console, JS_Console::trace, "trace", 0)
DOM_FUNCTIONS_FUNCTION(JS_Console, JS_Console::profile, "profile", "s")
DOM_FUNCTIONS_FUNCTION(JS_Console, JS_Console::profileEnd, "profileEnd", 0)
DOM_FUNCTIONS_FUNCTION(JS_Console, JS_Console::exception, "exception", "-")
DOM_FUNCTIONS_END(JS_Console)

#define DATA_LOG_LOG static_cast<int>(JS_Console::LOG_LOG)
#define DATA_LOG_DEBUG static_cast<int>(JS_Console::LOG_DEBUG)
#define DATA_LOG_INFO static_cast<int>(JS_Console::LOG_INFO)
#define DATA_LOG_WARN static_cast<int>(JS_Console::LOG_WARN)
#define DATA_LOG_ERROR static_cast<int>(JS_Console::LOG_ERROR)
#define DATA_LOG_ASSERT static_cast<int>(JS_Console::LOG_ASSERT)
#define DATA_LOG_DIR static_cast<int>(JS_Console::LOG_DIR)
#define DATA_LOG_DIRXML static_cast<int>(JS_Console::LOG_DIRXML)
#define DATA_LOG_GROUP static_cast<int>(JS_Console::LOG_GROUP)
#define DATA_LOG_GROUP_COLLAPSED static_cast<int>(JS_Console::LOG_GROUP_COLLAPSED)
#define DATA_LOG_GROUP_END static_cast<int>(JS_Console::LOG_GROUP_END)
#define DATA_LOG_COUNT static_cast<int>(JS_Console::LOG_COUNT)
#define DATA_LOG_TABLE static_cast<int>(JS_Console::LOG_TABLE)
#define DATA_LOG_CLEAR static_cast<int>(JS_Console::LOG_CLEAR)

DOM_FUNCTIONS_WITH_DATA_START(JS_Console)
DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_Console, JS_Console::log, DATA_LOG_LOG, "log", "-")
DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_Console, JS_Console::log, DATA_LOG_DEBUG, "debug", "-")
DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_Console, JS_Console::log, DATA_LOG_INFO, "info", "-")
DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_Console, JS_Console::log, DATA_LOG_WARN, "warn", "-")
DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_Console, JS_Console::log, DATA_LOG_ERROR, "error", "-")
DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_Console, JS_Console::log, DATA_LOG_ASSERT, "assert", "-")
DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_Console, JS_Console::log, DATA_LOG_DIR, "dir", "-")
DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_Console, JS_Console::log, DATA_LOG_DIRXML, "dirxml", "-")
DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_Console, JS_Console::log, DATA_LOG_GROUP, "group", "-")
DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_Console, JS_Console::log, DATA_LOG_GROUP_COLLAPSED, "groupCollapsed", "-")
DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_Console, JS_Console::log, DATA_LOG_GROUP_END, "groupEnd", "-")
DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_Console, JS_Console::log, DATA_LOG_COUNT, "count", "-")
DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_Console, JS_Console::log, DATA_LOG_TABLE, "table", "-")
DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_Console, JS_Console::log, DATA_LOG_CLEAR, "clear", 0)
DOM_FUNCTIONS_WITH_DATA_END(JS_Console)

