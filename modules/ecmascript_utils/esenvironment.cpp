/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef ESUTILS_ES_ENVIRONMENT_SUPPORT

#include "modules/ecmascript_utils/esenvironment.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/ecmascript/ecmascript.h"
#include "modules/prefs/prefsmanager/collections/pc_js.h"
#include "modules/doc/frm_doc.h"

OP_STATUS
ES_Environment::Create(ES_Environment *&env, FramesDocument *frames_doc, BOOL ignore_prefs, URL *site_url)
{
	if (frames_doc)
		return Create(env, frames_doc->GetESRuntime(), frames_doc->GetESScheduler(), frames_doc->GetESAsyncInterface(), ignore_prefs, site_url ? site_url : &frames_doc->GetURL());
	else
		return Create(env, NULL, NULL, NULL, ignore_prefs, site_url);
}

OP_STATUS
ES_Environment::Create(ES_Environment *&env, ES_Runtime *shadow_runtime, ES_ThreadScheduler *shadow_scheduler, ES_AsyncInterface *shadow_async_interface, BOOL ignore_prefs, URL *site_url)
{
	if (!ignore_prefs)
	{
		BOOL ecmascript_enabled = g_pcjs->GetIntegerPref(PrefsCollectionJS::EcmaScriptEnabled, site_url ? site_url->GetServerName() : NULL);

		if (!ecmascript_enabled)
			return OpStatus::ERR;
	}

	env = OP_NEW(ES_Environment, ());
	if (!env)
		return OpStatus::ERR_NO_MEMORY;

	if (shadow_runtime || shadow_scheduler || shadow_async_interface)
	{
		env->runtime = shadow_runtime;
		env->scheduler = shadow_scheduler;
		env->asyncif = shadow_async_interface;
		env->standalone = FALSE;
	}
	else
	{
		EcmaScript_Object *global_object = OP_NEW(EcmaScript_Object, ());
		if (!global_object)
			goto oom_error;

		env->runtime = OP_NEW(ES_Runtime, ());
		if (!env->runtime || OpStatus::IsMemoryError(env->runtime->Construct(global_object, "Global")))
		{
			OP_DELETE(global_object);
			OP_DELETE(env->runtime);
			env->runtime = NULL;
			goto oom_error;
		}

		env->scheduler = ES_ThreadScheduler::Make(env->runtime, ignore_prefs);
		if (!env->scheduler)
			goto oom_error;

		env->asyncif = OP_NEW(ES_AsyncInterface, (env->runtime, env->scheduler));
		if (!env->asyncif)
			goto oom_error;

		env->runtime->SetESScheduler(env->scheduler);
		env->runtime->SetESAsyncInterface(env->asyncif);
	}

	return OpStatus::OK;

oom_error:
	OP_DELETE(env);
	env = NULL;
	return OpStatus::ERR_NO_MEMORY;
}

void
ES_Environment::Destroy(ES_Environment *env)
{
	OP_DELETE(env);
}

BOOL
ES_Environment::Enabled()
{
	return runtime && runtime->Enabled();
}

ES_Runtime *
ES_Environment::GetRuntime()
{
	return runtime;
}

ES_ThreadScheduler *
ES_Environment::GetScheduler()
{
	return scheduler;
}

ES_AsyncInterface *
ES_Environment::GetAsyncInterface()
{
	return asyncif;
}

ES_Object *
ES_Environment::GetGlobalObject()
{
	return runtime ? (ES_Object *) runtime->GetGlobalObject() : NULL;
}

ES_Environment::ES_Environment()
	: runtime(NULL),
	  scheduler(NULL),
	  asyncif(NULL),
	  standalone(TRUE)
{
}

ES_Environment::~ES_Environment()
{
	if (standalone)
	{
		if (asyncif)
			OP_DELETE(asyncif);

		if (scheduler)
		{
			scheduler->RemoveThreads(TRUE);
			OP_DELETE(scheduler);
		}

		if (runtime)
			runtime->Detach();
	}
}

#endif // ESUTILS_ES_ENVIRONMENT_SUPPORT
