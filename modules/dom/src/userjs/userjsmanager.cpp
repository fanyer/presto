/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2003-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef USER_JAVASCRIPT

#include "modules/dom/src/domcore/element.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domevents/domevent.h"
#include "modules/dom/src/domevents/domeventdata.h"
#include "modules/dom/src/domevents/domeventlistener.h"
#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/dom/src/domevents/domeventthread.h"
#include "modules/dom/src/domglobaldata.h"
#include "modules/dom/src/userjs/userjsevent.h"
#include "modules/dom/src/userjs/userjsmanager.h"

#include "modules/doc/frm_doc.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/util/excepts.h"
#include "modules/util/glob.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/tempbuf.h"

#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/datasrcelm.h"
#include "modules/logdoc/htm_ldoc.h"
#include "modules/url/url_sn.h"
#include "modules/encodings/detector/charsetdetector.h"
#ifdef USER_JAVASCRIPT_ADVANCED
# include "modules/encodings/decoders/inputconverter.h"
# include "modules/encodings/decoders/utf16-decoder.h"
# include "modules/prefs/prefsmanager/collections/pc_js.h"
# include "modules/windowcommander/src/WindowCommander.h"
# include "modules/dochand/win.h"
#else // USER_JAVASCRIPT_ADVANCED
# include "modules/prefs/prefsmanager/collections/pc_files.h"
#endif // USER_JAVASCRIPT_ADVANCED

#ifdef USER_JAVASCRIPT_ADVANCED
# ifdef DOM_BROWSERJS_SUPPORT
#  include "modules/dom/src/userjs/browserjs_key.h"
# ifdef CRYPTO_VERIFY_SIGNED_TEXTFILE_SUPPORT
#  include "modules/libcrypto/include/CryptoVerifySignedTextFile.h"
# else // CRYPTO_VERIFY_SIGNED_TEXTFILE_SUPPORT
#  include "modules/libssl/tools/signed_textfile.h"
# endif // CRYPTO_VERIFY_SIGNED_TEXTFILE_SUPPORT
# endif // DOM_BROWSERJS_SUPPORT

#ifdef WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
#include "modules/dom/src/storage/storage.h"
#endif //WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT

#ifdef OPERA_CONSOLE
# include "modules/util/OpHashTable.h"
#endif // OPERA_CONSOLE

#ifdef EXTENSION_SUPPORT
# include "modules/gadgets/OpGadgetManager.h"
# include "modules/dom/src/extensions/domextensionmanager.h"
#endif // EXTENSION_SUPPORT

class DOM_UserJSScript;
class DOM_UserJSSource;

class DOM_UserJSRule
{
#ifdef EXTENSION_SUPPORT
	friend class DOM_UserJSSource;
#endif // EXTENSION_SUPPORT
private:
	DOM_UserJSRule() : matches_all(FALSE), next(NULL) { }

	OpString pattern;
	BOOL matches_all;
	DOM_UserJSRule *next;
public:
	~DOM_UserJSRule();

	static OP_STATUS Make(DOM_UserJSRule *&rule, const uni_char *pattern, unsigned pattern_length);
	BOOL Match(const uni_char *uri);
};

/**
 * Contains the (decoded) source code for a UserJS file. This class also
 * processes the special comment blocks, and extracts information from them.
 */
class DOM_UserJSSource
	: public ListElement<DOM_UserJSSource>
{
public:
	~DOM_UserJSSource();
	/**< Cleans up include/exclude patterns, and other information extracted
	     from the special comment block. */

	static OP_BOOLEAN Make(DOM_UserJSSource *&out, const uni_char* filename, DOM_UserJSManager::UserJSType type);
	/**< Make a new DOM_UserJSSource instance.
	 *
	 * @param out Set to point to the created object.
	 * @param filename The absolute path to the file from which JS source will be read.
	 * @param type Type of the JS (extension, browserjs etc.)
	 * @return
	 *  - IS_TRUE - in case of success.
	 *  - IS_FALSE - there was a read error.
	 *  - other errors.
	 */

	static OP_BOOLEAN Make(DOM_UserJSSource *&out, OpFile &file, DOM_UserJSManager::UserJSType type);
	/**< Make a new DOM_UserJSSource instance.
	 *
	 * @param out Set to point to the created object.
	 * @param file The file from which JS source will be read.
	 * @param type Type of the JS (extension, browserjs etc.).
	 * @return
	 *  - IS_TRUE - in case of success.
	 *  - IS_FALSE - there was a read error.
	 *  - ERR_NO_MEMORY - no memory.
	 *  - ERR - if reading or decoding the file does not succeed.
	 */

	OP_BOOLEAN Reload(OpFile &file);
	/**< Reloads the file contents.
	 *
	 * Reads the file contents and decodes it into memory.
	 * Useful especially after Empty has been called and the source text
	 * is needed again.
	 *
	 * @param file The file from which JS source will be read.
	 * @return
	 *  - IS_TRUE - in case of success
	 *  - IS_FALSE - there was a read error
	 *  - ERR_NO_MEMORY - no memory
	 *  - other errors if decoding of the file fails
	 */

	BOOL WouldUse(FramesDocument *frames_doc);
	BOOL WouldUse(DOM_EnvironmentImpl *environment);
	/**< @return TRUE if this environment would use this script (based on included/excluded
	     domain directives). */

	const OpString &GetSource() const { return decoded; }
	/**< @return the decoded source code. */

	void Empty() { decoded.Empty(); }
	/**< Call this when the decoded source code is no longer needed. Future calls to
	     to GetSource will result in an empty string. Calls to WouldUse will still
	     work. */

	BOOL IsEmpty() const { return decoded.IsEmpty(); }
	/**< Is the decoded source code in memory? If so, calls to GetSource will result
	     in an empty string. */

	OP_BOOLEAN IsStale() const;
	/**< Checks if this object is still valid.
	     The object becomes stale if the file from which source has been
	     loaded is modified, in which case data needs to be reloaded. */

	const uni_char *GetFilename() const { return filename; }
	time_t GetModified() const { return modified; }
	/**< Get modification date of the loaded source. */

	DOM_UserJSManager::UserJSType GetType() const { return type; }

	static void IncRef(DOM_UserJSSource *source);
	static void DecRef(DOM_UserJSSource *source);

	/** Computes a hash value based on the file's name and modification time. */
	unsigned GetFileHash() const;

#ifdef EXTENSION_SUPPORT
	BOOL CheckIfAlwaysEnabled();
	/**< Returns TRUE if the given UserJS is known to always activate. Used
	     to determine if creating a DOM environment ahead of time is required
	     to guarantee that it (and others) will get to run. */
#endif // EXTENSION_SUPPORT

private:

	DOM_UserJSSource();
	/**< Client code should not use this directly. Use DOM_UserJSSource::Make instead. */

	static OP_STATUS LoadFile(OpFile &file, char *&source, OpFileLength &source_length, time_t &last_modified);
	static OP_STATUS Decode(char *source, OpFileLength source_length, OpString &decoded_output);

	// Let Make access this class through an OpAutoPtr.
	friend class OpAutoPtr<DOM_UserJSScript>;

	OP_STATUS ProcessComment(const uni_char *comment);
	/**< Extract information from the special comment block. */

	OP_STATUS ProcessCommentIfPresent();
	/**< Verify that a special comment block is present, then extract information
	     from it. */

	void ResetComments();
	/**< Clear out all fields that are set by the UserJS comment section. Called
	     upon reload. */

	OpString decoded;
	DOM_UserJSRule *include, *exclude;
	uni_char *name, *ns, *description;
	time_t modified;
	const uni_char *filename;
	DOM_UserJSManager::UserJSType type;
	unsigned refcount;
}; // DOM_UserJSSource

class DOM_UserJSScript
	: public ListElement<DOM_UserJSScript>
{
protected:
	DOM_UserJSSource *source;
	ES_Static_Program *static_program;
	URL filename_url;
	unsigned refcount;

	DOM_UserJSScript();
	~DOM_UserJSScript();

	friend class OpAutoPtr<DOM_UserJSScript>;

public:
	/**
	 * @returns OpStatus::OK if a DOM_UserJSScript was created,
	 * OpStatus::ERR_NO_MEMORY if we ran out of memory and OpStatus::ERR
	 * if the source wasn't enough to create a script (for instance too
	 * short (i.e. empty) or not decodable by the charset decoder).
	 */
	static OP_STATUS Make(DOM_UserJSScript *&script, ES_Program *&program, ES_Runtime *runtime, DOM_UserJSSource *source, DOM_UserJSManager::UserJSType type);

	DOM_UserJSManager::UserJSType GetType() const { return source->GetType(); }

	BOOL WouldUse(FramesDocument *frames_doc);
	BOOL WouldUse(DOM_EnvironmentImpl *environment);
	OP_STATUS Use(DOM_EnvironmentImpl *environment, ES_Program *&program, ES_Thread *interrupt_thread, DOM_UserJSManager::UsedScript *usedscript);

	OP_STATUS GetProgram(ES_Program *&program, ES_Runtime *runtime);

	DOM_UserJSSource *GetUserJSSource() { return source; }

	const uni_char *GetFilename() const { return source->GetFilename(); }
	OP_BOOLEAN IsStale() const { return source->IsStale(); }

	static DOM_UserJSScript *IncRef(DOM_UserJSScript *script);
	static void DecRef(DOM_UserJSScript *script);
};

DOM_UserJSThread::DOM_UserJSThread(ES_Context *context, DOM_UserJSManager *manager, DOM_UserJSScript *script, DOM_UserJSManager::UserJSType type)
	: ES_Thread(context),
	  manager(manager),
	  script(DOM_UserJSScript::IncRef(script)),
#ifdef WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
	  script_storage(NULL),
#endif // WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
	  type(type)
{
}

DOM_UserJSThread::~DOM_UserJSThread()
{
	DOM_UserJSScript::DecRef(script);
#ifdef WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
	SetScriptStorage(NULL);
#endif // WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
}

/* virtual */ OP_STATUS
DOM_UserJSThread::Signal(ES_ThreadSignal signal)
{
#ifdef WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
	SetScriptStorage(NULL);
#endif // WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
	switch (signal)
	{
	case ES_SIGNAL_FINISHED:
	case ES_SIGNAL_FAILED:
	case ES_SIGNAL_CANCELLED:
		if (type != DOM_UserJSManager::GREASEMONKEY)
			manager->ScriptFinished();
	}

	return ES_Thread::Signal(signal);
}

#ifdef WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
void DOM_UserJSThread::SetScriptStorage(DOM_Storage *ss)
{
	if (script_storage != NULL)
	{
		script_storage->GetRuntime()->Unprotect(*script_storage);
		script_storage = NULL;
	};

	if (ss && IsSignalled())
	{
		OP_ASSERT(!"Whoops, this thread has ended, no point in calling this function");
		return;
	}

	script_storage = ss;
	if (script_storage && !script_storage->GetRuntime()->Protect(*script_storage))
		script_storage = NULL;
}

const uni_char*
DOM_UserJSThread::GetScriptFileName() const
{
	return script->GetFilename();
}
#endif // WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT

/* virtual */ const uni_char *
DOM_UserJSThread::GetInfoString()
{
	return UNI_L("User Javascript thread");
}

/* static */ OP_STATUS
DOM_UserJSRule::Make(DOM_UserJSRule *&rule, const uni_char *pattern, unsigned pattern_length)
{
	DOM_UserJSRule *new_rule = OP_NEW(DOM_UserJSRule, ());
	RETURN_OOM_IF_NULL(new_rule);
	OpAutoPtr<DOM_UserJSRule> new_rule_anchor(new_rule);
	RETURN_IF_ERROR(new_rule->pattern.Set(pattern, pattern_length));
	new_rule->pattern.MakeLower();
	new_rule->matches_all = new_rule->pattern.Compare("*") == 0;
	new_rule->next = rule;
	rule = new_rule_anchor.release();
	return OpStatus::OK;
}

