
/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 */

#ifndef DOMAPPLICATIONCACHE_H_
#define DOMAPPLICATIONCACHE_H_

#ifdef APPLICATION_CACHE_SUPPORT

#include "modules/dom/src/domobj.h"
#include "modules/applicationcache/application_cache_manager.h"
#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/dom/src/domevents/domevent.h"

/**
 * Implements the ApplicationCache object API defined in html5 specification 6.9 Offline Web applications
 */
class DOM_ApplicationCache
	: public DOM_Object
	, public DOM_EventTargetOwner
{
public:

	DOM_ApplicationCache();

	static void ConstructApplicationCacheObjectL(ES_Object *object, DOM_Runtime *runtime);

	virtual ~DOM_ApplicationCache(){}

	OP_STATUS OnEvent(DOM_EventType event_type, BOOL lengthComputable = FALSE, OpFileLength loaded = 0, OpFileLength total = 0);

	/* From DOM_Object: */
	virtual ES_GetState GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);

	/* From DOM_Object: */
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	/* From DOM_Object: */
	virtual ES_PutState PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);

	/* From DOM_Object: */
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_APPLICATION_CACHE || DOM_Object::IsA(type); }

	virtual void GCTrace();

	/* from DOM_EventTargetOwner: */
	virtual DOM_Object *GetOwnerObject() { return this; }

	/**
	 * Check for new versions of the application cache
	 */
	DOM_DECLARE_FUNCTION(update);

	/**
	 * Switch to a newer version of the cache
	 */
	DOM_DECLARE_FUNCTION(swapCache);

	/**
	 * Cache states as defined in html5
	 */
	enum
	{
		UNCACHED = 0,
		IDLE = 1,
		CHECKING = 2,
		DOWNLOADING = 3,
		UPDATEREADY = 4,
		OBSOLETE = 5,
	};

	enum
	{
		FUNCTIONS_update = 1,
		FUNCTIONS_swapCache,
		FUNCTIONS_dispatchEvent, // Reusing DOM_Node::dispatchEvent
		FUNCTIONS_ARRAY_SIZE
	};

	enum
	{
		FUNCTIONS_WITH_DATA_addEventListener = 1,
		FUNCTIONS_WITH_DATA_removeEventListener,
		FUNCTIONS_WITH_DATA_ARRAY_SIZE
	};

};

#endif // APPLICATION_CACHE_SUPPORT

#endif /* DOMAPPLICATIONCACHE_H_ */
