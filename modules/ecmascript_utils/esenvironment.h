/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef ES_UTILS_ESENVIRONMENT_H
#define ES_UTILS_ESENVIRONMENT_H

#ifdef ESUTILS_ES_ENVIRONMENT_SUPPORT

class ES_Runtime;
class ES_ThreadScheduler;
class ES_AsyncInterface;
class ES_Object;
class FramesDocument;

/** Representation of an ECMAScript environment in which scripts can be
    executed.  This can either be a pure ECMAScript-only environment, in
    which case no global functions or objects besides those defined by ES-262
    are available, or a standard DOM environment if a document was supplied
    to the ES_Environment::Create function.

    This class can be used when a script needs to be executed and either no
    document is available to execute it in or if the script needs to be
    isolated from the document's environment (so that no global variables
    or functions defined by the document interfere with the script, or so
    that the script does not interfere with other scripts executed by the
    document. */
class ES_Environment
{
public:
	static OP_STATUS Create(ES_Environment *&env, FramesDocument *doc = NULL, BOOL ignore_prefs = FALSE, URL *site_url = NULL);
	/**< Create an ES_Environment object.  If 'document' is not NULL, the
	     created environment will shadow the DOM environment present in that
	     document.  Otherwise a new clean (no DOM stuff) ECMAScript
	     environment will be created.  If the environment is created to
	     shadow a DOM environment, it becomes invalid when that DOM
	     environment is destroyed.

	     The created environment must be destroyed using the function
	     ES_Environment::Destroy.

	     @param env Set to the created environment on success.
	     @param doc Optional document.
	     @param ignore_prefs Ignore any preferences that disables ECMAScript
	            and create the environment anyway.  This will make it
	            possible to execute ECMAScript code even if ECMAScript
	            has been disabled and should not be used to execute
	            "author ECMAScript code".

	     @param site_url if checking preferences and non-NULL, the document URL
	            to use when resolving site-specific preferences. If not
	            provided, but 'doc' is, its current URL is used instead.

	     @return OpStatus::OK on success, OpStatus::ERR if ECMAScript is
	             disabled or a document with no ECMAScript environment was
	             supplied or OpStatus::ERR_NO_MEMORY on OOM. */

	static OP_STATUS Create(ES_Environment *&env, ES_Runtime *shadow_runtime, ES_ThreadScheduler *shadow_scheduler, ES_AsyncInterface *shadow_async_interface, BOOL ignore_prefs, URL *site_url);
	/**< Create an ES_Environment object.  Same function as other Create() method, but allowing the shadow triple
	     (runtime, scheduler, async_inteface) to be specified separately. */

	static void Destroy(ES_Environment *env);
	/**< Destroy the ES_Environment object.  Will deallocate it.  If the
	     environment shadows a document's DOM environment, nothing in it
	     will be destroyed.

	     @param env Environment to destroy. */

	BOOL Enabled();
	/**< Check if the environment is enabled.

	     @return TRUE or FALSE. */

	ES_Runtime *GetRuntime();
	/**< Retrieve the environment's ECMAScript runtime.

	     @return An ES_Runtime pointer (never NULL). */

	ES_ThreadScheduler *GetScheduler();
	/**< Retrieve the environment's thread scheduler.

	     @return An ES_ThreadScheduler pointer (never NULL). */

	ES_AsyncInterface *GetAsyncInterface();
	/**< Retrieve the environment's interface for asynchoronous execution.

	     @return An ES_AsyncInterface pointer (never NULL). */

	ES_Object *GetGlobalObject();
	/**< Retrieve the environment's global object.

	     @return An ES_Object pointer (never NULL). */

private:
	ES_Environment();
	/**< Private constructor.  Never mind. */

	~ES_Environment();
	/**< Private destructor.  Never mind. */

	ES_Runtime *runtime;
	/**< The environment's runtime. */

	ES_ThreadScheduler *scheduler;
	/**< The environment's thread scheduler.  Use the function
	     ES_Environment::GetScheduler() to access. */
	
	ES_AsyncInterface *asyncif;
	/**< The environment's interface for asynchoronous execution.  Use the
	     function ES_Environment::GetAsyncInterface() to access. */

	BOOL standalone;
	/**< TRUE is this is a standalone environment, FALSE if it shadows a
	     DOM environment in a document. */
};

#endif /* ESUTILS_ES_ENVIRONMENT_SUPPORT */
#endif /* ES_UTILS_ESENVIRONMENT_H */
