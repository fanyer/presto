/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef ECMASCRIPT_REMOTE_DEBUGGER

#include "modules/ecmascript_utils/esremotemessageparts.h"
#include "modules/ecmascript_utils/esremotedebugger.h"
#include "modules/ecmascript_utils/esdebugger.h"

ES_HelloMessagePart::ES_HelloMessagePart()
	: ES_DebugMessagePart(BODY_HELLO),
	  os(NULL),
	  platform(NULL),
	  useragent(NULL),
	  delete_strings(TRUE)
{
}

/* virtual */
ES_HelloMessagePart::~ES_HelloMessagePart()
{
	if (delete_strings)
	{
		FreeItemDataString(os, os_length);
		FreeItemDataString(platform, platform_length);
		FreeItemDataString(useragent, useragent_length);
	}
}

/* virtual */ unsigned
ES_HelloMessagePart::GetLength()
{
	return 6;
}

/* virtual */ void
ES_HelloMessagePart::GetItemData(unsigned index, const char *&data, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	switch (index)
	{
	case 0:
		GetItemDataUnsigned(data, length, ES_DebugMessage::TYPE_HELLO);
		return;

	case 1:
		GetItemDataUnsigned(data, length, ES_DebugMessage::PROTOCOL_VERSION);
		return;

	case 2:
		data = os;
		length = os_length;
		GetItemDataString(data, length);
		return;

	case 3:
		data = platform;
		length = platform_length;
		GetItemDataString(data, length);
		return;

	case 4:
		data = useragent;
		length = useragent_length;
		GetItemDataString(data, length);
		return;

	default:
#define SUPPORTED_COMMAND(command) (1 << (ES_DebugMessage::command - 1))
		GetItemDataUnsigned(data, length, (SUPPORTED_COMMAND(TYPE_NEW_SCRIPT) |
		                                   SUPPORTED_COMMAND(TYPE_THREAD_STARTED) |
		                                   SUPPORTED_COMMAND(TYPE_THREAD_FINISHED) |
		                                   SUPPORTED_COMMAND(TYPE_STOPPED_AT) |
		                                   SUPPORTED_COMMAND(TYPE_CONTINUE) |
		                                   SUPPORTED_COMMAND(TYPE_EVAL) |
		                                   SUPPORTED_COMMAND(TYPE_EVAL_REPLY) |
		                                   SUPPORTED_COMMAND(TYPE_EXAMINE) |
		                                   SUPPORTED_COMMAND(TYPE_EXAMINE_REPLY) |
		                                   SUPPORTED_COMMAND(TYPE_ADD_BREAKPOINT) |
		                                   SUPPORTED_COMMAND(TYPE_REMOVE_BREAKPOINT) |
		                                   SUPPORTED_COMMAND(TYPE_SET_CONFIGURATION) |
		                                   SUPPORTED_COMMAND(TYPE_BACKTRACE) |
		                                   SUPPORTED_COMMAND(TYPE_BACKTRACE_REPLY) |
		                                   SUPPORTED_COMMAND(TYPE_BREAK)));
	}
#else // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	OP_ASSERT(FALSE);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
}

/* virtual */ void
ES_HelloMessagePart::ParseL(const char *&data, unsigned &size, unsigned &length)
{
	OP_ASSERT(FALSE);
}

ES_SetConfigurationMessagePart::ES_SetConfigurationMessagePart()
	: ES_DebugMessagePart(BODY_SET_CONFIGURATION)
{
}

/* virtual */ unsigned
ES_SetConfigurationMessagePart::GetLength()
{
	return 6;
}

