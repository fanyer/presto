/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 2009-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef DOM_WW_WORKERS_H
#define DOM_WW_WORKERS_H

#ifdef DOM_WEBWORKERS_SUPPORT

#include "modules/dom/src/domobj.h"
#include "modules/dom/domtypes.h"
#include "modules/dom/src/domruntime.h"
#include "modules/dom/src/domevents/domevent.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domevents/domevent.h"
#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/ecmascript_utils/esenvironment.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/doc/frm_doc.h"

#include "modules/dom/src/domwebworkers/domwweventqueue.h"

class DOM_WebWorkerDomain;

/** DOM_WebWorkers: The DOM sub-module class holding the current set of
    Web Worker domains, maintaining a lookup map from origin URLs to
    the associated DOM_WebWorkerDomain. */
class DOM_WebWorkers
    : public OpPrefsListener /* pref for max-per-session workers */
{
private:
    List<DOM_WebWorkerDomain> web_workers;
    /**< List of active Worker domains. */

    unsigned num_worker_contexts;
    /**< The current number of active contexts. Should track the number of items in the 'web_workers' vector. */

    unsigned max_worker_instances;
    /**< Current upper limit on the number of worker instances. */

public:
    static OP_STATUS Init();
    /**< Module initialisation entry point; called by DomModule::Init(). */

    DOM_WebWorkers()
        : num_worker_contexts(0),
          max_worker_instances(DEFAULT_MAX_WEBWORKERS_PER_SESSION)
    {
    }

    virtual ~DOM_WebWorkers();

    static void Shutdown(DOM_WebWorkers *worker);
    /**< Tear down method called as part of closing down the module (DOM.) */

    OP_STATUS AddWorkerDomain(DOM_WebWorkerDomain *dom);
    /**< Add the given domain to the list of currently active domains.
         Returns OpStatus::ERR_NO_MEMORY on OOM, OpStatus::OK otherwise. */

    BOOL FindWebWorkerDomain(DOM_WebWorkerDomain *&wd, const URL &url, const uni_char *name);
    /**< Perform domain lookup based on a given script URL and an optional name. If successful,
         the active domain associated with that pair is returned. For a dedicated worker, the name
         is NULL and no sharing (or reuse) is attempted, hence OpStatus::IS_FALSE is always returned.

        @param [out]wd The result parameter for returning the matching domain,
               if one exists.
        @param url The script URL to perform resolution with respect to.
        @param name If supplied, require that the matching domain has the same name.
               If the empty string, match any with the same 'url' and a non-NULL
               name. If NULL, then only match domains with a NULL name (and the
               same URL.) The distinction between NULL and empty string is
               required to provide the resolution required for shared workers
               without a given name _and_ distinguish these from dedicated worker
               domains.
        @return FALSE if no matching domain, TRUE if there is one
                (which is then supplied via the 'wd' reference.) */

    static void RemoveWebWorkerDomain(DOM_WebWorkerDomain *wd);
    /**< Unchain the given domain from the list of active domains, decrementing
         the number of active domain count. */

    BOOL CanCreateWorker();
    /**< If TRUE, a new Worker may, modulo other local constraints, be
         created. If FALSE, the caller is not permitted to create new
         instance and must abort the creation attempt. */

    unsigned GetMaxWorkersPerSession() { return max_worker_instances; }

    virtual void PrefChanged(enum OpPrefsCollection::Collections id, int pref, int newvalue);
    /**<  Listener for changes to max session preference. */

#ifdef DOM_WEBWORKERS_WORKER_STAT_SUPPORT
    OP_STATUS GetStatistics(List<DOM_WebWorkerInfo> *workers);
    /**< If available, report back info on currently active web workers and their execution contexts. */
#endif // DOM_WEBWORKERS_WORKER_STAT_SUPPORT

private:
    void DecrementWorkerCount();
    /**< Notify the manager that one Worker has been shut down. */
};


#endif // DOM_WEBWORKERS_SUPPORT
#endif // DOM_WW_WORKERS_H
