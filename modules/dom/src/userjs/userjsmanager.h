/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2003-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef USERJSMANAGER_H
#define USERJSMANAGER_H

#ifdef USER_JAVASCRIPT

#include "modules/util/simset.h"
#include "modules/ecmascript_utils/esthread.h"

#ifdef EXTENSION_SUPPORT
#include "modules/ecmascript_utils/esenvironment.h"
#endif // EXTENSION_SUPPORT

class DOM_EnvironmentImpl;
class DOM_Event;
class DOM_EventTarget;
class DOM_EventListener;
class DOM_UserJSCallback;
class DOM_UserJSEvent;
class DOM_UserJSAnchor;
class DOM_UserJSScript;
class DOM_UserJSSource;
class DOM_Element;
class OpFile;
class ES_Thread;
class ES_JavascriptURLThread;
#ifdef WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
class DOM_Storage;
#endif //WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT

class DOM_UserJSManagerLink
	: public Link
{
};

#ifdef EXTENSION_SUPPORT
class DOM_UserJSExecutionContext
	: public ListElement<DOM_UserJSExecutionContext>
{
public:
	DOM_UserJSExecutionContext(OpGadget *o)
		: es_environment(NULL),
		  ref_count(1),
		  owner(o)
	{
	}

	~DOM_UserJSExecutionContext();

	static OP_STATUS Make(DOM_UserJSExecutionContext *&userjs_context, DOM_UserJSManager *manager, DOM_EnvironmentImpl *environment, OpGadget *owner);

	void IncRef() { ++ref_count; }
	void DecRef() { if (--ref_count == 0) OP_DELETE(this); }

	void BeforeDestroy();

	OpGadget* GetOwner() { return owner; }
	ES_Environment* GetESEnvironment() { return es_environment; }

private:
	ES_Environment *es_environment;
	unsigned ref_count;
	OpGadget *owner;
};
#endif // EXTENSION_SUPPORT

