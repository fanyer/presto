/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef ES_UTILS_ESREMOTEMESSAGEPARTS_H
#define ES_UTILS_ESREMOTEMESSAGEPARTS_H

#ifdef ECMASCRIPT_REMOTE_DEBUGGER

#include "modules/ecmascript_utils/esremotedebugger.h"

class ES_HelloMessagePart
	: public ES_DebugMessagePart
{
public:
	ES_HelloMessagePart();

	virtual ~ES_HelloMessagePart();
	virtual unsigned GetLength();
	virtual void GetItemData(unsigned index, const char *&data, unsigned &length);
	virtual void ParseL(const char *&data, unsigned &size, unsigned &length);

	char *os;
	unsigned os_length;
	char *platform;
	unsigned platform_length;
	char *useragent;
	unsigned useragent_length;
	BOOL delete_strings;
};

class ES_SetConfigurationMessagePart
	: public ES_DebugMessagePart
{
public:
	ES_SetConfigurationMessagePart();

	virtual unsigned GetLength();
	virtual void GetItemData(unsigned index, const char *&data, unsigned &length);
	virtual void ParseL(const char *&data, unsigned &size, unsigned &length);

	BOOL stop_at_script;
	BOOL stop_at_exception;
	BOOL stop_at_error;
	BOOL stop_at_abort;
	BOOL stop_at_gc;
};

class ES_NewScriptMessagePart
	: public ES_DebugMessagePart
{
public:
	ES_NewScriptMessagePart();

	virtual ~ES_NewScriptMessagePart();
	virtual unsigned GetLength();
	virtual void GetItemData(unsigned index, const char *&data, unsigned &length);
	virtual void ParseL(const char *&data, unsigned &size, unsigned &length);

	enum { SCRIPT_TYPE_INLINE = 1, SCRIPT_TYPE_LINKED, SCRIPT_TYPE_GENERATED, SCRIPT_TYPE_OTHER };

	unsigned runtime_id;
	unsigned script_id;
	unsigned script_type;
	char *source;
	unsigned source_length;
	char *uri;
	unsigned uri_length;
	BOOL delete_strings;
};

class ES_ThreadStartedMessagePart
	: public ES_DebugMessagePart
{
public:
	ES_ThreadStartedMessagePart();

	virtual ~ES_ThreadStartedMessagePart();
	virtual unsigned GetLength();
	virtual void GetItemData(unsigned index, const char *&data, unsigned &length);
	virtual void ParseL(const char *&data, unsigned &size, unsigned &length);

	enum { THREAD_TYPE_INLINE = 1, THREAD_TYPE_EVENT, THREAD_TYPE_URL, THREAD_TYPE_OTHER };

	unsigned runtime_id;
	unsigned thread_id;
	unsigned parent_thread_id;
	unsigned thread_type;
	char *event_namespace_uri;
	unsigned event_namespace_uri_length;
	char *event_type;
	unsigned event_type_length;
	BOOL delete_strings;
};

class ES_ThreadFinishedMessagePart
	: public ES_DebugMessagePart
{
public:
	ES_ThreadFinishedMessagePart();

	virtual unsigned GetLength();
	virtual void GetItemData(unsigned index, const char *&data, unsigned &length);
	virtual void ParseL(const char *&data, unsigned &size, unsigned &length);

	unsigned runtime_id;
	unsigned thread_id;
	unsigned status;
};

class ES_StoppedAtMessagePart
	: public ES_DebugMessagePart
{
public:
	ES_StoppedAtMessagePart();

	virtual unsigned GetLength();
	virtual void GetItemData(unsigned index, const char *&data, unsigned &length);
	virtual void ParseL(const char *&data, unsigned &size, unsigned &length);

	unsigned runtime_id;
	unsigned thread_id;
	ES_DebugPosition position;
};

class ES_ContinueMessagePart
	: public ES_DebugMessagePart
{
public:
	ES_ContinueMessagePart();

	virtual unsigned GetLength();
	virtual void GetItemData(unsigned index, const char *&data, unsigned &length);
	virtual void ParseL(const char *&data, unsigned &size, unsigned &length);

	unsigned runtime_id;
	unsigned thread_id;
	unsigned mode;
};

