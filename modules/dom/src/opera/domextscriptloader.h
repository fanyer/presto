/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 2009-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** @author sof
*/
#ifndef DOM_EXTENSIONSCRIPT_LOADER_H
#define DOM_EXTENSIONSCRIPT_LOADER_H

#ifdef EXTENSION_SUPPORT
#ifdef EXTENSION_FEATURE_IMPORTSCRIPT
#include "modules/dom/src/domobj.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/doc/frm_doc.h"

class DOM_Extension;


/** DOM_ExtensionScriptLoader sets up the loading and execution of external scripts in the context of a DOM_Extension.
	Used when the script requests additional scripts to be imported, via opera.extension.importScript(). */
class DOM_ExtensionScriptLoader
	: public DOM_Object,
	  public ES_ThreadListener,
	  public ExternalInlineListener
{
public:
	static OP_STATUS Make(DOM_ExtensionScriptLoader *&loader, const URL &load_url, FramesDocument *document, DOM_Extension *extension, DOM_Runtime *origining_runtime);
	/**<
	     @param [out]loader the resulting loader object.
	     @param load_url the resolved URL to load.
	     @param document the document to perform the inline load 'on'. Assumed to outlive the loader.
	     @param extension the extension to inject the script into and evaluate.
	     @param origining_runtime runtime of the context instantiating the loader.

	     @return OpStatus::ERR_NO_MEMORY if failed to allocate new instance,
	             OpStatus::OK if loader created and initialised successfully. */

	OP_BOOLEAN LoadScript(DOM_Object *this_object, ES_Value *return_value, ES_Thread *interrupt_thread);
	/**< Schedule an (a)synchronous load of a script in the context of an extension's UserJS execution environment.
	     If a thread is supplied, as it should, this will be done while blocking its execution. 

	     If the resolved URL's script is readily available, the script is fetched right away along with a return value of OpBoolean::IS_TRUE.
	     If not, an async inline-load listener is started, blocking the dependent thread.

	     @param this_object the object creating the loader; if an exception is reported, done via its runtime.
	     @param return_value if setting up the asynchronous loading and evaluation resulted in an exception, report it via
	            'return_value' (along with returning OpStatus::ERR.)
	     @param interrupt_thread if supplied, the external thread to block until the loading has completed.

	     @return OpStatus::ERR if loading failed to initialise, information provided via a DOM exception in return_value.
	             OpStatus::IS_TRUE if the loader was able to fetch the script resource right away, OpStatus::IS_FALSE if
	             loader successfully started the loading the script resource; if supplied, 'interrrupt_thread' is blocked
	             as a result. OOM is signalled via OpStatus::ERR_NO_MEMORY. */

	void Shutdown();
	/**< Shutdown cleanly, aborting all operation. */

	OP_STATUS HandleCallback(ES_AsyncStatus status, const ES_Value &result);

private:
	DOM_ExtensionScriptLoader()
		: owner(NULL),
		  document(NULL),
		  waiting(NULL),
		  aborted(FALSE),
		  loading_started(FALSE)
	{
	}

	virtual ~DOM_ExtensionScriptLoader();

	DOM_Extension *owner;
	FramesDocument *document;
	/**< The script loader uses the inline loading machinery, which is based off of a
		 FramesDocument (and all that gives you). */

	ES_Thread *waiting;
	URL script_url;
	IAmLoadingThisURL lock;
	BOOL aborted;
	BOOL loading_started;
	ES_Value excn_value;

	void Abort(FramesDocument *document);

	virtual OP_STATUS Signal(ES_Thread *signalled, ES_ThreadSignal signal);
	virtual void LoadingStopped(FramesDocument *document, const URL &url);

	OP_STATUS GetData(TempBuffer *buffer, BOOL &success);
	OP_STATUS GetScriptText(TempBuffer &buffer, ES_Value *return_value);
	static OP_STATUS RetrieveData(URL_DataDescriptor *descriptor, BOOL &more);

};
#endif // EXTENSION_FEATURE_IMPORTSCRIPT

#endif // EXTENSION_SUPPORT

#endif // DOM_EXTENSIONSCRIPT_LOADER_H