DOM_UserJSRule::~DOM_UserJSRule()
{
	OP_DELETE(next);
}

BOOL
DOM_UserJSRule::Match(const uni_char *uri)
{
	if (matches_all || OpGlob(uri, pattern, FALSE, FALSE))
		return TRUE;

	if (next)
		return next->Match(uri);
	else
		return FALSE;
}

DOM_UserJSScript::DOM_UserJSScript()
	: source(NULL),
	  static_program(NULL),
	  refcount(1)
{
}

DOM_UserJSScript::~DOM_UserJSScript()
{
	if (static_program)
		ES_Runtime::DeleteStaticProgram(static_program);

	DOM_UserJSSource::DecRef(source);
}

static BOOL
DOM_IsWhitespace(uni_char ch)
{
	if (ch == 32)
		return TRUE;
	else if (ch < 0xA0)
		return ch >= 9 && ch <= 13;
	else
		switch (ch)
		{
		case 0x00A0: // NO-BREAK SPACE
		case 0x200B: // ZERO WIDTH SPACE
		case 0xFEFF: // ZERO WIDTH NO-BREAK SPACE / BYTE ORDER MARK
		case 0xFFFE: // 0xFFEF in wrong byte-order
			return TRUE;
		default:
			return uni_isspace(ch);
		}
}

static BOOL
DOM_GetNextCommentLine(const uni_char *&comment, unsigned &comment_length, const uni_char *&source)
{
	while (*source)
	{
		while (*source && DOM_IsWhitespace(*source))
			++source;

		if (!*source)
			return FALSE;

		const uni_char *line = source;

		while (*source && *source != 10 && *source != 13)
			++source;

		if (line != source)
		{
			const uni_char *line_end = source;

			while (line_end != line && DOM_IsWhitespace(line_end[-1]))
				--line_end;

			if (line_end - line > 2)
				if (line[0] == '/' && line[1] == '/')
				{
					line += 2;

					while (line != line_end && DOM_IsWhitespace(*line))
						++line;

					comment = line;
					comment_length = line_end - line;

					return TRUE;
				}
		}
	}

	return FALSE;
}

static BOOL
DOM_IsCommentKeyword(const uni_char *&comment, unsigned &comment_length, const uni_char *keyword)
{
	unsigned length = uni_strlen(keyword);

	if (comment_length > length && uni_strncmp(comment, keyword, length) == 0 && DOM_IsWhitespace(comment[length]))
	{
		comment += length;
		comment_length -= length;

		while (comment_length && DOM_IsWhitespace(*comment))
		{
			++comment;
			--comment_length;
		}

		return comment_length != 0;
	}
	else
		return FALSE;
}

static void
DOM_InsertUserJSSourceIntoCache(DOM_UserJSSource *source)
{
	source->Into(&g_DOM_userScriptSources);
	DOM_UserJSSource::IncRef(source);
}

/* static */ OP_STATUS
DOM_UserJSScript::Make(DOM_UserJSScript *&script, ES_Program *&program, ES_Runtime *runtime, DOM_UserJSSource *source, DOM_UserJSManager::UserJSType type)
{
	const OpString &decoded = source->GetSource();

	if (!decoded.IsEmpty())
	{
		script = OP_NEW(DOM_UserJSScript, ());

		if (!script)
			return OpStatus::ERR_NO_MEMORY;

		OpAutoPtr<DOM_UserJSScript> script_anchor(script);

		ES_ProgramText program_text;
		program_text.program_text = decoded.CStr();
		program_text.program_text_length = decoded.Length();

		const uni_char *script_basename = uni_strrchr(source->GetFilename(), PATHSEPCHAR);
		if (script_basename)
			script->filename_url = g_url_api->GetURL(script_basename+1);

		ES_Runtime::CompileProgramOptions options;
		options.privilege_level = ES_Runtime::PRIV_LVL_USERJS;

#if defined(DOM_USER_JAVASCRIPT) && defined(OPERA_CONSOLE)
		if (g_opera->dom_module.missing_userjs_files.Find(static_cast<INT32>(source->GetFileHash())) != -1)
			options.report_error = FALSE;
		else
#endif // DOM_USER_JAVASCRIPT && OPERA_CONSOLE
			options.context = DOM_UserJSManager::GetErrorString(type, TRUE);

		options.script_url = script_basename ? &script->filename_url : NULL;
		options.script_type = DOM_UserJSManager::GetScriptType(type);
#ifdef ECMASCRIPT_DEBUGGER
		options.reformat_source = g_ecmaManager->GetWantReformatScript(runtime, program_text.program_text, program_text.program_text_length);
#endif // ECMASCRIPT_DEBUGGER

		OP_STATUS status = runtime->CompileProgram(&program_text, 1, &program, options);

		if (OpStatus::IsError(status))
		{
			source->Empty();
#if defined(DOM_USER_JAVASCRIPT) && defined(OPERA_CONSOLE)
			if (options.report_error)
				RETURN_IF_MEMORY_ERROR(g_opera->dom_module.missing_userjs_files.Add(source->GetFileHash()));
#endif // DOM_USER_JAVASCRIPT && OPERA_CONSOLE
			return status;
		}

		status = runtime->ExtractStaticProgram(script->static_program, program);

		if (OpStatus::IsError(status))
		{
			ES_Runtime::DeleteProgram(program);
			source->Empty();
			return status;
		}

		script_anchor.release();
		script->source = source;
		source->Empty(); // Done with the original source.

		DOM_UserJSSource::IncRef(source);

		return OpStatus::OK;
	}

	return OpStatus::ERR;

}

BOOL
DOM_UserJSScript::WouldUse(FramesDocument *frames_doc)
{
	return (source && source->WouldUse(frames_doc));
}

BOOL
DOM_UserJSScript::WouldUse(DOM_EnvironmentImpl *environment)
{
	return (source && source->WouldUse(environment));
}

