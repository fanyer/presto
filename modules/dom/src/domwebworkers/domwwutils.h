/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 2009-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef DOM_WW_UTILS_H
#define DOM_WW_UTILS_H

#ifdef DOM_WEBWORKERS_SUPPORT
#include "modules/dom/src/domobj.h"
#include "modules/dom/domtypes.h"
#include "modules/dom/src/domruntime.h"
#include "modules/ecmascript_utils/esenvironment.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/dom/src/domwebworkers/domwebworkers.h"

/* Web Worker internal tweaks */

#define ALLOW_WORKER_DATA_URLS 1
/**< If defined, permits Worker(), SharedWorker() and importScripts() to take
     data: URLs as input for inline definitions. In spirit with the specification,
     but strictly speaking outside. Makes selftests _so_ much easier to write. */

/** A 'static method' class holding utility functionality for supporting the
    initialisation of Worker context + convenient loading of scripts into a
    Worker's execution environment. And more. */
class DOM_WebWorker_Utils
{
public:
    static BOOL CheckImportScriptURL(const URL &worker_script_url);
    /**< Verify that a script resource URL is well-formed and is
         permitted used as an importScripts() source. Notice that
         this does not involve a same-origin check, as importScripts()
         do not impose such a constraint.

         @param worker_script_url URL of script to import; assumed
                resolved against the worker's origin URL.
         @return FALSE if access is disallowed, TRUE if script import
                 may be attempted. */

    static FramesDocument *GetWorkerFramesDocument(DOM_WebWorker *worker, DOM_EnvironmentImpl **environment);
    /**< Utility method for locating the FramesDocument of the browser
         context that created the given worker. Returns NULL if no such
         thing. Optionally, the function may also return the DOM_Environment
         that the FramesDocument hangs off of.

         @param worker the Web Worker instance whose FramesDocument to locate.
                May be a nested worker.
         @param [out]environ if non-NULL, the method will fill it in with
                DOM_Environment that the FramesDocumen belongs to.

         @return FramesDocument holding the owning browser context. */

    static OP_STATUS ReportQuotaExceeded(DOM_Object *origin_object, BOOL for_window, unsigned limit);
    /**< If a window or session exceeds Web Worker limits on how many
         active instances are allowed, an exception is raised along with
         reporting the event to the console. Scripts may not be
         prepared for the eventuality, hence separately informing the
         user of the failure condition is appropriate.

         @param origin_object The object that attempted to create a new
                Web Worker instance, but failed. Either a Window object
                or a Worker global scope.
         @param for_window TRUE if the per-window limit was exceeded.
                FALSE if it was the per-session limit.
         @param limit The current limit that was exceeded.
         @return OpStatus::OK if the message was posted to the console.
                 OpStatus::ERR_NO_MEMORY on OOM. */
};

/** Minor enhancement to DOM_ErrorEvent, adding a default action that handles
    the case when a worker's .onerror listener(s) passes on handling the event.
    In which case we are obliged to either forward the error event to the parent,
    to the worker object in the browser context that created the worker (which both
    may have .onerror handlers attached to them.) If all else fails, report the
    error back to the user. The latter is currently done by logging the error in
    the console. */
class DOM_WebWorker_ErrorEventDefault
    : public DOM_ErrorEvent
{
public:
    DOM_WebWorker_ErrorEventDefault(BOOL doPropagate)
        : propagate_up(doPropagate)
    {
    }

    virtual OP_STATUS DefaultAction(BOOL cancelled);

private:
    BOOL propagate_up;
    /**< Iff FALSE, turn off forwarding of the cancelled error event, i.e., leave
         it as unhandled and drop it. */
};

#endif // DOM_WEBWORKERS_SUPPORT
#endif // DOM_WW_UTILS_H
