/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  1995-2010
 */
#include "core/pch.h"

#ifndef _STANDALONE
#include "modules/doc/frm_doc.h"
#endif // _STANDALONE
#include "modules/ecmascript/ecmascript.h"
#include "modules/ecmascript/carakan/ecmascript_parameters.h"
#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/compiler/es_lexer.h"
#include "modules/ecmascript/carakan/src/compiler/es_parser.h"
#include "modules/ecmascript/carakan/src/compiler/es_native.h"
#include "modules/ecmascript/carakan/src/ecmascript_manager_impl.h"
#include "modules/ecmascript/carakan/src/ecma_pi.h"
#include "modules/ecmascript/carakan/src/es_program_cache.h"
#include "modules/ecmascript/carakan/src/es_currenturl.h"
#include "modules/ecmascript/carakan/src/builtins/es_json_builtins.h"
#include "modules/ecmascript/carakan/src/builtins/es_error_builtins.h"
#include "modules/ecmascript/carakan/src/object/es_clone_object.h"
#include "modules/ecmascript/carakan/src/object/es_typedarray.h"
#include "modules/ecmascript/carakan/src/util/es_formatter.h"
#include "modules/ecmascript/structured/es_persistent.h"
#include "modules/util/excepts.h"
#include "modules/util/opstring.h"
#include "modules/locale/locale-enum.h"

#ifndef _STANDALONE
# include "modules/prefs/prefsmanager/collections/pc_js.h"
#endif // _STANDALONE

#ifdef OPERA_CONSOLE
#include "modules/ecmascript_utils/essched.h"  // for info about failing thread
#include "modules/locale/oplanguagemanager.h"
#endif

#ifdef SCOPE_PROFILER
# include "modules/probetools/probetimeline.h"
#endif // SCOPE_PROFILER

class URL;

class ES_ErrorData
{
public:
#ifdef OPERA_CONSOLE
	OpConsoleEngine::Message message;
#endif // OPERA_CONSOLE
};

#ifdef SCOPE_PROFILER
/**
 * Custom probe for PROBE_EVENT_SCRIPT_COMPILATION events. We reduce noise
 * in the target function by splitting the probe activation code into a
 * separate class.
 */