OP_STATUS
DOM_UserJSScript::Use(DOM_EnvironmentImpl *environment, ES_Program *&program, ES_Thread *interrupt_thread, DOM_UserJSManager::UsedScript *usedscript)
{
	if (WouldUse(environment))
	{
		ES_Context *userjs_context;
		ES_Runtime *userjs_runtime;
		ES_ThreadScheduler *userjs_scheduler;

#ifdef EXTENSION_SUPPORT
		if (usedscript->execution_context)
		{
			userjs_runtime = usedscript->execution_context->GetESEnvironment()->GetRuntime();
			userjs_scheduler = usedscript->execution_context->GetESEnvironment()->GetScheduler();
		}
		else if (usedscript->script->GetType() != DOM_UserJSManager::EXTENSIONJS)
		{
			userjs_runtime = environment->GetRuntime();
			userjs_scheduler = environment->GetScheduler();
		}
		else
		{
			DOM_UserJSExecutionContext *userjs_context = environment->GetUserJSManager()->FindExecutionContext(usedscript->owner);

			if (!userjs_context)
				return OpStatus::ERR;

			/* By construction, only same-document UserJSs should share execution context (and DOM environment.) */
			userjs_context->IncRef();
			usedscript->execution_context = userjs_context;

			ES_Environment *userjs_environment = userjs_context->GetESEnvironment();
			userjs_runtime = userjs_environment->GetRuntime();
			userjs_scheduler = userjs_environment->GetScheduler();
		}

#else
		userjs_runtime = environment->GetRuntime();
		userjs_scheduler = environment->GetScheduler();
#endif // EXTENSION_SUPPORT

		if ((userjs_context = userjs_runtime->CreateContext(NULL)) != NULL)
		{
			DOM_UserJSManager *manager = environment->GetUserJSManager();
			DOM_UserJSThread *thread;

			if (!program)
			{
				OP_STATUS status = userjs_runtime->CreateProgramFromStatic(program, static_program);
				if (OpStatus::IsError(status))
				{
					ES_Runtime::DeleteContext(userjs_context);
					return OpStatus::ERR_NO_MEMORY;
				}
			}
			OP_STATUS status = userjs_runtime->PushProgram(userjs_context, program);

			ES_Runtime::DeleteProgram(program);
			program = NULL;

			if (OpStatus::IsError(status) || !(thread = OP_NEW(DOM_UserJSThread, (userjs_context, manager, this, source->GetType()))))
			{
				ES_Runtime::DeleteContext(userjs_context);
				return OpStatus::ERR_NO_MEMORY;
			}

			OP_BOOLEAN result = userjs_scheduler->AddRunnable(thread, interrupt_thread);
			RETURN_IF_ERROR(result);

			if (result == OpBoolean::IS_TRUE && source->GetType() != DOM_UserJSManager::GREASEMONKEY)
				manager->ScriptStarted();
		}
		else
			return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

/* static */ DOM_UserJSScript *
DOM_UserJSScript::IncRef(DOM_UserJSScript *script)
{
	if (script)
		++script->refcount;
	return script;
}

/* static */ void
DOM_UserJSScript::DecRef(DOM_UserJSScript *script)
{
	if (script && --script->refcount == 0)
		OP_DELETE(script);
}

DOM_UserJSSource::DOM_UserJSSource()
	: include(NULL),
	  exclude(NULL),
	  name(NULL),
	  ns(NULL),
	  description(NULL),
	  modified(0),
	  filename(NULL),
	  type(DOM_UserJSManager::NORMAL_USERJS),
	  refcount(0)
{
}

DOM_UserJSSource::~DOM_UserJSSource()
{
	OP_ASSERT(refcount == 0);

	Out();

	ResetComments();
	OP_DELETEA(filename);
}

void
DOM_UserJSSource::ResetComments()
{
	OP_DELETE(include);
	include = NULL;
	OP_DELETE(exclude);
	exclude = NULL;
	OP_DELETEA(name);
	name = NULL;
	OP_DELETEA(ns);
	ns = NULL;
	OP_DELETEA(description);
	description = NULL;
}

/* static */ void
DOM_UserJSSource::IncRef(DOM_UserJSSource *source)
{
	source->refcount++;
}

/* static */ void
DOM_UserJSSource::DecRef(DOM_UserJSSource *source)
{
	if (source && --source->refcount == 0)
		OP_DELETE(source);
}

/* static */ OP_STATUS
DOM_UserJSSource::LoadFile(OpFile &file, char *&source, OpFileLength &source_length, time_t &last_modified)
{
	RETURN_IF_ERROR(file.Open(OPFILE_READ | OPFILE_TEXT));

	RETURN_IF_ERROR(file.GetLastModified(last_modified));
	RETURN_IF_ERROR(file.GetFileLength(source_length));

	if (source_length > 0)
	{
		if (source_length >= INT_MAX)
			return OpStatus::ERR; // Too large file since we store it in memory.
		source = OP_NEWA(char, static_cast<int>(source_length) + 1);
		RETURN_OOM_IF_NULL(source);

		OpAutoArray<char> source_anchor(source);
		char *ptr = source;

		OpFileLength remaining_length = source_length;
		while (!file.Eof() && remaining_length != static_cast<OpFileLength>(0))
		{
			OpFileLength bytes_read;

			RETURN_IF_ERROR(file.Read(ptr, remaining_length, &bytes_read));

			ptr += bytes_read;
			remaining_length -= bytes_read;
		}

		/* The source might be in a multi-byte encoding, but in that
			case we won't depend on the null-termination in
			DOM_UserJSScript::Make. */
		*ptr = 0;
		source_anchor.release();
	}
	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_UserJSSource::Decode(char *source, OpFileLength source_length, OpString &decoded_output)
{
	const char *encoding = CharsetDetector::GetJSEncoding(source, static_cast<unsigned long>(source_length), TRUE/*UserJS*/);

	if (encoding)
	{
		InputConverter *converter;
		OP_STATUS valid = InputConverter::CreateCharConverter(encoding, &converter);
		if (OpStatus::IsSuccess(valid))
		{
			OpAutoPtr<InputConverter> anchor(converter);
			RETURN_IF_ERROR(SetFromEncoding(&decoded_output, converter, source, static_cast<int>(source_length), NULL));
		}
		else
			RETURN_IF_MEMORY_ERROR(valid);
	}

	if (decoded_output.IsEmpty())
		/* If attempt failed, fall back to UTF-8. */
		RETURN_IF_MEMORY_ERROR(decoded_output.SetFromUTF8(source, static_cast<int>(source_length)));

	return OpStatus::OK;
}

/* static */ OP_BOOLEAN
DOM_UserJSSource::Make(DOM_UserJSSource *&out, const uni_char* filename, DOM_UserJSManager::UserJSType type)
{
	OpFile file;
	RETURN_IF_ERROR(file.Construct(filename));
	return Make(out, file, type);
}

/* static */ OP_BOOLEAN
DOM_UserJSSource::Make(DOM_UserJSSource *&out, OpFile &file, DOM_UserJSManager::UserJSType type)
{
	OP_PROBE3(OP_PROBE_DOM_USERJSSOURCE_MAKE);
	char *source = NULL;
	OpFileLength length;
	time_t modified;

	OP_STATUS status = LoadFile(file, source, length, modified);
	// Delete source unconditionally.
	OpAutoArray<char> source_deleter(source);

	if (OpStatus::IsMemoryError(status))
		return status;
	else if (OpStatus::IsSuccess(status))
	{
		if (length > 0)
		{
			OpAutoPtr<DOM_UserJSSource> dom_source_auto(OP_NEW(DOM_UserJSSource, ()));
			RETURN_OOM_IF_NULL(dom_source_auto.get());

			dom_source_auto->filename = UniSetNewStr(file.GetFullPath());
			RETURN_OOM_IF_NULL(dom_source_auto->filename);

			dom_source_auto->type = type;
			dom_source_auto->modified = modified;

			RETURN_IF_ERROR(Decode(source, length, dom_source_auto->decoded));
			RETURN_IF_ERROR(dom_source_auto->ProcessCommentIfPresent());

			out = dom_source_auto.release();
			return OpBoolean::IS_TRUE;
		}
	}

	return OpBoolean::IS_FALSE;
}

OP_BOOLEAN
DOM_UserJSSource::Reload(OpFile &file)
{
	char *source = NULL;
	OpFileLength length;
	time_t last_modified;
	OP_STATUS status = LoadFile(file, source, length, last_modified);

	// Delete source unconditionally.
	OpAutoArray<char> source_deleter(source);

	if (OpStatus::IsMemoryError(status))
		return status;
	else if (OpStatus::IsSuccess(status))
	{
		modified = last_modified;
		if (length > 0)
		{
			RETURN_IF_ERROR(Decode(source, length, decoded));
			ResetComments();
			RETURN_IF_ERROR(ProcessCommentIfPresent());
			return OpBoolean::IS_TRUE;
		}
	}
	return OpBoolean::IS_FALSE;
}

BOOL
DOM_UserJSSource::WouldUse(DOM_EnvironmentImpl *environment)
{
	return WouldUse(environment->GetFramesDocument());
}

BOOL
DOM_UserJSSource::WouldUse(FramesDocument *frames_doc)
{
	OP_PROBE3(OP_PROBE_DOM_USERJSSCRIPT_WOULDUSE);
	if (!exclude && !include)
		return TRUE;

	URL url = frames_doc->GetURL();
	const uni_char *uri = url.GetAttribute(URL::KUniName).CStr();
	OpString lowercase_uri;
	OP_STATUS status = lowercase_uri.Set(uri);
	if (OpStatus::IsError(status))
	{
		if (OpStatus::IsMemoryError(status))
			g_memory_manager->RaiseCondition(status);
		return FALSE;
	}

	lowercase_uri.MakeLower();

	if (!exclude || !exclude->Match(lowercase_uri.CStr()))
		if (!include || include->Match(lowercase_uri.CStr()))
			return TRUE;

	return FALSE;
}

OP_BOOLEAN
DOM_UserJSSource::IsStale() const
{
	OpFile file;

	RETURN_IF_ERROR(file.Construct(filename, OPFILE_ABSOLUTE_FOLDER));

	BOOL exists;

	RETURN_IF_ERROR(file.Exists(exists));

	if (!exists)
		return OpBoolean::IS_TRUE;

	time_t new_modified;

	RETURN_IF_ERROR(file.GetLastModified(new_modified));

	if (new_modified != modified)
		return OpBoolean::IS_TRUE;

	return OpBoolean::IS_FALSE;
}

unsigned
DOM_UserJSSource::GetFileHash() const
{
	return static_cast<unsigned>(OpGenericStringHashTable::HashString(GetFilename(), TRUE))
	     + static_cast<unsigned>(GetModified() % UINT_MAX)
	     + static_cast<unsigned>(GetModified() / UINT_MAX);
}

OP_STATUS
DOM_UserJSSource::ProcessComment(const uni_char *source)
{
	const uni_char *comment;
	unsigned comment_length;
	BOOL in_userscript = FALSE;

	while (DOM_GetNextCommentLine(comment, comment_length, source))
	{
		if (!in_userscript)
		{
			if (comment_length == 14 && uni_strncmp(comment, UNI_L("==UserScript=="), 14) == 0)
				in_userscript = TRUE;
		}
		else
		{
			if (comment_length == 15 && uni_strncmp(comment, UNI_L("==/UserScript=="), 15) == 0)
				break;
			else if (DOM_IsCommentKeyword(comment, comment_length, UNI_L("@name")))
			{
				if (!name)
					RETURN_OOM_IF_NULL(name = UniSetNewStrN(comment, comment_length));
			}
			else if (DOM_IsCommentKeyword(comment, comment_length, UNI_L("@namespace")))
			{
				if (!ns)
					RETURN_OOM_IF_NULL(ns = UniSetNewStrN(comment, comment_length));
			}
			else if (DOM_IsCommentKeyword(comment, comment_length, UNI_L("@description")))
			{
				if (!description)
					RETURN_OOM_IF_NULL(description = UniSetNewStrN(comment, comment_length));
			}
			else if (DOM_IsCommentKeyword(comment, comment_length, UNI_L("@include")))
				RETURN_IF_ERROR(DOM_UserJSRule::Make(include, comment, comment_length));
			else if (DOM_IsCommentKeyword(comment, comment_length, UNI_L("@exclude")))
				RETURN_IF_ERROR(DOM_UserJSRule::Make(exclude, comment, comment_length));
		}
	}

	return OpStatus::OK;
}

OP_STATUS
DOM_UserJSSource::ProcessCommentIfPresent()
{
	int start = decoded.Find("==UserScript==");
	if (start != KNotFound)
	{
		int end = decoded.Find("==/UserScript==");
		if (end != KNotFound)
		{
			const uni_char *str = decoded.CStr();
			while (start > 0 && str[start - 1] != 0x0a && str[start - 1] != 0x0d)
				--start;
			while (str[end] && str[end] != 0x0a && str[end] != 0x0d)
				++end;

			OpString comment;
			RETURN_IF_ERROR(comment.Set(str + start, end - start));

			return ProcessComment(comment.CStr());
		}
	}

	return OpStatus::OK;
}

#ifdef EXTENSION_SUPPORT
BOOL
DOM_UserJSSource::CheckIfAlwaysEnabled()
{
	if (exclude)
		return FALSE;

	DOM_UserJSRule *rule = include;
	BOOL matches_all = rule == NULL;
	while (rule && !matches_all)
	{
		matches_all = rule->matches_all;
		rule = rule->next;
	}

	return matches_all;
}
#endif // EXTENSION_SUPPORT

#else // USER_JAVASCRIPT_ADVANCED

class DOM_UserJSCallback
	: public ES_AsyncCallback
{
protected:
	DOM_UserJSManager *manager;

public:
	DOM_UserJSCallback(DOM_UserJSManager *manager)
		: manager(manager)
	{
	}

	OP_STATUS HandleCallback(ES_AsyncOperation operation, ES_AsyncStatus status, const ES_Value &result)
	{
		manager->ScriptFinished();
		return OpStatus::OK;
	}
};

#endif // USER_JAVASCRIPT_ADVANCED

#ifdef DOM3_EVENTS

OP_BOOLEAN
DOM_UserJSManager::SendEventEvent(DOM_EventType type, const uni_char *namespaceURI, const uni_char *type_string, DOM_Event *real_event, ES_Object *listener, ES_Thread *interrupt_thread)
{
	if (event_target->HasListeners(type, namespaceURI, type_string, ES_PHASE_AT_TARGET))
	{
		DOM_UserJSEvent *event;
		RETURN_IF_ERROR(DOM_UserJSEvent::Make(event, this, real_event, type_string, listener));

		return environment->SendEvent(event, interrupt_thread);
	}
	else
		return OpBoolean::IS_FALSE;
}

#else // DOM3_EVENTS

OP_BOOLEAN
DOM_UserJSManager::SendEventEvent(DOM_EventType type, const uni_char *type_string, DOM_Event *real_event, ES_Object *listener, ES_Thread *interrupt_thread)
{
	if (event_target->HasListeners(type, type_string, ES_PHASE_AT_TARGET))
	{
		DOM_UserJSEvent *event;
		RETURN_IF_ERROR(DOM_UserJSEvent::Make(event, this, real_event, type_string, listener));

		return environment->SendEvent(event, interrupt_thread);
	}
	else
		return OpBoolean::IS_FALSE;
}

#endif // DOM3_EVENTS

/* virtual */ DOM_Object *
DOM_UserJSManager::GetOwnerObject()
{
	OP_ASSERT(FALSE);
	return NULL;
}

class DOM_UserJSAnchor
	: public DOM_Object
{
protected:
	DOM_UserJSManager *manager;

public:
	DOM_UserJSAnchor(DOM_UserJSManager *manager)
		: manager(manager)
	{
	}

	virtual void GCTrace()
	{
		manager->GetEventTarget()->GCTrace();
	}
};

DOM_UserJSManager::DOM_UserJSManager(DOM_EnvironmentImpl *environment, BOOL user_js_enabled, BOOL is_https)
	: environment(environment),
#ifndef USER_JAVASCRIPT_ADVANCED
	  callback(NULL),
#endif // USER_JAVASCRIPT_ADVANCED
	  anchor(NULL),
	  is_enabled(FALSE),
	  only_browserjs(!user_js_enabled),
	  is_https(is_https),
	  queued_run_greasemonkey(FALSE),
	  is_active(0),
	  handled_external_script(NULL),
	  handled_script(NULL),
	  handled_stylesheet(NULL)
{
}

#ifdef EXTENSION_SUPPORT

DOM_UserJSExecutionContext::~DOM_UserJSExecutionContext()
{
	OP_ASSERT(ref_count == 0);
	Out();

	if (es_environment)
	{
		/* A non-standalone ES_Environment, hence shutdown is manual. */
		OP_DELETE(es_environment->GetAsyncInterface());

		ES_ThreadScheduler *scheduler = es_environment->GetScheduler();
		if (scheduler)
		{
			scheduler->RemoveThreads(TRUE, TRUE);
			OP_ASSERT(!scheduler->IsActive());
			OP_DELETE(scheduler);
		}

		if (DOM_Runtime *runtime = static_cast<DOM_Runtime *>(es_environment->GetRuntime()))
		{
			runtime->SetESScheduler(NULL);
			runtime->SetESAsyncInterface(NULL);
			runtime->Detach();
		}

		ES_Environment::Destroy(es_environment);
	}
}

/* static */OP_STATUS
DOM_UserJSExecutionContext::Make(DOM_UserJSExecutionContext *&userjs_context, DOM_UserJSManager *manager, DOM_EnvironmentImpl *environment, OpGadget *owner)
{
	userjs_context = OP_NEW(DOM_UserJSExecutionContext, (owner));
	if (!userjs_context)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = OpStatus::OK;
	DOM_Runtime *dom_runtime = NULL;
	ES_ThreadScheduler *scheduler = NULL;
	ES_AsyncInterface *asyncif = NULL;
	FramesDocument *frames_doc = environment->GetFramesDocument();

	if (!(dom_runtime = OP_NEW(DOM_Runtime, ())))
		goto return_on_error;

	if (OpStatus::IsError(status = dom_runtime->Construct(DOM_Runtime::TYPE_EXTENSION_JS, environment, "Window", frames_doc->GetMutableOrigin(), FALSE, environment->GetRuntime())))
		goto return_on_error;

	if (!(scheduler = ES_ThreadScheduler::Make(dom_runtime, FALSE, TRUE)))
		goto oom_error;

	if (!(asyncif = OP_NEW(ES_AsyncInterface, (dom_runtime, scheduler))))
		goto oom_error;

	if (OpStatus::IsError(status = ES_Environment::Create(userjs_context->es_environment, dom_runtime, scheduler, asyncif, FALSE, frames_doc ? &frames_doc->GetURL() : NULL)))
		goto return_on_error;

	dom_runtime->SetESAsyncInterface(asyncif);
	dom_runtime->SetESScheduler(scheduler);

	RETURN_IF_ERROR(environment->GetRuntime()->MergeHeapWith(dom_runtime));
	return DOM_ExtensionScope::Initialize(userjs_context->es_environment, owner);

oom_error:
	status = OpStatus::ERR_NO_MEMORY;

return_on_error:
	OP_DELETE(scheduler);
	OP_DELETE(asyncif);

	if (dom_runtime)
		dom_runtime->Detach();

	userjs_context->DecRef();
	return status;
}

void
DOM_UserJSExecutionContext::BeforeDestroy()
{
	if (DOM_ExtensionScope *global_scope = DOM_ExtensionScope::GetGlobalScope(es_environment))
		global_scope->BeforeDestroy();
}

#endif // EXTENSION_SUPPORT

DOM_UserJSManager::~DOM_UserJSManager()
{
#ifndef USER_JAVASCRIPT_ADVANCED
	OP_DELETE(callback);
#endif // USER_JAVASCRIPT_ADVANCED

	while (UsedScript *usedscript = (UsedScript *) usedscripts.First())
	{
		usedscript->Out();

		DOM_UserJSScript::DecRef(usedscript->script);
		ES_Runtime::DeleteProgram(usedscript->program);

		OP_DELETE(usedscript);
	}

#ifdef EXTENSION_SUPPORT
	UnreferenceExecutionContexts();
	OP_ASSERT(userjs_execution_contexts.Empty());
#endif // EXTENSION_SUPPORT

	if (anchor)
		anchor->GetRuntime()->Unprotect(*anchor);

	DOM_UserJSManagerLink::Out();
}

#ifdef EXTENSION_SUPPORT

DOM_UserJSExecutionContext*
DOM_UserJSManager::FindExecutionContext(OpGadget *owner)
{
	for(DOM_UserJSExecutionContext *c = userjs_execution_contexts.First(); c; c = c->Suc())
		if (c->GetOwner() == owner)
			return c;

	return NULL;
}

OpGadget*
DOM_UserJSManager::FindRuntimeGadget(DOM_Runtime *runtime)
{
	for (DOM_UserJSExecutionContext *c = userjs_execution_contexts.First(); c; c = c->Suc())
		if (c->GetESEnvironment()->GetRuntime() == runtime)
			return c->GetOwner();

	return NULL;
}

void
DOM_UserJSManager::UnreferenceExecutionContexts()
{
	DOM_UserJSExecutionContext *c = userjs_execution_contexts.First();

	while (c)
	{
		DOM_UserJSExecutionContext *d = c;
		c = c->Suc();
		d->DecRef();
	}
}

#endif // EXTENSION_SUPPORT

#ifdef USER_JAVASCRIPT_ADVANCED

OP_STATUS
#ifdef EXTENSION_SUPPORT
DOM_UserJSManager::UseScript(DOM_UserJSScript *script, ES_Program *program, OpGadget *owner)
#else
DOM_UserJSManager::UseScript(DOM_UserJSScript *script, ES_Program *program)
#endif // EXTENSION_SUPPORT
{
	UsedScript *usedscript = OP_NEW(UsedScript, ());

	if (!usedscript)
	{
		ES_Runtime::DeleteProgram(program);
		return OpStatus::ERR_NO_MEMORY;
	}

	usedscript->script = DOM_UserJSScript::IncRef(script);
	usedscript->program = program;
#ifdef EXTENSION_SUPPORT
	usedscript->owner = owner;
#endif // EXTENSION_SUPPORT

	/* Insert script sorted in usedscripts. */
	Link *existing = usedscripts.First();

	while (existing)
	{
		const uni_char *existing_file_name = static_cast<UsedScript *>(existing)->script->GetFilename();

		int cmp = uni_stricmp(script->GetFilename(), existing_file_name);

		if (cmp > 0 || cmp == 0 && uni_strcmp(script->GetFilename(), existing_file_name) > 0)
			existing = existing->Suc();
		else
			break;
	}

	if (existing)
		usedscript->Precede(existing);
	else
		usedscript->Into(&usedscripts);

	return OpStatus::OK;
}

OP_BOOLEAN
#ifdef EXTENSION_SUPPORT
DOM_UserJSManager::LoadFile(OpFile &file, UserJSType type, BOOL cache, OpGadget *owner)
#else
DOM_UserJSManager::LoadFile(OpFile &file, UserJSType type, BOOL cache)
#endif
{
	OP_PROBE3(OP_PROBE_DOM_USERJSMANAGER_LOADFILE);
	DOM_UserJSScript *script = cache ? GetCachedScript(file.GetFullPath(), type) : NULL;
	ES_Program *program = NULL;

#ifdef EXTENSION_SUPPORT
	ES_Runtime* es_extension_runtime = NULL;

	if (type == EXTENSIONJS && owner)
	{
		DOM_UserJSExecutionContext *userjs_context = FindExecutionContext(owner);

		if (!userjs_context)
		{
			RETURN_IF_ERROR(DOM_UserJSExecutionContext::Make(userjs_context, this, environment, owner));
			userjs_context->Into(&userjs_execution_contexts);
		}

		es_extension_runtime = userjs_context->GetESEnvironment()->GetRuntime();
	}
#endif // EXTENSION_SUPPORT

	if (!script)
	{
		DOM_UserJSSource *dom_source = GetLoadedSource(file.GetFullPath(), type);
		if (!dom_source)
		{
			OP_BOOLEAN loaded = DOM_UserJSSource::Make(dom_source, file, type);
			if (loaded != OpBoolean::IS_TRUE)
				return loaded;

#ifdef EXTENSION_SUPPORT
			if (dom_source->GetType() == EXTENSIONJS && dom_source->CheckIfAlwaysEnabled())
				g_DOM_have_always_enabled_extension_js = TRUE;
#endif // EXTENSION_SUPPORT

			DOM_InsertUserJSSourceIntoCache(dom_source);
		}
		else if (dom_source->IsEmpty())
		{
			OP_BOOLEAN loaded = dom_source->Reload(file);
			if (loaded != OpBoolean::IS_TRUE)
				return loaded;
		}

		ES_Runtime *runtime = environment->GetRuntime();

#ifdef EXTENSION_SUPPORT
		if (type == EXTENSIONJS && es_extension_runtime)
			runtime = es_extension_runtime;
#endif // EXTENSION_SUPPORT

		// When caching is disabled, only compile the script if it
		// would be used. If caching is enabled, however, compile it
		// now, so it can fetched from cache later.
		if (!cache && !dom_source->WouldUse(environment))
			return OpStatus::OK;

		OP_STATUS status = DOM_UserJSScript::Make(script, program, runtime, dom_source, type);

		if (OpStatus::IsMemoryError(status))
			return status;
		else if (OpStatus::IsError(status))
			script = NULL;
		else if (cache)
			script->Into(&g_DOM_userScripts);
	}

	if (script)
	{
#ifdef EXTENSION_SUPPORT
		OP_STATUS status = UseScript(script, program, owner);
#else
		OP_STATUS status = UseScript(script, program);
#endif // EXTENSION_SUPPORT

		// If caching is disabled, the reference held by usedscripts
		// should be the only reference. Or, if UseScript failed, then
		// this will delete the script.
		if (!cache)
			DOM_UserJSScript::DecRef(script);

		if (OpStatus::IsError(status))
			return status;
	}

	return OpBoolean::IS_TRUE;
}

#ifdef DOM_BROWSERJS_SUPPORT

/* Also used from dom/src/domenvironmentimpl.cpp. */
OP_BOOLEAN
DOM_CheckBrowserJSSignature(const OpFile &file)
{
	OpString resolved;
	OpString path;
	OP_MEMORY_VAR BOOL successful;

	RETURN_IF_ERROR(path.Append("file:"));
	RETURN_IF_ERROR(path.Append(file.GetFullPath()));
	RETURN_IF_LEAVE(successful = g_url_api->ResolveUrlNameL(path, resolved));


	if (!successful || resolved.Find("file://") != 0)
		return OpStatus::ERR;

	URL url = g_url_api->GetURL(resolved.CStr());

	if (!url.QuickLoad(TRUE))
		return OpStatus::ERR;

#ifdef CRYPTO_VERIFY_SIGNED_TEXTFILE_SUPPORT
	if (!CryptoVerifySignedTextFile(url, "//", DOM_BROWSERJS_KEY, sizeof DOM_BROWSERJS_KEY))
#else
	if (!VerifySignedTextFile(url, "//", DOM_BROWSERJS_KEY, sizeof DOM_BROWSERJS_KEY))
#endif // CRYPTO_VERIFY_SIGNED_TEXTFILE_SUPPORT
		return OpBoolean::IS_FALSE;
	else
		return OpBoolean::IS_TRUE;
}

static OP_STATUS
DOM_CheckBrowserJSSignatureDelux(const OpFile &file)
{
	OP_BOOLEAN result = DOM_CheckBrowserJSSignature(file);

	if (result == OpBoolean::IS_FALSE)
	{
#ifdef PREFS_WRITE
		RETURN_IF_LEAVE(OpStatus::Ignore(g_pcjs->WriteIntegerL(PrefsCollectionJS::BrowserJSSetting, 1)));
#endif // PREFS_WRITE
		return OpStatus::ERR;
	}
	else if (result == OpBoolean::IS_TRUE)
		return OpStatus::OK;
	else
		return result;
}

#endif // DOM_BROWSERJS_SUPPORT

#ifdef DOM_USER_JAVASCRIPT
static OP_STATUS WarnLoadFailure(const uni_char *filename, DOM_UserJSManager::UserJSType type)
{
#ifdef OPERA_CONSOLE
	/* A hash is used as a signature for the filename; collisions a possibility. Effect would be
	   that the new entry is not reported as failing. Problems surrounding overly large sets of
	   errant filenames not considered very likely, so no mitigations. */
	INT32 hashed_filename = static_cast<INT32>(OpGenericStringHashTable::HashString(filename, TRUE));
	if (g_opera->dom_module.missing_userjs_files.Find(hashed_filename) == -1)
	{
		RETURN_IF_MEMORY_ERROR(g_opera->dom_module.missing_userjs_files.Add(hashed_filename));

		const uni_char *provenance = DOM_UserJSManager::GetErrorString(type, FALSE/*not compiling => loading*/);

		ES_ErrorInfo error_info(provenance);
		uni_snprintf(error_info.error_text, ARRAY_SIZE(error_info.error_text), UNI_L("Failed to load path/file: %s\n"), filename);

		/* URL: in this context isn't meaningful, leave as empty. */
		RETURN_IF_ERROR(DOM_EnvironmentImpl::PostError(NULL, error_info, provenance, NULL));
	}
#endif // OPERA_CONSOLE
	return OpStatus::OK;
}
#endif // DOM_USER_JAVASCRIPT

#ifdef DOM_BROWSERJS_SUPPORT
static OP_STATUS GetBrowserJSPath(OpString &result)
{
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_HOME_FOLDER, result));
	return result.Append(UNI_L("browser.js"));
}
#endif // DOM_BROWSERJS_SUPPORT

OP_STATUS
DOM_UserJSManager::LoadScripts()
{
	OP_PROBE3(OP_PROBE_DOM_USERJSMANAGER_LOADSCRIPTS);
	BOOL cache = TRUE;
#ifdef ECMASCRIPT_DEBUGGER
	// We want to disable cache if debugging is enabled for this ES_Runtime. This ensures
	// that the debugger always received the NewScript event on the correct ES_Runtime.
	cache = !g_ecmaManager->IsDebugging(environment->GetRuntime());
#endif // ECMASCRIPT_DEBUGGER

#ifdef DOM_BROWSERJS_SUPPORT
	if (g_pcjs->GetIntegerPref(PrefsCollectionJS::BrowserJSSetting) == 2)
	{
		if (DOM_UserJSScript *browserjs = cache ? GetCachedScript(NULL, BROWSERJS) : NULL)
			RETURN_IF_ERROR(UseScript(browserjs, NULL));
		else
		{
			OpFile file;
			OP_STATUS status;
			OpString browserjs_path;

			RETURN_IF_ERROR(GetBrowserJSPath(browserjs_path));
			if (OpStatus::IsSuccess(status = file.Construct(browserjs_path.CStr(), OPFILE_ABSOLUTE_FOLDER)))
			{
				BOOL exists = FALSE;

				RETURN_IF_ERROR(file.Exists(exists));

				if (exists)
				{
					status = DOM_CheckBrowserJSSignatureDelux(file);

					if (OpStatus::IsMemoryError(status))
						return status;
					else if (OpStatus::IsSuccess(status))
						status = LoadFile(file, BROWSERJS, cache);
				}
#ifdef PREFS_WRITE
				else
					TRAP_AND_RETURN(status, OpStatus::Ignore(g_pcjs->WriteIntegerL(PrefsCollectionJS::BrowserJSSetting, 0)));
#endif // PREFS_WRITE
			}

			if (OpStatus::IsMemoryError(status))
				return status;
		}
	}
#endif // DOM_BROWSERJS_SUPPORT

#ifdef DOM_USER_JAVASCRIPT

	const uni_char *filenames = g_pcjs->GetStringPref(PrefsCollectionJS::UserJSFiles, DOM_EnvironmentImpl::GetHostName(environment->GetFramesDocument())).CStr();

# if defined REVIEW_USER_SCRIPTS
	OpString reviewed_filenames;
	Window *window = environment->GetFramesDocument()->GetWindow();
	if (window)
	{
		WindowCommander *wc = window->GetWindowCommander();
		if(wc->GetDocumentListener()->OnReviewUserScripts(wc, environment->GetFramesDocument()->GetHostName(), filenames, reviewed_filenames))
			filenames = reviewed_filenames.DataPtr();// or CStr
	}
# endif // REVIEW_USER_SCRIPTS

	if (filenames)
		while (*filenames)
		{
			while (*filenames == ' ' || *filenames == ',')
				++filenames;

			const uni_char *filename = filenames, *options = NULL;
			while (*filenames && *filenames != ',')
			{
				if (!options && *filenames == ';')
					options = filenames;
				++filenames;
			}

			if (filename != filenames)
			{
				const uni_char *filename_end = options;
				BOOL opera = FALSE, greasemonkey = FALSE;

				if (options)
					while (TRUE)
					{
						const uni_char *option_end = options + 1;

						while (*option_end && *option_end != ';' && *option_end != ',' && *option_end != ' ')
							++option_end;

						unsigned option_length = option_end - options;

						if (option_length == 6 && uni_strncmp(options, UNI_L(";opera"), 6) == 0)
							opera = TRUE;
						else if (option_length == 13 && uni_strncmp(options, UNI_L(";greasemonkey"), 13) == 0)
							greasemonkey = TRUE;

						if (!*option_end || *option_end == ',')
							break;
						else
							options = option_end;
					}
				else
					filename_end = filenames;

				while (filename_end > filename && *(filename_end-1) == ' ')
					--filename_end;

				TempBuffer buffer;
				RETURN_IF_ERROR(buffer.Append(filename, filename_end - filename));
				filename = buffer.GetStorage();

				OpFile file;
				OP_STATUS status;
				/* For constructing the file, we use here OPFILE_SERIALIZED_FOLDER. It was not used before,
				   because the INI parser was also expanding environment variables, and CORE-36151 changed it.
				   If the user js path contains any environment variables, these will have to be expanded
				   when constructing the file. Using OPFILE_SERIALIZED_FOLDER will allow the platform
				   implementation of OpLowLevelFile to expand environment variables, while previously used
				   OPFILE_ABSOLUTE_FOLDER would cause treating the path as a proper path without any more
				   work needed, and the error log would contain then entries like:

					User JS loading
					Failed to load path/file: $OPERA_HOME/userjs/

				   It looks like expanding environment varialbes in other prefs is still working well
				   (eg. User Prefs|Console Error Log=$OPERA_HOME/error.log will cause saving the log
				   in the proper place) so the change is needed only in places, where User Prefs|User JavaScript File
				   pref value is used. */
				if (OpStatus::IsError(status = file.Construct(filename, OPFILE_SERIALIZED_FOLDER)))
					if (OpStatus::IsMemoryError(status))
						return status;
					else
					{
						RETURN_IF_MEMORY_ERROR(WarnLoadFailure(filename, greasemonkey ? GREASEMONKEY : NORMAL_USERJS));
						continue;
					}

				BOOL exists = FALSE;
				if (OpStatus::IsError(status = file.Exists(exists)))
					if (OpStatus::IsMemoryError(status))
						return status;
					else
					{
						RETURN_IF_MEMORY_ERROR(WarnLoadFailure(filename, greasemonkey ? GREASEMONKEY : NORMAL_USERJS));
						continue;
					}

				OP_BOOLEAN result = OpBoolean::IS_FALSE;
				OpFileInfo::Mode mode;
				if (!exists)
				{
					RETURN_IF_MEMORY_ERROR(WarnLoadFailure(filename, greasemonkey ? GREASEMONKEY : NORMAL_USERJS));
					continue;
				}
				else
				{
					RETURN_IF_ERROR(file.GetMode(mode));

					if (filename[buffer.Length() - 1] != PATHSEPCHAR && mode != OpFileInfo::DIRECTORY)
					{
						BOOL use_greasemonkey = greasemonkey;

						if (!opera && !greasemonkey)
						{
							unsigned filename_length = uni_strlen(filename);

							if (filename_length > 8 && uni_str_eq(filename + filename_length - 8, ".user.js"))
								use_greasemonkey = TRUE;
						}

						RETURN_IF_MEMORY_ERROR(result = LoadFile(file, use_greasemonkey ? GREASEMONKEY : NORMAL_USERJS, cache));
						/* Report warning, but continue. */
						if (OpStatus::IsError(result))
						{
							RETURN_IF_MEMORY_ERROR(WarnLoadFailure(filename, use_greasemonkey ? GREASEMONKEY : NORMAL_USERJS));
							continue;
						}
					}
				}

				/* Process the directory, but ignoring problematic files. */
				if (result == OpBoolean::IS_FALSE && mode != OpFileInfo::FILE)
				{
					OpFolderLister *lister = OpFile::GetFolderLister(OPFILE_ABSOLUTE_FOLDER, UNI_L("*.js"), file.GetFullPath());
					if (!lister)
						return OpStatus::ERR_NO_MEMORY;

					OP_STATUS status = OpStatus::OK;

					while (lister->Next())
					{
						if ((filename = lister->GetFullPath()) == NULL)
							continue;

						BOOL use_greasemonkey = greasemonkey;

						if (!opera && !greasemonkey)
						{
							unsigned filename_length = uni_strlen(filename);

							if (filename_length > 8 && uni_str_eq(filename + filename_length - 8, ".user.js"))
								use_greasemonkey = TRUE;
						}

						OpFile file;
						if (OpStatus::IsMemoryError(status = file.Construct(filename)) ||
						    OpStatus::IsMemoryError(status = LoadFile(file, use_greasemonkey ? GREASEMONKEY : NORMAL_USERJS, cache)))
						{
							OP_DELETE(lister);
							return status;
						}

						/* An error, but report it as a warning and continue. */
						if (OpStatus::IsError(status) && OpStatus::IsMemoryError(WarnLoadFailure(filename, use_greasemonkey ? GREASEMONKEY : NORMAL_USERJS)))
						{
							OP_DELETE(lister);
							return OpStatus::ERR_NO_MEMORY;
						}
					}

					OP_DELETE(lister);
				}
			}
		}

#endif // DOM_USER_JAVASCRIPT

#ifdef EXTENSION_SUPPORT
	// Add any files defined by active extensions
	if (AllowExtensionJS(environment, NULL))
		for (OpGadgetExtensionUserJS *extension_userjs = g_gadget_manager->GetFirstActiveExtensionUserJS();
			 extension_userjs; extension_userjs = extension_userjs->Suc())
		{
			OpFile userjs_file;
			RETURN_IF_ERROR(userjs_file.Construct(extension_userjs->userjs_filename, OPFILE_ABSOLUTE_FOLDER, OPFILE_FLAGS_ASSUME_ZIP));
			BOOL exists;
			RETURN_IF_ERROR(userjs_file.Exists(exists));
			if (exists)
			{
				OP_ASSERT(extension_userjs->owner);
				OP_BOOLEAN result;
				RETURN_IF_ERROR(result = AllowExtensionJS(environment, extension_userjs->owner));
				if (result == OpBoolean::IS_TRUE)
					RETURN_IF_ERROR(LoadFile(userjs_file, EXTENSIONJS, cache, extension_userjs->owner));
			}
		}
#endif // EXTENSION_SUPPORT

	return OpStatus::OK;
}

/* static */ DOM_UserJSScript *
DOM_UserJSManager::GetCachedScript(const uni_char *filename, DOM_UserJSManager::UserJSType type)
{
	for (DOM_UserJSScript *script = g_DOM_userScripts.First(); script; script = script->Suc())
	{
		if (type == BROWSERJS)
		{
			if (type == script->GetType())
				return script;
		}
		else if (uni_str_eq(filename, script->GetFilename()))
		{
			OP_ASSERT(script->GetType() == type);
			return script;
		}
	}

	return NULL;
}

/* static */ DOM_UserJSSource *
DOM_UserJSManager::GetLoadedSource(const uni_char *filename, DOM_UserJSManager::UserJSType type)
{
	for (DOM_UserJSSource *source = g_DOM_userScriptSources.First(); source; source = source->Suc())
	{
		if (type == BROWSERJS && type == source->GetType())
			return source;
		else if (uni_str_eq(filename, source->GetFilename()))
		{
			OP_ASSERT(source->GetType() == type);
			return source;
		}
	}

	return NULL;
}

OP_STATUS
DOM_UserJSManager::Construct()
{
	DOM_Object *object;
	RETURN_IF_ERROR(GetObject(object));

	event_target = OP_NEW(DOM_EventTarget, (object));
	if (!event_target)
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsMemoryError(DOM_Object::DOMSetObjectRuntime(anchor = OP_NEW(DOM_UserJSAnchor, (this)), environment->GetDOMRuntime())) ||
	    !anchor->GetRuntime()->Protect(*anchor))
	{
		anchor = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}

	OP_STATUS status = OpStatus::OK;
#ifdef EXTENSION_SUPPORT
	/* Create the support object for an extension background (NULL if this
	 * is no such thing) + assign ownership to the environment. */
	DOM_ExtensionBackground *extension_background = NULL;
	if (FramesDocument *frames_doc = GetEnvironment()->GetFramesDocument())
	{
		status = g_extension_manager->AddExtensionContext(frames_doc->GetWindow(), GetEnvironment()->GetDOMRuntime(), &extension_background);
		if (OpStatus::IsSuccess(status) && extension_background)
			GetEnvironment()->SetExtensionBackground(extension_background);
	}
#endif // EXTENSION_SUPPORT

	if (OpStatus::IsSuccess(status) && OpStatus::IsSuccess(status = ReloadScripts()))
	{
		is_enabled = TRUE;
		status = RunScripts(FALSE);
	}

	if (OpStatus::IsError(status))
	{
#ifdef EXTENSION_SUPPORT
		/* If we don't have an extension background then the manager we were trying
		 * to construct here was for a separate extension-associated window, so
		 * remove the extension context from the environment instead of effectively
		 * shutting down the extension as RemoveExtensionContext(DOM_ExtensionSupport *) would do.
		 *
		 * Refer to the DOM_ExtensionManager documentation for more details. */
		if (extension_background)
			g_extension_manager->RemoveExtensionContext(extension_background->GetExtensionSupport());
		else
			g_extension_manager->RemoveExtensionContext(GetEnvironment());

		GetEnvironment()->SetExtensionBackground(NULL);
#endif // EXTENSION_SUPPORT
		BeforeDestroy();
	}
	return status;
}