class ES_EvalMessagePart
	: public ES_DebugMessagePart
{
public:
	ES_EvalMessagePart();

	virtual ~ES_EvalMessagePart();
	virtual unsigned GetLength();
	virtual void GetItemData(unsigned index, const char *&data, unsigned &length);
	virtual void ParseL(const char *&data, unsigned &size, unsigned &length);

	unsigned tag;
	unsigned runtime_id;
	unsigned thread_id;
	unsigned frame_index;
	const char *script;
	unsigned script_length;
	BOOL delete_strings;
};

class ES_EvalReplyMessagePart
	: public ES_DebugMessagePart
{
public:
	ES_EvalReplyMessagePart();

	virtual unsigned GetLength();
	virtual void GetItemData(unsigned index, const char *&data, unsigned &length);
	virtual void ParseL(const char *&data, unsigned &size, unsigned &length);

	unsigned tag;
	unsigned status;
};

class ES_ExamineMessagePart
	: public ES_DebugMessagePart
{
public:
	ES_ExamineMessagePart();

	virtual unsigned GetLength();
	virtual void GetItemData(unsigned index, const char *&data, unsigned &length);
	virtual void ParseL(const char *&data, unsigned &size, unsigned &length);

	unsigned tag;
	unsigned runtime_id;
	BOOL in_scope;
	unsigned thread_id;
	unsigned frame_index;
	unsigned objects_count;
	unsigned *object_ids;
};

class ES_ExamineReplyMessagePart
	: public ES_DebugMessagePart
{
public:
	ES_ExamineReplyMessagePart();

	virtual ~ES_ExamineReplyMessagePart();
	virtual unsigned GetLength();
	virtual void GetItemData(unsigned index, const char *&data, unsigned &length);
	virtual void ParseL(const char *&data, unsigned &size, unsigned &length);

	void AddObjectL(unsigned id, unsigned properties_count);
	void SetPropertyL(const uni_char *name, const ES_DebugValue &value);

	class Object
	{
	public:
		unsigned object_id;
		unsigned properties_count;
		char **names;
		unsigned *name_lengths;
		ES_DebugValue *values;
	};

	unsigned tag;
	unsigned objects_count;
	Object **objects;
	BOOL delete_strings;
};

class ES_BacktraceMessagePart
	: public ES_DebugMessagePart
{
public:
	ES_BacktraceMessagePart();

	virtual unsigned GetLength();
	virtual void GetItemData(unsigned index, const char *&data, unsigned &length);
	virtual void ParseL(const char *&data, unsigned &size, unsigned &length);

	unsigned tag;
	unsigned runtime_id;
	unsigned thread_id;
	unsigned max_frames;
};

class ES_BacktraceReplyMessagePart
	: public ES_DebugMessagePart
{
public:
	ES_BacktraceReplyMessagePart();

	virtual ~ES_BacktraceReplyMessagePart();
	virtual unsigned GetLength();
	virtual void GetItemData(unsigned index, const char *&data, unsigned &length);
	virtual void ParseL(const char *&data, unsigned &size, unsigned &length);

	unsigned tag;
	unsigned stack_length;
	ES_DebugStackFrame *stack;
};

class ES_ObjectInfoMessagePart
	: public ES_DebugMessagePart
{
public:
	ES_ObjectInfoMessagePart();

	virtual ~ES_ObjectInfoMessagePart();
	virtual unsigned GetLength();
	virtual void GetItemData(unsigned index, const char *&data, unsigned &length);
	virtual void ParseL(const char *&data, unsigned &size, unsigned &length);

	unsigned object_id;
	unsigned prototype_id;
	BOOL is_callable;
	BOOL is_function;
	char *name; ///< function name if is_function==TRUE, class name if is_function==FALSE
	unsigned name_length;
	BOOL delete_strings;
};

class ES_WatchUpdateMessagePart
	: public ES_DebugMessagePart
{
public:
	ES_WatchUpdateMessagePart();

	virtual ~ES_WatchUpdateMessagePart();
	virtual unsigned GetLength();
	virtual void GetItemData(unsigned index, const char *&data, unsigned &length);
	virtual void ParseL(const char *&data, unsigned &size, unsigned &length);

	class Changed
	{
	public:
		char *property_name;
		unsigned property_name_length;
		ES_DebugValue property_value;
	};

	unsigned object_id;
	unsigned changed_count;
	Changed *changed;
	BOOL delete_strings;
};

