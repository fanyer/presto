/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 2009-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef DOM_CROSSUTILS_H
#define DOM_CROSSUTILS_H

#ifdef DOM_CROSSDOCUMENT_MESSAGING_SUPPORT
#include "modules/dom/src/domobj.h"
#include "modules/dom/domtypes.h"
#include "modules/dom/src/domruntime.h"
#include "modules/ecmascript_utils/esenvironment.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/ecmascript_utils/esasyncif.h"

class DOM_CrossMessage_Utils
{
public:
    static ES_PutState PutEventHandler(DOM_Object *owner, const uni_char *event_name, DOM_EventType event_type, DOM_EventListener *&the_handler, DOM_EventQueue *event_queue, ES_Value *value, ES_Runtime *origining_runtime);
    /**< Sharing the code required to register an .onXXX handler on a web worker and message ports.
         Entirely internal, see uses for details of parameters and calling interface. */
};

/** Internal class gathering together functionality over ErrorEvent and MessageEvents. */
class DOM_ErrorException_Utils
{
public:
    static OP_STATUS ReportErrorEvent(DOM_ErrorEvent *error_event, const URL &url, DOM_EnvironmentImpl *environment);

    static OP_STATUS CopyErrorEvent(DOM_Object *target_object, DOM_ErrorEvent *&target, DOM_ErrorEvent *source, const URL &url, BOOL propagate_up);

    static OP_STATUS BuildErrorEvent(DOM_Object *target_object, DOM_ErrorEvent *&event, const uni_char *location, const uni_char *message, unsigned line_number, BOOL propagate_error);

    static OP_STATUS CloneErrorEvent(DOM_Object *target_object, DOM_ErrorEvent *&target, DOM_ErrorEvent *source, const URL &origin_url, BOOL propagate_up);

    static OP_STATUS CloneDOMEvent(DOM_Event *event, DOM_Object *target_object, DOM_Event *&target_event);
};

#endif // DOM_CROSSDOCUMENT_MESSAGING_SUPPORT
#endif // DOM_CROSSUTILS_H
