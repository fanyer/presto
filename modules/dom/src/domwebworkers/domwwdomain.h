/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 2009-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef DOM_WW_DOMAIN_H
#define DOM_WW_DOMAIN_H

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

/** DOM_WebWorkerDomain: A web worker 'domain' controls and shares an
    execution context between one or more worker instances/threads.
    i.e., a DOM environment, scheduler and (logical) heap is common
    across them. For a dedicated worker, it would be the sole
    inhabitant, shared workers could be grouped together.

    Presently, domains aren't shared. One per worker, with the
    domain handling the set up and tear down of execution context
    and management of running a worker within such a context.

    The domain is created upon initial instantiation of a Web Worker
    from some browser context. If the domain represents a shared worker,
    subsequent browser contexts will then register with the domain
    (as an 'owner') and thereby gain access to the execution context.

    Upon dropping the connection to a worker, the browser context will
    unregister with the domain. Once its list of owners is empty,
    the domain will shut down the worker. */
class DOM_WebWorkerDomain
    :  public ListElement<DOM_WebWorkerDomain>
{
private:
    List<AsListElement<DOM_WebWorker> > workers;
    /**< The list of active web workers running within this domain. */

    List<AsListElement<DOM_EnvironmentImpl> > owners;
    /**< The list of environments currently using the domain's worker. */

    DOM_EnvironmentImpl *environment;
    /**< The environment in which the Web Worker operates/executes. */

    BOOL is_dedicated;
    /**< If TRUE, a singular domain hosting at most one Worker. */

    URL origin_url;
    /**< The base URL of the domain; the workers residing in this domain
         will currently have to have the same origin. */

    BOOL is_closing;
    /**< Flag indicating that the domain is in the process of
         shutting down. */

    DOM_WebWorkerDomain()
        : environment(NULL),
          is_dedicated(TRUE),
          is_closing(FALSE)
    {
    }

public:
    static OP_STATUS Make(DOM_WebWorkerDomain *&domain, DOM_Object *origin_object, const URL &url, BOOL is_dedicated, ES_Value *return_value);
    /**< Create a new Web Worker domain instance, failing on OOM or if
         the requested domain conflicts with instances already created,
         returns OpStatus::ERR along with a DOM exception.

         Attached to this new domain is a Worker execution context, a
         DOM_Environment but without a document. That is, its FramesDocument
         will be NULL and the global object is a WorkerGlobalScope object.

         @param[out]domain The resulting domain.
         @param origin_object The object creating the new domain. If
                successful, the new domain is registered with its environment.
                On failure, exceptions will be reported with respect to
                its runtime.
         @param url The base URL for the new domain.
         @param is_dedicated TRUE if this domain host a 'dedicated' worker instance.
         @param return_value if non-NULL, contains a DOM exception if the operation failed.

         @return OpStatus::OK if successful,
                 OpStatus::ERR_NO_MEMORY on OOM.
                 OpStatus::ERR in case of conflicting domain creation request. */

    virtual ~DOM_WebWorkerDomain();

    static void Detach(DOM_WebWorkerDomain *domain, DOM_EnvironmentImpl *owner = NULL);
    /**< When the browsing context goes away or otherwise determines that
         the Web Worker resources it has sprung into action are no longer
         needed, Detach() is called. It unregister its interest, which may
         notify the Worker execution context to close. Worker domains are
         independent of the environments that created them, but once an
         environment has detached it may no longer access the worker
         and its domain. */

    BOOL IsDedicated() { return is_dedicated; }
    /**< If TRUE, this domain is singular in that it will permit at most
         one dedicated worker to be active. */

    const URL &GetOriginURL() { return origin_url; }
    /**< Return the URL for this domain. */

    DOM_WebWorker *FindSharedWorker(const URL &origin, const uni_char *name);
    /**< Return the web worker matching the given origin and name.

         @return NULL if no matching worker found in this domain;
                 otherwise a pointer to an active worker instance. */

    DOM_EnvironmentImpl* GetEnvironment() { return environment; }
    /**< Return the environment that the Workers are executing
         in the context of. */

    OP_STATUS AddDomainOwner(DOM_EnvironmentImpl *owner);
    /**< Register an environment as using the worker represented
         by this domain. */

    void RemoveDomainOwner(DOM_EnvironmentImpl *owner);
    /**< The given environment is no longer using the domain's
         associated worker; unregister it as an 'owner'. */

    DOM_EnvironmentImpl* GetCurrentDomainOwner();
    /**< Web Workers are instantiated from a browsing context, with domains
         keep track of them all (only one for dedicated workers, multiple
         for shared.) Return a reference to the current/first one still alive. */

    BOOL IsClosing() { return is_closing; }
    /**< If TRUE, this domain is in the process of shutting down. */

#ifdef DOM_WEBWORKERS_WORKER_STAT_SUPPORT
    OP_STATUS GetStatistics(List<DOM_WebWorkerInfo> *info);
    /**< If available, report back info on currently active web
         workers for this domain. */
#endif // DOM_WEBWORKERS_WORKER_STAT_SUPPORT

#ifdef ECMASCRIPT_DEBUGGER
    OP_STATUS GetWindows(OpVector<Window> &windows);
    /**< Adds the Windows associated with this domain. The current domain
         owner(s) reveal which Windows are associated. Dedicated workers
         have only one Window associated, shared worker can have more.

         @param windows [out] Associated Windows are added to this vector.
         @return OpStatus::ERR_NO_MEMORY, otherwise OpStatus::OK. */
#endif // ECMASCRIPT_DEBUGGER

private:
    friend class DOM_WebWorker;
    void RemoveWebWorker(DOM_WebWorker *w);
    /**< Unregistration of a Web Worker. Called by the worker
         as part of its shutdown steps. */

    OP_STATUS AddWebWorker(DOM_WebWorker *ww);
    /**< Adds a new web worker to this domain; if successful,
         the operation will update the worker to point to the
         domain as its parent.

         @return OpStatus::OK if successful,
                 OpStatus::ERR_NO_MEMORY on OOM. */

    void Shutdown();
    /**< Shut down this domain.

         When all browsing contexts have dropped interest in a domain,
         the domain shuts down. It notifies the Worker execution
         context to close and lets go of any reference to it. */
};

#endif // DOM_WEBWORKERS_SUPPORT
#endif // DOM_WEBWORKERS_H