void
DOM_UserJSManager::BeforeDestroy()
{
#ifdef EXTENSION_SUPPORT
	for(DOM_UserJSExecutionContext *c = userjs_execution_contexts.First(); c; c = c->Suc())
		c->BeforeDestroy();
#endif // EXTENSION_SUPPORT
}

#ifdef EXTENSION_SUPPORT
OP_STATUS
DOM_UserJSManager::GetExtensionESRuntimes(OpVector<ES_Runtime> &es_runtimes)
{
	DOM_UserJSExecutionContext *c = userjs_execution_contexts.First();

	while (c)
	{
		if (c->GetESEnvironment())
			RETURN_IF_ERROR(es_runtimes.Add(c->GetESEnvironment()->GetRuntime()));
		c = c->Suc();
	}

	return OpStatus::OK;
}

/* static */ OP_BOOLEAN
DOM_UserJSManager::AllowExtensionJS(DOM_EnvironmentImpl *environment, OpGadget *owner)
{
	FramesDocument *frames_doc = environment->GetFramesDocument();

	BOOL allowed = FALSE;
	RETURN_IF_ERROR(OpSecurityManager::CheckSecurity(OpSecurityManager::DOM_EXTENSION_ALLOW, OpSecurityContext(frames_doc), OpSecurityContext(frames_doc->GetURL(), owner), allowed));
	return (allowed ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE);
}
#endif // EXTENSION_SUPPORT

