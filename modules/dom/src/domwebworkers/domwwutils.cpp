/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 2009-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** @author sof
*/
#include "core/pch.h"

#ifdef DOM_WEBWORKERS_SUPPORT
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domwebworkers/domwwutils.h"
#include "modules/dom/src/domevents/domeventlistener.h"

#include "modules/dom/src/domwebworkers/domwebworkers.h"
#include "modules/dom/domutils.h"

#include "modules/dom/src/domwebworkers/domcrossmessage.h"
#include "modules/dom/src/domwebworkers/domcrossutils.h"

/* static */ BOOL
DOM_WebWorker_Utils::CheckImportScriptURL(const URL &worker_script_url)
{
    if (worker_script_url.IsEmpty() || !worker_script_url.IsValid() || DOM_Utils::IsOperaIllegalURL(worker_script_url))
        return FALSE;

#ifndef ALLOW_WORKER_DATA_URLS
    if (worker_script_url.Type() == URL_DATA)
        return FALSE;
#endif // !ALLOW_WORKER_DATA_URLS

    return TRUE;
}

/*
 * Utility method tha locates and returns the FramesDocument of the browser context that spawned
 * the given Worker (and, if of interest, its environment.)
 */
/* static */ FramesDocument*
DOM_WebWorker_Utils::GetWorkerFramesDocument(DOM_WebWorker *worker, DOM_EnvironmentImpl **origin_environment)
{
    for (; worker && !worker->IsClosed(); worker = worker->GetParentWorker())
    {
        OP_ASSERT(worker->GetWorkerDomain() && worker->GetWorkerDomain()->GetCurrentDomainOwner());
        /* I am being defensive here.. */
        if (!worker->GetWorkerDomain() || !worker->GetWorkerDomain()->GetCurrentDomainOwner())
            return NULL;

        DOM_EnvironmentImpl* environment = worker->GetWorkerDomain()->GetCurrentDomainOwner();
        if (environment->GetFramesDocument())
        {
            if (origin_environment)
                *origin_environment = environment;
            return environment->GetFramesDocument();
        }
    }
    return NULL;
}

/* static */ OP_STATUS
DOM_WebWorker_Utils::ReportQuotaExceeded(DOM_Object *origin_object, BOOL for_window, unsigned limit)
{
    OpConsoleEngine::Message msg(OpConsoleEngine::EcmaScript, OpConsoleEngine::Information);

    DOM_Runtime *origin_runtime = origin_object->GetRuntime();
    FramesDocument *frames_doc;
    if (origin_object->IsA(DOM_TYPE_WEBWORKERS_WORKER))
        frames_doc = DOM_WebWorker_Utils::GetWorkerFramesDocument(static_cast<DOM_WebWorker *>(origin_object), NULL);
    else
        frames_doc = origin_runtime->GetFramesDocument();

    if (frames_doc)
        msg.window = frames_doc->GetWindow()->Id();

    RETURN_IF_ERROR(origin_runtime->GetDisplayURL(msg.url));
    RETURN_IF_ERROR(msg.message.AppendFormat("Maximum number of Web Worker instances\
(%d) exceeded for this %s.\nIf the content legitimately requires more instances to\
 function,\nplease set opera:config#UserPrefs|MaxWebWorkersPer%s to a higher value.", limit, for_window ? UNI_L("window") : UNI_L("session"), for_window ? UNI_L("Window") : UNI_L("Session")));

    TRAPD(status, g_console->PostMessageL(&msg));
    return status;
}

/* virtual */ OP_STATUS
DOM_WebWorker_ErrorEventDefault::DefaultAction(BOOL canc)
{
    /* It looks odd, but the return value listener will set prevent-default to TRUE
       if the onerror handler returned false (=> the handler signalling that the event was "handled")
       In which case we want to stop right here. If not, propagate the unhandled error event. */
    DOM_Object *object = GetTarget();
    if (propagate_up && !GetPreventDefault() && !GetStopPropagation())
    {
        propagate_up = FALSE;
        if (object && object->IsA(DOM_TYPE_WEBWORKERS_WORKER_OBJECT))
            return (static_cast<DOM_WebWorkerObject *>(object))->PropagateErrorException(this);
        else if (object && object->IsA(DOM_TYPE_WEBWORKERS_WORKER))
        {
            DOM_WebWorker *worker_scope = static_cast<DOM_WebWorker *>(object);
            return worker_scope->PropagateErrorException(this);
        }
    }
    else if (object && object->IsA(DOM_TYPE_WEBWORKERS_WORKER))
    {
        DOM_WebWorker *worker_scope = static_cast<DOM_WebWorker *>(object);
        worker_scope->ProcessedException(this);
    }
    return OpStatus::OK;
}

#endif // DOM_WEBWORKERS_SUPPORT
