/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 */

#include "core/pch.h"

#ifdef APPLICATION_CACHE_SUPPORT
#include "modules/dom/src/domapplicationcache/domapplicationcache.h"
#include "modules/applicationcache/application_cache_manager.h"

#include "modules/dom/domenvironment.h"
#include "modules/dom/domeventtypes.h"

#include "modules/doc/frm_doc.h"

#include "modules/dom/src/domglobaldata.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/domdefines.h"
#include "modules/dom/src/dominternaltypes.h"
#include "modules/dom/src/domevents/domprogressevent.h"
#include "modules/dom/src/domevents/domeventlistener.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/userjs/userjsmanager.h"

#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/applicationcache/application_cache.h"
#include "modules/applicationcache/application_cache_group.h"

#ifdef DEBUG_ENABLE_OPASSERT
static BOOL IsValidEventType(DOM_EventType event_type)
{
	return event_type == ONPROGRESS ||
		event_type == ONERROR ||
		event_type == APPCACHECACHED ||
		event_type == APPCACHECHECKING ||
		event_type == APPCACHEDOWNLOADING ||
		event_type == APPCACHENOUPDATE ||
		event_type == APPCACHEOBSOLETE ||
		event_type == APPCACHEUPDATEREADY;
}
#endif // DEBUG_ENABLE_OPASSERT

class DOM_ApplicationCacheEvent
	: public DOM_Event
{
public:
	DOM_ApplicationCacheEvent(){}
	virtual OP_STATUS DefaultAction(BOOL cancelled);
};


/* virtual */ OP_STATUS
DOM_ApplicationCacheEvent::DefaultAction(BOOL cancelled)
{
	if (cancelled || prevent_default)
		return OpStatus::OK;

	WindowCommander *wnd_commander = g_application_cache_manager->GetWindowCommanderFromCacheHost(GetEnvironment());
	OpApplicationCacheListener *listener  = NULL;
	if (wnd_commander && (listener = wnd_commander->GetApplicationCacheListener()))
	{
		switch (known_type)
		{
			case APPCACHECHECKING:
				listener->OnAppCacheChecking(wnd_commander);
				break;
			case APPCACHEDOWNLOADING:
				listener->OnAppCacheDownloading(wnd_commander);
				break;
			case APPCACHECACHED:
				listener->OnAppCacheCached(wnd_commander);
				break;
			case APPCACHEUPDATEREADY:
				listener->OnAppCacheUpdateReady(wnd_commander);
				break;
			case APPCACHENOUPDATE:
				listener->OnAppCacheNoUpdate(wnd_commander);
				break;
			case APPCACHEOBSOLETE:
				listener->OnAppCacheObsolete(wnd_commander);
				break;
			case ONERROR:
				listener->OnAppCacheError(wnd_commander);
				break;
			default:
				OP_ASSERT(!"Unexpected event type!");
		}
	}

	return OpStatus::OK;
}

class DOM_ApplicationCacheProgressEvent
	: public DOM_ProgressEvent
{
public:
	DOM_ApplicationCacheProgressEvent()
	: DOM_ProgressEvent()
	{}

	virtual OP_STATUS DefaultAction(BOOL cancelled);
};


/* virtual */ OP_STATUS
DOM_ApplicationCacheProgressEvent::DefaultAction(BOOL cancelled)
{
	if (cancelled || prevent_default)
		return OpStatus::OK;

	WindowCommander *wnd_commander = g_application_cache_manager->GetWindowCommanderFromCacheHost(GetEnvironment());
	OpApplicationCacheListener *listener  = NULL;

	if (wnd_commander && (listener = wnd_commander->GetApplicationCacheListener()))
		listener->OnAppCacheProgress(wnd_commander);

	return OpStatus::OK;
}

DOM_ApplicationCache::DOM_ApplicationCache()
{}

/* static */void
DOM_ApplicationCache::ConstructApplicationCacheObjectL(ES_Object* object, DOM_Runtime *runtime)
{
	PutNumericConstantL(object, "UNCACHED", DOM_ApplicationCache::UNCACHED, runtime);

	PutNumericConstantL(object,"IDLE", DOM_ApplicationCache::IDLE, runtime);

	PutNumericConstantL(object,"CHECKING", DOM_ApplicationCache::CHECKING, runtime);

	PutNumericConstantL(object,"DOWNLOADING", DOM_ApplicationCache::DOWNLOADING, runtime);

	PutNumericConstantL(object,"UPDATEREADY", DOM_ApplicationCache::UPDATEREADY, runtime);

	PutNumericConstantL(object,"OBSOLETE", DOM_ApplicationCache::OBSOLETE, runtime);
}

