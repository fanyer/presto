/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 2009-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef DOM_WW_INFO_H
#define DOM_WW_INFO_H

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

#ifdef DOM_WEBWORKERS_WORKER_STAT_SUPPORT
/** DOM_WebWorkerInfo gathers up statistics about an active Worker execution context. */
class DOM_WebWorkerInfo
    : public ListElement<DOM_WebWorkerInfo>
{
public:
    List<DOM_WebWorkerInfo> children;
    /**< The active children workers that this worker has created. */

    List<DOM_WebWorkerInfo> clients;
    /**< The users of this worker - ToDo: determine if we are better off having a sep. class for these. */

    const URL *worker_url;
    /**< The base URL for the Worker. */
    BOOL is_dedicated;
    const uni_char *worker_name;
    int ports_active;
    /**< Number of ports currently tracked as belonging to this worker. */
public:
    DOM_WebWorkerInfo()
        : is_dedicated(FALSE), worker_name(NULL), ports_active(0), worker_url(NULL)
    {
    }

    virtual ~DOM_WebWorkerInfo()
    {
        children.Clear();
        clients.Clear();
    }
};
#endif // DOM_WEBWORKERS_WORKER_STAT_SUPPORT

#endif // DOM_WEBWORKERS_SUPPORT
#endif // DOM_WW_INFO_H
