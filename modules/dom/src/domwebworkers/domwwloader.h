/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 2009-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef DOM_WW_LOADER_H
#define DOM_WW_LOADER_H

#ifdef DOM_WEBWORKERS_SUPPORT

#include "modules/dom/src/domwebworkers/domwebworkers.h"

/** DOM_WebWorker_Loader sets up the loading and execution of external scripts in the context of a DOM_WebWorker.
    Used when loading the initial script of the worker, and when it later on calls the importScripts() global function. */
class DOM_WebWorker_Loader
    : public EcmaScript_Object,
      public ES_ThreadListener,
      public ExternalInlineListener,
      public ES_Runtime::ErrorHandler
{
public:
    static OP_STATUS LoadScripts(DOM_Object *this_object, DOM_WebWorker *worker, DOM_WebWorkerObject *worker_object, OpAutoVector<URL> *load_urls, ES_Value *return_value, ES_Thread *interrupt_thread);
    /**< Schedule an (a)synchronous load and execution of a list of scripts in the context of a Web Worker.
         If a thread is supplied, will be done while blocking its execution. The two uses of
         this for Web Workers is to load the script URL supplied with a Worker constructor, or
         when the worker code invokes the 'importScripts()' global function.

         If the (first) resolved URL's script is readily available, we load up the script
         right away and execute it asynchronously. If not, an async inline-load listener is
         set up to fire off the execution once the first script becomes available.

         @param this_object   the object creating the loader; if an exception is reported, done via its runtime.
         @param worker        the execution context to inject the scripts into and execute them in.
         @param origin_worker if the loader encounters an error loading any of the external scripts for a shared worker,
                              use 'origin_worker' to report back the errors via its 'onerror' handler. Otherwise the errors
                              are reported and propagated via the DOM_WebWorker.
         @param load_urls     list of external script URLs.
         @param return_value  if setting up the asynchronous loading and evaluation resulted in an exception, report it via
                              'return_value' (along with returning OpStatus::ERR.)
         @param interrupt_thread if supplied, the external thread to block until the loading has completed. Used for importScripts(),
                              which is synchronous. Loading and execution of new Workers is, naturally, done asynchronously in the background.

         @return OpStatus::ERR if loading failed to initialise, information provided via a DOM exception in return_value.
                 OpStatus::OK if the loader started and is under way; OOM via OpStatus::ERR_NO_MEMORY. */

    void Shutdown();
    /**< Shutdown cleanly, aborting all operation. */

private:
    DOM_WebWorker_Loader()
        : worker(NULL),
          origin_worker(NULL),
          interrupted_thread(NULL),
          interrupting_thread(NULL),
          import_urls(NULL),
          index_url(0),
          aborted(FALSE),
          loading_started(FALSE),
          exception_raised(FALSE),
          has_started(FALSE)
    {
    }

    virtual ~DOM_WebWorker_Loader();

    friend OP_STATUS DOM_WebWorker::HandleCallback(ES_AsyncOperation operation, ES_AsyncStatus status, const ES_Value &result);

    /** Handle uncaught exceptions stemming from evaluating one of the imported scripts. */
    OP_STATUS HandleCallback(ES_AsyncStatus status, const ES_Value &result);

    DOM_WebWorker *worker;
    DOM_WebWorkerObject *origin_worker;

    ES_Thread *interrupted_thread;
    /**< If non-NULL, the thread currently blocked waiting for this script
         import to complete. */

    ES_Thread *interrupting_thread;
    /**< If non-NULL, the thread we're about to block. Need to separately record
         this to handle interaction with loading of inline (scripts.) */

    OpAutoVector<URL> *import_urls;
    unsigned index_url;

    URL target;
    IAmLoadingThisURL lock;

    BOOL aborted;
    BOOL loading_started;
    BOOL exception_raised;
    BOOL has_started;

    ES_Value excn_value;

    void Abort(FramesDocument *document);

    /* importScript() may be given a list of script URLs, whose loading should be sequenced.
       Some helper functions for moving over that set of URLs: */

    BOOL MoveToNextScript()
    {
        index_url++;
        return (!import_urls || index_url < import_urls->GetCount());
    }
    /**< Done processing one script, step on to next. Returns TRUE if any remaining. */

    URL *GetCurrentScriptURL()
    {
        URL *current_script_url = (!import_urls || import_urls->GetCount()==0) ? NULL : import_urls->Get(index_url);
        return current_script_url;
    }

    BOOL NoMoreScripts()
    {
        return (!import_urls || import_urls->GetCount() == 0 || index_url >= import_urls->GetCount());
    }
    /**< Determine if all scripts have now been loaded, TRUE if so. */

    OP_BOOLEAN LoadNextScript(DOM_Object *this_object = 0, ES_Value *return_value = 0, ES_Thread *interrupt_thread = 0);

    virtual OP_STATUS Signal(ES_Thread *signalled, ES_ThreadSignal signal);
    virtual void LoadingStopped(FramesDocument *document, const URL &url);
    virtual void LoadingRedirected(FramesDocument *document, const URL &from, const URL &to);

    virtual OP_STATUS HandleError(ES_Runtime *runtime, const ES_Runtime::ErrorHandler::ErrorInformation *information, const uni_char *msg, const uni_char *url, const ES_SourcePosition &position, const ES_ErrorData *data);

    OP_STATUS GetData(TempBuffer *buffer, BOOL &success);
    OP_STATUS StartEvalScript(ES_Value *return_value);
    static OP_STATUS RetrieveData(URL_DataDescriptor *descriptor, BOOL &more);

    void ClearWorker();
    /**< Shutdown cleanly by cleaning out ref. to worker. */

    void Cancel();
    /**< Worker-initiated cancellation of URL load; at the moment, it is something weaker,
         simply stop evaluation or delivery of exceptions to the attached worker. */
};

#endif // DOM_WEBWORKERS_SUPPORT
#endif // DOM_WW_LOADER_H