OP_STATUS
DOM_ApplicationCache::OnEvent(DOM_EventType event_type, BOOL lengthComputable, OpFileLength loaded, OpFileLength total)
{
	// Check that the event type is relevant to the application cache
	OP_ASSERT(IsValidEventType(event_type));

	DOM_Runtime* runtime = GetRuntime();
	OP_ASSERT(runtime);

	DOM_Event *event = NULL;
	if (event_type == ONPROGRESS)
	{
		RETURN_IF_ERROR(DOMSetObjectRuntime(event = OP_NEW(DOM_ApplicationCacheProgressEvent, ()), runtime, runtime->GetPrototype(DOM_Runtime::PROGRESSEVENT_PROTOTYPE), "ProgressEvent"));
		static_cast<DOM_ProgressEvent*>(event)->InitProgressEvent(lengthComputable, loaded, total);
	}
	else
		RETURN_IF_ERROR(DOMSetObjectRuntime(event = OP_NEW(DOM_ApplicationCacheEvent,()), runtime, runtime->GetPrototype(DOM_Runtime::EVENT_PROTOTYPE), "Event"));

	OP_ASSERT(event);

	event->InitEvent(event_type, this);

	RETURN_IF_ERROR(runtime->GetEnvironment()->SendEvent(event));

	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_ApplicationCache::GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	ES_GetState result = GetEventProperty(property_name, value, static_cast<DOM_Runtime *>(origining_runtime));
	if (result != GET_FAILED)
		return result;

	return DOM_Object::GetName(property_name, property_code, value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_ApplicationCache::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_status)
	{
		ApplicationCache *cache = g_application_cache_manager->GetApplicationCacheFromCacheHost(GetEnvironment());

		const ApplicationCache::CacheState state = cache ? cache->GetCacheState() : ApplicationCache::UNCACHED;

		DOMSetNumber(value, static_cast <int>(state));
		return GET_SUCCESS;
	}

	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_ApplicationCache::PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name[0] == 'o' && property_name[1] == 'n')
	{
		ES_PutState state = PutEventProperty(property_name, value, static_cast<DOM_Runtime *>(origining_runtime));
		if (state != PUT_FAILED)
			return state;
	}

	return DOM_Object::PutName(property_name, property_code, value, origining_runtime);

}

/* virtual */ ES_PutState
DOM_ApplicationCache::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
		case OP_ATOM_status:
			return PUT_SUCCESS; // Simply sink the attempt to assign a value to the read only property
	}

	return PUT_FAILED;
}

/*virtual*/ void
DOM_ApplicationCache::GCTrace()
{
	GCMark(FetchEventTarget());
}


/* static */ int
DOM_ApplicationCache::update(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(application_cache, DOM_TYPE_APPLICATION_CACHE, DOM_ApplicationCache);

	ApplicationCache *cache = g_application_cache_manager->GetApplicationCacheFromCacheHost(application_cache->GetEnvironment());

	// If application cache is absent or if it is marked as obsolete then return an error
	if (!cache || cache->GetCacheGrup()->IsObsolete())
		return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);

	CALL_FAILED_IF_ERROR(cache->GetCacheGrup()->UpdateCache(application_cache->GetEnvironment()));

	return ES_FAILED; // No value return
}


/* static */ int
DOM_ApplicationCache::swapCache(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(application_cache, DOM_TYPE_APPLICATION_CACHE, DOM_ApplicationCache);

	const ApplicationCache *cache = g_application_cache_manager->GetApplicationCacheFromCacheHost(application_cache->GetEnvironment());
	// Check that the cache is associated with a document

	if (!cache)
		return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);

	// Check that the application cache is not obsolete
	if (cache->GetCacheGrup()->IsObsolete())
	{
		g_application_cache_manager->RemoveCacheHostAssociation(application_cache->GetEnvironment());
		return ES_FAILED;
	}

	// Check that the newest app cache has completeness flag as `complete'
	ApplicationCache *newest_complete_cache = cache->GetCacheGrup()->GetMostRecentCache(TRUE);

	// Check that newest cache is newer than this cache
	if (!newest_complete_cache || cache == newest_complete_cache)
		return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);


	// Finally, swap cache
	OP_STATUS status;
	if (OpStatus::IsError(status = cache->GetCacheGrup()->SwapCache(application_cache->GetEnvironment())))
	{
		if (OpStatus::IsMemoryError(status))
			return ES_NO_MEMORY;

		return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);
	}

	return ES_FAILED;
}


DOM_FUNCTIONS_START(DOM_ApplicationCache)
	DOM_FUNCTIONS_FUNCTION(DOM_ApplicationCache, DOM_ApplicationCache::update, "update", NULL)
	DOM_FUNCTIONS_FUNCTION(DOM_ApplicationCache, DOM_ApplicationCache::swapCache, "swapCache", NULL)
	DOM_FUNCTIONS_FUNCTION(DOM_ApplicationCache, DOM_Node::dispatchEvent, "dispatchEvent", NULL)
DOM_FUNCTIONS_END(DOM_ApplicationCache)

DOM_FUNCTIONS_WITH_DATA_START(DOM_ApplicationCache)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_ApplicationCache, DOM_Node::accessEventListener, 0, "addEventListener", "s-b-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_ApplicationCache, DOM_Node::accessEventListener, 1, "removeEventListener", "s-b-")
DOM_FUNCTIONS_WITH_DATA_END(DOM_ApplicationCache)

#endif // APPLICATION_CACHE_SUPPORT
