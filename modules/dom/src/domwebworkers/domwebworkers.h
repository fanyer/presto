/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 2009-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef DOM_WEBWORKERS_H
#define DOM_WEBWORKERS_H

#ifdef DOM_WEBWORKERS_SUPPORT

template<class T>
class AsListElement : public ListElement<AsListElement<T> >
{
public:
    T* ref;

    AsListElement(T* r)
        : ref(r)
    {
    }
};

/* Tweaks/extension of Web Workers: */

/* Gathering up statistics on currently running workers; for eventual profiling and/or a web worker task manager */
/* #define DOM_WEBWORKERS_WORKER_STAT_SUPPORT */

/*  HTML5 Web Workers: design and implementation overview
 *  -----------------------------------------------------
 *
 * What follows is an overview of the Web Workers design and implementation in Opera.
 * The overall goal is to provide the functionality specified in the HTML5/WHATWG
 * Web Workers specification <http://www.whatwg.org/specs/web-workers/current-work/> , and
 * its required sub-component, HTML5 Cross Document Messaging.
 *
 * The starting point for Web Workers was to build on the implementation of its pre-cursor,
 * Google Gears Workerpools. An Opera implementation of the supporting infrastructure for
 * this already exists, so simply reuse it makes good sense. However, one immediate stumbling
 * block is that the substrate has to get to grips with the ornate details of interop'ing with
 * Gears via NPAPI and reconciling two notions of threading. This portion is not required
 * for a 'native' Web Workers implementation, but the code isn't easily separable.
 * Teasing apart the Gears-supporting aspect from the worker pool implementation proper would
 * be non-trivial, but _if_ Google Gears had a brighter future, we could try harder, but the
 * initial decision was to base the Web Worker on the underlying design of worker threads
 * used in worker pools, but not reusing the code.
 *
 *  Type/Class structure:
 *
 *   Web workers:
 *
 *     DOM_WebWorkers:        the global list of available and active web worker domains.
 *
 *     DOM_WebWorkerDomain:   book-keeps and controls the operation of one or more workers attached
 *                            to a given DOM-based execution context, sharing document, thread scheduler
 *                            and logical heap. "document" in the DOM codebase sense (i.e., a DOM_Environment)
 *                            rather than an actual document. (At the moment, domains are 1-1 with their workers.)
 *
 *     DOM_WebWorkerController: for each browser context that creates a Web Worker (=> a DOM_WebWorkerDomain in that context), we
 *                            keep track of these via a 'controller' (awfully generic name..). It's stored on the DOM_EnvironmentImpl
 *                            (DOM_EnvironmentImpl::GetWorkerController()) and keeps these worker domains/contexts alive + handles
 *                            their orderly shutdown when the environment is itself destroyed. One additional feature is that it
 *                            caps the number of workers that a context may create (pref configurable.)
 *
 *                            Notice that a Web Worker may spawn child workers, so its DOM_EnvironmentImpl may also have a controller attached to it.
 *
 *     DOM_WebWorker:         the DOM interface to a Web Worker within its execution environments, supporting the main operations
 *                            for controlling and communicating with a worker. It also represents the global object for the
 *
 *     DOM_WebWorkerObject:   the proxies / representation of Web Worker objects that's returned to the caller of the DOM constructors
 *                            Worker()/SharedWorker(). Resides in the caller's context and mediates communication with the worker and
 *                            its environment.
 *
 *     DOM_WebWorkerBase:     an implementation type gathering together the bit of functionality that's shared between the previous two.
 *
 *   For DOM_WebWorkerObject, we further split these into DOM_DedicatedWorkerObject and DOM_SharedWorkerObject, providing the implementation of the Worker and
 *   SharedWorker DOM objects in the Web Workers specification.
 *
 *   Dually, on the inner side, DOM_SharedWorker and DOM_DedicatedWorker are sub-classes of DOM_WebWorker; providing the global scope
 *   objects for the two kinds of worker execution contexts.
 *
 *   Note: Web Workers are dependent on the HTML5 Messaging infrastructure of channels and ports to operate; the classes encompassing those are
 *   defined and outlined in domcrossmessage.h (FEATURE_CROSSDOCUMENT_MESSAGING is a separate, but required feature for web workers.) */

class DOM_MessagePort;
class DOM_MessageEvent;

#ifdef DOM_WEBWORKERS_WORKER_STAT_SUPPORT
class DOM_WebWorkerInfo;
#endif // DOM_WEBWORKERS_WORKER_STAT_SUPPORT

#include "modules/dom/src/domwebworkers/domwwbase.h"
#include "modules/dom/src/domwebworkers/domwwdomain.h"
#include "modules/dom/src/domwebworkers/domwwcontroller.h"
#include "modules/dom/src/domwebworkers/domwworkers.h"
#include "modules/dom/src/domwebworkers/domwworker.h"
#include "modules/dom/src/domwebworkers/domwwobject.h"

#endif // DOM_WEBWORKERS_SUPPORT
#endif // DOM_WEBWORKERS_H