class DOM_UserJSManager
	: public DOM_UserJSManagerLink,
	  public DOM_EventTargetOwner
{
public:
	enum UserJSType
	{
		NORMAL_USERJS,
		GREASEMONKEY,
		BROWSERJS,
		EXTENSIONJS
	};

protected:
	DOM_EnvironmentImpl *environment;
#ifndef USER_JAVASCRIPT_ADVANCED
	DOM_UserJSCallback *callback;
#endif
	DOM_UserJSAnchor *anchor;

	BOOL is_enabled, only_browserjs, is_https, queued_run_greasemonkey;
	unsigned is_active;

	DOM_Element *handled_external_script;
	DOM_Element *handled_script;
	DOM_Node *handled_stylesheet;

#ifdef USER_JAVASCRIPT_ADVANCED
	friend class DOM_UserJSScript;

	class UsedScript
		: public Link
	{
	public:
		friend class DOM_UserJSScript;

		DOM_UserJSScript *script;
		ES_Program *program;
#ifdef EXTENSION_SUPPORT
		DOM_UserJSExecutionContext *execution_context;
		OpGadget *owner;
#endif // EXTENSION_SUPPORT

		UsedScript()
			: script(NULL)
			, program(NULL)
#ifdef EXTENSION_SUPPORT
			, execution_context(NULL)
			, owner(NULL)
#endif // EXTENSION_SUPPORT
		{
		}

#ifdef EXTENSION_SUPPORT
		~UsedScript()
		{
			if (execution_context)
			    execution_context->DecRef();
		}
#endif // EXTENSION_SUPPORT
	};

	Head usedscripts;

#ifdef EXTENSION_SUPPORT
	List<DOM_UserJSExecutionContext> userjs_execution_contexts;

	DOM_UserJSExecutionContext* FindExecutionContext(OpGadget *o);
	/**< Locate an execution context to inject script into by unique id; return NULL if none available. */

	void UnreferenceExecutionContexts();
	/**< Decrease the reference count of each DOM_UserJSExecutionContext by one. */
#endif // EXTENSION_SUPPORT

#ifdef EXTENSION_SUPPORT
	OP_STATUS UseScript(DOM_UserJSScript *script, ES_Program *program, OpGadget *owner = NULL);
	OP_BOOLEAN LoadFile(OpFile &file, UserJSType type, BOOL cache, OpGadget *owner = NULL);
	static OP_BOOLEAN AllowExtensionJS(DOM_EnvironmentImpl *environment, OpGadget *owner);
#else
	OP_STATUS UseScript(DOM_UserJSScript *script, ES_Program *program);
	OP_BOOLEAN LoadFile(OpFile &file,UserJSType type, BOOL cache);
	/**
	 * @returns OpStatus::ERR_NO_MEMORY if we ran out of memory,
	 * OpStatus::ERR if processing of the UserJS filename path failed.
	 * OpStatus::OK otherwise, even if individual scripts failed to load and compile.
	 *
	 * Note: failure to load and process a script does not result in an overall error; logged to the
	 * error console if available instead (and the processing of succeeding scripts will proceed.)
	 */
#endif // EXTENSION_SUPPORT

	OP_STATUS LoadScripts();

	/** Get script object for the given file path and type.
	 *
	 * Note: The file paths are compared by string so care must be taken to
	 * avoid handling paths from different sources (e.g. OpFile::GetFullPath and
	 * OpFolderLister::GetFullPath may yield different strings for the same file).
	 *
	 * Note 2: BrowserJS is searched only by type. It is both an optimization and
	 * a workaround for the full paths not always having identical string representation.
	 */
	static DOM_UserJSScript *GetCachedScript(const uni_char *filepath, UserJSType type);

	/** Get source object for the given file path and type.
	 *
	 * Note: The file paths are compared by string so care must be taken to
	 * avoid handling paths from different sources (e.g. OpFile::GetFullPath and
	 * OpFolderLister::GetFullPath may yield different strings for the same file).
	 *
	 * Note 2: BrowserJS is searched only by type. It is both an optimization and
	 * a workaround for the full paths not always having identical string representation.
	 */
	static DOM_UserJSSource *GetLoadedSource(const uni_char *filepath, UserJSType type);
#endif // USER_JAVASCRIPT_ADVANCED

#ifdef DOM3_EVENTS
	OP_BOOLEAN SendEventEvent(DOM_EventType type, const uni_char *namespaceURI, const uni_char *type_string, DOM_Event *real_event, ES_Object *listener, ES_Thread *interrupt_thread);
#else // DOM3_EVENTS
	OP_BOOLEAN SendEventEvent(DOM_EventType type, const uni_char *type_string, DOM_Event *real_event, ES_Object *listener, ES_Thread *interrupt_thread);
#endif // DOM3_EVENTS

	/* From DOM_EventTargetOwner: */
	virtual DOM_Object *GetOwnerObject();

	static OP_STATUS InitScriptLists();
public:
	DOM_UserJSManager(DOM_EnvironmentImpl *environment, BOOL user_js_enabled, BOOL is_https);
	~DOM_UserJSManager();

	OP_STATUS Construct();

	void BeforeDestroy();
#ifdef EXTENSION_SUPPORT
	OP_STATUS GetExtensionESRuntimes(OpVector<ES_Runtime> &es_runtimes);
#endif // EXTENSION_SUPPORT

#ifdef USER_JAVASCRIPT_ADVANCED
	OP_STATUS RunScripts(BOOL greasemonkey);
#endif // USER_JAVASCRIPT_ADVANCED

	OP_BOOLEAN BeforeScriptElement(DOM_Element *element, ES_Thread *interrupt_thread);
	OP_BOOLEAN AfterScriptElement(DOM_Element *element, ES_Thread *interrupt_thread);
	OP_BOOLEAN BeforeExternalScriptElement(DOM_Element *element, ES_Thread *interrupt_thread);
#ifdef _PLUGIN_SUPPORT_
	OP_BOOLEAN PluginInitializedElement(DOM_Element *element);
#endif // _PLUGIN_SUPPORT_
	OP_BOOLEAN BeforeEvent(DOM_Event *event, ES_Thread *interrupt_thread);
	OP_BOOLEAN AfterEvent(DOM_Event *event, ES_Thread *interrupt_thread);
	OP_BOOLEAN BeforeEventListener(DOM_Event *event, ES_Object *listener, ES_Thread *interrupt_thread);
	OP_BOOLEAN CreateAfterEventListener(DOM_UserJSEvent *&event1, DOM_UserJSEvent *&event2, DOM_Event *real_event, ES_Object *listener);
	OP_BOOLEAN SendAfterEventListener(DOM_UserJSEvent *event1, DOM_UserJSEvent *event2, ES_Thread *interrupt_thread);
	OP_STATUS BeforeJavascriptURL(ES_JavascriptURLThread *thread);
	OP_STATUS AfterJavascriptURL(ES_JavascriptURLThread *thread);

	OP_BOOLEAN BeforeCSS(DOM_Node *element, ES_Thread *interrupt_thread);
	OP_BOOLEAN AfterCSS(DOM_Node *element, ES_Thread *interrupt_thread);

	OP_STATUS EventFinished(DOM_UserJSEvent *event);
	OP_STATUS EventCancelled(DOM_UserJSEvent *event);

	OP_BOOLEAN HasListener(const uni_char *base, const char *specific8, const uni_char *specific16);
	OP_BOOLEAN HasListeners(const uni_char *base1, const uni_char *base2, DOM_EventType type, const uni_char *type_string);
	OP_BOOLEAN HasBeforeOrAfterEvent(DOM_EventType type, const uni_char *type_string);
	OP_BOOLEAN HasBeforeOrAfterEventListener(DOM_EventType type, const uni_char *type_string);

	BOOL GetIsEnabled() { return is_enabled; }

	void ScriptStarted() { ++is_active; }
	void ScriptFinished() { --is_active; }
	BOOL GetIsActive(ES_Runtime *origining_runtime = NULL);
	/**< Returns if there is currently a user script or extension thread running in the runtime. */
	BOOL GetIsHandlingScriptElement(ES_Runtime *origining_runtime, HTML_Element *element);
	/**< Returns if the element is currently being handled by a BeforeExternalScript or BeforeScript
	     event in the runtime. */

	DOM_EnvironmentImpl *GetEnvironment() { return environment; }
	DOM_EventTarget *GetEventTarget() { return event_target; }

	OP_STATUS GetObject(DOM_Object *&object);
	OP_STATUS ReloadScripts();

	static unsigned GetFilesCount();
	static const uni_char *GetFilename(unsigned index);
	static OP_STATUS SanitizeScripts();
	static void RemoveScripts();
	static ES_ScriptType GetScriptType(UserJSType type);
	static const uni_char *GetErrorString(UserJSType type, BOOL when_compiling);

	static BOOL IsActiveInRuntime(ES_Runtime *runtime);
	/**< Returns if there is currently a user script or extension thread running in the runtime. */
#ifdef EXTENSION_SUPPORT
	/** Check if extension user scripts should run in the given frames document.
	 *
	 * This method also loads any newly installed scripts if necessary.
	 * If there are errors loading any of the scripts, they are displayed
	 * on the console and the operation continues.
	 *
	 * @return Whether the given frames_doc requires userjs to run.
	 */
	static BOOL CheckExtensionScripts(FramesDocument *frames_doc);

	/** Find a gadget which constructed given runtime in this manager's shared environment.
	  * Returns NULL if not found or runtime is not an extension UserJS runtime. */
	OpGadget *FindRuntimeGadget(DOM_Runtime *runtime);
#endif // EXTENSION_SUPPORT
};


class DOM_UserJSThread
	: public ES_Thread
{
protected:
	DOM_UserJSManager *manager;
	DOM_UserJSScript *script;
#ifdef WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
	DOM_Storage *script_storage;
#endif // WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
	DOM_UserJSManager::UserJSType type;
public:
	DOM_UserJSThread(ES_Context *context, DOM_UserJSManager *manager, DOM_UserJSScript *script, DOM_UserJSManager::UserJSType type);
	~DOM_UserJSThread();

	virtual OP_STATUS Signal(ES_ThreadSignal signal);
	virtual const uni_char *GetInfoString();

#ifdef WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
	DOM_Storage* GetScriptStorage() const { return script_storage; }
	void SetScriptStorage(DOM_Storage *ss);
	const uni_char* GetScriptFileName() const;
#endif //WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT

	DOM_UserJSManager::UserJSType GetType() const { return type; }
};

#endif // USER_JAVASCRIPT
#endif // USERJSMANAGER_H