OP_STATUS
DOM_UserJSManager::RunScripts(BOOL greasemonkey)
{
	OP_PROBE3(OP_PROBE_DOM_USERJSMANAGER_RUNSCRIPTS);

#ifdef DOM_USER_JAVASCRIPT
	if (DOM_UserJSManagerLink::InList())
	{
		OP_ASSERT(greasemonkey);
		queued_run_greasemonkey = TRUE;
		return OpStatus::OK;
	}

	BOOL execute_regular_scripts = !only_browserjs;
	if (execute_regular_scripts && is_https)
		execute_regular_scripts = g_pcjs->GetIntegerPref(PrefsCollectionJS::UserJSEnabledHTTPS, DOM_EnvironmentImpl::GetHostName(environment->GetFramesDocument())) != 0;
#else
	const BOOL execute_regular_scripts = FALSE;
#endif // DOM_USER_JAVASCRIPT

	for (UsedScript *usedscript = (UsedScript *) usedscripts.First(); usedscript; usedscript = (UsedScript *) usedscript->Suc())
	{
		if (usedscript->script->GetType() == EXTENSIONJS)
		{
#ifdef EXTENSION_SUPPORT
			OP_BOOLEAN result;
			RETURN_IF_ERROR(result = AllowExtensionJS(environment, usedscript->owner));
			if (result == OpBoolean::IS_FALSE)
#endif // EXTENSION_SUPPORT
				continue;
		}
		if ((usedscript->script->GetType() == BROWSERJS ||
		     usedscript->script->GetType() == EXTENSIONJS ||
		     execute_regular_scripts) && !greasemonkey == (usedscript->script->GetType() != GREASEMONKEY))
			RETURN_IF_ERROR(usedscript->script->Use(environment, usedscript->program, NULL, usedscript));
	}

#ifdef DOM_USER_JAVASCRIPT
	if (queued_run_greasemonkey)
	{
		queued_run_greasemonkey = FALSE;
		return RunScripts(TRUE);
	}
#endif // DOM_USER_JAVASCRIPT

	return OpStatus::OK;
}

