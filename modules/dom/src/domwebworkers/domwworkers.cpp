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

#include "modules/dom/src/domwebworkers/domwebworkers.h"
#include "modules/dom/src/domwebworkers/domwworkers.h"

#include "modules/prefs/prefsmanager/collections/pc_js.h"

#ifdef DOM_WEBWORKERS_WORKER_STAT_SUPPORT
#include "modules/dom/src/domwebworkers/domwwinfo.h"
#endif // DOM_WEBWORKERS_WORKER_STAT_SUPPORT

/* ++++: DOM_WebWorkers */

DOM_WebWorkers::~DOM_WebWorkers()
{
    while (DOM_WebWorkerDomain *domain = web_workers.First())
        DOM_WebWorkerDomain::Detach(domain);

    OP_ASSERT(num_worker_contexts == 0);
}

/* static */ OP_STATUS
DOM_WebWorkers::Init()
{
    OP_ASSERT(g_webworkers == NULL);
    if ((g_webworkers = OP_NEW(DOM_WebWorkers, ())) == NULL)
        return OpStatus::ERR_NO_MEMORY;

    if (g_pcjs)
    {
        if (int max_wcs = g_pcjs->GetIntegerPref(PrefsCollectionJS::MaxWorkersPerSession))
            g_webworkers->max_worker_instances = max_wcs;
        else
            g_webworkers->max_worker_instances = DEFAULT_MAX_WEBWORKERS_PER_SESSION;

        g_pcjs->RegisterListenerL(g_webworkers);
    }
    return OpStatus::OK;
}

/* static */ void
DOM_WebWorkers::Shutdown(DOM_WebWorkers *workers)
{
    OP_ASSERT(g_webworkers && g_webworkers == workers);

    if (g_pcjs)
        g_pcjs->UnregisterListener(workers);

    OP_DELETE(workers);
}

/* virtual */ void
DOM_WebWorkers::PrefChanged(OpPrefsCollection::Collections id, int pref, int new_value)
{
    if (OpPrefsCollection::JS == id && pref == PrefsCollectionJS::MaxWorkersPerSession)
        max_worker_instances = ( new_value >= 0 ? new_value : DEFAULT_MAX_WEBWORKERS_PER_SESSION);
}

BOOL
DOM_WebWorkers::CanCreateWorker()
{
    if (num_worker_contexts < max_worker_instances)
    {
        num_worker_contexts++;
        return TRUE;
    }
    else
        return FALSE;
}

void
DOM_WebWorkers::DecrementWorkerCount()
{
    if (num_worker_contexts > 0)
        --num_worker_contexts;
}

OP_STATUS
DOM_WebWorkers::AddWorkerDomain(DOM_WebWorkerDomain *domain)
{
    domain->Into(&web_workers);
    return OpStatus::OK;
}

BOOL
DOM_WebWorkers::FindWebWorkerDomain(DOM_WebWorkerDomain *&domain, const URL & origin_url, const uni_char *name)
{
    /* No sharing or reuse is done for dedicated workers, so return IS_FALSE for these.
     * We _may_ want to change this and allow dedicated workers to share domains (=> execution contexts),
     * hence letting FindWebWorkerDomain() be the arbiter of this decision makes sense (even now.)
     */
    if (!name)
        return FALSE;

    for (DOM_WebWorkerDomain *existing = web_workers.First(); existing; existing = existing->Suc())
        /* Put all Workers with the same origin into one domain (if they are shared.) */
        if (!existing->IsDedicated() && DOM_Object::OriginCheck(existing->GetOriginURL(), origin_url))
        {
            /* Not so easy to achieve...sharing a DOM_Environment can't be done right now.
               Hence we are forced to perform the same checks here as in FindSharedWorker(), i.e.,
               domains are unique for shared workers too. */
            if (existing->FindSharedWorker(origin_url, name) != NULL)
            {
                domain = existing;
                return TRUE;
            }
        }

    /* A shared worker, but with no existing domain to be based in => create new. */
    return FALSE;
}

/* static */ void
DOM_WebWorkers::RemoveWebWorkerDomain(DOM_WebWorkerDomain *wd)
{
    OP_ASSERT(wd);
    wd->Out();
    if (g_webworkers)
        g_webworkers->DecrementWorkerCount();
}

#ifdef DOM_WEBWORKERS_WORKER_STAT_SUPPORT
OP_STATUS
DOM_WebWorkers::GetStatistics(List<DOM_WebWorkerInfo> *workers)
{
    for (DOM_WebWorkerDomain *domain = web_workers.First(); domain; domain = domain->Suc())
        domain->GetStatistics(workers);

    return OpStatus::OK;
}
#endif // DOM_WEBWORKERS_WORKER_STAT_SUPPORT

#endif // DOM_WEBWORKERS_SUPPORT
