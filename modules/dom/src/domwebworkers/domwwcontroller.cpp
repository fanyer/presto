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
#include "modules/dom/src/dominternaltypes.h"
#include "modules/dom/src/domenvironmentimpl.h"

#include "modules/dom/src/domwebworkers/domwebworkers.h"
#include "modules/dom/src/domwebworkers/domwwcontroller.h"

#include "modules/prefs/prefsmanager/collections/pc_js.h"

DOM_WebWorkerController::~DOM_WebWorkerController()
{
    web_worker_objects.Clear();
    if (g_pcjs)
        g_pcjs->UnregisterListener(this);
    DetachWebWorkers();
}

void
DOM_WebWorkerController::SetDocumentManagerL(DocumentManager *doc_manager, ES_Runtime *es_runtime)
{
    /* Need to configure up the window...but can only do that when once we've got access to document manager. */
    if (!worker_document_manager && doc_manager)
    {
        worker_document_manager = doc_manager;
        DOM_Runtime *runtime = static_cast<DOM_Runtime *>(es_runtime);
        runtime->GetEnvironment()->RegisterCallbacksL(DOM_Environment::GLOBAL_CALLBACK, current_worker);
    }
    else
        SetDocumentManager(doc_manager);
}

OP_STATUS
DOM_WebWorkerController::AddWebWorkerObject(DOM_WebWorkerObject *w)
{
    AsListElement<DOM_WebWorkerObject> *element = OP_NEW(AsListElement<DOM_WebWorkerObject>, (w));
    RETURN_OOM_IF_NULL(element);
    element->Into(&web_worker_objects);
    return OpStatus::OK;
}

void
DOM_WebWorkerController::RemoveWebWorkerObject(DOM_WebWorkerObject *worker)
{
    for (AsListElement<DOM_WebWorkerObject> *element = web_worker_objects.First(); element; element = element->Suc())
         if (element->ref == worker)
         {
             element->Out();
             OP_DELETE(element);
             return;
         }
}

OP_STATUS
DOM_WebWorkerController::AddWebWorkerDomain(DOM_WebWorkerDomain *domain)
{
    for (AsListElement<DOM_WebWorkerDomain> *element = web_worker_domains.First(); element; element = element->Suc())
        if (element->ref == domain)
            return OpStatus::OK;

    AsListElement<DOM_WebWorkerDomain> *element = OP_NEW(AsListElement<DOM_WebWorkerDomain>, (domain));
    RETURN_OOM_IF_NULL(element);
    element->Into(&web_worker_domains);
    return OpStatus::OK;
}

void
DOM_WebWorkerController::RemoveWebWorkerDomain(DOM_WebWorkerDomain *domain)
{
    for (AsListElement<DOM_WebWorkerDomain> *element = web_worker_domains.First(); element; element = element->Suc())
         if (element->ref == domain)
         {
             element->Out();
             OP_DELETE(element);
             return;
         }
}

void
DOM_WebWorkerController::DetachWebWorkers()
{
    /* Let go of anything web worker-like; after this call, this environment
       cannot reliably access or use any of these resources. */
    while (AsListElement<DOM_WebWorkerDomain> *element = web_worker_domains.First())
    {
        element->Out();
        if (!element->ref->IsClosing())
            DOM_WebWorkerDomain::Detach(element->ref, owner);

        OP_DELETE(element);
    }

}

void
DOM_WebWorkerController::GCTrace(ES_Runtime *runtime)
{
    for (AsListElement<DOM_WebWorkerObject> *element = web_worker_objects.First(); element; element = element->Suc())
        runtime->GCMark(element->ref);

    /* Note: ports are assumed to be protected or otherwise traced. */
}

BOOL
DOM_WebWorkerController::AllowNewWorkerContext()
{
    if (num_worker_contexts < max_browser_worker_contexts)
    {
        num_worker_contexts++;
        return TRUE;
    }
    else
       return FALSE;
}

void
DOM_WebWorkerController::DropWorkerContext()
{
    if (num_worker_contexts > 0)
        num_worker_contexts--;
}

/* virtual */ void
DOM_WebWorkerController::PrefChanged(OpPrefsCollection::Collections id, int pref, int new_value)
{
    if (OpPrefsCollection::JS == id && pref == PrefsCollectionJS::MaxWorkersPerWindow)
        max_browser_worker_contexts = ( new_value >= 0 ? new_value : DEFAULT_MAX_WEBWORKERS_PER_WINDOW);
}

OP_STATUS
DOM_WebWorkerController::InitWorkerPrefs(DOM_EnvironmentImpl *environment)
{
    if (int max_wcs = g_pcjs->GetIntegerPref(PrefsCollectionJS::MaxWorkersPerWindow, DOM_EnvironmentImpl::GetHostName(environment ? environment->GetFramesDocument() : NULL)))
        max_browser_worker_contexts = max_wcs;
    else
        max_browser_worker_contexts = DEFAULT_MAX_WEBWORKERS_PER_WINDOW;

    return g_pcjs->RegisterListener(this);
}

#ifdef ECMASCRIPT_DEBUGGER
OP_STATUS
DOM_WebWorkerController::GetDomainRuntimes(OpVector<ES_Runtime> &es_runtimes)
{
    for (AsListElement<DOM_WebWorkerDomain> *domain = web_worker_domains.First(); domain; domain = domain->Suc())
        if (DOM_EnvironmentImpl *environment = domain->ref->GetEnvironment())
            RETURN_IF_ERROR(environment->GetESRuntimes(es_runtimes));

    return OpStatus::OK;
}
#endif // ECMASCRIPT_DEBUGGER

#endif // DOM_WEBWORKERS_SUPPORT