#else // USER_JAVASCRIPT_ADVANCED

static OP_STATUS
DOM_GetUserJSFile(OpFile &file, DOM_EnvironmentImpl *environment = NULL)
{
	TRAP_AND_RETURN(status, g_pcfiles->GetFileL(PrefsCollectionFiles::UserJSFile, file, environment ? DOM_EnvironmentImpl::GetHostName(environment->GetFramesDocument()) : NULL));
	return OpStatus::OK;
}

static OP_STATUS
DOM_OpenUserJSFile(OpFile &file, DOM_EnvironmentImpl *environment)
{
	RETURN_IF_ERROR(DOM_GetUserJSFile(file, environment));

	return file.Open(OPFILE_READ | OPFILE_TEXT);
}

OP_STATUS
DOM_UserJSManager::Construct()
{
	DOM_Object *object;
	RETURN_IF_ERROR(GetObject(object));

	event_target = OP_NEW(DOM_EventTarget, (object));
	if (!event_target)
		return OpStatus::ERR_NO_MEMORY;

	callback = OP_NEW(DOM_UserJSCallback, (this));
	if (!callback)
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsMemoryError(DOM_Object::DOMSetObjectRuntime(anchor = OP_NEW(DOM_UserJSAnchor, (this)), environment->GetDOMRuntime())) ||
	    !anchor->GetRuntime()->Protect(*anchor))
	{
		anchor = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}

	OpFile file;

	RETURN_IF_ERROR(DOM_OpenUserJSFile(file, environment));

	TempBuffer buffer;
	char buffer8[1024]; // ARRAY OK 2008-02-28 jl

	buffer.SetExpansionPolicy(TempBuffer::AGGRESSIVE);
	buffer.SetCachedLengthPolicy(TempBuffer::TRUSTED);

	while (!file.Eof())
	{
		OpFileLength bytes_read;
		RETURN_IF_ERROR(file.Read(buffer8, sizeof buffer8, &bytes_read));
		RETURN_IF_ERROR(buffer.Append(buffer8, bytes_read));
	}

	if (buffer.Length() != 0)
	{
		ES_ProgramText program;
		program.program_text = buffer.GetStorage();
		program.program_text_length = buffer.Length();

		RETURN_IF_ERROR(environment->GetAsyncInterface()->Eval(&program, 1, NULL, 0, callback, NULL));

		is_enabled = TRUE;
		++is_active;
	}

	return OpStatus::OK;
}

#endif // USER_JAVASCRIPT_ADVANCED

#ifdef DOM3_EVENTS
# define EVENT_TYPE(name) DOM_EVENT_CUSTOM, UNI_L("http://www.opera.com/userjs"), UNI_L(name)
# define EVENT_TYPE_BUFFER(buffer) DOM_EVENT_CUSTOM, UNI_L("http://www.opera.com/userjs"), buffer.GetStorage()
#else // DOM3_EVENTS
# define EVENT_TYPE(name) DOM_EVENT_CUSTOM, UNI_L(name)
# define EVENT_TYPE_BUFFER(buffer) DOM_EVENT_CUSTOM, buffer.GetStorage()
#endif // DOM3_EVENTS

OP_BOOLEAN
DOM_UserJSManager::BeforeScriptElement(DOM_Element *element, ES_Thread *interrupt_thread)
{
	if (element == handled_script || !is_active && !event_target->HasListeners(EVENT_TYPE("BeforeScript"), ES_PHASE_AT_TARGET))
		return OpBoolean::IS_FALSE;

	DOM_UserJSEvent *event;
	RETURN_IF_ERROR(DOM_UserJSEvent::Make(event, this, element, UNI_L("BeforeScript")));

	return environment->SendEvent(event, interrupt_thread);
}

OP_BOOLEAN
DOM_UserJSManager::AfterScriptElement(DOM_Element *element, ES_Thread *interrupt_thread)
{
	if (!event_target->HasListeners(EVENT_TYPE("AfterScript"), ES_PHASE_AT_TARGET))
		return OpBoolean::IS_FALSE;

	DOM_UserJSEvent *event;
	RETURN_IF_ERROR(DOM_UserJSEvent::Make(event, this, element, UNI_L("AfterScript")));

	return environment->SendEvent(event, interrupt_thread);
}

OP_BOOLEAN
DOM_UserJSManager::BeforeExternalScriptElement(DOM_Element *element, ES_Thread *interrupt_thread)
{
	if (element == handled_external_script || !is_active && !event_target->HasListeners(EVENT_TYPE("BeforeExternalScript"), ES_PHASE_AT_TARGET))
		return OpBoolean::IS_FALSE;

	DOM_UserJSEvent *event;
	RETURN_IF_ERROR(DOM_UserJSEvent::Make(event, this, element, UNI_L("BeforeExternalScript")));

	return environment->SendEvent(event, interrupt_thread);
}

#ifdef _PLUGIN_SUPPORT_
OP_BOOLEAN
DOM_UserJSManager::PluginInitializedElement(DOM_Element *element)
{
	if (!event_target->HasListeners(EVENT_TYPE("PluginInitialized"), ES_PHASE_AT_TARGET))
		return OpBoolean::IS_FALSE;

	DOM_UserJSEvent *event;
	RETURN_IF_ERROR(DOM_UserJSEvent::Make(event, this, element, UNI_L("PluginInitialized")));

	return environment->SendEvent(event);
}
#endif // _PLUGIN_SUPPORT_

OP_BOOLEAN
DOM_UserJSManager::BeforeEvent(DOM_Event *real_event, ES_Thread *interrupt_thread)
{
	if (real_event->IsA(DOM_TYPE_USERJSEVENT) || !event_target)
		return OpBoolean::IS_FALSE;

	TempBuffer buffer;
	RETURN_IF_ERROR(buffer.Append("BeforeEvent."));

	if (real_event->GetKnownType() == DOM_EVENT_CUSTOM)
		RETURN_IF_ERROR(buffer.Append(real_event->GetType()));
	else
		RETURN_IF_ERROR(buffer.Append(DOM_EVENT_NAME(real_event->GetKnownType())));

	OP_BOOLEAN result1, result2;

	RETURN_IF_ERROR(result1 = SendEventEvent(EVENT_TYPE_BUFFER(buffer), real_event, NULL, interrupt_thread));
	RETURN_IF_ERROR(result2 = SendEventEvent(EVENT_TYPE("BeforeEvent"), real_event, NULL, interrupt_thread));

	return (result1 == OpBoolean::IS_TRUE || result2 == OpBoolean::IS_TRUE) ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
}