class ES_StackMessagePart
	: public ES_DebugMessagePart
{
public:
	ES_StackMessagePart();

	virtual ~ES_StackMessagePart();
	virtual unsigned GetLength();
	virtual void GetItemData(unsigned index, const char *&data, unsigned &length);
	virtual void ParseL(const char *&data, unsigned &size, unsigned &length);

	class Frame
	{
	public:
		unsigned script_id;
		unsigned linenr;
		unsigned function_object_id;
		unsigned arguments_object_id;
		unsigned variables_object_id;
	};

	unsigned frame_count;
	Frame *frame;
};

class ES_BreakpointTriggeredMessagePart
	: public ES_DebugMessagePart
{
public:
	ES_BreakpointTriggeredMessagePart();

	virtual unsigned GetLength();
	virtual void GetItemData(unsigned index, const char *&data, unsigned &length);
	virtual void ParseL(const char *&data, unsigned &size, unsigned &length);

	unsigned id;
};

class ES_ReturnValueMessagePart
	: public ES_DebugMessagePart
{
public:
	ES_ReturnValueMessagePart();

	virtual ~ES_ReturnValueMessagePart();
	virtual unsigned GetLength();
	virtual void GetItemData(unsigned index, const char *&data, unsigned &length);
	virtual void ParseL(const char *&data, unsigned &size, unsigned &length);

	unsigned function_object_id;
	ES_DebugValue value;
};

class ES_ExceptionThrownMessagePart
	: public ES_DebugMessagePart
{
public:
	ES_ExceptionThrownMessagePart();

	virtual ~ES_ExceptionThrownMessagePart();
	virtual unsigned GetLength();
	virtual void GetItemData(unsigned index, const char *&data, unsigned &length);
	virtual void ParseL(const char *&data, unsigned &size, unsigned &length);

	ES_DebugValue exception;
};

class ES_EvalVariableMessagePart
	: public ES_DebugMessagePart
{
public:
	ES_EvalVariableMessagePart();

	virtual ~ES_EvalVariableMessagePart();
	virtual unsigned GetLength();
	virtual void GetItemData(unsigned index, const char *&data, unsigned &length);
	virtual void ParseL(const char *&data, unsigned &size, unsigned &length);

	const char *name;
	unsigned name_length;
	ES_DebugValue value;
	BOOL delete_strings;
};

class ES_ChangeBreakpointMessagePart
	: public ES_DebugMessagePart
{
public:
	ES_ChangeBreakpointMessagePart(BOOL add);

	virtual ~ES_ChangeBreakpointMessagePart();
	virtual unsigned GetLength();
	virtual void GetItemData(unsigned index, const char *&data, unsigned &length);
	virtual void ParseL(const char *&data, unsigned &size, unsigned &length);

	BOOL add;
	unsigned id;
	ES_DebugBreakpointData bpdata;
	OpString namespaceuri;
	OpString eventtype;
	BOOL delete_strings;
};

class ES_RuntimeInformationMessagePart
	: public ES_DebugMessagePart
{
public:
	ES_RuntimeInformationMessagePart();

	virtual ~ES_RuntimeInformationMessagePart();
	virtual unsigned GetLength();
	virtual void GetItemData(unsigned index, const char *&data, unsigned &length);
	virtual void ParseL(const char *&data, unsigned &size, unsigned &length);

	unsigned dbg_runtime_id;
	unsigned dbg_window_id;
	char *framepath;
	unsigned framepath_length;
	char *documenturi;
	unsigned documenturi_length;
	BOOL delete_strings;
};

class ES_HeapStatisticsMessagePart
	: public ES_DebugMessagePart
{
public:
	ES_HeapStatisticsMessagePart();

	virtual unsigned GetLength();
	virtual void GetItemData(unsigned index, const char *&data, unsigned &length);
	virtual void ParseL(const char *&data, unsigned &size, unsigned &length);

	double load_factor;
	unsigned unsigneds[11];
};

class ES_BreakMessagePart
	: public ES_DebugMessagePart
{
public:
	ES_BreakMessagePart();

	virtual unsigned GetLength();
	virtual void GetItemData(unsigned index, const char *&data, unsigned &length);
	virtual void ParseL(const char *&data, unsigned &size, unsigned &length);

	unsigned runtime_id;
	unsigned thread_id;
};

#endif /* ECMASCRIPT_REMOTE_DEBUGGER */
#endif /* ES_UTILS_ESREMOTEMESSAGEPARTS_H */
