/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 2009-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#ifdef DOM_WEBWORKERS_SUPPORT
#include "modules/dom/src/domenvironmentimpl.h"

#include "modules/dom/src/domwebworkers/domwebworkers.h"
#include "modules/dom/src/domwebworkers/domwwdomain.h"
#include "modules/dom/src/domwebworkers/domwwutils.h"

#include "modules/prefs/prefsmanager/collections/pc_js.h"

#ifdef DOM_WEBWORKERS_WORKER_STAT_SUPPORT
#include "modules/dom/src/domwebworkers/domwwinfo.h"
#endif // DOM_WEBWORKERS_WORKER_STAT_SUPPORT

DOM_WebWorkerDomain::~DOM_WebWorkerDomain()
{
    DOM_WebWorkers::RemoveWebWorkerDomain(this);
    while (AsListElement<DOM_EnvironmentImpl> *owner = owners.First())
    {
        owner->Out();
        if (owner->ref)
            owner->ref->GetWorkerController()->RemoveWebWorkerDomain(this);
        OP_DELETE(owner);
    }

    DOM_Environment::Destroy(environment);
    environment = NULL;
}

/* static */ void
DOM_WebWorkerDomain::Detach(DOM_WebWorkerDomain *domain, DOM_EnvironmentImpl *o)
{
    domain->RemoveDomainOwner(o);

    if (domain->owners.Empty() && !domain->IsClosing())
    {
        domain->Shutdown();
        OP_DELETE(domain);
    }
}

DOM_EnvironmentImpl*
DOM_WebWorkerDomain::GetCurrentDomainOwner()
{
    return (owners.Empty() ? NULL : owners.First()->ref);
}

#ifdef ECMASCRIPT_DEBUGGER
OP_STATUS
DOM_WebWorkerDomain::GetWindows(OpVector<Window> &windows)
{
    AsListElement<DOM_EnvironmentImpl> *environment = owners.First();

    while (environment)
    {
        FramesDocument *doc = environment->ref->GetFramesDocument();
        if (doc)
            RETURN_IF_MEMORY_ERROR(windows.Add(doc->GetWindow()));
        environment = environment->Suc();
    }

    return OpStatus::OK;
}
#endif // ECMASCRIPT_DEBUGGER

OP_STATUS
DOM_WebWorkerDomain::AddDomainOwner(DOM_EnvironmentImpl *owner)
{
    for (AsListElement<DOM_EnvironmentImpl> *element = owners.First(); element; element = element->Suc())
        if (element->ref == owner)
            return OpStatus::OK;

    AsListElement<DOM_EnvironmentImpl> *element = OP_NEW(AsListElement<DOM_EnvironmentImpl>, (owner));
    RETURN_OOM_IF_NULL(element);
    element->Into(&owners);
    return OpStatus::OK;
}

void
DOM_WebWorkerDomain::RemoveDomainOwner(DOM_EnvironmentImpl *impl)
{
    AsListElement<DOM_EnvironmentImpl> *element = owners.First();
    while (element)
    {
        AsListElement<DOM_EnvironmentImpl> *next = element->Suc();
        if (!impl || element->ref == impl)
        {
            element->Out();

            DOM_WebWorkerController *controller = element->ref->GetWorkerController();
            OP_ASSERT(controller);
            controller->RemoveWebWorkerDomain(this);
            if (!is_closing)
                controller->DropWorkerContext();

            OP_DELETE(element);
            if (impl)
                return;
        }
        element = next;
    }
    OP_ASSERT(!impl);
}

void
DOM_WebWorkerDomain::Shutdown()
{
    if (is_closing)
        return;

    DOM_WebWorkers::RemoveWebWorkerDomain(this);

    /* Not messing about here, if domain is shut down, we tear down the worker's context too. */
    if (!is_closing && environment)
    {
        is_closing = TRUE;
        /* But first, trigger termination of attached workers */
        while (AsListElement<DOM_WebWorker> *element = workers.First())
            element->ref->CloseWorker();
    }

    /* It is resistant to NULLness, but looks tidier this way. */
    if (environment)
    {
        // Even though there is no document there can be an lsloader
        // associated with this environment which needs to be aborted.
        environment->BeforeDestroy();
        DOM_Environment::Destroy(environment);
    }
    environment = NULL;
}


static OP_STATUS
SetDocumentManager(DocumentManager *docman, DOM_EnvironmentImpl *impl, ES_Runtime *runtime)
{
    TRAPD(status, impl->GetWorkerController()->SetDocumentManagerL(docman, runtime));
    return status;
}