OP_BOOLEAN
DOM_UserJSManager::AfterEvent(DOM_Event *real_event, ES_Thread *interrupt_thread)
{
	if (real_event->IsA(DOM_TYPE_USERJSEVENT) || !event_target)
		return OpBoolean::IS_FALSE;

	TempBuffer buffer;
	RETURN_IF_ERROR(buffer.Append("AfterEvent."));

	if (real_event->GetKnownType() == DOM_EVENT_CUSTOM)
		RETURN_IF_ERROR(buffer.Append(real_event->GetType()));
	else
		RETURN_IF_ERROR(buffer.Append(DOM_EVENT_NAME(real_event->GetKnownType())));

	OP_BOOLEAN result1, result2;

	RETURN_IF_ERROR(result1 = SendEventEvent(EVENT_TYPE_BUFFER(buffer), real_event, NULL, interrupt_thread));
	RETURN_IF_ERROR(result2 = SendEventEvent(EVENT_TYPE("AfterEvent"), real_event, NULL, interrupt_thread));

	return (result1 == OpBoolean::IS_TRUE || result2 == OpBoolean::IS_TRUE) ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
}

OP_BOOLEAN
DOM_UserJSManager::BeforeEventListener(DOM_Event *real_event, ES_Object *listener, ES_Thread *interrupt_thread)
{
	if (real_event->IsA(DOM_TYPE_USERJSEVENT) || !event_target)
		return OpBoolean::IS_FALSE;

	TempBuffer buffer;
	RETURN_IF_ERROR(buffer.Append("BeforeEventListener."));

	if (real_event->GetKnownType() == DOM_EVENT_CUSTOM)
		RETURN_IF_ERROR(buffer.Append(real_event->GetType()));
	else
		RETURN_IF_ERROR(buffer.Append(DOM_EVENT_NAME(real_event->GetKnownType())));

	OP_BOOLEAN result1, result2;

	RETURN_IF_ERROR(result1 = SendEventEvent(EVENT_TYPE_BUFFER(buffer), real_event, listener, interrupt_thread));
	RETURN_IF_ERROR(result2 = SendEventEvent(EVENT_TYPE("BeforeEventListener"), real_event, listener, interrupt_thread));

	return (result1 == OpBoolean::IS_TRUE || result2 == OpBoolean::IS_TRUE) ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
}

OP_BOOLEAN
DOM_UserJSManager::CreateAfterEventListener(DOM_UserJSEvent *&event1, DOM_UserJSEvent *&event2, DOM_Event *real_event, ES_Object *listener)
{
	if (real_event->IsA(DOM_TYPE_USERJSEVENT))
		return OpBoolean::IS_FALSE;

	TempBuffer buffer;
	RETURN_IF_ERROR(buffer.Append("AfterEventListener."));

	if (real_event->GetKnownType() == DOM_EVENT_CUSTOM)
		RETURN_IF_ERROR(buffer.Append(real_event->GetType()));
	else
		RETURN_IF_ERROR(buffer.Append(DOM_EVENT_NAME(real_event->GetKnownType())));

	RETURN_IF_ERROR(DOM_UserJSEvent::Make(event1, this, real_event, UNI_L("AfterEventListener"), listener));
	RETURN_IF_ERROR(DOM_UserJSEvent::Make(event2, this, real_event, buffer.GetStorage(), listener));

	if (event1->GetRuntime()->Protect(*event1))
	{
		if (event2->GetRuntime()->Protect(*event2))
			return OpBoolean::IS_TRUE;
		else
			event1->GetRuntime()->Unprotect(*event1);
	}

	return OpStatus::ERR_NO_MEMORY;
}

OP_BOOLEAN
DOM_UserJSManager::SendAfterEventListener(DOM_UserJSEvent *event1, DOM_UserJSEvent *event2, ES_Thread *interrupt_thread)
{
	if (!event_target)
		return OpBoolean::IS_FALSE;

	TempBuffer buffer;
	RETURN_IF_ERROR(buffer.Append("AfterEventListener."));

	DOM_Event *real_event = event1->GetEvent();

	if (real_event->GetKnownType() == DOM_EVENT_CUSTOM)
		RETURN_IF_ERROR(buffer.Append(real_event->GetType()));
	else
		RETURN_IF_ERROR(buffer.Append(DOM_EVENT_NAME(real_event->GetKnownType())));

	OP_BOOLEAN result1 = OpBoolean::IS_FALSE, result2 = OpBoolean::IS_FALSE;

	if (event_target->HasListeners(EVENT_TYPE("AfterEventListener"), ES_PHASE_AT_TARGET))
		RETURN_IF_ERROR(result1 = environment->SendEvent(event1, interrupt_thread));

	if (event_target->HasListeners(EVENT_TYPE_BUFFER(buffer), ES_PHASE_AT_TARGET))
		RETURN_IF_ERROR(result2 = environment->SendEvent(event2, interrupt_thread));

	return (result1 == OpBoolean::IS_TRUE || result2 == OpBoolean::IS_TRUE) ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
}

OP_STATUS
DOM_UserJSManager::BeforeJavascriptURL(ES_JavascriptURLThread *thread)
{
	if (!event_target)
		return OpStatus::OK;

	if (!event_target->HasListeners(EVENT_TYPE("BeforeJavascriptURL"), ES_PHASE_AT_TARGET))
		return OpStatus::OK;

	DOM_UserJSEvent *event;
	RETURN_IF_ERROR(DOM_UserJSEvent::Make(event, this, thread, UNI_L("BeforeJavascriptURL")));

	OP_BOOLEAN result = environment->SendEvent(event, thread);

	return OpStatus::IsError(result) ? result : OpStatus::OK;
}

OP_STATUS
DOM_UserJSManager::AfterJavascriptURL(ES_JavascriptURLThread *thread)
{
	if (!event_target)
		return OpStatus::OK;

	if (!event_target->HasListeners(EVENT_TYPE("AfterJavascriptURL"), ES_PHASE_AT_TARGET))
		return OpStatus::OK;

	DOM_UserJSEvent *event;
	RETURN_IF_ERROR(DOM_UserJSEvent::Make(event, this, thread, UNI_L("AfterJavascriptURL")));

	OP_BOOLEAN result = environment->SendEvent(event, thread);

	return OpStatus::IsError(result) ? result : OpStatus::OK;
}

OP_BOOLEAN
DOM_UserJSManager::BeforeCSS(DOM_Node *node, ES_Thread *interrupt_thread)
{

	if (node == handled_stylesheet || !is_active && !event_target->HasListeners(EVENT_TYPE("BeforeCSS"), ES_PHASE_AT_TARGET))
		return OpBoolean::IS_FALSE;

	if (node->GetIsWaitingForBeforeCSS())
		return OpBoolean::IS_TRUE;
	node->SetIsWaitingForBeforeCSS(TRUE);

	DOM_UserJSEvent *event;
	RETURN_IF_ERROR(DOM_UserJSEvent::Make(event, this, node, UNI_L("BeforeCSS")));

	OP_BOOLEAN res = environment->SendEvent(event, interrupt_thread);

	if (res == OpBoolean::IS_TRUE)
		environment->GetFramesDocument()->IncWaitForStyles();

	return res;
}

OP_BOOLEAN
DOM_UserJSManager::AfterCSS(DOM_Node *node, ES_Thread *interrupt_thread)
{
	if (node != handled_stylesheet || !event_target->HasListeners(EVENT_TYPE("AfterCSS"), ES_PHASE_AT_TARGET))
		return OpBoolean::IS_FALSE;

	DOM_UserJSEvent *event;
	RETURN_IF_ERROR(DOM_UserJSEvent::Make(event, this, node, UNI_L("AfterCSS")));

	return environment->SendEvent(event, interrupt_thread);
}

OP_STATUS
DOM_UserJSManager::EventFinished(DOM_UserJSEvent *event)
{
	const uni_char *type = event->GetType();
	ES_Thread *interrupt_thread = event->GetThread()->GetInterruptedThread();

	if (uni_str_eq(type, UNI_L("BeforeScript")) || uni_str_eq(type, UNI_L("BeforeExternalScript")))
	{
		/* Default action: start the script (execute it or load the external
		   source and then execute it).  Preventing the default action means
		   cancelling that and just pretend the script element wasn't there. */

		// BeforeScript/AfterScript events are only sent to script elements
		// which are ensured to always be at least DOM_TYPE_ELEMENT.
		OP_ASSERT(event->GetNode()->IsA(DOM_TYPE_ELEMENT));
		DOM_Element *dom_element = static_cast<DOM_Element *>(event->GetNode());
		HTML_Element *html_element = dom_element->GetThisElement();
		HLDocProfile *hld_profile = environment->GetFramesDocument()->GetHLDocProfile();

		if (event->GetPreventDefault())
			html_element->CancelScriptElement(hld_profile);
		else
		{
			if (!hld_profile->GetESLoadManager()->HasScript(html_element))
			{
				// Script canceled for some reason while waiting for
				// the user.js to run, or by some action performed
				// by the user.js
				return OpStatus::OK;
			}
			OP_BOOLEAN result;

			// Interrupt a thread to make sure the script is
			// handled before the queued ONLOAD event is processed.
			ES_Thread *thread = event->GetThread();
			if (uni_str_eq(type, UNI_L("BeforeScript")))
			{
				handled_script = dom_element;
				result = html_element->HandleScriptElement(hld_profile, thread);
				handled_script = NULL;
			}
			else
			{
				handled_external_script = dom_element;
				result = html_element->HandleScriptElement(hld_profile, thread);
				handled_external_script = NULL;
			}

			return OpStatus::IsMemoryError(result) ? result : OpStatus::OK;
		}
	}
	else if (uni_str_eq(type, UNI_L("AfterJavascriptURL")))
	{
		if (event->GetPreventDefault())
			event->GetJavascriptURLThread()->SetResult(NULL);
	}
	else if (uni_str_eq(type, UNI_L("BeforeJavascriptURL")) || uni_strni_eq(type, "BEFOREEVENT", 11) || uni_strni_eq(type, "BEFOREEVENTLISTENER", 19))
	{
		/* Default action: fire the event (BeforeEvent) or call the listener
		   (BeforeEventListener).  Both these events are evaluated by threads
		   interrupting an already started thread performing the default
		   action, so preventing the default action means cancelling the
		   interrupted thread. */

		if (event->GetPreventDefault())
			environment->GetScheduler()->CancelThread(interrupt_thread);
	}
	else if (uni_str_eq(type, UNI_L("BeforeCSS")))
	{
		DOM_Node *dom_node = event->GetNode();
		HTML_Element *html_element = dom_node->GetThisElement();
		FramesDocument *doc = environment->GetFramesDocument();

		dom_node->SetIsWaitingForBeforeCSS(FALSE);

		if (event->GetPreventDefault())
		{
			if (html_element)
				if (DataSrc *data_src = html_element->GetDataSrc())
					data_src->DeleteAll();
		}
		else if (doc->GetDocRoot()->IsAncestorOf(html_element))
		{
			handled_stylesheet = dom_node;
			OP_STATUS status = html_element->LoadStyle(doc, FALSE);
			handled_stylesheet = NULL;

			doc->DecWaitForStyles();

			return status;
		}

		doc->DecWaitForStyles();

	}
	else if (uni_strni_eq(type, "AFTEREVENTLISTENER", 18))
	{
		/* Default action: respect calls to event.stopPropagation. */

#ifdef DOM3_EVENTS
		if (event->GetPreventDefault() && !event->WasStoppedBefore() && event->GetEvent()->GetStopPropagation(event->GetCurrentGroup()))
			event->GetEvent()->SetStopPropagation(FALSE, FALSE);
#else // DOM3_EVENTS
		if (event->GetPreventDefault() && !event->WasStoppedBefore() && event->GetEvent()->GetStopPropagation())
			event->GetEvent()->SetStopPropagation(FALSE);
#endif // DOM3_EVENTS
	}
	else if (uni_strni_eq(type, "AFTEREVENT", 10))
	{
		/* Default action: respect calls to event.preventDefault. */

		if (event->GetPreventDefault())
			event->GetEvent()->SetPreventDefault(FALSE);
	}

	return OpStatus::OK;
}

