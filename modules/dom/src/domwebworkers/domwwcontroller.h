/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 2009-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef DOM_WW_CONTROLLER_H
#define DOM_WW_CONTROLLER_H

class DOM_WebWorkerDomain;
class DOM_WebWorker;
class DOM_WebWorkerObject;
class DOM_MessagePort;

/** Container class holding the information that Web Workers pin on the
    DOM_EnvironmentImpl where created. Kept separate rather than contaminating
    DOM_EnvironmentImpl with too many extra fields and operations. */
class DOM_WebWorkerController
    : public OpPrefsListener
{
private:
    friend class DOM_EnvironmentImpl;

    DOM_EnvironmentImpl *owner;
    DocumentManager *worker_document_manager;

    DOM_WebWorker *current_worker;
    /**< The worker object associated with this execution environment */

    List<AsListElement<DOM_WebWorkerDomain> > web_worker_domains;

    List<AsListElement<DOM_WebWorkerObject> > web_worker_objects;
    /**< constructor objects that have been created in this execution environment. */

    List<DOM_MessagePort> entangled_ports;
    /**< The ports that are entangled with objects in this environment. */

    unsigned max_browser_worker_contexts;
    unsigned num_worker_contexts;

public:
    DOM_WebWorkerController(DOM_EnvironmentImpl *o)
        : owner(o),
          worker_document_manager(NULL),
          current_worker(NULL),
          max_browser_worker_contexts(0),
          num_worker_contexts(0)
    {
    }

    ~DOM_WebWorkerController();

    void SetDocumentManager(DocumentManager *doc_manager) { worker_document_manager = doc_manager; }
    void SetDocumentManagerL(DocumentManager *doc_manager, ES_Runtime *es_runtime);

    void SetWorkerObject(DOM_WebWorker *w) { current_worker = w; }
    DOM_WebWorker *GetWorkerObject() { return current_worker; }

    BOOL AllowNewWorkerContext();
    /**< Prior to creating a new worker with a new context, check
         that the per-browser context will permit this. */

    void DropWorkerContext();
    /**< Upon shutting down a worker's execution context, notify
         this browser context. Allows other/more execution contexts
         to be created later. */

    OP_STATUS AddWebWorkerDomain(DOM_WebWorkerDomain *w);
    void RemoveWebWorkerDomain(DOM_WebWorkerDomain *ww);

    OP_STATUS AddWebWorkerObject(DOM_WebWorkerObject *w);
    void RemoveWebWorkerObject(DOM_WebWorkerObject *w);

    DocumentManager* GetWorkerDocManager() { return worker_document_manager; }
    void DetachWebWorkers();
    void GCTrace(ES_Runtime *runtime);

    void PrefChanged(enum OpPrefsCollection::Collections id, int pref, int newvalue);
    /**<  Notify of updates to preferences for max workers per-session and per-window. */

    OP_STATUS InitWorkerPrefs(DOM_EnvironmentImpl *environment);
    /**< Read in web worker specific preferences and add a listener
         for any subsequent changes.

         @return OpStatus::ERR_NO_MEMORY on OOM; OpStatus::OK otherwise. */

    unsigned GetMaxWorkersPerWindow() { return max_browser_worker_contexts; }

#ifdef ECMASCRIPT_DEBUGGER
    OP_STATUS GetDomainRuntimes(OpVector<ES_Runtime> &es_runtimes);
    /**< Gets all the ES_Runtimes associated with this controller.

         This function goes through the list of domains, and adds each ES_Runtime
         associated with each domain.

         @param [out] es_runtimes A vector to store the ES_Runtimes in.
         @return OpStatus::OK on success, otherwise OOM. */
#endif // ECMASCRIPT_DEBUGGER

};

#endif // DOM_WW_CONTROLLER_H