/* static */ OP_STATUS
DOM_WebWorkerDomain::Make(DOM_WebWorkerDomain *&domain, DOM_Object *origin_object, const URL &url, BOOL is_dedicated, ES_Value *exception_value)
{
    DOM_EnvironmentImpl *owner = origin_object->GetEnvironment();

    if (!owner->GetWorkerController()->AllowNewWorkerContext())
    {
        RETURN_IF_ERROR(DOM_WebWorker_Utils::ReportQuotaExceeded(origin_object, TRUE, owner->GetWorkerController()->GetMaxWorkersPerWindow()));
        origin_object->CallDOMException(DOM_Object::QUOTA_EXCEEDED_ERR, exception_value); // ignore ES_EXCEPTION.
        return OpStatus::ERR;
    }

    RETURN_OOM_IF_NULL(domain = OP_NEW(DOM_WebWorkerDomain, ()));
    RETURN_IF_ERROR(domain->AddDomainOwner(owner));
    domain->origin_url = url;
    domain->is_dedicated = is_dedicated;

    DOM_Environment *environment = NULL;

    DOM_Runtime::Type type = is_dedicated ? DOM_Runtime::TYPE_DEDICATED_WEBWORKER : DOM_Runtime::TYPE_SHARED_WEBWORKER;
    RETURN_IF_ERROR(DOM_EnvironmentImpl::Make(environment, NULL/*frames_document*/, &domain->origin_url, type));

    DOM_WebWorker *worker = static_cast<DOM_WebWorker *>(environment->GetWindow());
    worker->SetWorkerDomain(domain);

    /* We need to get at the URL of the context that spawned the domain
       from within the DOM_Environment of the Worker, but as it doesn't
       have a FramesDocument to get at, the environment keeps a (weak)
       reference around to the DocumentManager of our creator. */
    DocumentManager *docman;
    if (owner->GetFramesDocument())
        docman = owner->GetFramesDocument()->GetDocManager();
    else
        docman = owner->GetWorkerController()->GetWorkerDocManager();

    DOM_EnvironmentImpl *impl = static_cast<DOM_EnvironmentImpl*>(environment);
    RETURN_IF_ERROR(SetDocumentManager(docman, impl, environment->GetRuntime()));

    domain->environment = impl;
#ifdef ECMASCRIPT_DEBUGGER
    if (g_ecmaManager->GetDebugListener())
        g_ecmaManager->GetDebugListener()->RuntimeCreated(impl->GetRuntime());
#endif

    /* Hook new domain into the _owner_ document so that we are notified
       of document destruction.

       Note: the new domain is attached to the DOM environment of the
       owning browser context, _not_  the environment that we set up to
       execute the Workers of this domain. */
    return owner->GetWorkerController()->AddWebWorkerDomain(domain);
}

DOM_WebWorker *
DOM_WebWorkerDomain::FindSharedWorker(const URL &origin_url, const uni_char *name)
{
    /* It can be the empty string, but not NULL. */
    OP_ASSERT(name);

    for (AsListElement<DOM_WebWorker> *element = workers.First(); element; element = element->Suc())
    {
        DOM_WebWorker *w = element->ref;
        OP_ASSERT(!w->IsClosed());

        /* This matches current allocation of workers to domains, but will leave in explicit check. */
        if (!DOM_Object::OriginCheck(w->GetLocationURL(), origin_url))
            continue;

        /* Can't share a dedicated worker.. */
        if (w->IsDedicated())
            continue;
        /* An existing shared worker matches iff:
            - has same name as the given one,
            - or a match-all empty string is supplied for incoming name. */
        if (*name && uni_str_eq(name, w->GetWorkerName()))
            return w;
        if (!*name)
        {
            OpString url_a;
            OpString url_b;
            if (OpStatus::IsError(origin_url.GetAttribute(URL::KUniName_With_Fragment, url_a)))
                return NULL;
            if (OpStatus::IsError(w->GetLocationURL().GetAttribute(URL::KUniName_With_Fragment, url_b)))
                return NULL;
            if (uni_str_eq(url_a.CStr(), url_b.CStr()))
                return w;
        }
    }
    return NULL;
}

OP_STATUS
DOM_WebWorkerDomain::AddWebWorker(DOM_WebWorker *ww)
{
    AsListElement<DOM_WebWorker> *element = OP_NEW(AsListElement<DOM_WebWorker>, (ww));
    RETURN_OOM_IF_NULL(element);
    element->Into(&workers);
    return OpStatus::OK;
}

void
DOM_WebWorkerDomain::RemoveWebWorker(DOM_WebWorker *w)
{
    for (AsListElement<DOM_WebWorker> *element = workers.First(); element; element = element->Suc())
        if (element->ref == w)
        {
            element->Out();
            OP_DELETE(element);

            if (workers.Empty())
                DOM_WebWorkerDomain::Detach(this);
            return;
        }
}

#ifdef DOM_WEBWORKERS_WORKER_STAT_SUPPORT
OP_STATUS
DOM_WebWorkerDomain::GetStatistics(List<DOM_WebWorkerInfo> *info)
{
    OP_STATUS status;
    for (AsListElement<DOM_WebWorker> *element = workers.First(); element; element = element->Suc())
    {
        DOM_WebWorker *ww = element->ref;
        DOM_WebWorkerInfo *wi = OP_NEW(DOM_WebWorkerInfo, ());
        RETURN_OOM_IF_NULL(wi);
        if (OpStatus::IsError(status = ww->GetStatistics(wi)))
        {
            OP_DELETE(wi);
            return status;
        }
        wi->Into(info);
    }
    return OpStatus::OK;
}
#endif // DOM_WEBWORKERS_WORKER_STAT_SUPPORT

#endif // DOM_WEBWORKERS_SUPPORT