OP_STATUS
DOM_UserJSManager::EventCancelled(DOM_UserJSEvent *event)
{
	if (uni_str_eq(event->GetType(), UNI_L("BeforeCSS")))
	{
		environment->GetFramesDocument()->DecWaitForStyles();
	}

	return OpStatus::OK;
}

OP_BOOLEAN
DOM_UserJSManager::HasListener(const uni_char *base, const char *specific8, const uni_char *specific16)
{
	TempBuffer buffer;
	RETURN_IF_ERROR(buffer.Expand(uni_strlen(base) + 2));

	OpStatus::Ignore(buffer.Append(base)); // Buffer is successfully expanded above

	if (event_target->HasListeners(EVENT_TYPE_BUFFER(buffer), ES_PHASE_AT_TARGET))
		return OpBoolean::IS_TRUE;

	OpStatus::Ignore(buffer.Append('.'));

	if (specific8)
		RETURN_IF_ERROR(buffer.Append(specific8));
	else
		RETURN_IF_ERROR(buffer.Append(specific16));

	if (event_target->HasListeners(EVENT_TYPE_BUFFER(buffer), ES_PHASE_AT_TARGET))
		return OpBoolean::IS_TRUE;

	return OpBoolean::IS_FALSE;
}

OP_BOOLEAN
DOM_UserJSManager::HasListeners(const uni_char *base1, const uni_char *base2, DOM_EventType known_type, const uni_char *type)
{
	if (!event_target)
		return OpBoolean::IS_FALSE;

	const char *specific8 = NULL;
	const uni_char *specific16 = NULL;

	if (known_type == DOM_EVENT_CUSTOM)
		specific16 = type;
	else
		specific8 = DOM_EVENT_NAME(known_type);

	OP_BOOLEAN result = HasListener(base1, specific8, specific16);
	if (result != OpBoolean::IS_FALSE)
		return result;

	return HasListener(base2, specific8, specific16);
}

OP_BOOLEAN
DOM_UserJSManager::HasBeforeOrAfterEvent(DOM_EventType type, const uni_char *type_string)
{
	return HasListeners(UNI_L("BeforeEvent"), UNI_L("AfterEvent"), type, type_string);
}

OP_BOOLEAN
DOM_UserJSManager::HasBeforeOrAfterEventListener(DOM_EventType type, const uni_char *type_string)
{
	return HasListeners(UNI_L("BeforeEventListener"), UNI_L("AfterEventListener"), type, type_string);
}

BOOL
DOM_UserJSManager::GetIsActive(ES_Runtime *origining_runtime)
{
	if (is_enabled)
		if (origining_runtime)
		{
			ES_Thread *thread = DOM_Object::GetCurrentThread(origining_runtime);

			while (thread)
			{
				if (ES_Context *context = thread->GetContext())
					if (!ES_Runtime::HasPrivilegeLevelAtLeast(context, ES_Runtime::PRIV_LVL_USERJS))
						return FALSE;

#ifdef USER_JAVASCRIPT_ADVANCED
				if (thread->Type() == ES_THREAD_EVENT && ((DOM_EventThread *) thread)->GetEvent()->IsA(DOM_TYPE_USERJSEVENT) ||
				    thread->Type() == ES_THREAD_COMMON && uni_str_eq(thread->GetInfoString(), "User Javascript thread") && ((DOM_UserJSThread *) thread)->GetType() != GREASEMONKEY)
#else // USER_JAVASCRIPT_ADVANCED
				if (thread->Type() == ES_THREAD_EVENT && ((DOM_EventThread *) thread)->GetEvent()->IsA(DOM_TYPE_USERJSEVENT))
#endif // USER_JAVASCRIPT_ADVANCED
					return TRUE;
				else
					thread = thread->GetInterruptedThread();
			}
		}
		else
			return is_active != 0;

	return FALSE;
}

BOOL
DOM_UserJSManager::GetIsHandlingScriptElement(ES_Runtime *origining_runtime, HTML_Element *element)
{
	if (is_enabled && origining_runtime && element)
	{
		ES_Thread *thread = DOM_Object::GetCurrentThread(origining_runtime);

		while (thread)
		{
			if (thread->Type() == ES_THREAD_EVENT)
			{
				DOM_Event *event = ((DOM_EventThread *) thread)->GetEvent();

				if (event->IsA(DOM_TYPE_USERJSEVENT))
					if (uni_str_eq(event->GetType(), "BeforeExternalScript") || uni_str_eq(event->GetType(), "BeforeScript"))
						if (static_cast<DOM_UserJSEvent *>(event)->GetNode()->GetThisElement() == element)
							return TRUE;
			}

			thread = thread->GetInterruptedThread();
		}
	}

	return FALSE;
}

OP_STATUS
DOM_UserJSManager::GetObject(DOM_Object *&object)
{
	ES_Value value;
	DOM_Object *window = environment->GetWindow();
	OP_BOOLEAN result = window->GetPrivate(DOM_PRIVATE_opera, &value);

	if (result == OpBoolean::IS_TRUE)
	{
		object = DOM_VALUE2OBJECT(value, DOM_Object);
		return OpStatus::OK;
	}
	else if (result == OpBoolean::IS_FALSE)
		return OpStatus::ERR;
	else
		return result;
}

OP_STATUS
DOM_UserJSManager::ReloadScripts()
{
#ifdef USER_JAVASCRIPT_ADVANCED
	RETURN_IF_ERROR(SanitizeScripts());
	RETURN_IF_MEMORY_ERROR(LoadScripts());
#endif // USER_JAVASCRIPT_ADVANCED
	return OpStatus::OK;
}

/* static */ unsigned
DOM_UserJSManager::GetFilesCount()
{
#ifdef USER_JAVASCRIPT_ADVANCED
	return g_DOM_userScripts.Cardinal();
#else // USER_JAVASCRIPT_ADVANCED
	OpFile file;
	if (OpStatus::IsSuccess(DOM_GetUserJSFile(file)))
	{
		BOOL exists = FALSE;

		OP_STATUS status = file.Exists(exists);

		if (OpStatus::IsSuccess(status) && exists)
			return 1;
	}
	return 0;
#endif // USER_JAVASCRIPT_ADVANCED
}

/* static */ const uni_char *
DOM_UserJSManager::GetFilename(unsigned index)
{
#ifdef USER_JAVASCRIPT_ADVANCED
	DOM_UserJSScript *script = g_DOM_userScripts.First();
	while (index != 0 && script)
	{
		script = script->Suc();
		--index;
	}

	if (script)
		return script->GetFilename();
#else // USER_JAVASCRIPT_ADVANCED
	OpFile file;
	if (OpStatus::IsSuccess(DOM_GetUserJSFile(file)))
	{
		TempBuffer &buffer = g_DOM_tempbuf;
		buffer.Clear();
		if (OpStatus::IsSuccess(buffer.Append(file.GetFullPath())))
			return buffer.GetStorage();
	}
#endif // USER_JAVASCRIPT_ADVANCED
	return UNI_L("");
}

/* static */ OP_STATUS
DOM_UserJSManager::SanitizeScripts()
{
#ifdef USER_JAVASCRIPT_ADVANCED
	for (DOM_UserJSScript *script = g_DOM_userScripts.First(), *nextscript; script; script = nextscript)
	{
		nextscript = script->Suc();

		OP_BOOLEAN isstale = script->IsStale();
		RETURN_IF_ERROR(isstale);

		if (isstale == OpBoolean::IS_TRUE)
		{
			script->Out();
			DOM_UserJSScript::DecRef(script);
		}
	}

	return OpStatus::OK;
#endif // USER_JAVASCRIPT_ADVANCED
}

/* static */ void
DOM_UserJSManager::RemoveScripts()
{
#ifdef USER_JAVASCRIPT_ADVANCED
	while (DOM_UserJSScript *script = g_DOM_userScripts.First())
	{
		script->Out();
		DOM_UserJSScript::DecRef(script);
	}

	while (DOM_UserJSSource *source = g_DOM_userScriptSources.First())
	{
		source->Out();
		DOM_UserJSSource::DecRef(source);
	}
#endif // USER_JAVASCRIPT_ADVANCED
}

/* static */ ES_ScriptType
DOM_UserJSManager::GetScriptType(UserJSType type)
{
	switch (type)
	{
	case GREASEMONKEY:
		return SCRIPT_TYPE_USER_JAVASCRIPT_GREASEMONKEY;
	case BROWSERJS:
		return SCRIPT_TYPE_BROWSER_JAVASCRIPT;
	case EXTENSIONJS:
		return SCRIPT_TYPE_EXTENSION_JAVASCRIPT;
	default:
		return SCRIPT_TYPE_USER_JAVASCRIPT;
	}
}

/* static */ const uni_char *
DOM_UserJSManager::GetErrorString(UserJSType type, BOOL when_compiling)
{
	switch (type)
	{
	case GREASEMONKEY:
		return when_compiling ? UNI_L("Greasemonkey JS compilation") : UNI_L("Greasemonkey JS loading");
	case BROWSERJS:
		return when_compiling ? UNI_L("Browser JS compilation") : UNI_L("Browser JS loading");
	case EXTENSIONJS:
		return when_compiling ? UNI_L("Extension JS compilation") : UNI_L("Extension JS loading");
	default:
		return when_compiling ? UNI_L("User JS compilation") : UNI_L("User JS loading");
	}
}

/* static */ BOOL
DOM_UserJSManager::IsActiveInRuntime(ES_Runtime *runtime)
{
	if (DOM_UserJSManager *user_js_manager = static_cast<DOM_Runtime *>(runtime)->GetEnvironment()->GetUserJSManager())
		return user_js_manager->GetIsActive(runtime);
	return FALSE;
}

#ifdef EXTENSION_SUPPORT
/* static */ BOOL
DOM_UserJSManager::CheckExtensionScripts(FramesDocument *frames_doc)
{
	OP_PROBE3(OP_PROBE_DOM_USERJSMANAGER_WOULDACTIVATEEXTENSION);

	BOOL would_activate = FALSE;
	for (DOM_UserJSSource *source = g_DOM_userScriptSources.First(); source; source = source->Suc())
		if (source->GetType() == EXTENSIONJS && source->WouldUse(frames_doc))
		{
			would_activate = TRUE;
			break;
		}

	if (g_gadget_manager->HasUserJSUpdates())
	{
		g_DOM_have_always_enabled_extension_js = FALSE;
		for (OpGadgetExtensionUserJS *extension_userjs = g_gadget_manager->GetFirstActiveExtensionUserJS();
			 extension_userjs; extension_userjs = extension_userjs->Suc())
		{
			if (GetCachedScript(extension_userjs->userjs_filename, EXTENSIONJS)
				|| GetLoadedSource(extension_userjs->userjs_filename, EXTENSIONJS))
				continue;

			DOM_UserJSSource *source;
			OP_BOOLEAN loaded = DOM_UserJSSource::Make(source, extension_userjs->userjs_filename, EXTENSIONJS);
			if (!OpStatus::IsSuccess(loaded) || loaded == OpBoolean::IS_FALSE)
			{
				WarnLoadFailure(extension_userjs->userjs_filename, EXTENSIONJS);
				continue;
			}

			if (source->WouldUse(frames_doc))
				would_activate = TRUE;
			else
				source->Empty(); // Don't store the source in memory, it will be reloaded if needed.

			if (source->CheckIfAlwaysEnabled())
				g_DOM_have_always_enabled_extension_js = TRUE;

			DOM_InsertUserJSSourceIntoCache(source);
		}
		g_gadget_manager->ResetUserJSUpdated();
	}

	if (g_DOM_have_always_enabled_extension_js)
		return TRUE;

	if (!would_activate)
		for (DOM_UserJSScript *script = g_DOM_userScripts.First(); script; script = script->Suc())
			if (script->GetType() == EXTENSIONJS && script->WouldUse(frames_doc))
				return TRUE;

	return would_activate;
}
#endif // EXTENSION_SUPPORT

#endif // USER_JAVASCRIPT