class ES_CompilationProbe
	: public OpPropertyProbe
{
public:

	/**
	 * Activate the ES_CompilationProbe.
	 *
	 * @param timeline The timeline to report to. Can not be NULL.
	 * @param url The URL that contains the source for the script. Can be NULL.
	 * @param script_type The type for this script.
	 *
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS
	Activate(OpProbeTimeline *timeline, URL *url, ES_ScriptType script_type)
	{
		OpPropertyProbe::Activator<2> act(*this);
		const OpProbeEventType type = PROBE_EVENT_SCRIPT_COMPILATION;

		if (url)
			RETURN_IF_ERROR(act.Activate(timeline, type, url));
		else
		{
			int key = static_cast<int>(script_type);
			RETURN_IF_ERROR(act.Activate(timeline, type, key));
		}

		act.AddUnsigned("type", static_cast<unsigned>(script_type));

		if (url)
			RETURN_IF_ERROR(act.AddURL("url", url));

		return OpStatus::OK;
	}
};
#endif // SCOPE_PROFILER

ES_RuntimeReaper::ES_RuntimeReaper(ES_Runtime *rt)
	: runtime(rt)
{
}

OP_STATUS
ES_RuntimeReaper::Initialize(ES_Context *context)
{
	RETURN_IF_ERROR(SetObjectRuntime(runtime, runtime->GetObjectPrototype(), "reaper", TRUE));
	return runtime->Protect(*this) ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

ES_RuntimeReaper::~ES_RuntimeReaper()
{
	if (runtime)
		runtime->runtime_reaper = NULL;
}

/* virtual */ void
ES_RuntimeReaper::GCTrace()
{
	ES_Object *global_object = runtime->global_object;
	runtime->GCMark(global_object);
	runtime->global_object = static_cast<ES_Global_Object *>(global_object);
	runtime->GCTrace();
}

ES_Runtime::ES_Runtime()
  : next_runtime_per_heap(NULL)
  , runtime_reaper(NULL)
  , global_object(NULL)
  , rt_data(NULL)
  , heap(NULL)
  , frames_doc(NULL)
  , keepalive_counter(ES_PRIVATE_keepalive)
  , initializing(TRUE)
  , enabled(FALSE)
  , error_messages(TRUE)
  , allows_suspend(TRUE)
  , debug_privileges(FALSE)
  , next_inline_script_no(0)
  , next_linked_script_no(0)
  , next_generated_script_no(0)
#ifdef _DEBUG
  , debug_next(NULL)
#endif
  , scheduler(NULL)
  , async_interface(NULL)
  , tostring_context(NULL)
  , error_handler(NULL)
{
}

OP_STATUS
ES_Runtime::Construct(EcmaScript_Object* host_global_object/* = NULL*/, const char *classname/* = NULL*/, BOOL use_native_dispatcher0/* = FALSE*/, ES_Runtime *parent_runtime/* = NULL*/, BOOL create_host_global_object/* = TRUE*/, BOOL create_host_global_object_prototype/* = FALSE*/)
{
	use_native_dispatcher = use_native_dispatcher0;

#ifndef _STANDALONE
#ifdef ES_NATIVE_SUPPORT
#ifdef ARCHITECTURE_IA32
	if (use_native_dispatcher && !ES_CodeGenerator::SupportsSSE2())
#ifdef ES_OVERRIDE_FPMODE
		if (g_pcjs->GetStringPref(PrefsCollectionJS::FPMode).CompareI("sse2") == 0)
#endif // ES_OVERRIDE_FPMODE
			use_native_dispatcher = FALSE;
#endif // ARCHITECTURE_IA32
#endif // ES_NATIVE_SUPPORT
#endif // _STANDALONE

	if (create_host_global_object && (host_global_object == NULL) != (classname == NULL))
		return OpStatus::ERR;

	rt_data = g_esrt;

	OP_STATUS r;

	if (parent_runtime && parent_runtime->heap->IsSuitableForReuse())
	{
		heap = parent_runtime->heap;
		ES_Allocation_Context parent_context(parent_runtime);

		/**
		 * Doing a garbage collect here seems to give better
		 * performance and reduces heap fragmentation, so less memory
		 * is needed. This could possibly be left out in favor of
		 * letting the es heap do a garbage collect when needed at
		 * allocation time.  The tests I have done indicates that it
		 * is favorable to do the gc here. I guess an alternative
		 * could be to tweak the conditions for the allocation time gc
		 * to take imminent merge into account.
		 */
		heap->ForceCollect(&parent_context, GC_REASON_MERGE);

		/**
		 * This calls serves two purposes. It keeps the heap from
		 * doing additional gc:s until the runtime has been created
		 * (until SuitableForReuse(FALSE) has been called) and it
		 * stores the live_bytes so that we can later compensate for
		 * the additional memory that creating the environment adds to
		 * the heap.
		 */
		heap->BeforeRuntimeCreation();
	}
	else
	{
		TRAP(r, heap = ES_Heap::MakeL(RT_DATA.heap->GetMarkStack(), this, NULL, parent_runtime ? parent_runtime->GetHeap() : NULL));
		if (OpStatus::IsError(r))
		{
			heap = NULL;
			return OpStatus::ERR_NO_MEMORY;
		}
	}


	{
		GCLOCK(context, (this));

		TRAP(r, global_object = ES_Global_Object::Make(&context, classname ? classname : "Global", create_host_global_object_prototype));
		if (OpStatus::IsError(r))
			global_object = NULL;

		if (!global_object)
			return OpStatus::ERR_NO_MEMORY;

		runtime_reaper = OP_NEW(ES_RuntimeReaper, (this));

		if (runtime_reaper == NULL)
		{
			global_object = NULL;
			return OpStatus::ERR_NO_MEMORY;
		}

		if (OpStatus::IsMemoryError(runtime_reaper->Initialize(&context)))
		{
			OP_DELETE(runtime_reaper);
			runtime_reaper = NULL;
			global_object = NULL;
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	if (host_global_object == NULL && create_host_global_object)
	{
		host_global_object = OP_NEW(EcmaScript_Object, ());
		if (host_global_object == NULL)
		{
			/* Prevent the runtime reaper from accessing the runtime when it is GC:ed. */
			runtime_reaper->runtime = NULL;

			global_object = NULL;
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	OP_ASSERT(create_host_global_object || !host_global_object);
	if (create_host_global_object)
		/* Cannot fail, hence acceptable to ignore result. */
		OpStatus::Ignore(SetHostGlobalObject(host_global_object, NULL));

	enabled = TRUE;

	heap->AddRuntime(this);

#ifdef ECMASCRIPT_DEBUGGER
	if (g_ecmaManager->GetDebugListener())
		g_ecmaManager->GetDebugListener()->RuntimeCreated(this);
#endif

	return OpStatus::OK;
}

static void
UpdateHostGlobalPrototype(ES_Context *context, ES_Object *global_proto, ES_Class *klass)
{
	if (!global_proto->IsPrototypeObject())
		global_proto->ConvertToPrototypeObject(context);
	global_proto->AddInstance(context, klass, TRUE);
}

OP_STATUS
ES_Runtime::SetHostGlobalObject(EcmaScript_Object *host_global_object, EcmaScript_Object *host_global_object_prototype)
{
	global_object->SetIsHostObject();
	global_object->SetHostObject(host_global_object);

	host_global_object->runtime = this;
	host_global_object->native_object = global_object;

	GCLOCK(context, (this));
	if (host_global_object_prototype)
	{
		ES_Host_Object *global_proto = static_cast<ES_Host_Object *>(global_object->GetGlobalObjectPrototype());
		global_proto->SetHostObject(host_global_object_prototype);

		host_global_object_prototype->runtime = this;
		host_global_object_prototype->native_object = global_proto;
		TRAPD(status, UpdateHostGlobalPrototype(&context, global_proto, global_object->Class()));
		return status;
	}
	return OpStatus::OK;
}

OP_STATUS
ES_Runtime::SetPrototype(EcmaScript_Object* object, ES_Object* prototype)
{
	GCLOCK(context, (this));
	TRAPD(status, object->GetNativeObject()->ChangePrototype(&context, prototype));
	if (OpStatus::IsError(status))
		return status;
	object->GetNativeObject()->Invalidate();

	return OpStatus::OK;
}

void
ES_Runtime::InitializationFinished()
{
	initializing = FALSE;

	heap->InitializeAllocationLimit();
}

ES_Runtime::~ES_Runtime()
{
}

#ifndef _STANDALONE
ES_ThreadScheduler*
ES_Runtime::GetESScheduler()
{
	return frames_doc ? frames_doc->GetESScheduler() : scheduler;
}

ES_AsyncInterface*
ES_Runtime::GetESAsyncInterface()
{
	return frames_doc ? frames_doc->GetESAsyncInterface() : async_interface;
}
#endif // _STANDALONE


/* static */ BOOL
ES_Runtime::HasPrivilegeLevelAtLeast(const ES_Context *context, PrivilegeLevel privilege_level)
{
	ES_Execution_Context *exec_context = static_cast<ES_Execution_Context *>(const_cast<ES_Context *>(context));
	ES_FrameStackIterator frames(exec_context);

	while (frames.Next())
		if (frames.GetCode() && frames.GetCode()->data->privilege_level < privilege_level)
			return FALSE;

	return TRUE;
}

ES_Runtime::CompileOptions::CompileOptions()
	: privilege_level(ES_Runtime::PRIV_LVL_UNSPEC),
	  prevent_debugging(FALSE),
	  report_error(TRUE),
	  allow_cross_origin_error_reporting(FALSE),
	  reformat_source(FALSE),
	  script_url(NULL),
	  script_url_string(NULL),
	  script_type(SCRIPT_TYPE_UNKNOWN),
	  when(NULL),
	  context(UNI_L("Script compilation")),
	  error_callback(NULL),
	  error_callback_user_data(NULL)
{
}

ES_Runtime::CompileProgramOptions::CompileProgramOptions()
	: program_is_function(FALSE),
	  generate_result(FALSE),
	  global_scope(TRUE),
	  is_external(FALSE),
	  allow_top_level_function_expr(FALSE),
	  is_eval(FALSE),
	  start_line(1),
	  start_line_position(0)
{
}

void
ES_Runtime::ReportCompilationErrorL(ES_Context *context, ES_Parser &parser, const CompileOptions &options, ES_ParseErrorInfo *info)
{
	if (options.report_error || options.error_callback || parser.GetIsDebugging())
	{
		JString *parser_message;
		unsigned index, line;
		if (!info)
		{
			ES_ParseErrorInfo parse_error;
			parser.GetError(parse_error);
			parser_message = parse_error.message;
			index = parse_error.index;
			line = parse_error.line;
		}
		else
		{
			parser_message = info->message;
			index = info->index;
			line = info->line;
		}

		ES_SourcePosition error_position = parser.GetErrorPosition(index, line);

		OpStackAutoPtr<ES_ErrorData> error_data;
#ifdef OPERA_CONSOLE
		OpConsoleEngine::Message *console_message, console_message_local;
		ANCHOR(OpConsoleEngine::Message, console_message_local);

		if (options.report_error && error_handler)
		{
			error_data.reset(OP_NEW_L(ES_ErrorData, ()));
			console_message = &error_data->message;
		}
		else
			console_message = &console_message_local;

		console_message->source = OpConsoleEngine::EcmaScript;
		console_message->severity = OpConsoleEngine::Error;
		console_message->window = frames_doc ? frames_doc->GetWindow()->Id() : 0;
		console_message->context.SetL(options.context);

		if (frames_doc)
			console_message->window = frames_doc->GetWindow()->Id();

		OpString &message = console_message->message;
		OpString &url = console_message->url;
#else // OPERA_CONSOLE
		OpString message;
		OpString url;
#endif // OPERA_CONSOLE

		LEAVE_IF_ERROR(message.AppendFormat(UNI_L("Syntax error at line %u%s%s: %s"), error_position.GetAbsoluteLine(), options.when ? UNI_L(" ") : UNI_L(""), options.when ? options.when : UNI_L(""), StorageZ(context, parser_message)));

		OpString oneline; ANCHOR(OpString, oneline);

		if (options.report_error && error_handler || options.error_callback)
			oneline.SetL(message);

#ifdef OPERA_CONSOLE
		if (options.script_url)
			options.script_url->GetAttributeL(URL::KUniName_Username_Password_Hidden, url);
		else if (options.script_url_string)
			url.SetL(options.script_url_string);
#endif // OPERA_CONSOLE

		message.AppendL("\n");

		parser.GenerateErrorSourceViewL(message);

		if (options.error_callback)
			LEAVE_IF_ERROR(options.error_callback(oneline.CStr(), message.CStr(), error_position, options.error_callback_user_data));

#ifdef ECMASCRIPT_DEBUGGER
		if (parser.GetIsDebugging())
		{
			OP_ASSERT(g_ecmaManager->GetDebugListener());

			ES_ErrorInfo err(options.context);

			uni_strlcpy(err.error_text, message.CStr(), ARRAY_SIZE(err.error_text));

			err.error_length = uni_strlen(err.error_text);
			err.error_position = error_position;
			err.script_guid = parser.GetScriptGuid();

			g_ecmaManager->GetDebugListener()->ParseError(this, &err);
		}
#endif // ECMASCRIPT_DEBUGGER

#ifndef _STANDALONE
		if (options.report_error)
			if (error_handler)
			{
				const uni_char *msg = oneline.CStr();
				const uni_char *url_string = url.CStr();

				if (url_string && !*url_string)
					url_string = NULL;

				ErrorHandler::CompilationErrorInformation error_information;
				error_information.script_type = options.script_type;
				error_information.cross_origin_allowed = options.allow_cross_origin_error_reporting;

				OP_STATUS status = error_handler->HandleError(this, &error_information, msg, url_string, error_position, error_data.get());

				if (OpStatus::IsSuccess(status))
				{
					error_data.release();
					return;
				}
				else
					LEAVE(status);
			}
#ifdef OPERA_CONSOLE
			else
				g_console->PostMessageL(console_message);
#endif // OPERA_CONSOLE
#endif // _STANDALONE
	}
}

OP_STATUS
ES_Runtime::ReportScriptError(const uni_char *message, URL *script_url, unsigned document_line_number, BOOL allow_cross_origin_errors)
{
	GCLOCK(context, (this));

	if (error_handler)
	{
		ES_ErrorData *error_data = OP_NEW(ES_ErrorData, ());
		if (!error_data)
			return OpStatus::ERR_NO_MEMORY;

		OpAutoPtr<ES_ErrorData> anchor_error_data(error_data);

		OpString url;
#ifdef OPERA_CONSOLE
		RETURN_IF_ERROR(script_url->GetAttribute(URL::KUniName_Username_Password_Hidden, url));
#endif // OPERA_CONSOLE
		const uni_char *url_string = url.CStr();

		if (url_string && !*url_string)
			url_string = NULL;

#ifdef OPERA_CONSOLE
		RETURN_IF_ERROR(error_data->message.message.Set(message));
		RETURN_IF_ERROR(error_data->message.url.Set(url));
		error_data->message.source = OpConsoleEngine::EcmaScript;
		error_data->message.severity = OpConsoleEngine::Error;
		error_data->message.window = frames_doc ? frames_doc->GetWindow()->Id() : 0;
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_ES_LOADING_LINKED_SCRIPT_FAILED, error_data->message.context));
#endif // OPERA_CONSOLE

		ErrorHandler::LoadErrorInformation error_information;
		error_information.cross_origin_allowed = allow_cross_origin_errors;
		ES_SourcePosition position(0, 0, 0, document_line_number, 0);

		OP_STATUS status = error_handler->HandleError(this, &error_information, message, url_string, position, error_data);
		if (OpStatus::IsSuccess(status))
			anchor_error_data.release();

		return status;
	}
	else
		return OpStatus::OK;
}

OP_STATUS
ES_Runtime::CompileProgram(ES_ProgramText* program_array,
						   int elements,
						   ES_Program **return_program,
						   const CompileProgramOptions& options)
{
#ifdef SCOPE_PROFILER
	ES_CompilationProbe probe;

	if (OpProbeTimeline *t = GetFramesDocument() ? GetFramesDocument()->GetTimeline() : NULL)
		RETURN_IF_ERROR(probe.Activate(t, options.script_url, options.script_type));
#endif // SCOPE_PROFILER|

	BOOL is_debugging = !options.prevent_debugging && g_ecmaManager->IsDebugging(this);
	bool is_eval = !!options.is_eval;

	*return_program = NULL;
	GCLOCK(context, (this));

#ifdef ES_NATIVE_SUPPORT
	context.use_native_dispatcher = !is_debugging && use_native_dispatcher;
#endif // ES_NATIVE_SUPPORT

	OpString url;

	if (options.script_url)
#ifdef _STANDALONE
		RETURN_IF_ERROR(url.Set(options.script_url->string));
#else // _STANDALONE
		RETURN_IF_ERROR(options.script_url->GetAttribute(URL::KUniName, url));
#endif // _STANDALONE
	else if (options.script_url_string)
		RETURN_IF_ERROR(url.Set(options.script_url_string));

	PrivilegeLevel privilege_level = options.privilege_level;

	if (GetGlobalObject())
	{
		if (!is_debugging && privilege_level <= PRIV_LVL_UNTRUSTED && options.global_scope)
		{
			if (ES_ProgramCodeStatic *data = rt_data->program_cache->Find(program_array, elements))
			{
				*return_program = NULL;

				ES_ProgramCode *program_code;

				TRAPD(status, program_code = ES_ProgramCode::Make(&context, global_object, data, TRUE, url.HasContent() ? JString::Make(&context, url.CStr()) : NULL));

				if (OpStatus::IsSuccess(status))
				{
					*return_program = OP_NEW(ES_Program, (heap, program_code));

					return *return_program ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
				}
				else
					return status;
			}
		}

		const uni_char **fragments = OP_NEWA(const uni_char*, elements);
		if (!fragments)
			return OpStatus::ERR_NO_MEMORY;

		ANCHOR_ARRAY(const uni_char *, fragments);

		unsigned *fragment_lengths = OP_NEWA(unsigned, elements);
		if (!fragment_lengths)
			return OpStatus::ERR_NO_MEMORY;

		ANCHOR_ARRAY(unsigned, fragment_lengths);

		for (int i = 0; i < elements; i++)
		{
			fragments[i] = program_array[i].program_text;
			fragment_lengths[i] = program_array[i].program_text_length;
		}
		ES_Fragments program_fragments(fragments, fragment_lengths);
		program_fragments.fragments_count = elements;

		OP_STATUS status;

		if (options.reformat_source)
		{
			BOOL success = FALSE;
			uni_char *program_text_out;
			unsigned program_text_length_out;

			ES_Lexer lexer(&context, NULL, &program_fragments, NULL, options.start_line, options.start_line_position);
			ES_Formatter formatter(&lexer, &context, is_eval);

			formatter.SetAllowReturnInProgram(!!options.program_is_function);

			TRAP(status, success = formatter.FormatProgram(program_text_out, program_text_length_out, options.allow_top_level_function_expr));
			RETURN_IF_MEMORY_ERROR(status);

			if (success)
			{
				// Create the fragment with formatted string and substitute an original.
				OP_DELETEA(fragments);
				OP_DELETEA(fragment_lengths);

				fragments = OP_NEWA(const uni_char*, 1);
				if (!fragments)
					return OpStatus::ERR_NO_MEMORY;

				ANCHOR_ARRAY_RESET(fragments);

				fragment_lengths = OP_NEWA(unsigned, 1);
				if (!fragment_lengths)
					return OpStatus::ERR_NO_MEMORY;

				ANCHOR_ARRAY_RESET(fragment_lengths);

				fragments[0] = program_text_out;
				fragment_lengths[0] = program_text_length_out;
				program_fragments.fragments = fragments;
				program_fragments.fragment_lengths = fragment_lengths;
				program_fragments.fragments_count = 1;
			}
		}

		ES_Lexer lexer(&context, NULL, &program_fragments, NULL, options.start_line, options.start_line_position);
		ES_Parser parser(&lexer, this, &context, global_object, is_eval);

		parser.SetGlobalScope(options.global_scope);
		parser.SetGenerateResult(options.generate_result);
		parser.SetAllowReturnInProgram(options.program_is_function);
#ifdef ECMASCRIPT_DEBUGGER
		parser.SetIsDebugging(is_debugging);
		parser.SetScriptType(options.script_type);
#endif // ECMASCRIPT_DEBUGGER

		if (url.HasContent())
		{
			TRAP(status, parser.SetURL(JString::Make(&context, url.CStr())));
			if (OpStatus::IsMemoryError(status))
				return status;
		}

		ES_ProgramCode *program_code;
		BOOL res = FALSE;
		TRAP(status, res = parser.ParseProgram(program_code, !!options.allow_top_level_function_expr));
		if (OpStatus::IsMemoryError(status))
			return status;

#ifdef ECMASCRIPT_DEBUGGER
		parser.DebuggerSignalNewScript();
#endif // ECMASCRIPT_DEBUGGER

		if (!res || OpStatus::IsError(status))
		{
			TRAP(status, ReportCompilationErrorL(&context, parser, options));
			if (OpStatus::IsMemoryError(status))
				return status;

			*return_program = NULL;
			return OpStatus::ERR;		// Syntax or other compile error
		}

		if (!is_debugging && privilege_level <= PRIV_LVL_UNTRUSTED && options.global_scope)
		{
			TRAPD(sharing_status, program_code->PrepareStaticForSharing(&context));
			if (OpStatus::IsSuccess(sharing_status))
				rt_data->program_cache->AddProgram(program_code->GetData());
			// We continue as usual even if PrepareStaticForSharing() failed.
		}

		program_code->SetIsExternal(options.is_external);
		program_code->SetPrivilegeLevel(privilege_level);

		*return_program = OP_NEW(ES_Program, (heap, program_code));
		return *return_program ? sanitize(status) : OpStatus::ERR_NO_MEMORY;
	}
	else
		*return_program = NULL;
	return OpStatus::OK;
}

OP_STATUS
ES_Runtime::ExtractStaticProgram(ES_Static_Program *&static_program, ES_Program *program)
{
	static_program = NULL;

	GCLOCK(context, (this));

	TRAPD(status, program->program->PrepareStaticForSharing(&context));
	RETURN_IF_ERROR(status);

	static_program = OP_NEW(ES_Static_Program, (program->program->GetData()));

	return static_program ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

OP_STATUS
ES_Runtime::CreateProgramFromStatic(ES_Program *&program, ES_Static_Program *static_program)
{
	program = NULL;

	GCLOCK(context, (this));

	ES_ProgramCode *program_code;

	TRAPD(status, program_code = ES_ProgramCode::Make(&context, global_object, static_program->program, TRUE));

	if (OpStatus::IsSuccess(status))
	{
		program = OP_NEW(ES_Program, (heap, program_code));

		return program ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
	}
	else
		return status;
}

/* static */ void
ES_Runtime::DeleteStaticProgram(ES_Static_Program *static_program)
{
	OP_DELETE(static_program);
}

/* static */ void
ES_Runtime::DeleteProgram(ES_Program* program)
{
	OP_DELETE(program);
}

ES_Runtime::CreateFunctionOptions::CreateFunctionOptions()
	: generate_result(FALSE),
	  is_external(FALSE)
{
}

OP_STATUS
ES_Runtime::CreateFunction(ES_Object** scope_chain,
						   int scope_chain_length,
						   const uni_char* function_text,
						   unsigned function_text_length,
						   ES_Object **return_object,
						   unsigned argc,
						   const uni_char** argn,
						   const CreateFunctionOptions& options)
{
	GCLOCK(context, (this));

	*return_object = NULL;

	OP_STATUS status;
	BOOL success = FALSE;

	if (options.reformat_source)
	{
		ES_Formatter formatter(NULL, &context, false);

		uni_char *program_text_out;
		unsigned program_text_out_length;

		TRAP(status, success = formatter.FormatFunction(program_text_out, program_text_out_length, function_text, function_text_length));
		RETURN_IF_MEMORY_ERROR(status);

		if (success)
		{
			/* Caller deletes original string. */
			function_text = program_text_out;
			function_text_length = program_text_out_length;
		}
	}

	ES_Parser parser(NULL, this, &context, global_object);
	ES_FunctionCode *code;

	if (scope_chain_length != 0)
		parser.SetGlobalScope(FALSE);

	unsigned *formals_length = OP_NEWA(unsigned, argc);
	OpAutoArray<unsigned> formals_length_anchor(formals_length);

	if (formals_length == NULL)
		return OpStatus::ERR_NO_MEMORY;

	for (unsigned i = 0; i < argc; ++i)
		formals_length[i] = uni_strlen(argn[i]);

#ifdef ECMASCRIPT_DEBUGGER
	parser.SetIsDebugging(!options.prevent_debugging && g_ecmaManager->IsDebugging(this));
	parser.SetScriptType(options.script_type);
#endif // ECMASCRIPT_DEBUGGER

	OpString url;

	if (options.script_url)
#ifdef _STANDALONE
		RETURN_IF_ERROR(url.Set(options.script_url->string));
#else // _STANDALONE
		RETURN_IF_ERROR(options.script_url->GetAttribute(URL::KUniName, url));
#endif // _STANDALONE
	else if (options.script_url_string)
		RETURN_IF_ERROR(url.Set(options.script_url_string));

	if (url.HasContent())
	{
		TRAP(status, parser.SetURL(JString::Make(&context, url.CStr())));
		RETURN_IF_MEMORY_ERROR(status);
	}

	ES_ParseErrorInfo parse_error;

	TRAP(status, success = parser.ParseFunction(code, argn, argc, formals_length, function_text, function_text_length, &parse_error));
	RETURN_IF_MEMORY_ERROR(status);

#ifdef ECMASCRIPT_DEBUGGER
	parser.DebuggerSignalNewScript(function_text);
#endif // ECMASCRIPT_DEBUGGER

	if (!success || OpStatus::IsError(status))
	{
		TRAP(status, ReportCompilationErrorL(&context, parser, options, &parse_error));
		RETURN_IF_MEMORY_ERROR(status);

		return OpStatus::ERR;
	}
	else
	{
		code->SetIsExternal(options.is_external);

		if (scope_chain_length != 0)
			RETURN_IF_MEMORY_ERROR(code->SetScopeChain(scope_chain, scope_chain_length));

		ES_Function *fn = NULL;

		TRAP(status, fn = ES_Function::Make(&context, global_object, code, 0));
		RETURN_IF_MEMORY_ERROR(status);

		*return_object = fn;
		return OpStatus::OK;
	}
}

OP_STATUS
ES_Runtime::CreateNativeObject(ES_Value value, ES_Prototype tag, ES_Object** obj)
{
	GCLOCK(context, (this));

	if (!global_object)
	{
		*obj = NULL;
		return OpStatus::ERR;
	}

    ES_Value_Internal v;
    OP_STATUS r = OpStatus::ERR;

    TRAP(r, v.ImportL(&context, value));
    if (OpStatus::IsSuccess(r))
    {
        switch (tag)
        {
        case ENGINE_NUMBER_PROTOTYPE :
            if (v.IsNumber())
			{
				TRAP(r, *obj = ES_Number_Object::Make(&context, global_object, v.GetNumAsDouble()));
			}
            break;

        case ENGINE_STRING_PROTOTYPE:
            if (v.IsString())
			{
				TRAP(r, *obj = ES_String_Object::Make(&context, global_object, v.GetString()));
			}
            break;

        case ENGINE_BOOLEAN_PROTOTYPE:
            if (v.IsBoolean())
			{
			    TRAP(r, *obj = ES_Boolean_Object::Make(&context, global_object, v.GetBoolean()));
			}
            break;

        case ENGINE_DATE_PROTOTYPE :
            if (v.IsNumber())
			{
			    TRAP(r, *obj = ES_Date_Object::Make(&context, global_object, v.GetNumAsDouble()));
			}
            break;

		default:
			OP_ASSERT(!"Illegal argument to ES_CreateObject");
			r = OpStatus::ERR;
        }
    }

    return r;
}

OP_STATUS
ES_Runtime::CreateNativeObject(ES_Object **obj, ES_Object *prototype, const char *class_name, unsigned flags)
{
    OP_ASSERT(!prototype || prototype->IsPrototypeObject());

	GCLOCK(context, (this));

	BOOL singleton = (flags & FLAG_SINGLETON) != 0;

	TRAPD(r,
        ES_Class *klass;
        if (singleton)
            klass = ES_Class::MakeSingleton(&context, prototype, class_name);
        else
            klass = ES_Class::MakeRoot(&context, prototype, class_name, singleton);
        *obj = ES_Object::Make(&context, klass));

	return r;
}

OP_STATUS
ES_Runtime::GetNativeValueOf(ES_Object *obj, ES_Value *return_value)
{
	if (!obj || !return_value)
		return OpStatus::ERR;

	switch (obj->GCTag())
	{
	case GCTAG_ES_Object_Number:
		{
			ES_Number_Object *number = static_cast<ES_Number_Object *>(obj);

			return_value->type = VALUE_NUMBER;
			return_value->value.number = number->GetValue();

			return OpStatus::OK;
		}
	case GCTAG_ES_Object_String:
		{
			GCLOCK(context, (this));

			ES_String_Object *string = static_cast<ES_String_Object *>(obj);

			return_value->type = VALUE_STRING;
			return_value->value.string = StorageZ(&context, string->GetValue());

			unsigned length = Length(string->GetValue());
			return_value->string_length = length < 0x1000000u ? length : 0xffffffu;

			return OpStatus::OK;
		}
	case GCTAG_ES_Object_Boolean:
		{
			ES_Boolean_Object *flag = static_cast<ES_Boolean_Object *>(obj);

			return_value->type = VALUE_BOOLEAN;
			return_value->value.boolean = !!flag->GetValue();

			return OpStatus::OK;
		}
	case GCTAG_ES_Object_Date:
		{
			ES_Date_Object *date = static_cast<ES_Date_Object *>(obj);

			return_value->type = VALUE_NUMBER;
			return_value->value.number = date->GetValue();

			return OpStatus::OK;
		}
	default:
		return OpStatus::ERR;
	}
}

OP_STATUS
ES_Runtime::CreatePrototypeObject(ES_Object **obj, ES_Object *prototype, const char *class_name, unsigned size)
{
    OP_ASSERT(prototype->IsPrototypeObject());

	GCLOCK(context, (this));

	ES_Class *instance;
	TRAPD(r, *obj = ES_Object::MakePrototypeObject(&context, instance, prototype, class_name, size));

	return r;
}

OP_STATUS
ES_Runtime::CreateNativeObjectObject(ES_Object** obj)
{
    GCLOCK(context, (this));

    TRAPD(r, *obj = ES_Object::Make(&context, global_object->GetObjectClass()));
    return r;
}

OP_STATUS
ES_Runtime::CreateNativeArrayObject(ES_Object** obj, unsigned len)
{
    GCLOCK(context, (this));

    TRAPD(r, *obj = ES_Array::Make(&context, global_object, len, len));
    return r;
}

OP_STATUS
ES_Runtime::CreateNativeErrorObject(ES_Object **error, ES_NativeError type, const uni_char *message)
{
	if (!global_object)
	{
		*error = NULL;
		return OpStatus::ERR;
	}

	return CreateErrorObject(error, message, global_object->GetNativeErrorPrototype(type));
}

OP_STATUS
ES_Runtime::CreateErrorPrototype(ES_Object **prototype, ES_Object *prototype_prototype, const char *class_name)
{
	GCLOCK(context, (this));

	TRAPD(r, *prototype = ES_Error::MakeErrorPrototype(&context, global_object, prototype_prototype, class_name));
	return r;
}

OP_STATUS
ES_Runtime::CreateErrorObject(ES_Object **obj, const uni_char *message, ES_Object *prototype)
{
	if (!prototype)
		prototype = GetErrorPrototype();

	OP_ASSERT(prototype && prototype->IsPrototypeObject());

	GCLOCK(context, (this));

	ES_Global_Object *global_object = static_cast<ES_Global_Object *>(GetGlobalObject());
	ES_Class *sub_object_class = ES_Error::MakeErrorInstanceClass(&context, prototype->GetSubObjectClass());
	TRAPD(r,
		*obj = ES_Error::Make(&context, global_object, sub_object_class, FALSE);
		if (message)
			(*obj)->PutCachedAtIndex(ES_PropertyIndex(ES_Error::PROP_message), JString::Make(&context, message)));

	return r;
}

/* static */ unsigned char *
ES_Runtime::GetByteArrayStorage(ES_Object* obj)
{
	OP_ASSERT(obj->GetIndexedProperties() && ES_Indexed_Properties::GetType(obj->GetIndexedProperties()) == ES_Indexed_Properties::TYPE_BYTE_ARRAY);

	return static_cast<ES_Byte_Array_Indexed *>(obj->GetIndexedProperties())->Storage();
}

/* static */ unsigned
ES_Runtime::GetByteArrayLength(ES_Object* obj)
{
	OP_ASSERT(obj->GetIndexedProperties() && ES_Indexed_Properties::GetType(obj->GetIndexedProperties()) == ES_Indexed_Properties::TYPE_BYTE_ARRAY);

	return static_cast<ES_Byte_Array_Indexed *>(obj->GetIndexedProperties())->Capacity();
}

OP_STATUS
ES_Runtime::CreateNativeArrayBufferObject(ES_Object** obj, unsigned length, unsigned char *bytes)
{
    GCLOCK(context, (this));

    TRAPD(r, *obj = ES_ArrayBuffer::Make(&context, global_object, length, bytes, FALSE));
    return *obj ? r : OpStatus::ERR_NO_MEMORY;
}

/* static */ unsigned char *
ES_Runtime::GetArrayBufferStorage(ES_Object* obj)
{
	OP_ASSERT(obj->GCTag() == GCTAG_ES_Object_ArrayBuffer);
	return static_cast<ES_ArrayBuffer *>(obj)->GetStorage();
}

/* static */ unsigned
ES_Runtime::GetArrayBufferLength(ES_Object* obj)
{
	OP_ASSERT(obj->GCTag() == GCTAG_ES_Object_ArrayBuffer);
	return static_cast<ES_ArrayBuffer *>(obj)->GetSize();
}

OP_STATUS
ES_Runtime::CreateNativeTypedArrayObject(ES_Object** obj, NativeTypedArrayKind kind, ES_Object* data)
{
	OP_ASSERT(data->GCTag() == GCTAG_ES_Object_ArrayBuffer);
	GCLOCK(context, (this));

	ES_TypedArray::Kind ta_kind;
	switch (kind)
	{
	case TYPED_ARRAY_INT8:
		ta_kind = ES_TypedArray::Int8Array;
		break;
	case TYPED_ARRAY_UINT8:
		ta_kind = ES_TypedArray::Uint8Array;
		break;
	case TYPED_ARRAY_UINT8CLAMPED:
		ta_kind = ES_TypedArray::Uint8ClampedArray;
		break;
	case TYPED_ARRAY_INT16:
		ta_kind = ES_TypedArray::Int16Array;
		break;
	case TYPED_ARRAY_UINT16:
		ta_kind = ES_TypedArray::Uint16Array;
		break;
	case TYPED_ARRAY_INT32:
		ta_kind = ES_TypedArray::Int32Array;
		break;
	case TYPED_ARRAY_UINT32:
		ta_kind = ES_TypedArray::Uint32Array;
		break;
	case TYPED_ARRAY_FLOAT32:
		ta_kind = ES_TypedArray::Float32Array;
		break;
	case TYPED_ARRAY_FLOAT64:
		ta_kind = ES_TypedArray::Float64Array;
		break;
	case TYPED_ARRAY_ANY:
	default:
		OP_ASSERT(!"Unhandled NativeTypedArrayKind");
		return OpStatus::ERR;
	}

	TRAPD(r, *obj = ES_TypedArray::Make(&context, global_object, ta_kind, static_cast<ES_ArrayBuffer *>(data)));
    return *obj ? r : OpStatus::ERR_NO_MEMORY;
}


/* static */ void *
ES_Runtime::GetStaticTypedArrayStorage(ES_Object* object)
{
	OP_ASSERT(object->GCTag() == GCTAG_ES_Object_TypedArray);

	return static_cast<ES_TypedArray *>(object)->GetStorage();
}

/* static */ ES_Object *
ES_Runtime::GetStaticTypedArrayBuffer(ES_Object* object)
{
	OP_ASSERT(object->GCTag() == GCTAG_ES_Object_TypedArray);

	return static_cast<ES_TypedArray *>(object)->GetArrayBuffer();
}

/* static */ unsigned
ES_Runtime::GetStaticTypedArrayLength(ES_Object* obj)
{
	OP_ASSERT(obj->GetIndexedProperties() && ES_Indexed_Properties::GetType(obj->GetIndexedProperties()) == ES_Indexed_Properties::TYPE_TYPE_ARRAY);

	return static_cast<ES_Type_Array_Indexed *>(obj->GetIndexedProperties())->Capacity();
}

/* static */ unsigned
ES_Runtime::GetStaticTypedArraySize(ES_Object* obj)
{
	OP_ASSERT(obj->GCTag() == GCTAG_ES_Object_TypedArray);

	return static_cast<ES_TypedArray *>(obj)->GetSize();
}

/* static */ ES_Runtime::NativeTypedArrayKind
ES_Runtime::GetStaticTypedArrayKind(ES_Object* obj)
{
	OP_ASSERT(obj->GCTag() == GCTAG_ES_Object_TypedArray);

	ES_TypedArray::Kind kind = static_cast<ES_TypedArray *>(obj)->GetKind();
	switch (kind)
	{
	case ES_TypedArray::Int8Array:
		return TYPED_ARRAY_INT8;
	case ES_TypedArray::Int16Array:
		return TYPED_ARRAY_INT16;
	case ES_TypedArray::Int32Array:
		return TYPED_ARRAY_INT32;
	case ES_TypedArray::Uint8Array:
		return TYPED_ARRAY_UINT8;
	case ES_TypedArray::Uint8ClampedArray:
		return TYPED_ARRAY_UINT8CLAMPED;
	case ES_TypedArray::Uint16Array:
		return TYPED_ARRAY_UINT16;
	case ES_TypedArray::Uint32Array:
		return TYPED_ARRAY_UINT32;
	case ES_TypedArray::Float32Array:
		return TYPED_ARRAY_FLOAT32;
	case ES_TypedArray::Float64Array:
		return TYPED_ARRAY_FLOAT64;
	default:
		OP_ASSERT(!"The 'impossible' happened, unhandled kind.");
		/* fallthrough */
	case ES_TypedArray::DataViewArray:
		return TYPED_ARRAY_ANY;
	}
}

/* static */ BOOL
ES_Runtime::IsNativeArrayBufferObject(ES_Object* object)
{
	return object->GCTag() == GCTAG_ES_Object_ArrayBuffer;
}


/* static */ BOOL
ES_Runtime::IsCallable(ES_Object* object)
{
	ES_Value_Internal value(object);
	return value.IsCallable(NULL);
}

/* static */ BOOL
ES_Runtime::IsNativeTypedArrayObject(ES_Object* object, NativeTypedArrayKind kind)
{
	if (object->GCTag() != GCTAG_ES_Object_TypedArray)
		return FALSE;
	switch (kind)
	{
	case TYPED_ARRAY_INT8:
		return static_cast<ES_TypedArray *>(object)->GetKind() == ES_TypedArray::Int8Array;
	case TYPED_ARRAY_UINT8:
		return static_cast<ES_TypedArray *>(object)->GetKind() == ES_TypedArray::Uint8Array;
	case TYPED_ARRAY_UINT8CLAMPED:
		return static_cast<ES_TypedArray *>(object)->GetKind() == ES_TypedArray::Uint8ClampedArray;
	case TYPED_ARRAY_INT16:
		return static_cast<ES_TypedArray *>(object)->GetKind() == ES_TypedArray::Int16Array;
	case TYPED_ARRAY_UINT16:
		return static_cast<ES_TypedArray *>(object)->GetKind() == ES_TypedArray::Uint16Array;
	case TYPED_ARRAY_INT32:
		return static_cast<ES_TypedArray *>(object)->GetKind() == ES_TypedArray::Int32Array;
	case TYPED_ARRAY_UINT32:
		return static_cast<ES_TypedArray *>(object)->GetKind() == ES_TypedArray::Uint32Array;
	case TYPED_ARRAY_FLOAT32:
		return static_cast<ES_TypedArray *>(object)->GetKind() == ES_TypedArray::Float32Array;
	case TYPED_ARRAY_FLOAT64:
		return static_cast<ES_TypedArray *>(object)->GetKind() == ES_TypedArray::Float64Array;
	default:
		OP_ASSERT(!"Unhandled NativeTypedArrayKind");
	case TYPED_ARRAY_ANY:
		return TRUE;
	}
}

/* static */ void
ES_Runtime::TransferArrayBuffer(ES_Object *source, ES_Object *target)
{
	OP_ASSERT(source->GCTag() == GCTAG_ES_Object_ArrayBuffer && target->GCTag() == GCTAG_ES_Object_ArrayBuffer);
	ES_ArrayBuffer::Transfer(static_cast<ES_ArrayBuffer *>(source), static_cast<ES_ArrayBuffer *>(target));
}

BOOL
ES_Runtime::IsStringObject(ES_Object* obj)
{
	return obj->IsStringObject();
}

BOOL
ES_Runtime::IsFunctionObject(ES_Object* obj)
{
	return obj->IsFunctionObject();
}

OP_STATUS
ES_Runtime::PutInGlobalObject(EcmaScript_Object* object, const uni_char* property_name)
{
	ES_Value value;
	value.type = VALUE_OBJECT;
	value.value.object = *object;
	return PutName(global_object, property_name, value);
}

OP_STATUS
ES_Runtime::PutInGlobalObject(ES_Object* object, const uni_char* property_name)
{
	ES_Value value;
	value.type = VALUE_OBJECT;
	value.value.object = object;
	return PutName(global_object, property_name, value);
}

// NULL == OOM
/*static*/ ES_Object*
ES_Runtime::CreateHostObjectWrapper(EcmaScript_Object* host_object, ES_Object* prototype, const char* object_class, BOOL dummy)
{
    GCLOCK(context, (host_object->runtime));

    ES_Host_Object* OP_MEMORY_VAR object=0;
    OP_STATUS r;
    TRAP_AND_RETURN_VALUE_IF_ERROR
        (r, (object = ES_Host_Object::Make(&context, host_object,
											prototype ? ES_Value_Internal(prototype).GetObject(&context) : host_object->runtime->global_object->GetObjectPrototype(),
											object_class ? object_class : "Object")),
          NULL);

    return static_cast<ES_Object *>(object);
}

/*static*/ ES_Object*
ES_Runtime::CreateHostFunctionWrapper(EcmaScript_Object* host_object, ES_Object* prototype,
									  const uni_char* function_name, const char* object_class, const char* parameters)
{
    GCLOCK(context, (host_object->runtime));

    ES_Host_Function* OP_MEMORY_VAR function=0;
    OP_STATUS r;
    TRAP_AND_RETURN_VALUE_IF_ERROR
        (r, (function = ES_Host_Function::Make(&context,
                                                host_object->runtime->global_object,
                                                host_object,
                                                prototype ? ES_Value_Internal(prototype).GetObject(&context) : host_object->runtime->global_object->GetFunctionPrototype(),
                                                function_name,
                                                object_class ? object_class : "Function",
                                                parameters)),
          NULL);

    return static_cast<ES_Object *>(function);
}

// NULL == OOM
ES_Context*
ES_Runtime::CreateContext(ES_Object* this_object, BOOL prevent_debugging/* = FALSE*/)
{
	BOOL is_debugging = !prevent_debugging && g_ecmaManager->IsDebugging(this);

	if (!global_object)
		return NULL;

	if (!this_object)
		this_object = global_object;

	ES_Execution_Context *context = OP_NEW(ES_Execution_Context, (rt_data, global_object, this_object, this, heap));
	if (!context)
		return NULL;

	if (OpStatus::IsMemoryError(context->Initialize()))
	{
		OP_DELETE(context);
		return NULL;
	}

	context->use_native_dispatcher = !is_debugging && use_native_dispatcher;

#ifdef ECMASCRIPT_DEBUGGER
	if (is_debugging)
	{
		g_ecmaManager->GetDebugListener()->NewContext(this, context);
		context->SetIsDebugged(TRUE);
	}
#endif

	return context;
}

#ifdef ECMASCRIPT_DEBUGGER
/*static*/
BOOL
ES_Runtime::HasDebugInfo(ES_Program *program)
{
	if (program)
		return program->program->data->has_debug_code;
	return FALSE;
}

/*static*/
BOOL
ES_Runtime::IsContextDebugged(ES_Context *context)
{
	if (context->GetExecutionContext())
		return context->GetExecutionContext()->IsDebugged();
	else
		return FALSE;
}
#endif // ECMASCRIPT_DEBUGGER

void ES_Runtime::MigrateContext(ES_Context* context)
{
	MergeHeapWith(context->GetRuntime());

	static_cast<ES_Execution_Context *>(context)->MigrateTo(global_object, this);
}

void ES_Runtime::GCMarkRuntime()
{
	GCMark(runtime_reaper);
}

BOOL ES_Runtime::GCMark(EcmaScript_Object* obj, BOOL mark_host_as_excluded)
{
	if (!*obj)
		return TRUE;
	else
		return GCMark(*obj, mark_host_as_excluded);
}

BOOL ES_Runtime::GCMark(ES_Object *obj, BOOL mark_host_as_excluded)
{
	if (!obj)
		return TRUE;
	else
	{
		if (mark_host_as_excluded && obj->IsHostObject())
		{
			static_cast<ES_Host_Object *>(obj)->ClearMustTraceForeignObject();
		}
		return heap->Mark(obj);
	}
}

BOOL ES_Runtime::GCMarkSafe(ES_Object *obj)
{
	if (!obj || !heap->IsCollecting())
		return TRUE;
	else
		return heap->Mark(obj);
}

void ES_Runtime::GCExcludeHostFromTrace(EcmaScript_Object* obj)
{
	OP_ASSERT(*obj && obj->GetNativeObject()->IsHostObject());

	static_cast<ES_Host_Object *>(obj->GetNativeObject())->ClearMustTraceForeignObject();
}

void ES_Runtime::Detach()
{
	if (heap)
	{
		heap->SetNeedsGC();
		heap->RegisterActivity();
		heap->DetachedRuntime();
	}

	if (runtime_reaper != NULL)
		Unprotect(*runtime_reaper);

#ifdef ECMASCRIPT_DEBUGGER
	/* Don't check EcmaScript_Manager::IsDebugging here as it will return false
	   if runtime's window was closed already. */
	if (g_ecmaManager->GetDebugListener())
			g_ecmaManager->GetDebugListener()->DestroyRuntime(this);
#endif // ECMASCRIPT_DEBUGGER
}

OP_STATUS
ES_Runtime::AllocateHostObject(ES_Object *&object, void *&payload, unsigned payload_size, ES_Object *prototype, const char *class_name, unsigned flags)
{
	GCLOCK(context, (this));

	TRAPD(status, object = ES_Host_Object::Make(&context, payload, payload_size, prototype, class_name, (flags & FLAG_NEED_DESTROY) != 0, (flags & FLAG_SINGLETON) != 0));

	return status;
}

OP_STATUS
ES_Runtime::AllocateHostFunction(ES_Object *&object, void *&payload, unsigned payload_size, ES_Object *prototype, const uni_char *function_name, const char *class_name, const char *parameter_types, unsigned flags)
{
	GCLOCK(context, (this));

	TRAPD(status, object = ES_Host_Function::Make(&context, global_object, payload, payload_size, prototype, function_name, class_name, parameter_types, (flags & FLAG_NEED_DESTROY) != 0, (flags & FLAG_SINGLETON) != 0));

	return status;
}

OP_STATUS
ES_Runtime::RegisterRuntimeOn(EcmaScript_Object *obj)
{
    GCLOCK(context, (this));

	if (*obj != NULL)
	{
		ES_Value val;
		val.type = VALUE_OBJECT;
		val.value.object = *runtime_reaper;
		if (PutPrivate(obj->GetNativeObject(),ES_PRIVATE_runtime,val) == PUT_NO_MEMORY)
			return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

OP_STATUS
ES_Runtime::KeepAliveWithRuntime(EcmaScript_Object *obj)
{
	return KeepAliveWithRuntime(*obj);
}

OP_STATUS
ES_Runtime::KeepAliveWithRuntime(ES_Object *obj)
{
	GCLOCK(context, (this));

	ES_Value val;
	val.type = VALUE_OBJECT;
	val.value.object = obj;
	if (PutPrivate(runtime_reaper->GetNativeObject(),keepalive_counter++,val) == PUT_NO_MEMORY)
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

#if defined(_DEBUG) || defined(SELFTEST)
void
ES_Runtime::DebugForceGC()
{
	heap->ForceCollect(NULL, GC_REASON_DEBUG);
}
#endif // _DEBUG || SELFTEST

void
ES_Runtime::SuitableForReuse(BOOL merge)
{
	heap->SuitableForReuse(merge);
}

BOOL
ES_Runtime::HasHighMemoryFragmentation()
{
	return heap->HasHighFragmentation();
}

OP_STATUS
ES_Runtime::MergeHeapWith(ES_Runtime *other)
{
	if (heap != other->heap)
	{
		RETURN_IF_ERROR(heap->MergeWith(other->heap));

		/* Should be updated by ES_Heap::MergeWith. */
		OP_ASSERT(other->heap == heap);
	}

	return OpStatus::OK;
}

ES_Object *
ES_Runtime::GetGlobalObject()
{
	OP_ASSERT(enabled || global_object == NULL);
	return global_object;
}

ES_Object *
ES_Runtime::GetGlobalObjectAsPlainObject() const
{
	return enabled ? global_object : NULL;
}

/* static */ ES_Object *
ES_Runtime::GetGlobalObjectInScope(ES_Context *context)
{
	if (ES_Execution_Context *exec_context = context ? context->GetExecutionContext() : NULL)
		if (ES_Code *code = exec_context->Code())
			return static_cast<ES_Object *>(code->global_object);

	return NULL;
}

OP_STATUS
ES_Runtime::SignalExecutionEvent(unsigned int ev)
{
#ifdef USER_JAVASCRIPT
    switch (ev)
    {
    case ES_EVENT_AFTER_GLOBAL_VARIABLES_INITIALIZED:
        return OnAfterGlobalVariablesInitialized();
    }
#endif // USER_JAVASCRIPT
	return OpStatus::OK;
}

#ifdef USER_JAVASCRIPT

/* virtual */ OP_STATUS
ES_Runtime::OnAfterGlobalVariablesInitialized()
{
    // Nothing
	return OpStatus::OK;
}

#endif

void
ES_Runtime::MarkObjectAsHidden(ES_Object *obj)
{
	obj->SetIsHiddenObject();
}

/* static */ void
ES_Runtime::SetIsCrossDomainAccessible(ES_Object *obj)
{
    obj->SetIsCrossDomainAccessible();
}

/* static */ void
ES_Runtime::SetIsCrossDomainHostAccessible(EcmaScript_Object *obj)
{
    obj->GetNativeObject()->SetIsCrossDomainHostAccessible();
}

/* virtual */ void
ES_Runtime::GCTrace()
{
}

OP_STATUS
ES_Runtime::StructuredClone(ES_Object *source_object, ES_Runtime *target_runtime, ES_Object *&target_object, CloneTransferMap *transfer_map /* = NULL */, CloneStatus *clone_status /* = NULL*/, BOOL uncloned_to_null /* = TRUE */)
{
	ES_Value source_value, target_value;

	source_value.type = VALUE_OBJECT;
	source_value.value.object = source_object;

	RETURN_IF_ERROR(Clone(source_value, target_runtime, target_value, transfer_map, clone_status, uncloned_to_null));

	OP_ASSERT(target_value.type == VALUE_OBJECT);

	target_object = target_value.value.object;
	return OpStatus::OK;
}

OP_STATUS
ES_Runtime::Clone(const ES_Value &source_value, ES_Runtime *target_runtime, ES_Value &target_value, CloneTransferMap *transfer_map, CloneStatus *clone_status, BOOL uncloned_to_null)
{
	GCLOCK(source_context, (this));
	GCLOCK(target_context, (target_runtime));

	CloneStatus local_clone_status, *clone_status_ptr = clone_status ? clone_status : &local_clone_status;
	ES_Value_Internal source_value_internal;

	RETURN_IF_ERROR(source_value_internal.Import(&source_context, source_value));

	ES_CloneFromObject source(this, &source_context, source_value_internal, *clone_status_ptr, uncloned_to_null);
	ES_CloneToObject target(target_runtime, &target_context);

    if (transfer_map)
    {
        OP_ASSERT(transfer_map->source_objects.GetCount() == transfer_map->target_objects.GetCount());
        for (unsigned i = 0; i < transfer_map->source_objects.GetCount(); i++)
        {
            RETURN_IF_ERROR(source.AddTransferObject(transfer_map->source_objects.Get(i)));
            RETURN_IF_ERROR(target.AddTransferObject(transfer_map->target_objects.Get(i)));
        }
    }

	TRAPD(status, source.CloneL(&target));

	if (OpStatus::IsSuccess(status))
		return target.GetResult().Export(&target_context, target_value);

	return status;
}

#ifdef ES_PERSISTENT_SUPPORT

OP_STATUS
ES_Runtime::CloneToPersistent(const ES_Value &source_value, ES_PersistentValue *&target_value, unsigned size_limit)
{
	GCLOCK(context, (this));

	CloneStatus local_clone_status;
	ES_Value_Internal value;

	RETURN_IF_ERROR(value.Import(&context, source_value));

	ES_CloneFromObject source(this, &context, value, local_clone_status, FALSE);
	ES_CloneToPersistent target(size_limit);

	TRAPD(status, source.CloneL(&target));

	if (OpStatus::IsSuccess(status))
		TRAP(status, target_value = target.GetResultL());

	return status;
}

OP_STATUS
ES_Runtime::CloneFromPersistent(ES_PersistentValue *source_value, ES_Value &target_value, BOOL destroy)
{
	GCLOCK(context, (this));

	ES_CloneFromPersistent source(source_value);
	ES_CloneToObject target(this, &context);

	TRAPD(status, source.CloneL(&target));

	if (OpStatus::IsSuccess(status))
		status = target.GetResult().Export(&context, target_value);

	if (destroy)
		OP_DELETE(source_value);

	return status;
}

#endif // ES_PERSISTENT_SUPPORT

OP_STATUS
ES_Runtime::ParseJSON(ES_Value &target_value, const uni_char *string, unsigned length)
{
	GCLOCK(context, (this));

	ES_Value_Internal value;

	TRAPD(status, ES_JsonBuiltins::ParseL(&context, value, string, length));
	RETURN_IF_ERROR(status);

	return value.Export(&context, target_value);
}

OP_STATUS
ES_Runtime::SerializeJSON(TempBuffer &buffer, const ES_Value &source_value)
{
	GCLOCK(context, (this));

	ES_Value_Internal value;
	const uni_char *result;

	RETURN_IF_ERROR(value.Import(&context, source_value));
	TRAPD(status, ES_JsonBuiltins::StringifyL(&context, result, value));
	RETURN_IF_ERROR(status);

	return buffer.Append(result);
}

void
ES_Runtime::SetErrorHandler(ErrorHandler *callback)
{
	error_handler = callback;
}

ES_Runtime::ErrorHandler *
ES_Runtime::GetErrorHandler()
{
	return error_handler;
}

void
ES_Runtime::ReportErrorToConsole(const ES_ErrorData *data)
{
#ifdef OPERA_CONSOLE
	TRAPD(status, g_console->PostMessageL(&data->message));
#endif // OPERA_CONSOLE

	OP_DELETE(data);
}

void
ES_Runtime::IgnoreError(const ES_ErrorData *data)
{
	OP_DELETE(data);
}

void
ES_Runtime::GetPropertyNamesL(ES_Object* object, OpVector<OpString> &propnames)
{
    GCLOCK(context, (this));

    for (ES_Boxed_List *list = object->GetPropertyNamesSafeL(&context, FALSE); list; list = list->tail)
    {
        ES_Indexed_Properties *names = static_cast<ES_Indexed_Properties *>(list->head);
        ES_Indexed_Property_Iterator iterator(&context, NULL, names);

        unsigned index;
        while (iterator.Next(index))
        {
            ES_Value_Internal value;
            if (iterator.GetValue(value))
            {
                if (!value.IsString())
                    value.ToString(&context);

                OpString* s = OP_NEW_L(OpString,());
                LEAVE_IF_ERROR(propnames.Add(s));
                JString *jstring = value.GetString();
                s->SetL(Storage(&context, jstring), Length(jstring));
            }
        }
    }
}

OP_STATUS
ES_Runtime::GetPropertyNames(ES_Object* object, OpVector<OpString> &propnames)
{
	TRAPD(stat, GetPropertyNamesL(object, propnames));
	return stat;
}

// Moved from _inlines

/* static */ BOOL
ES_Runtime::GetIsExternal(ES_Context* context)
{
    if (context == NULL)
        context = RT_DATA.saved_callback_context;

    if (context != NULL && context->GetExecutionContext())
        return context->GetExecutionContext()->GetIsExternal();
    else
        return FALSE;
}

/* static */ BOOL
ES_Runtime::GetIsInIdentifierExpression(ES_Context* context)
{
//    if (context == NULL)
//        context = RT_DATA.saved_callback_context;

	if (context != NULL)
        return static_cast<ES_Execution_Context *>(context)->GetIsInIdentifierExpression();
    else
        return FALSE;
}

/* static */ OP_STATUS
ES_Runtime::GetCallerInformation(ES_Context* context, CallerInformation& call_info)
{
	call_info.url = NULL;
	call_info.privilege_level = PRIV_LVL_UNSPEC;
	call_info.script_guid = 0;
	call_info.line_no = 0;

	if (context != NULL)
		return static_cast<ES_Execution_Context *>(context)->GetCallerInformation(call_info);
	else
		return OpStatus::OK;
}

/* static */ OP_STATUS
ES_Runtime::GetErrorStackCallerInformation(ES_Context* context, ES_Object* error_object, unsigned stack_position, BOOL resolve, CallerInformation& call_info)
{
	call_info.url = NULL;
	call_info.privilege_level = PRIV_LVL_UNSPEC;
	call_info.script_guid = 0;
	call_info.line_no = 0;

	if (context != NULL)
		return static_cast<ES_Execution_Context *>(context)->GetErrorStackCallerInformation(error_object, stack_position, resolve, call_info);
	else
		return OpStatus::OK;
}

static JString*
ToStringShallow(ES_Execution_Context *context, ES_Execution_Context *error_context, ES_Value_Internal *value)
{
	/* When presenting exception values, be judicious about what and how much output to provide. */
	if (!value->IsString())
		if (!value->IsObject())
			return value->AsString(context).GetString();
		else
		{
			/* Attempt to stringify object as something Error-like; if not one, use [object Type] as fallback. */
			ES_Object *object = value->GetObject(context);

			JString *error_string  = ES_Error::GetErrorString(context, object, FALSE/*disallow getter invocation*/);
			if (error_string == context->rt_data->strings[STRING_empty])
			{
				JString *obj_str = JString::Make(context, UNI_L("[object "));
				Append(context, obj_str, object->ObjectName());
				return Append(context, obj_str, UNI_L("]"));
			}
			else if (object->IsErrorObject())
			{
				ES_Error *error_object = static_cast<ES_Error *>(object);
				if (!error_object->ViaConstructor())
					Append(context, error_string, error_object->GetStackTraceString(context, error_context, ES_Error::FORMAT_READABLE, 2));
			}
			return error_string;
		}
	else
		return value->GetString();
}

static BOOL NeverTimeOut(void *) { return FALSE; }

/* static */ ES_Eval_Status
ES_Runtime::ExecuteContext(ES_Context* context, BOOL want_string_result, BOOL want_exceptions, BOOL allow_cross_origin_errors, BOOL (*external_out_of_time)(void *), void *external_out_of_time_data)
{
	ES_Eval_Status eval_status;

	TRAPD(status, eval_status = ExecuteContextL(context, want_string_result, want_exceptions, allow_cross_origin_errors, external_out_of_time, external_out_of_time_data));

	ES_Runtime *runtime = context->runtime;
	if (runtime->tostring_context)
	{
		runtime->DeleteContext(runtime->tostring_context);
		runtime->tostring_context = NULL;
	}

	if (OpStatus::IsError(status))
	{
		const uni_char* msg = NULL;

		if (OpStatus::IsMemoryError(status))
		{
			msg = UNI_L("Out of memory; script terminated.");
			eval_status = ES_ERROR_NO_MEMORY;
		}
		else
		{
			msg = UNI_L("Runtime error; script terminated.");
			eval_status = ES_ERROR;
		}

#ifdef OPERA_CONSOLE
		if (context->GetRuntime()->GetErrorEnable())
			ES_ImportedAPI::PostToConsole(msg, context->GetRuntime()->GetFramesDocument());
#elif defined(_STANDALONE)
		while (*msg)
		{
			fputc((*msg & 0xff), stderr);
			++msg;
		}
		fputc('\n', stderr);
#endif // OPERA_CONSOLE || _STANDALONE
	}

#ifdef ES_PROPERTY_CACHE_PROFILING
#ifdef OPERA_CONSOLE
	switch (eval_status)
	{
	case ES_NORMAL:
	case ES_NORMAL_AFTER_VALUE:
	case ES_ERROR:
	case ES_THREW_EXCEPTION:
		ESRT_Data *rt_data = context->rt_data;
		unsigned pcache_sum = rt_data->pcache_misses + rt_data->pcache_fails + rt_data->pcache_fills + rt_data->pcache_fills_poly;

		if (pcache_sum != rt_data->pcache_last_reported)
		{
			OpConsoleEngine::Message message(OpConsoleEngine::EcmaScript, OpConsoleEngine::Information);

			if (FramesDocument *document = context->GetRuntime()->GetFramesDocument())
			{
				OpStatus::Ignore(document->GetURL().GetAttribute(URL::KUniName, message.url));

#ifdef CORE_GOGI
				if (message.url.Find("/console.html") != KNotFound)
					break;
#endif // CORE_GOGI

				message.window = document->GetWindow()->Id();
			}

			message.message.AppendFormat(UNI_L("Property cache counters:\n  Cache misses: %u\n  Cache fails:  %u\n  Cache fills:  %u (total)\n  Cache fills:  %u (polymorphic)\nMemory allocated:\n  Property caches: %u\n  Global caches:   %u"),
			                             rt_data->pcache_misses, rt_data->pcache_fails, rt_data->pcache_fills, rt_data->pcache_fills_poly,
										 rt_data->pcache_allocated_property, rt_data->pcache_allocated_global);

			TRAPD(status, g_console->PostMessageL(&message));
			OpStatus::Ignore(status);

			rt_data->pcache_last_reported = pcache_sum;
		}
	}
#endif // OPERA_CONSOLE
#endif // ES_PROPERTY_CACHE_PROFILING

	return eval_status;
}

/* static */ ES_Eval_Status
ES_Runtime::ExecuteContextL(ES_Context* context, BOOL want_string_result, BOOL want_exceptions, BOOL allow_cross_origin_errors, BOOL (*external_out_of_time)(void *), void *external_out_of_time_data)
{
	ES_Execution_Context *exec_context = context->GetExecutionContext();
	ES_Runtime *runtime = exec_context->GetRuntime();

#ifdef CRASHLOG_CAP_PRESENT
	runtime->UpdateURL();
	ES_CurrentURL current_url(runtime);
#endif // CRASHLOG_CAP_PRESENT

#ifdef ES_DISASSEMBLER_SUPPORT
#ifdef _DEBUG
#if defined UNIX || defined LINGOGI || defined LINUXSDK
	extern void ES_DisassembleFromDebugger(ES_Execution_Context *, ES_Code *);

	if (!exec_context)
		ES_DisassembleFromDebugger(NULL, NULL);

#ifdef ES_DEBUG_COMPACT_OBJECTS
	extern void ES_PrintClass(ES_Execution_Context *, ES_Class *);
	if (!exec_context)
		ES_PrintClass(NULL, NULL);
#endif // ES_DEBUG_COMPACT_OBJECTS

#endif // UNIX || LINGOGI || defined LINUXSDK
#endif // _DEBUG
#endif // ES_DISASSEMBLER_SUPPORT

	if (!external_out_of_time)
		external_out_of_time = &NeverTimeOut;

	exec_context->SetExternalOutOfTime(external_out_of_time, external_out_of_time_data);

#ifdef ECMASCRIPT_DEBUGGER
	BOOL is_debugged = exec_context->IsDebugged();

	if (is_debugged && g_ecmaManager->GetDebugListener())
		g_ecmaManager->GetDebugListener()->EnterContext(runtime, context);
#endif

	ES_Eval_Status eval_status = exec_context->ExecuteProgram(want_string_result);

#ifdef ECMASCRIPT_DEBUGGER
	if (is_debugged && g_ecmaManager->GetDebugListener())
		g_ecmaManager->GetDebugListener()->LeaveContext(runtime, context);
#endif

	switch (eval_status)
	{
	case ES_ERROR_NO_MEMORY:
		LEAVE(OpStatus::ERR_NO_MEMORY);
	case ES_ERROR:
		LEAVE(OpStatus::ERR);
	case ES_THREW_EXCEPTION:
		if (!want_exceptions)
		{
#ifdef OPERA_CONSOLE
			/* If not logging nor handling, return unwanted exception as ES_ERROR. */
			if (!g_console->IsLogging() && !runtime->GetErrorHandler())
				return ES_ERROR;
#endif

			runtime->tostring_context = runtime->CreateContext(runtime->global_object);

			if (!runtime->tostring_context)
				return ES_ERROR_NO_MEMORY;
			else
			{
				ES_CollectorLock gclock(context); ANCHOR(ES_CollectorLock, gclock);

				ES_Runtime::ErrorHandler *error_handler = runtime->GetErrorHandler();
				OpStackAutoPtr<ES_ErrorData> error_data;

#ifdef OPERA_CONSOLE
				FramesDocument *frames_doc = runtime->frames_doc;
				OpConsoleEngine::Message *console_message, console_message_local;

				if (error_handler)
				{
					error_data.reset(OP_NEW_L(ES_ErrorData, ()));
					console_message = &error_data->message;
				}
				else
					console_message = &console_message_local;

				console_message->source = OpConsoleEngine::EcmaScript;
				console_message->severity = OpConsoleEngine::Error;
				console_message->window = frames_doc ? frames_doc->GetWindow()->Id() : 0;

				if (frames_doc)
				{
					frames_doc->GetURL().GetAttributeL(URL::KUniName, console_message->url);
					console_message->context.SetL(frames_doc->GetESScheduler()->GetThreadInfoString());
				}
				else
					console_message->context.SetL("Unknown thread");

				OpString &message = console_message->message;
#else // OPERA_CONSOLE
				OpString message; ANCHOR(OpString, message);
#endif // OPERA_CONSOLE

				message.Set("Uncaught exception: ");

				ES_Value_Internal exception = exec_context->GetCurrentException();
				ES_Error *error;

				if (exception.IsObject() && exception.GetObject()->IsErrorObject())
					error = static_cast<ES_Error *>(exception.GetObject());
				else
					error = NULL;

				JString *exception_str = ToStringShallow(runtime->tostring_context->GetExecutionContext(), exec_context, &exception);
				message.AppendL(Storage(runtime->tostring_context, exception_str), Length(exception_str));

				OpString oneline; ANCHOR(OpString, oneline);

				if (error_handler)
				{
					int length = message.Find("\n");
					if (length == KNotFound)
						length = KAll;
					oneline.SetL(message, length);
				}

				message.AppendL("\n");

				TempBuffer buffer;

				if (!error || error->ViaConstructor())
					if (exec_context->GenerateStackTraceL(buffer, exec_context, error))
						message.AppendL(buffer.GetStorage());

#ifndef _STANDALONE
				if (error_handler)
				{
					JString *url_string = NULL;

					ES_SourcePosition position;
					exec_context->GetStackLocationAtIndex(error, 0, url_string, position); // ignore if not located; best effort.

					const uni_char *msg = oneline.CStr();
					const uni_char *url = url_string ? StorageZ(context, url_string) : NULL;

					ErrorHandler::RuntimeErrorInformation error_information;
					error_information.cross_origin_allowed = allow_cross_origin_errors;
					exception.ExportL(context, error_information.exception);

					OP_STATUS status = error_handler->HandleError(runtime, &error_information, msg, url, position, error_data.get());

					if (OpStatus::IsSuccess(status))
						error_data.release();
					else
						LEAVE(status);
				}
#ifdef OPERA_CONSOLE
				else
					g_console->PostMessageL(console_message);
#endif // OPERA_CONSOLE
#else // _STANDALONE
				const uni_char *unicode = message.CStr();
				unsigned length = message.Length();
				char *ascii = OP_NEWA_L(char, (length + 2));

				for (unsigned index = 0; index < length; ++index)
					ascii[index] = (char) unicode[index];
				ascii[length] = '\n';
				ascii[length + 1] = '\0';
				fputs(ascii, stdout);
				OP_DELETEA(ascii);
#endif // _STANDALONE
			}

			eval_status = ES_ERROR;
		}
		break;
	}

	return eval_status;
}

OP_BOOLEAN
ES_Runtime::GetIndex(ES_Object* object, unsigned index, ES_Value* value)
{
    GCLOCK(context, (this));

    ES_Value_Internal internal_value;
   	BOOL found = FALSE;

    RETURN_IF_LEAVE(found = object->GetNativeL(&context, index, internal_value));

    if (found)
    {
        if (value)
            RETURN_IF_LEAVE(internal_value.ExportL(&context, *value));
        return OpBoolean::IS_TRUE;
    }
    else
        return OpBoolean::IS_FALSE;
}

OP_BOOLEAN
ES_Runtime::GetName(ES_Object* object, const uni_char* property_name, ES_Value* value)
{
    GCLOCK(context, (this));

    ES_Value_Internal internal_value;
   	BOOL found = FALSE;

    RETURN_IF_LEAVE(
		  JString *name = JString::Make(&context, property_name);
		  found = object->GetNativeL(&context, name, internal_value)
        );

    if (found)
    {
        if (value)
            RETURN_IF_LEAVE(internal_value.ExportL(&context, *value));
        return OpBoolean::IS_TRUE;
    }
    else
        return OpBoolean::IS_FALSE;
}

OP_BOOLEAN
ES_Runtime::GetPrivate(ES_Object* object, int private_name, ES_Value* value)
{
    GCLOCK(context, (this));

	ES_Value_Internal local_value;
	BOOL found = FALSE; // Silent compiler warnings

    RETURN_IF_LEAVE(found=object->GetPrivateL(&context, (unsigned)private_name, local_value));
	if (found)
	{
		if (value)
			RETURN_IF_LEAVE(local_value.ExportL(&context, *value));
		return OpBoolean::IS_TRUE;
	}
	else
	    return OpBoolean::IS_FALSE;
}

OP_STATUS
ES_Runtime::PutPrivate(ES_Object* object, int private_name, const ES_Value& value)
{
    GCLOCK(context, (this));

    ES_Value_Internal local_value;

    RETURN_IF_LEAVE(local_value.ImportL(&context, value));
    RETURN_IF_LEAVE(object->PutPrivateL(&context, (unsigned)private_name, local_value, global_object->GetObjectClass()));

	return OpStatus::OK;
}

BOOL
ES_Runtime::Protect(ES_Object *obj)
{
	return heap->AddDynamicRoot(obj);
}

void
ES_Runtime::Unprotect(ES_Object *obj)
{
	heap->RemoveDynamicRoot(obj);
}

/* static */ OP_STATUS
ES_Runtime::PushCall(ES_Context* context, ES_Object* native_object, int argc, const ES_Value* argv)
{
    ES_CollectorLock gclock(context);
    TRAPD(r,static_cast<ES_Execution_Context *>(context)->PushCall(native_object, argc, argv));
    return r;
}

OP_STATUS
ES_Runtime::PushProgram(ES_Context* context, ES_Program* program, ES_Object** scope_chain, int scope_chain_length)
{
    ES_CollectorLock gclock(context);
	return static_cast<ES_Execution_Context *>(context)->SetupExecution(program->program, scope_chain, scope_chain_length);
}

/* static */ EcmaScript_Object*
ES_Runtime::GetHostObject(ES_Object* object)
{
    if (object->IsHostObject())
        return static_cast<ES_Host_Object *>(object)->GetHostObject();
    else
        return NULL;
}

/* static */ ES_Object *
ES_Runtime::Identity(ES_Object *object)
{
	OP_ASSERT(object);

	EcmaScript_Object *host = ES_Runtime::GetHostObject(object);

	if (!host)
		return object;

	ES_Object *target;

	if (OpStatus::IsError(host->Identity(&target)))
		return NULL;

	return target;
}

/* static */ void
ES_Runtime::DecoupleHostObject(ES_Object *object)
{
	if (object->IsHostObject())
		static_cast<ES_Host_Object *>(object)->DecoupleHostObject();
}

/* static */ void
ES_Runtime::DeleteContext(ES_Context* context)
{
	OP_ASSERT(context->GetExecutionContext());

#ifdef ECMASCRIPT_DEBUGGER
	if (g_ecmaManager->GetDebugListener() && context->GetExecutionContext()->IsDebugged())
		g_ecmaManager->GetDebugListener()->DestroyContext(context->GetExecutionContext()->GetRuntime(), context);
#endif

	OP_DELETE(context->GetExecutionContext());
}

/* static */ void
ES_Runtime::RequestTimeoutCheck(ES_Context* context)
{
	static_cast<ES_Execution_Context *>(context)->RequestTimeoutCheck();
}

/* static */ void
ES_Runtime::SuspendContext(ES_Context* context)
{
	static_cast<ES_Execution_Context *>(context)->SuspendExecution();
}

/* static */ OP_STATUS
ES_Runtime::GetReturnValue(ES_Context* context, ES_Value* return_value)
{
    ES_CollectorLock gclock(context);
    return static_cast<ES_Execution_Context *>(context)->ReturnValue().Export(context, *return_value);
}

/* static */ const char*
ES_Runtime::GetClass(ES_Object* object)
{
    return object->ObjectName();
}

OP_STATUS
ES_Runtime::PutIndex(ES_Object* object, unsigned index, const ES_Value& value)
{
	GCLOCK(context, (this));
	TRAPD(r,
		ES_Value_Internal internal_value;
		internal_value.ImportL(&context, value);
		object->PutNativeL(&context, index, 0, internal_value)
	);
	return sanitize(r);
}

OP_STATUS
ES_Runtime::PutName(ES_Object* object, const uni_char* property_name, const ES_Value& value, int flags)
{
	ES_Property_Info info;

	info.data.read_only = flags & PROP_READ_ONLY ? 1 : 0;
	info.data.dont_enum = flags & PROP_DONT_ENUM ? 1 : 0;
	info.data.dont_delete = flags & PROP_DONT_DELETE ? 1 : 0;
	info.data.dont_put_native = flags & PROP_HOST_PUT ? 1 : 0;

	GCLOCK(context, (this));
	TRAPD(r,
		ES_Value_Internal internal_value;
		JString *name = initializing ? rt_data->GetSharedString(property_name) : JString::Make(&context, property_name);
		internal_value.ImportL(&context, value);
		object->PutNativeL(&context, name, info, internal_value)
	);
    return sanitize(r);
}

OP_STATUS
ES_Runtime::DeleteName(ES_Object* object, const uni_char* property_name, BOOL force)
{
	GCLOCK(context, (this));
	RETURN_IF_LEAVE(
		  JString *name = JString::Make(&context, property_name);
		  object->DeleteNativeL(&context, name)
        );

	return OpStatus::OK;
}

BOOL
ES_Runtime::IsSpecialProperty(ES_Object *object, const uni_char *property_name)
{
	JTemporaryString temporary(property_name, uni_strlen(property_name));
	ES_Property_Info info;
	object->HasOwnNativeProperty(temporary, info);
	return info.IsSpecial();
}

void
ES_Runtime::SetFramesDocument(FramesDocument* frames_document)
{
	frames_doc = frames_document;

#ifdef CRASHLOG_CAP_PRESENT
	UpdateURL();
#endif // CRASHLOG_CAP_PRESENT
}

// CARAKAN TODO: might use future common function:
ES_Object* ES_Runtime::GetObjectPrototype()
{
	return global_object->GetObjectPrototype();
}

ES_Object* ES_Runtime::GetFunctionPrototype()
{
	return global_object->GetFunctionPrototype();
}

ES_Object* ES_Runtime::GetNumberPrototype()
{
	return global_object->GetNumberPrototype();
}

ES_Object* ES_Runtime::GetStringPrototype()
{
	return global_object->GetStringPrototype();
}

ES_Object* ES_Runtime::GetBooleanPrototype()
{
	return global_object->GetBooleanPrototype();
}

ES_Object* ES_Runtime::GetArrayPrototype()
{
	return global_object->GetArrayPrototype();
}

ES_Object* ES_Runtime::GetDatePrototype()
{
	return global_object->GetDatePrototype();
}

ES_Object* ES_Runtime::GetErrorPrototype()
{
	return global_object->GetErrorPrototype();
}

ES_Object* ES_Runtime::GetTypeErrorPrototype()
{
	return global_object->GetTypeErrorPrototype();
}

ESRT_Data *
ES_Runtime::GetRuntimeData()
{
    return rt_data;
}

ES_Runtime *&ES_Runtime::GetNextRuntimePerHeap()
{
	return next_runtime_per_heap;
}

ES_Heap *ES_Runtime::GetHeap()
{
	return heap;
}

void ES_Runtime::SetHeap(ES_Heap *h)
{
	heap = h;
}

BOOL
ES_Runtime::InGC()
{
	return GetHeap()->InCollector();
}

BOOL ES_Runtime::IsSameHeap(ES_Runtime *r)
{
	return (r && r->heap == heap);
}

void ES_Runtime::RecordExternalAllocation(unsigned bytes)
{
	heap->RecordExternalBytes(bytes);
}

#ifdef ECMASCRIPT_DEBUGGER

/* virtual */ const char *
ES_Runtime::GetDescription() const
{
	return "unknown";
}

/* virtual */ OP_STATUS
ES_Runtime::GetWindows(OpVector<Window> &windows)
{
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
ES_Runtime::GetURLDisplayName(OpString &str)
{
	str.Empty();
	return OpStatus::OK;
}

# ifdef EXTENSION_SUPPORT
/* virtual */ OP_STATUS
ES_Runtime::GetExtensionName(OpString &extension_name)
{
	extension_name.Empty();
	return OpStatus::ERR;
}
# endif // EXTENSION_SUPPORT

#endif // ECMASCRIPT_DEBUGGER

OP_STATUS
ES_Runtime::DeletePrivate(ES_Object* object, int private_name)
{
	GCLOCK(context, (this));
	RETURN_IF_LEAVE(object->DeletePrivateL(&context, (unsigned int)private_name));
	return OpStatus::OK;
}

#ifdef CRASHLOG_CAP_PRESENT

void
ES_Runtime::UpdateURL()
{
	if (frames_doc)
	{
		FramesDocument *doc = frames_doc;

		while (FramesDocument *parent_doc = doc->GetParentDoc())
			doc = parent_doc;

		OpStatus::Ignore(doc->GetURL().GetAttribute(URL::KName, url));
	}
}

#endif // CRASHLOG_CAP_PRESENT