/* virtual */ void
ES_SetConfigurationMessagePart::GetItemData(unsigned index, const char *&data, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
	BOOL value;

	switch (index)
	{
	case 0:
		GetItemDataUnsigned(data, length, ES_DebugMessage::TYPE_SET_CONFIGURATION;
		return;

	case 1:
		value = stop_at_script;
		break;

	case 2:
		value = stop_at_exception;
		break;

	case 3:
		value = stop_at_error;
		break;

	case 4:
		value = stop_at_abort;
		break;

	default:
		value = stop_at_gc;
		break;
	}

	GetItemDataBoolean(data, length, value);
#else // ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
	OP_ASSERT(FALSE);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
}

/* virtual */ void
ES_SetConfigurationMessagePart::ParseL(const char *&data, unsigned &size, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	ES_DebugMessage::ParseBooleanL(data, size, length, stop_at_script);
	ES_DebugMessage::ParseBooleanL(data, size, length, stop_at_exception);
	ES_DebugMessage::ParseBooleanL(data, size, length, stop_at_error);
	ES_DebugMessage::ParseBooleanL(data, size, length, stop_at_abort);
	ES_DebugMessage::ParseBooleanL(data, size, length, stop_at_gc);
#else // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	OP_ASSERT(FALSE);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
}

ES_NewScriptMessagePart::ES_NewScriptMessagePart()
	: ES_DebugMessagePart(BODY_NEW_SCRIPT),
	  source(NULL),
	  uri(NULL),
	  delete_strings(TRUE)
{
}

/* virtual */
ES_NewScriptMessagePart::~ES_NewScriptMessagePart()
{
	if (delete_strings)
	{
		FreeItemDataString(source, source_length);

		if (script_type == SCRIPT_TYPE_LINKED)
			FreeItemDataString(uri, uri_length);
	}
}

/* virtual */ unsigned
ES_NewScriptMessagePart::GetLength()
{
	return script_type == SCRIPT_TYPE_LINKED ? 6 : 5;
}

/* virtual */ void
ES_NewScriptMessagePart::GetItemData(unsigned index, const char *&data, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	switch (index)
	{
	case 0:
		GetItemDataUnsigned(data, length, ES_DebugMessage::TYPE_NEW_SCRIPT);
		return;

	case 1:
		GetItemDataUnsigned(data, length, runtime_id);
		return;

	case 2:
		GetItemDataUnsigned(data, length, script_id);
		return;

	case 3:
		GetItemDataUnsigned(data, length, script_type);
		return;

	case 4:
		data = source;
		length = source_length;
		GetItemDataString(data, length);
		return;

	default:
		data = uri;
		length = uri_length;
		GetItemDataString(data, length);
	}
#else // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	OP_ASSERT(FALSE);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
}

/* virtual */ void
ES_NewScriptMessagePart::ParseL(const char *&data, unsigned &size, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
	delete_strings = FALSE;

	ES_DebugMessage::ParseUnsignedL(data, size, length, runtime_id);
	ES_DebugMessage::ParseUnsignedL(data, size, length, script_id);
	ES_DebugMessage::ParseUnsignedL(data, size, length, script_type);
	ES_DebugMessage::ParseStringL(data, size, length, source, source_length);

	if (script_type == SCRIPT_TYPE_LINKED)
		ES_DebugMessage::ParseStringL(data, size, length, uri, uri_length);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
}

ES_ThreadStartedMessagePart::ES_ThreadStartedMessagePart()
	: ES_DebugMessagePart(BODY_THREAD_STARTED),
	  event_namespace_uri(NULL),
	  event_type(NULL),
	  delete_strings(TRUE)
{
}

/* virtual */
ES_ThreadStartedMessagePart::~ES_ThreadStartedMessagePart()
{
	if (delete_strings && thread_type == THREAD_TYPE_EVENT)
	{
		FreeItemDataString(event_namespace_uri, event_namespace_uri_length);
		FreeItemDataString(event_type, event_type_length);
	}
}

/* virtual */ unsigned
ES_ThreadStartedMessagePart::GetLength()
{
	return thread_type == THREAD_TYPE_EVENT ? 7 : 5;
}

/* virtual */ void
ES_ThreadStartedMessagePart::GetItemData(unsigned index, const char *&data, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	switch (index)
	{
	case 0:
		GetItemDataUnsigned(data, length, ES_DebugMessage::TYPE_THREAD_STARTED);
		return;

	case 1:
		GetItemDataUnsigned(data, length, runtime_id);
		return;

	case 2:
		GetItemDataUnsigned(data, length, thread_id);
		return;

	case 3:
		GetItemDataUnsigned(data, length, parent_thread_id);
		return;

	case 4:
		GetItemDataUnsigned(data, length, thread_type);
		return;

	case 5:
		data = event_namespace_uri;
		length = event_namespace_uri_length;
		GetItemDataString(data, length);
		return;

	default:
		data = event_type;
		length = event_type_length;
		GetItemDataString(data, length);
	}
#else // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	OP_ASSERT(FALSE);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
}

/* virtual */ void
ES_ThreadStartedMessagePart::ParseL(const char *&data, unsigned &size, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
	delete_strings = FALSE;

	ES_DebugMessage::ParseUnsignedL(data, size, length, runtime_id);
	ES_DebugMessage::ParseUnsignedL(data, size, length, thread_id);
	ES_DebugMessage::ParseUnsignedL(data, size, length, parent_thread_id);
	ES_DebugMessage::ParseUnsignedL(data, size, length, thread_type);

	if (thread_type == THREAD_TYPE_EVENT)
	{
		ES_DebugMessage::ParseStringL(data, size, length, event_namespace_uri, event_namespace_uri_length);
		ES_DebugMessage::ParseStringL(data, size, length, event_type, event_type_length);
	}
#endif // ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
}

ES_ThreadFinishedMessagePart::ES_ThreadFinishedMessagePart()
	: ES_DebugMessagePart(BODY_THREAD_FINISHED)
{
}

/* virtual */ unsigned
ES_ThreadFinishedMessagePart::GetLength()
{
	return 4;
}

/* virtual */ void
ES_ThreadFinishedMessagePart::GetItemData(unsigned index, const char *&data, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	switch (index)
	{
	case 0:
		GetItemDataUnsigned(data, length, ES_DebugMessage::TYPE_THREAD_FINISHED);
		return;

	case 1:
		GetItemDataUnsigned(data, length, runtime_id);
		return;

	case 2:
		GetItemDataUnsigned(data, length, thread_id);
		return;

	default:
		GetItemDataUnsigned(data, length, status);
	}
#else // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	OP_ASSERT(FALSE);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
}

/* virtual */ void
ES_ThreadFinishedMessagePart::ParseL(const char *&data, unsigned &size, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
	ES_DebugMessage::ParseUnsignedL(data, size, length, runtime_id);
	ES_DebugMessage::ParseUnsignedL(data, size, length, thread_id);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
}

ES_StoppedAtMessagePart::ES_StoppedAtMessagePart()
	: ES_DebugMessagePart(BODY_STOPPED_AT)
{
}

/* virtual */ unsigned
ES_StoppedAtMessagePart::GetLength()
{
	return 5;
}

/* virtual */ void
ES_StoppedAtMessagePart::GetItemData(unsigned index, const char *&data, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	switch (index)
	{
	case 0:
		GetItemDataUnsigned(data, length, ES_DebugMessage::TYPE_STOPPED_AT);
		return;

	case 1:
		GetItemDataUnsigned(data, length, runtime_id);
		return;

	case 2:
		GetItemDataUnsigned(data, length, thread_id);
		return;

	case 3:
		GetItemDataUnsigned(data, length, position.scriptid);
		return;

	default:
		GetItemDataUnsigned(data, length, position.linenr);
		return;
	}
#else // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	OP_ASSERT(FALSE);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
}

/* virtual */ void
ES_StoppedAtMessagePart::ParseL(const char *&data, unsigned &size, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
	ES_DebugMessage::ParseUnsignedL(data, size, length, runtime_id);
	ES_DebugMessage::ParseUnsignedL(data, size, length, thread_id);
	ES_DebugMessage::ParseUnsignedL(data, size, length, position.scriptid);
	ES_DebugMessage::ParseUnsignedL(data, size, length, position.linenr);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
}

ES_ContinueMessagePart::ES_ContinueMessagePart()
	: ES_DebugMessagePart(BODY_CONTINUE)
{
}

/* virtual */ unsigned
ES_ContinueMessagePart::GetLength()
{
	return 4;
}

/* virtual */ void
ES_ContinueMessagePart::GetItemData(unsigned index, const char *&data, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
	switch (index)
	{
	case 0:
		GetItemDataUnsigned(data, length, ES_DebugMessage::TYPE_CONTINUE);
		return;

	case 1:
		GetItemDataUnsigned(data, length, runtime_id);
		return;

	case 2:
		GetItemDataUnsigned(data, length, thread_id);
		return;

	default:
		GetItemDataUnsigned(data, length, mode);
		return;
	}
#endif // ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
}

/* virtual */ void
ES_ContinueMessagePart::ParseL(const char *&data, unsigned &size, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	ES_DebugMessage::ParseUnsignedL(data, size, length, runtime_id);
	ES_DebugMessage::ParseUnsignedL(data, size, length, thread_id);
	ES_DebugMessage::ParseUnsignedL(data, size, length, mode);
#else // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	OP_ASSERT(FALSE);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
}

ES_EvalMessagePart::ES_EvalMessagePart()
	: ES_DebugMessagePart(BODY_EVAL),
	  script(NULL),
	  delete_strings(TRUE)
{
}

ES_EvalMessagePart::~ES_EvalMessagePart()
{
	if (delete_strings)
		FreeItemDataString(script, script_length);
}

/* virtual */ unsigned
ES_EvalMessagePart::GetLength()
{
	return 6;
}

/* virtual */ void
ES_EvalMessagePart::GetItemData(unsigned index, const char *&data, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
	switch (index)
	{
	case 0:
		GetItemDataUnsigned(data, length, ES_DebugMessage::TYPE_EVAL);
		return;

	case 1:
		GetItemDataUnsigned(data, length, tag);
		return;

	case 2:
		GetItemDataUnsigned(data, length, runtime_id);
		return;

	case 3:
		GetItemDataUnsigned(data, length, thread_id);
		return;

	case 4:
		GetItemDataUnsigned(data, length, frame_index);
		return;

	default:
		data = script;
		length = script_length;
		GetItemDataString(data, length);
	}
#else // ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
	OP_ASSERT(FALSE);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
}

/* virtual */ void
ES_EvalMessagePart::ParseL(const char *&data, unsigned &size, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	delete_strings = FALSE;

	ES_DebugMessage::ParseUnsignedL(data, size, length, tag);
	ES_DebugMessage::ParseUnsignedL(data, size, length, runtime_id);
	ES_DebugMessage::ParseUnsignedL(data, size, length, thread_id);
	ES_DebugMessage::ParseUnsignedL(data, size, length, frame_index);
	ES_DebugMessage::ParseStringL(data, size, length, script, script_length);
#else // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	OP_ASSERT(FALSE);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
}

ES_EvalReplyMessagePart::ES_EvalReplyMessagePart()
	: ES_DebugMessagePart(BODY_EVAL_REPLY)
{
}

/* virtual */ unsigned
ES_EvalReplyMessagePart::GetLength()
{
	return 3;
}

/* virtual */ void
ES_EvalReplyMessagePart::GetItemData(unsigned index, const char *&data, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	switch (index)
	{
	case 0:
		GetItemDataUnsigned(data, length, ES_DebugMessage::TYPE_EVAL_REPLY);
		return;

	case 1:
		GetItemDataUnsigned(data, length, tag);
		return;

	default:
		GetItemDataUnsigned(data, length, status);
	}
#else // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	OP_ASSERT(FALSE);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
}

/* virtual */ void
ES_EvalReplyMessagePart::ParseL(const char *&data, unsigned &size, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
	ES_DebugMessage::ParseUnsignedL(data, size, length, tag);
	ES_DebugMessage::ParseUnsignedL(data, size, length, status);
#else // ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
	OP_ASSERT(FALSE);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
}

ES_ExamineMessagePart::ES_ExamineMessagePart()
	: ES_DebugMessagePart(BODY_EXAMINE),
	  objects_count(0),
	  object_ids(NULL)
{
}

/* virtual */ unsigned
ES_ExamineMessagePart::GetLength()
{
	return 4 + objects_count;
}

/* virtual */ void
ES_ExamineMessagePart::GetItemData(unsigned index, const char *&data, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
	switch (index)
	{
	case 0:
		GetItemDataUnsigned(data, length, ES_DebugMessage::TYPE_EXAMINE);
		return;

	case 1:
		GetItemDataUnsigned(data, length, tag);
		return;

	case 2:
		GetItemDataUnsigned(data, length, runtime_id);
		return;

	case 3:
		GetItemDataUnsigned(data, length, objects_count);
		return;

	default:
		OP_ASSERT(index - 3 < objects_count);
		GetItemDataUnsigned(data, length, object_ids[index - 3]);
	}
#else // ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
	OP_ASSERT(FALSE);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
}

/* virtual */ void
ES_ExamineMessagePart::ParseL(const char *&data, unsigned &size, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	ES_DebugMessage::ParseUnsignedL(data, size, length, tag);
	ES_DebugMessage::ParseUnsignedL(data, size, length, runtime_id);
	ES_DebugMessage::ParseBooleanL(data, size, length, in_scope);

	if (in_scope)
	{
		ES_DebugMessage::ParseUnsignedL(data, size, length, thread_id);
		ES_DebugMessage::ParseUnsignedL(data, size, length, frame_index);
	}
	else
	{
		ES_DebugMessage::ParseUnsignedL(data, size, length, objects_count);

		object_ids = new (ELeave) unsigned[objects_count];

		for (unsigned index = 0; index < objects_count; ++index)
			ES_DebugMessage::ParseUnsignedL(data, size, length, object_ids[index]);
	}
#else // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	OP_ASSERT(FALSE);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
}

ES_ExamineReplyMessagePart::ES_ExamineReplyMessagePart()
	: ES_DebugMessagePart(BODY_EXAMINE_REPLY),
	  objects_count(0),
	  objects(NULL)
{
}

/* virtual */
ES_ExamineReplyMessagePart::~ES_ExamineReplyMessagePart()
{
	for (unsigned oindex = 0; oindex < objects_count; ++oindex)
	{
		if (delete_strings)
			for (unsigned pindex = 0; pindex < objects[oindex]->properties_count; ++pindex)
			{
				const char *data = objects[oindex]->names[pindex];
				unsigned length = objects[oindex]->name_lengths[pindex];
				FreeItemDataString(data, length);
			}

		delete[] objects[oindex]->names;
		delete[] objects[oindex]->name_lengths;
		delete[] objects[oindex]->values;
	}

	delete[] objects;
}

/* virtual */ unsigned
ES_ExamineReplyMessagePart::GetLength()
{
	unsigned length = 3;
	for (unsigned index = 0; index < objects_count; ++index)
		length += 2 + objects[index]->properties_count * 3;
	return length;
}

/* virtual */ void
ES_ExamineReplyMessagePart::GetItemData(unsigned index, const char *&data, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	switch (index)
	{
	case 0:
		GetItemDataUnsigned(data, length, ES_DebugMessage::TYPE_EXAMINE_REPLY);
		return;

	case 1:
		GetItemDataUnsigned(data, length, tag);
		return;

	case 2:
		GetItemDataUnsigned(data, length, objects_count);
		return;

	default:
		index -= 3;

		for (unsigned oindex = 0; oindex < objects_count; ++oindex)
			if (index < 2 + objects[oindex]->properties_count * 3)
				switch (index)
				{
				case 0:
					GetItemDataUnsigned(data, length, objects[oindex]->object_id);
					return;

				case 1:
					GetItemDataUnsigned(data, length, objects[oindex]->properties_count);
					return;

				default:
					index -= 2;
					unsigned pindex = index / 3;
					switch (index % 3)
					{
					case 0:
						data = objects[oindex]->names[pindex];
						length = objects[oindex]->name_lengths[pindex];
						GetItemDataString(data, length);
						return;

					case 1:
						GetItemDataValueType(data, length, objects[oindex]->values[pindex]);
						return;

					case 2:
						GetItemDataValueValue(data, length, objects[oindex]->values[pindex]);
						return;
					}
				}
			else
				index -= 2 + objects[oindex]->properties_count * 3;

		OP_ASSERT(FALSE);
	}
#else // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	OP_ASSERT(FALSE);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
}

/* virtual */ void
ES_ExamineReplyMessagePart::ParseL(const char *&data, unsigned &size, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
	ES_DebugMessage::ParseUnsignedL(data, size, length, tag);
	ES_DebugMessage::ParseUnsignedL(data, size, length, objects_count);
	OP_ASSERT(FALSE);
#else // ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
	OP_ASSERT(FALSE);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
}

void
ES_ExamineReplyMessagePart::AddObjectL(unsigned id, unsigned properties_count)
{
	Object **new_objects = new (ELeave) Object *[objects_count + 1];
	op_memcpy(new_objects, objects, objects_count * sizeof objects[0]);
	delete[] objects;
	objects = new_objects;

	Object *object = objects[objects_count] = new (ELeave) Object;

	object->object_id = id;
	object->properties_count = 0;
	object->names = new (ELeave) char *[properties_count];
	object->name_lengths = new (ELeave) unsigned[properties_count];
	object->values = new (ELeave) ES_DebugValue[properties_count];

	objects_count += 1;
}

void
ES_ExamineReplyMessagePart::SetPropertyL(const uni_char *name, const ES_DebugValue &value)
{
	Object *object = objects[objects_count - 1];
	unsigned pindex = object->properties_count;

	object->names[pindex] = NULL;
	object->values[pindex].type = VALUE_UNDEFINED;

	++object->properties_count;

	SetString16L(object->names[pindex], object->name_lengths[pindex], name);
	SetValueL(object->values[pindex], value);
}

ES_BacktraceMessagePart::ES_BacktraceMessagePart()
	: ES_DebugMessagePart(BODY_BACKTRACE)
{
}

/* virtual */ unsigned
ES_BacktraceMessagePart::GetLength()
{
	return 4;
}

/* virtual */ void
ES_BacktraceMessagePart::GetItemData(unsigned index, const char *&data, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
	switch (index)
	{
	case 0:
		GetItemDataUnsigned(data, length, ES_DebugMessage::TYPE_BACKTRACE);
		return;

	case 1:
		GetItemDataUnsigned(data, length, tag);
		return;

	case 2:
		GetItemDataUnsigned(data, length, runtime_id);
		return;

	default:
		GetItemDataUnsigned(data, length, thread_id);
	}
#else // ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
	OP_ASSERT(FALSE);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
}

/* virtual */ void
ES_BacktraceMessagePart::ParseL(const char *&data, unsigned &size, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	ES_DebugMessage::ParseUnsignedL(data, size, length, tag);
	ES_DebugMessage::ParseUnsignedL(data, size, length, runtime_id);
	ES_DebugMessage::ParseUnsignedL(data, size, length, thread_id);
	ES_DebugMessage::ParseUnsignedL(data, size, length, max_frames);
#else // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	OP_ASSERT(FALSE);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
}

ES_BacktraceReplyMessagePart::ES_BacktraceReplyMessagePart()
	: ES_DebugMessagePart(BODY_BACKTRACE_REPLY),
	  stack(NULL)
{
}

/* virtual */
ES_BacktraceReplyMessagePart::~ES_BacktraceReplyMessagePart()
{
	delete[] stack;
}

/* virtual */ unsigned
ES_BacktraceReplyMessagePart::GetLength()
{
	return 3 + 5 * stack_length;
}

/* virtual */ void
ES_BacktraceReplyMessagePart::GetItemData(unsigned index, const char *&data, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	switch (index)
	{
	case 0:
		GetItemDataUnsigned(data, length, ES_DebugMessage::TYPE_BACKTRACE_REPLY);
		return;

	case 1:
		GetItemDataUnsigned(data, length, tag);
		return;

	case 2:
		GetItemDataUnsigned(data, length, stack_length);
		return;

	default:
		index -= 3;
		ES_DebugStackFrame &frame = stack[index / 5];
		switch (index % 5)
		{
		case 0:
			GetItemDataUnsigned(data, length, frame.function.id);
			return;

		case 1:
			GetItemDataUnsigned(data, length, frame.arguments);
			return;

		case 2:
			GetItemDataUnsigned(data, length, frame.variables);
			return;

		case 3:
			GetItemDataUnsigned(data, length, frame.script_no);
			return;

		default:
			GetItemDataUnsigned(data, length, frame.line_no);
		}
	}
#else // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	OP_ASSERT(FALSE);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
}

/* virtual */ void
ES_BacktraceReplyMessagePart::ParseL(const char *&data, unsigned &size, unsigned &length)
{
	OP_ASSERT(FALSE);
}

ES_ObjectInfoMessagePart::ES_ObjectInfoMessagePart()
	: ES_DebugMessagePart(AUXILIARY_OBJECT_INFO),
	  name(NULL),
	  delete_strings(TRUE)
{
}

/* virtual */
ES_ObjectInfoMessagePart::~ES_ObjectInfoMessagePart()
{
	if (delete_strings && name)
		FreeItemDataString(name, name_length);
}

/* virtual */ unsigned
ES_ObjectInfoMessagePart::GetLength()
{
	return 6;
}

/* virtual */ void
ES_ObjectInfoMessagePart::GetItemData(unsigned index, const char *&data, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	switch (index)
	{
	case 0:
		GetItemDataUnsigned(data, length, ES_DebugMessage::AUXILIARY_OBJECT_INFO);
		return;

	case 1:
		GetItemDataUnsigned(data, length, object_id);
		return;

	case 2:
		GetItemDataUnsigned(data, length, prototype_id);
		return;

	case 3:
		GetItemDataBoolean(data, length, is_callable);
		return;

	case 4:
		GetItemDataBoolean(data, length, is_function);
		return;

	default:
		data = name;
		length = name_length;
		GetItemDataString(data, length);
	}
#else // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	OP_ASSERT(FALSE);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
}

/* virtual */ void
ES_ObjectInfoMessagePart::ParseL(const char *&data, unsigned &size, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
	delete_strings = FALSE;
	ES_DebugMessage::ParseUnsignedL(data, size, length, object_id);
	ES_DebugMessage::ParseBooleanL(data, size, length, is_callable);
	ES_DebugMessage::ParseBooleanL(data, size, length, is_function);
	ES_DebugMessage::ParseStringL(data, size, length, name, name_length);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
}

ES_WatchUpdateMessagePart::ES_WatchUpdateMessagePart()
	: ES_DebugMessagePart(AUXILIARY_WATCH_UPDATE),
	  changed(NULL),
	  delete_strings(TRUE)
{
}

/* virtual */
ES_WatchUpdateMessagePart::~ES_WatchUpdateMessagePart()
{
	if (delete_strings)
		for (unsigned index = 0; index < changed_count; ++index)
			delete[] changed[index].property_name;

	delete[] changed;
}

/* virtual */ unsigned
ES_WatchUpdateMessagePart::GetLength()
{
	return 3 + changed_count * 3;
}

/* virtual */ void
ES_WatchUpdateMessagePart::GetItemData(unsigned index, const char *&data, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	switch (index)
	{
	case 0:
		GetItemDataUnsigned(data, length, ES_DebugMessage::AUXILIARY_WATCH_UPDATE);
		return;

	case 1:
		GetItemDataUnsigned(data, length, object_id);
		return;

	case 2:
		GetItemDataUnsigned(data, length, changed_count);
		return;

	default:
		Changed &c = changed[index / 3 - 1];

		switch (index % 3)
		{
		case 0:
			data = c.property_name;
			length = c.property_name_length;
			GetItemDataString(data, length);
			return;

		case 1:
			GetItemDataValueType(data, length, c.property_value);
			return;

		default:
			GetItemDataValueValue(data, length, c.property_value);
		}
	}
#else // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	OP_ASSERT(FALSE);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
}

/* virtual */ void
ES_WatchUpdateMessagePart::ParseL(const char *&data, unsigned &size, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
	OP_ASSERT(FALSE);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
}

ES_StackMessagePart::ES_StackMessagePart()
	: ES_DebugMessagePart(AUXILIARY_STACK),
	  frame(NULL)
{
}

/* virtual */
ES_StackMessagePart::~ES_StackMessagePart()
{
	delete[] frame;
}

/* virtual */ unsigned
ES_StackMessagePart::GetLength()
{
	return 2 + frame_count * 5;
}

/* virtual */ void
ES_StackMessagePart::GetItemData(unsigned index, const char *&data, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	switch (index)
	{
	case 0:
		GetItemDataUnsigned(data, length, ES_DebugMessage::AUXILIARY_STACK);
		return;

	case 1:
		GetItemDataUnsigned(data, length, frame_count);
		return;

	default:
		Frame &f = frame[(index - 2) / 5];

		switch ((index - 2) % 5)
		{
		case 0:
			GetItemDataUnsigned(data, length, f.script_id);
			return;

		case 1:
			GetItemDataUnsigned(data, length, f.linenr);
			return;

		case 2:
			GetItemDataUnsigned(data, length, f.function_object_id);
			return;

		case 3:
			GetItemDataUnsigned(data, length, f.arguments_object_id);
			return;

		default:
			GetItemDataUnsigned(data, length, f.variables_object_id);
			return;
		}
	}
#else // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	OP_ASSERT(FALSE);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
}

/* virtual */ void
ES_StackMessagePart::ParseL(const char *&data, unsigned &size, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
	OP_ASSERT(FALSE);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
}

ES_BreakpointTriggeredMessagePart::ES_BreakpointTriggeredMessagePart()
	: ES_DebugMessagePart(AUXILIARY_BREAKPOINT_TRIGGERED)
{
}

/* virtual */ unsigned
ES_BreakpointTriggeredMessagePart::GetLength()
{
	return 2;
}

/* virtual */ void
ES_BreakpointTriggeredMessagePart::GetItemData(unsigned index, const char *&data, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	switch (index)
	{
	case 0:
		GetItemDataUnsigned(data, length, ES_DebugMessage::AUXILIARY_BREAKPOINT_TRIGGERED);
		return;

	case 1:
		GetItemDataUnsigned(data, length, id);
	}
#else // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	OP_ASSERT(FALSE);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
}

/* virtual */ void
ES_BreakpointTriggeredMessagePart::ParseL(const char *&data, unsigned &size, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
	ES_DebugMessage::ParseUnsignedL(data, size, length, id);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
}

ES_ReturnValueMessagePart::ES_ReturnValueMessagePart()
	: ES_DebugMessagePart(AUXILIARY_RETURN_VALUE)
{
}

/* virtual */
ES_ReturnValueMessagePart::~ES_ReturnValueMessagePart()
{
}

/* virtual */ unsigned
ES_ReturnValueMessagePart::GetLength()
{
	return 4;
}

/* virtual */ void
ES_ReturnValueMessagePart::GetItemData(unsigned index, const char *&data, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	switch (index)
	{
	case 0:
		GetItemDataUnsigned(data, length, ES_DebugMessage::AUXILIARY_RETURN_VALUE);
		return;

	case 1:
		GetItemDataUnsigned(data, length, function_object_id);
		return;

	case 2:
		GetItemDataValueType(data, length, value);
		return;

	default:
		GetItemDataValueValue(data, length, value);
	}
#else // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	OP_ASSERT(FALSE);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
}

/* virtual */ void
ES_ReturnValueMessagePart::ParseL(const char *&data, unsigned &size, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
	OP_ASSERT(FALSE);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
}

ES_ExceptionThrownMessagePart::ES_ExceptionThrownMessagePart()
	: ES_DebugMessagePart(AUXILIARY_EXCEPTION_THROWN)
{
	exception.type = VALUE_UNDEFINED;
}

/* virtual */
ES_ExceptionThrownMessagePart::~ES_ExceptionThrownMessagePart()
{
}

/* virtual */ unsigned
ES_ExceptionThrownMessagePart::GetLength()
{
	return 3;
}

/* virtual */ void
ES_ExceptionThrownMessagePart::GetItemData(unsigned index, const char *&data, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	switch (index)
	{
	case 0:
		GetItemDataUnsigned(data, length, ES_DebugMessage::AUXILIARY_EXCEPTION_THROWN);
		return;

	case 1:
		GetItemDataValueType(data, length, exception);
		return;

	default:
		GetItemDataValueValue(data, length, exception);
	}
#else // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	OP_ASSERT(FALSE);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
}

/* virtual */ void
ES_ExceptionThrownMessagePart::ParseL(const char *&data, unsigned &size, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
	OP_ASSERT(FALSE);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
}

ES_EvalVariableMessagePart::ES_EvalVariableMessagePart()
	: ES_DebugMessagePart(AUXILIARY_EVAL_VARIABLE),
	  name(NULL),
	  delete_strings(TRUE)
{
}

/* virtual */
ES_EvalVariableMessagePart::~ES_EvalVariableMessagePart()
{
}

/* virtual */ unsigned
ES_EvalVariableMessagePart::GetLength()
{
	return 4;
}

/* virtual */ void
ES_EvalVariableMessagePart::GetItemData(unsigned index, const char *&data, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
	switch (index)
	{
	case 0:
		GetItemDataUnsigned(data, length, ES_DebugMessage::AUXILIARY_EVAL_VARIABLE);
		return;

	case 1:
		data = name;
		length = name_length;
		GetItemDataString(data, length);
		return;

	case 2:
		GetItemDataValueType(data, length, value);
		return;

	default:
		GetItemDataValueValue(data, length, value);
	}
#else // ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
	OP_ASSERT(FALSE);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
}

/* virtual */ void
ES_EvalVariableMessagePart::ParseL(const char *&data, unsigned &size, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	delete_strings = FALSE;

	ES_DebugMessage::ParseStringL(data, size, length, name, name_length);
	ES_DebugMessage::ParseValueL(data, size, length, value);
#else // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	OP_ASSERT(FALSE);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
}

ES_ChangeBreakpointMessagePart::ES_ChangeBreakpointMessagePart(BOOL add)
	: ES_DebugMessagePart(add ? BODY_ADD_BREAKPOINT : BODY_REMOVE_BREAKPOINT),
	  add(add)
{
}

/* virtual */
ES_ChangeBreakpointMessagePart::~ES_ChangeBreakpointMessagePart()
{
}

/* virtual */ unsigned
ES_ChangeBreakpointMessagePart::GetLength()
{
	if (add)
		if (bpdata.type == ES_DebugBreakpointData::TYPE_FUNCTION)
			return 3;
		else
			return 4;
	else
		return 2;
}

/* virtual */ void
ES_ChangeBreakpointMessagePart::GetItemData(unsigned index, const char *&data, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
	switch (index)
	{
	case 0:
		GetItemDataUnsigned(data, length, add ? ES_DebugMessage::BODY_ADD_BREAKPOINT : ES_DebugMessage::BODY_REMOVE_BREAKPOINT);
		return;

	case 1:
		GetItemDataUnsigned(data, length, tag);
		return;
	}
#else // ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
	OP_ASSERT(FALSE);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
}

/* virtual */ void
ES_ChangeBreakpointMessagePart::ParseL(const char *&data, unsigned &size, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	ES_DebugMessage::ParseUnsignedL(data, size, length, id);

	if (add)
	{
		unsigned type;
		ES_DebugMessage::ParseUnsignedL(data, size, length, type);
		bpdata.type = (ES_DebugBreakpointData::Type) type;

		switch (bpdata.type)
		{
		case ES_DebugBreakpointData::TYPE_POSITION:
			ES_DebugMessage::ParseUnsignedL(data, size, length, bpdata.data.position.scriptid);
			ES_DebugMessage::ParseUnsignedL(data, size, length, bpdata.data.position.linenr);
			break;

		case ES_DebugBreakpointData::TYPE_FUNCTION:
			ES_DebugMessage::ParseUnsignedL(data, size, length, bpdata.data.function.id);
			break;

		case ES_DebugBreakpointData::TYPE_EVENT:
			const char *tmp_namespaceuri;
			unsigned namespaceuri_length;
			const char *tmp_eventtype;
			unsigned eventtype_length;

			ES_DebugMessage::ParseStringL(data, size, length, tmp_namespaceuri, namespaceuri_length);
			ES_DebugMessage::ParseStringL(data, size, length, tmp_eventtype, eventtype_length);

			namespaceuri.SetFromUTF8L(tmp_namespaceuri, namespaceuri_length);
			eventtype.SetFromUTF8L(tmp_eventtype, eventtype_length);

			bpdata.data.event.namespace_uri = namespaceuri.CStr();
			bpdata.data.event.event_type = eventtype.CStr();
		}
	}
#else // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	OP_ASSERT(FALSE);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
}

ES_RuntimeInformationMessagePart::ES_RuntimeInformationMessagePart()
	: ES_DebugMessagePart(AUXILIARY_RUNTIME_INFORMATION),
	  framepath(NULL),
	  documenturi(NULL),
	  delete_strings(TRUE)
{
}

/* virtual */
ES_RuntimeInformationMessagePart::~ES_RuntimeInformationMessagePart()
{
	if (delete_strings)
	{
		FreeItemDataString(framepath, framepath_length);
		FreeItemDataString(documenturi, documenturi_length);
	}
}

/* virtual */ unsigned
ES_RuntimeInformationMessagePart::GetLength()
{
	return 5;
}

/* virtual */ void
ES_RuntimeInformationMessagePart::GetItemData(unsigned index, const char *&data, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	switch (index)
	{
	case 0:
		GetItemDataUnsigned(data, length, ES_DebugMessage::AUXILIARY_RUNTIME_INFORMATION);
		return;

	case 1:
		GetItemDataUnsigned(data, length, dbg_runtime_id);
		return;

	case 2:
		GetItemDataUnsigned(data, length, dbg_window_id);
		return;

	case 3:
		data = framepath;
		length = framepath_length;
		GetItemDataString(data, length);
		return;

	case 4:
		data = documenturi;
		length = documenturi_length;
		GetItemDataString(data, length);
	}
#else // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	OP_ASSERT(FALSE);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
}

/* virtual */ void
ES_RuntimeInformationMessagePart::ParseL(const char *&data, unsigned &size, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
	delete_strings = FALSE;

	ParseUnsignedL(data, size, length, windowid);
	ParseStringL(data, size, length, framepath, framepath_length);
	ParseStringL(data, size, length, documenturi, documenturi_length);
#else // ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
	OP_ASSERT(FALSE);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
}

ES_HeapStatisticsMessagePart::ES_HeapStatisticsMessagePart()
	: ES_DebugMessagePart(AUXILIARY_HEAP_STATISTICS)
{
}

/* virtual */ unsigned
ES_HeapStatisticsMessagePart::GetLength()
{
	return 13;
}

/* virtual */ void
ES_HeapStatisticsMessagePart::GetItemData(unsigned index, const char *&data, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	switch (index)
	{
	case 0:
		GetItemDataUnsigned(data, length, ES_DebugMessage::AUXILIARY_HEAP_STATISTICS);
		return;

	case 1:
		GetItemDataDouble(data, length, load_factor);
		return;

	default:
		index -= 2;
		GetItemDataUnsigned(data, length, unsigneds[index]);
	}
#else // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	OP_ASSERT(FALSE);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
}

/* virtual */ void
ES_HeapStatisticsMessagePart::ParseL(const char *&data, unsigned &size, unsigned &length)
{
	OP_ASSERT(FALSE);
}

ES_BreakMessagePart::ES_BreakMessagePart()
	: ES_DebugMessagePart(BODY_BREAK)
{
}

/* virtual */ unsigned
ES_BreakMessagePart::GetLength()
{
	return 3;
}

/* virtual */ void
ES_BreakMessagePart::GetItemData(unsigned index, const char *&data, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
	switch (index)
	{
	case 0:
		GetItemDataUnsigned(data, length, ES_DebugMessage::TYPE_BREAK);
		return;

	case 1:
		GetItemDataUnsigned(data, length, runtime_id);
		return;

	default:
		GetItemDataUnsigned(data, length, thread_id);
		return;
	}
#endif // ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
}

/* virtual */ void
ES_BreakMessagePart::ParseL(const char *&data, unsigned &size, unsigned &length)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	ES_DebugMessage::ParseUnsignedL(data, size, length, runtime_id);
	ES_DebugMessage::ParseUnsignedL(data, size, length, thread_id);
#else // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
	OP_ASSERT(FALSE);
#endif // ECMASCRIPT_REMOTE_DEBUGGER_FRONTEND
}

#endif /* ECMASCRIPT_REMOTE_DEBUGGER */
