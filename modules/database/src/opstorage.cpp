/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/database/src/opdatabase_base.h"

#ifdef OPSTORAGE_SUPPORT

#include "modules/database/opdatabase.h"
#include "modules/database/opdatabasemanager.h"
#include "modules/database/opstorage.h"
#include "modules/util/tempbuf.h"

# define USES_GMGR(type) ((type) != WEB_STORAGE_SESSION)

/**
 * WebStorageOperationCallbackOpStorageWrapper
 *
 * A wrapper to the callback passed to the many OpStorage functions
 * Allows to do some optimizations
 */
class WebStorageOperationCallbackOpStorageWrapper
	: public WebStorageOperationCallback
	, public PS_ObjDelListener::ResourceShutdownCallback
	, public ListElement<WebStorageOperationCallbackOpStorageWrapper>
{
	WebStorageOperationCallbackOpStorageWrapper()
		: m_ops(NULL)
		, m_cb(NULL)
		, m_key(NULL)
		, m_value(NULL)
	{}

	~WebStorageOperationCallbackOpStorageWrapper() { Clear(); }
public:

	void Setup(OpStorage* ops, WebStorageOperationCallback* cb, const OpStorageEventContext& event_ctx = OpStorageEventContext())
	{
		//asserts to make sure that the class is empty
		OP_ASSERT(m_ops == NULL);
		OP_ASSERT(m_cb == NULL);

		m_ops = ops;
		m_cb = cb;
		OP_ASSERT(m_context.m_url.IsEmpty());
		m_context = event_ctx;

		m_ops->AddShutdownCallback(this);
	}

	void Clear()
	{
		if (m_ops != NULL)
		{
			m_ops->RemoveShutdownCallback(this);
			m_ops = NULL;
		}
		m_cb = NULL;
		if (m_key != NULL)
		{
			OP_DELETE(m_key);
			m_key = NULL;
		}
		if (m_value != NULL)
		{
			OP_DELETE(m_value);
			m_value = NULL;
		}
		m_context.Clear();
	}

	virtual void HandleResourceShutdown()
	{
		//OpStorage being deleted
		m_ops = NULL;
	}

	virtual OP_STATUS HandleOperation(const WebStorageOperation* result)
	{
		//object got deleted meanwhile
		if (m_ops == NULL)
			return OpStatus::OK;

		if (!result->IsError() && result->m_type == WebStorageOperation::GET_ITEM_COUNT)
		{
			m_ops->SetCachedNumberOfItems(result->m_data.m_item_count);
		}
		else if(!result->IsError() && result->m_data.m_storage.m_storage_mutated)
		{
			switch(result->m_type)
			{
			case WebStorageOperation::SET_ITEM:
#ifdef WEBSTORAGE_RO_PAIRS_SUPPORT
			case WebStorageOperation::SET_ITEM_RO:
#endif //WEBSTORAGE_RO_PAIRS_SUPPORT
				if (m_ops->GetEntry()->m_message_handler != NULL)
				{
					OpStatus::Ignore(m_ops->GetEntry()->m_message_handler->
							FireValueChangedListeners(m_key, result->m_data.m_storage.m_value, m_value, m_context));
				}
				break;
			case WebStorageOperation::CLEAR:
#ifdef WEBSTORAGE_RO_PAIRS_SUPPORT
			case WebStorageOperation::CLEAR_RO:
#endif //WEBSTORAGE_RO_PAIRS_SUPPORT
				if (m_ops->GetEntry()->m_message_handler != NULL)
				{
					OpStatus::Ignore(m_ops->GetEntry()->m_message_handler->FireClearListeners(m_context));
				}
				break;
			default:
				OP_ASSERT(!"unhandled operation type");
			}
		}
		return m_cb != NULL ? m_cb->HandleOperation(result) : OpStatus::OK;
	}
	virtual Window* GetWindow()
	{
		return m_cb != NULL ? m_cb->GetWindow() : NULL;
	}
	virtual void Discard()
	{
		if (m_cb != NULL)
		{
			WebStorageOperationCallback* cb = m_cb;
			m_cb = NULL;
			cb->Discard();
		}
		if (m_ops != NULL)
			m_ops->GetEntry()->DisposeCallbackObj(this);
		else
			OP_DELETE(this);
	}
	OpStorage* m_ops;
	WebStorageOperationCallback* m_cb;
	WebStorageValue* m_key;
	WebStorageValue* m_value;
	OpStorageEventContext m_context;

	friend class OpStorageManager;
};

static OP_STATUS CheckStorageOrigin(const uni_char *origin, BOOL is_persistent)
{
	if (origin == NULL || *origin == 0)
	{
		OP_ASSERT(!"Origin may not be blank");
		return OpStatus::ERR_NULL_POINTER;
	}

	return OpStatus::OK;
}

/***
 * OpStorageManager
 */
OpStorageManager::OpStorageManager() : m_callback_wrapper_pool_length(0)
{
#ifdef SELFTEST
	m_selftest_instance = FALSE;
#endif //SELFTEST
	op_memset(m_storage_index, 0, sizeof(m_storage_index));
	IncRefCount();
}

void
OpStorageManager::Release()
{
	OP_ASSERT(GetRefCount() > 0);
	DecRefCount();
	if (GetRefCount() == 0)
		OP_DELETE(this);
}

OpStorageManager::~OpStorageManager()
{
	OP_ASSERT(GetRefCount() == 0);
	for (unsigned k = WEB_STORAGE_START; k < WEB_STORAGE_END; k++)
	{
		if (m_storage_index[0][k] != NULL)
			m_storage_index[0][k]->DeleteAll();
		if (m_storage_index[1][k] != NULL)
			m_storage_index[1][k]->DeleteAll();
		OP_DELETE(m_storage_index[0][k]);
		m_storage_index[0][k] = NULL;
		OP_DELETE(m_storage_index[1][k]);
		m_storage_index[1][k] = NULL;
	}

	m_callback_wrapper_pool.Clear();
}

/* static */
OpStorageManager*
OpStorageManager::Create()
{
	return OP_NEW(OpStorageManager,());
}

#ifdef SELFTEST
/* static */
OpStorageManager*
OpStorageManager::CreateForSelfTest()
{
	OpStorageManager* new_osm = OP_NEW(OpStorageManager,());
	if (new_osm)
		new_osm->m_selftest_instance = TRUE;
	return new_osm;
}
#endif

OP_STATUS
OpStorageManager::GetStorage(WebStorageType type, URL_CONTEXT_ID context_id, const uni_char* origin, BOOL is_persistent, OpStorage** ops)
{
	is_persistent = !!is_persistent;
#ifdef SELFTEST
	if(!m_selftest_instance)
#endif //SELFTEST
	if (USES_GMGR(type) && this != g_webstorage_manager)
		return g_webstorage_manager->GetStorage(type, context_id, origin, is_persistent, ops);

#ifdef DEBUG_ENABLE_OPASSERT
	//logic xor
#ifdef SELFTEST
	if(!m_selftest_instance)
#endif //SELFTEST
	OP_ASSERT(	(!USES_GMGR(type) && this != g_webstorage_manager) ||
				(USES_GMGR(type) && this == g_webstorage_manager));
#endif // DEBUG_ENABLE_OPASSERT

	OpStorageMgrIndexEntry* st_entry;
	RETURN_IF_ERROR(CheckStorageOrigin(origin, is_persistent));
	RETURN_IF_ERROR(CreateEntry(type, context_id, origin, is_persistent, &st_entry));
	OP_ASSERT(st_entry != NULL);

	if (st_entry->m_storage_obj == NULL)
	{
		WebStorageBackend* wsb = NULL;
		OP_STATUS status;
		if (OpStatus::IsSuccess(status = WebStorageBackend::Create(type, origin, is_persistent, context_id, &wsb)) &&
			(st_entry->m_storage_obj = OP_NEW(OpStorage,(st_entry, wsb))) != NULL)
		{
			if (st_entry->m_storage_obj->IsVolatile())
			{
				//we have to increase the ref count on these suckers, else
				//a single page reload might delete all data, because while the
				//session is still open, there might not be anything holding references
				st_entry->m_storage_obj->IncRefCount();
			}
		}
		else
		{
			if (wsb != NULL)
				wsb->Release(FALSE);
			st_entry->SafeDelete();
			return OpStatus::IsSuccess(status) ? OpStatus::ERR_NO_MEMORY : status;
		}
	}
	st_entry->m_storage_obj->IncRefCount();
	*ops = st_entry->m_storage_obj;
	return OpStatus::OK;
}

OP_STATUS
OpStorageManager::ClearStorage(WebStorageType type, URL_CONTEXT_ID context_id, const uni_char* origin)
{
	OpStorage *storage;
	RETURN_IF_ERROR(GetStorage(type, context_id, origin, TRUE, &storage));
	AutoReleaseOpStoragePtr storage_ptr(storage);
	return WEBSTORAGE_CLEAR_ALL(storage, NULL);
}

OpStorageManager::OpStorageMgrIndexEntry*
OpStorageManager::GetEntry(WebStorageType type, URL_CONTEXT_ID context_id, const uni_char* origin, BOOL is_persistent) const
{
#ifdef SELFTEST
	if(!m_selftest_instance)
#endif //SELFTEST
	if (USES_GMGR(type) && this != g_webstorage_manager)
		return g_webstorage_manager->GetEntry(type, context_id, origin, is_persistent);

	LEAVE_IF_ERROR(CheckStorageOrigin(origin, is_persistent));
	OP_ASSERT(origin != NULL);
	is_persistent = !!is_persistent;
	OP_ASSERT(is_persistent == 0 || is_persistent == 1);

	OpStorageMgrIndexEntry* st_entry = NULL;
	StorageIndexMap* hmap = NULL;
	if (m_storage_index[is_persistent][type] == NULL ||
		OpStatus::IsError(m_storage_index[is_persistent][type]->GetData(INT_TO_PTR(context_id), &hmap)) ||
		OpStatus::IsError(hmap->GetData(origin, &st_entry)))
	{
		return NULL;
	}

	return st_entry;
}

OP_STATUS
OpStorageManager::CreateEntry(WebStorageType type, URL_CONTEXT_ID context_id, const uni_char* origin, BOOL is_persistent, OpStorageMgrIndexEntry** st_entry)
{
#ifdef SELFTEST
	if(!m_selftest_instance)
#endif //SELFTEST
	if (USES_GMGR(type) && this != g_webstorage_manager)
		return g_webstorage_manager->CreateEntry(type, context_id, origin, is_persistent, st_entry);

	OP_ASSERT(origin != NULL && *origin != 0);
	is_persistent = !!is_persistent;
	OP_ASSERT(is_persistent == 0 || is_persistent == 1);

	OpStorageMgrIndexEntry* new_entry = GetEntry(type, context_id, origin, is_persistent);
	StorageIndexMap* hmap = NULL;
	if (new_entry == NULL)
	{
		if (m_storage_index[is_persistent][type] == NULL)
			if ((m_storage_index[is_persistent][type] = OP_NEW(ContextIdToStorageIndexHashTable,())) == NULL)
				goto oom_cleanup;

		if (OpStatus::IsError(m_storage_index[is_persistent][type]->GetData(INT_TO_PTR(context_id), &hmap)))
		{
			if ((hmap = OP_NEW(StorageIndexMap,())) == NULL)
				goto oom_cleanup;
			if (OpStatus::IsError(m_storage_index[is_persistent][type]->Add(INT_TO_PTR(context_id), hmap)))
				goto oom_cleanup;
		}

		if ((new_entry = OpStorageMgrIndexEntry::Create(this, type, context_id, origin, is_persistent)) == NULL)
			goto oom_cleanup;
		if (OpStatus::IsError(hmap->Add(new_entry->m_origin, new_entry)))
			goto oom_cleanup;
	}

	*st_entry = new_entry;
	return OpStatus::OK;

oom_cleanup:

	OP_DELETE(new_entry);
	if (hmap != NULL && hmap->GetCount() == 0)
	{
		m_storage_index[is_persistent][type]->Remove(INT_TO_PTR(context_id), &hmap);
		OP_DELETE(hmap);
	}
	if (m_storage_index[is_persistent][type] != NULL &&
		m_storage_index[is_persistent][type]->GetCount() == 0)
	{
		OP_DELETE(m_storage_index[is_persistent][type]);
		m_storage_index[is_persistent][type] = NULL;
	}
	return OpStatus::ERR_NO_MEMORY;
}

void
OpStorageManager::DeleteEntry(WebStorageType type, URL_CONTEXT_ID context_id, const uni_char* origin, BOOL is_persistent)
{
	is_persistent = !!is_persistent;
#ifdef SELFTEST
	if(!m_selftest_instance)
#endif //SELFTEST
	if (USES_GMGR(type) && this != g_webstorage_manager)
		g_webstorage_manager->DeleteEntry(type, context_id, origin, is_persistent);

	OP_ASSERT(origin != NULL);
	if (m_storage_index[is_persistent][type] != NULL)
	{
		OpStorageMgrIndexEntry* st_entry = NULL;
		StorageIndexMap* hmap = NULL;
		if (OpStatus::IsSuccess(m_storage_index[is_persistent][type]->GetData(INT_TO_PTR(context_id), &hmap)) && hmap != NULL)
		{
			if (OpStatus::IsSuccess(hmap->Remove(origin, &st_entry)) && st_entry != NULL)
			{
				OP_ASSERT(st_entry->CanBeDeleted());
				OP_DELETE(st_entry);

				if (hmap->GetCount() == 0 &&
					OpStatus::IsSuccess(m_storage_index[is_persistent][type]->Remove(INT_TO_PTR(context_id), &hmap)))
				{
					OP_DELETE(hmap);
				}
			}
		}
	}
}

OP_STATUS
OpStorageManager::AddStorageEventListener(WebStorageType type, URL_CONTEXT_ID context_id, const uni_char* origin, BOOL is_persistent, OpStorageEventListener* listener)
{
#ifdef SELFTEST
	if(!m_selftest_instance)
#endif //SELFTEST
	if (USES_GMGR(type) && this != g_webstorage_manager)
		return g_webstorage_manager->AddStorageEventListener(type, context_id, origin, is_persistent, listener);

	OP_ASSERT(listener != NULL);

	OpStorageMgrIndexEntry* st_entry;
	LEAVE_IF_ERROR(CheckStorageOrigin(origin, is_persistent));
	RETURN_IF_ERROR(CreateEntry(type, context_id, origin, is_persistent, &st_entry));
	OP_ASSERT(st_entry != NULL);

	if (st_entry->m_message_handler == NULL)
	{
		st_entry->m_message_handler = OP_NEW(OpStorageEventMessageHandler,(st_entry));
		OP_STATUS status = st_entry->m_message_handler == NULL ?
				OpStatus::ERR_NO_MEMORY :
				st_entry->m_message_handler->Init();
		if (OpStatus::IsError(status))
		{
			st_entry->SafeDelete();
			return SIGNAL_OP_STATUS_ERROR(status);
		}
	}
	st_entry->m_message_handler->AddStorageEventListener(listener);
	return OpStatus::OK;
}

void
OpStorageManager::RemoveStorageEventListener(WebStorageType type, URL_CONTEXT_ID context_id, const uni_char* origin, BOOL is_persistent, OpStorageEventListener* listener)
{
#ifdef SELFTEST
	if(!m_selftest_instance)
#endif //SELFTEST
	if (USES_GMGR(type) && this != g_webstorage_manager)
		g_webstorage_manager->RemoveStorageEventListener(type, context_id, origin, is_persistent, listener);

	OpStorageMgrIndexEntry* st_entry = GetEntry(type, context_id, origin, is_persistent);
	if (st_entry != NULL)
	{
		OpStorageEventMessageHandler *handler = st_entry->m_message_handler;
		if (handler != NULL)
		{
			handler->RemoveStorageEventListener(listener);
			st_entry->SafeDelete();
		}
	}
}

BOOL
OpStorageManager::HasStorageEventListeners(WebStorageType type, URL_CONTEXT_ID context_id, const uni_char* origin, BOOL is_persistent) const
{
#ifdef SELFTEST
	if(!m_selftest_instance)
#endif //SELFTEST
	if (USES_GMGR(type) && this != g_webstorage_manager)
		return g_webstorage_manager->HasStorageEventListeners(type, context_id, origin, is_persistent);

	OpStorageMgrIndexEntry* st_entry = GetEntry(type, context_id, origin, is_persistent);
	return st_entry != NULL && st_entry->m_message_handler != NULL && st_entry->m_message_handler->HasStorageEventListeners();
}

unsigned
OpStorageManager::GetNumberOfListeners(WebStorageType type, URL_CONTEXT_ID context_id, const uni_char* origin, BOOL is_persistent) const
{
#ifdef SELFTEST
	if(!m_selftest_instance)
#endif //SELFTEST
	if (USES_GMGR(type) && this != g_webstorage_manager)
		return g_webstorage_manager->GetNumberOfListeners(type, context_id, origin, is_persistent);

	OpStorageMgrIndexEntry* st_entry = GetEntry(type, context_id, origin, is_persistent);
	return st_entry != NULL && st_entry->m_message_handler != NULL ?
			st_entry->m_message_handler->GetNumberOfListeners() :
			0;
}

OpStorageManager::OpStorageMgrIndexEntry::~OpStorageMgrIndexEntry()
{
	OP_ASSERT(!GetFlag(BEING_DELETED));
	SetFlag(BEING_DELETED);

	if (m_storage_obj != NULL && IsVolatile())
		// Non persistent entries ref counter is incremented by the
		// manager so they aren't cleared when refreshing the document
		// due to there not being any dom objects owning references.
		// See OpStorageManager::GetStorage().
		m_storage_obj->Release();

	OP_DELETE(m_storage_obj);
	m_storage_obj = NULL;

	OP_DELETE(m_message_handler);
	m_message_handler = NULL;

	OP_DELETEA(m_origin);
	m_origin = NULL;
}

/*static*/
OpStorageManager::OpStorageMgrIndexEntry*
OpStorageManager::OpStorageMgrIndexEntry::Create(OpStorageManager* new_storage_manager, WebStorageType type,
		URL_CONTEXT_ID context_id, const uni_char* origin, BOOL is_persistent)
{
	OpStorageMgrIndexEntry *new_hi = OP_NEW(OpStorageMgrIndexEntry,());
	RETURN_VALUE_IF_NULL(new_hi, NULL);
	OpStackAutoPtr<OpStorageMgrIndexEntry> new_hi_ptr(new_hi);

	RETURN_VALUE_IF_NULL(new_hi->m_origin = UniSetNewStr(origin), NULL);
	new_hi->m_storage_manager = new_storage_manager;
	new_hi->m_context_id = context_id;
	new_hi->m_type = type;
	if (is_persistent)
		new_hi->SetFlag(IS_PERSISTENT_STORAGE);
	new_hi->m_storage_obj = NULL;
	new_hi->m_message_handler = NULL;
	new_hi->m_storage_manager = new_storage_manager;

	return new_hi_ptr.release();
}

void
OpStorageManager::OpStorageMgrIndexEntry::SafeDelete()
{
	if (CanBeDeleted())
		m_storage_manager->DeleteEntry(m_type, m_context_id, m_origin, IsPersistent());
}

BOOL
OpStorageManager::OpStorageMgrIndexEntry::CanBeDeleted()
{
	BOOL can_be_deleted = !GetFlag(BEING_DELETED);
	if (can_be_deleted && m_message_handler != NULL)
		can_be_deleted = can_be_deleted && !m_message_handler->HasStorageEventListeners();
	if (can_be_deleted && m_storage_obj != NULL)
		can_be_deleted = can_be_deleted && m_storage_obj->GetRefCount() == 0;
	return can_be_deleted;
}

WebStorageOperationCallbackOpStorageWrapper*
OpStorageManager::GetUnusedCallbackObj()
{
	OP_ASSERT(m_callback_wrapper_pool_length == m_callback_wrapper_pool.Cardinal());

	WebStorageOperationCallbackOpStorageWrapper *cb = m_callback_wrapper_pool.Last();

	if (cb != NULL)
	{
		cb->Out();
		m_callback_wrapper_pool_length--;
		return cb;
	}

	return OP_NEW(WebStorageOperationCallbackOpStorageWrapper,());
}

void
OpStorageManager::DisposeCallbackObj(WebStorageOperationCallbackOpStorageWrapper* obj)
{
	OP_ASSERT(m_callback_wrapper_pool_length == m_callback_wrapper_pool.Cardinal());

	if (m_callback_wrapper_pool_length >= MAX_POOL_SIZE)
		OP_DELETE(obj);
	else
	{
		obj->Clear();
		obj->Into(&m_callback_wrapper_pool);
		m_callback_wrapper_pool_length++;
	}
}


/***
 * OpStorage
 */

OpStorage::OpStorage(OpStorageManager::OpStorageMgrIndexEntry* storage_entry, WebStorageBackend* wsb)
	: m_storage_entry(storage_entry)
	, m_wsb(wsb)
{
	OP_ASSERT(wsb != NULL);
	OP_ASSERT(storage_entry != NULL);
	wsb->AddListener(this);
}

OpStorage::~OpStorage()
{
	OP_ASSERT(!GetFlag(BEING_DELETED));
	SetFlag(BEING_DELETED);
	FireShutdownCallbacks();
	ReleaseWSB(!OpDbUtils::IsOperaRunning());
	OP_ASSERT(m_wsb == NULL);
	m_storage_entry->m_storage_obj = NULL;
}

void
OpStorage::ReleaseWSB(BOOL force)
{
	if (m_wsb != NULL)
	{
		WebStorageBackend *wsb = m_wsb;
		m_wsb = NULL;
		wsb->RemoveListener(this);
		wsb->Release(force);
		UnsetCachedNumberOfItems();
	}
}

void
OpStorage::SafeDelete()
{
	INTEGRITY_CHECK();
	OpStorageManager::OpStorageMgrIndexEntry* storage_entry = m_storage_entry;
	if (GetRefCount() == 0 && !GetFlag(BEING_DELETED))
	{
		OP_DELETE(this);
	}
	storage_entry->SafeDelete();
}

BOOL
OpStorage::HasStorageEventListeners() const
{
	return GetEntry()->m_message_handler != NULL && GetEntry()->m_message_handler->HasStorageEventListeners();
}

#ifdef WEBSTORAGE_WIDGET_PREFS_SUPPORT
void
OpStorage::OnDefaultWidgetPrefsSet()
{
	if (m_wsb != NULL)
		m_wsb->OnDefaultWidgetPrefsSet();
}
#endif // WEBSTORAGE_WIDGET_PREFS_SUPPORT

OP_STATUS
OpStorage::GetNumberOfItems(WebStorageOperationCallback* callback)
{
	if (m_wsb == NULL) return OpStatus::ERR;

	if (HasCachedNumberOfItems())
		//bypass callback object because we already have cached count
		return m_wsb->GetNumberOfItems(callback);

	WebStorageOperationCallbackOpStorageWrapper *wp = GetEntry()->GetUnusedCallbackObj();
	RETURN_OOM_IF_NULL(wp);
	wp->Setup(this, callback);

	OP_STATUS status = m_wsb->GetNumberOfItems(wp);

	if (OpStatus::IsError(status))
		GetEntry()->DisposeCallbackObj(wp);

	return status;
}

OP_STATUS
OpStorage::GetKeyAtIndex(unsigned index, WebStorageOperationCallback* callback)
{
	if (m_wsb == NULL) return OpStatus::ERR;
	return m_wsb->GetKeyAtIndex(index, callback);
}

OP_STATUS
OpStorage::GetItem(const WebStorageValue *key, WebStorageOperationCallback* callback)
{
	if (m_wsb == NULL) return OpStatus::ERR;
	return m_wsb->GetItem(key, callback);
}

/*virtual*/
OP_STATUS
OpStorage::EnumerateAllKeys(WebStorageKeyEnumerator* enumerator)
{
	if (m_wsb == NULL) return OpStatus::ERR;
	return m_wsb->EnumerateAllKeys(enumerator);
}

OP_STATUS
OpStorage::SetItem(const WebStorageValue *key, const WebStorageValue *value, WebStorageOperationCallback* callback, const OpStorageEventContext& event_ctx)
{
	return SetItemAux(WebStorageOperation::SET_ITEM, key, value, FALSE, callback, event_ctx);
}

#ifdef WEBSTORAGE_RO_PAIRS_SUPPORT
OP_STATUS
OpStorage::SetItemReadOnly(const WebStorageValue *key, const WebStorageValue *value, BOOL is_readonly, WebStorageOperationCallback* callback, const OpStorageEventContext& event_ctx)
{
	return SetItemAux(WebStorageOperation::SET_ITEM_RO, key, value, is_readonly, callback, event_ctx);
}

OP_STATUS
OpStorage::SetNewItemReadOnly(const WebStorageValue *key, const WebStorageValue *value, BOOL is_readonly, WebStorageOperationCallback* callback, const OpStorageEventContext& event_ctx)
{
	return SetItemAux(WebStorageOperation::SET_NEW_ITEM_RO, key, value, is_readonly, callback, event_ctx);
}
#endif //WEBSTORAGE_RO_PAIRS_SUPPORT

/**
 * Internal to SetItem, SetItemReadOnly and SetnewItemReadonly.
 */
OP_STATUS
OpStorage::SetItemAux(WebStorageOperation::WebStorageOperationEnum type, const WebStorageValue *key,
	const WebStorageValue *value, BOOL is_readonly, WebStorageOperationCallback* callback, const OpStorageEventContext& event_ctx)
{
	if (m_wsb == NULL) return OpStatus::ERR;
	OP_ASSERT(key != NULL);

	OP_STATUS status;

	if (HasStorageEventListeners())
	{
		WebStorageOperationCallbackOpStorageWrapper *wp = GetEntry()->GetUnusedCallbackObj();
		RETURN_OOM_IF_NULL(wp);
		wp->Setup(this, callback, event_ctx);

		wp->m_key = key->Create(key);
		if (wp->m_key == NULL)
		{
			GetEntry()->DisposeCallbackObj(wp);
			return OpStatus::ERR_NO_MEMORY;
		}

		if (value != NULL)
		{
			wp->m_value = value->Create(value);
			if (wp->m_value == NULL)
			{
				GetEntry()->DisposeCallbackObj(wp);
				return OpStatus::ERR_NO_MEMORY;
			}
		}

#ifdef WEBSTORAGE_RO_PAIRS_SUPPORT
		if (type == WebStorageOperation::SET_ITEM_RO)
			status = m_wsb->SetItemReadOnly(key, value, is_readonly, wp);
		else if (type == WebStorageOperation::SET_NEW_ITEM_RO)
			status = m_wsb->SetNewItemReadOnly(key, value, is_readonly, wp);
		else
#endif // WEBSTORAGE_RO_PAIRS_SUPPORT
			status = m_wsb->SetItem(key, value, wp);

		if (OpStatus::IsSuccess(status))
			UnsetCachedNumberOfItems();
		else
			GetEntry()->DisposeCallbackObj(wp);
	}
	else
	{
#ifdef WEBSTORAGE_RO_PAIRS_SUPPORT
		if (type == WebStorageOperation::SET_ITEM_RO)
			status = m_wsb->SetItemReadOnly(key, value, is_readonly, callback);
		else if (type == WebStorageOperation::SET_NEW_ITEM_RO)
			status = m_wsb->SetNewItemReadOnly(key, value, is_readonly, callback);
		else
#endif // WEBSTORAGE_RO_PAIRS_SUPPORT
			status = m_wsb->SetItem(key, value, callback);
	}

	if (OpStatus::IsSuccess(status))
		UnsetCachedNumberOfItems();

	return status;
}

OP_STATUS
OpStorage::Clear(WebStorageOperationCallback* callback, const OpStorageEventContext& event_ctx)
{
	if (m_wsb == NULL) return OpStatus::ERR;

	OP_STATUS status;

	if (HasStorageEventListeners())
	{
		WebStorageOperationCallbackOpStorageWrapper *wp = GetEntry()->GetUnusedCallbackObj();
		RETURN_OOM_IF_NULL(wp);
		wp->Setup(this, callback, event_ctx);

		status = m_wsb->Clear(wp);

		if (OpStatus::IsSuccess(status))
			UnsetCachedNumberOfItems();
		else
			GetEntry()->DisposeCallbackObj(wp);

		return status;
	}

	status = m_wsb->Clear(callback);

	if (OpStatus::IsSuccess(status))
		UnsetCachedNumberOfItems();

	return status;
}

#ifdef WEBSTORAGE_RO_PAIRS_SUPPORT
OP_STATUS
OpStorage::ClearAll(WebStorageOperationCallback* callback, const OpStorageEventContext& event_ctx)
{
	if (m_wsb == NULL) return OpStatus::ERR;

	OP_STATUS status;

	if (HasStorageEventListeners())
	{
		WebStorageOperationCallbackOpStorageWrapper *wp = GetEntry()->GetUnusedCallbackObj();
		RETURN_OOM_IF_NULL(wp);
		wp->Setup(this, callback, event_ctx);

		UnsetCachedNumberOfItems();

		status = m_wsb->ClearAll(wp);

		if (OpStatus::IsError(status))
			GetEntry()->DisposeCallbackObj(wp);

		return status;
	}

	status = m_wsb->ClearAll(callback);

	if (OpStatus::IsSuccess(status))
		UnsetCachedNumberOfItems();

	return status;
}
#endif //WEBSTORAGE_RO_PAIRS_SUPPORT

#ifdef SELFTEST
void
OpStorage::CancelAll()
{
	if (m_wsb != NULL)
		m_wsb->CancelAll();
}
#endif


/*virtual*/
void
OpStorage::HandleStateChange(WebStorageStateChangeListener::StateChange v)
{
	switch(v)
	{
	case STORAGE_MUTATED:
		UnsetCachedNumberOfItems();
		break;
	case SHUTDOWN:
		ReleaseWSB(FALSE);
		break;
	}
}


#define ALIGNED_UNI_LENGTH(v) ((UNICODE_SIZE((v)+1) + 3) & ~3)

/*static*/
OP_STATUS
OpStorageValueChangedEvent::Create(WebStorageType storage_type,
		const uni_char* key, unsigned key_length, const uni_char* old_value,
		unsigned old_value_length, const uni_char* new_value, unsigned new_value_length,
		const OpStorageEventContext& event_ctx, OpStorageValueChangedEvent*& event)
{

	if (key == NULL && new_value == NULL && old_value == NULL)
		return Create(storage_type, event_ctx, event);

	//buffer will hold the OpStorageValueChangedEvent, ValueChangedData struct + buffers -> single allocation, less fragmentation
	//buffer will be used both for the key and new value.
	byte* data_buffer = OP_NEWA(byte, (sizeof(OpStorageValueChangedEvent) +
			sizeof(OpStorageValueChangedEvent::ValueChangedData) +
			(key       == NULL ? 0 : ALIGNED_UNI_LENGTH(key_length))+
			(new_value == NULL ? 0 : ALIGNED_UNI_LENGTH(new_value_length)) +
			(old_value == NULL ? 0 : ALIGNED_UNI_LENGTH(old_value_length))));

	if (data_buffer == NULL)
		return SIGNAL_OP_STATUS_ERROR(OpStatus::ERR_NO_MEMORY);

	event = new (data_buffer) OpStorageValueChangedEvent();
	OP_ASSERT(event->GetRefCount() == 0);//checks proper initialization

	data_buffer += sizeof(OpStorageValueChangedEvent);

	OpStorageValueChangedEvent::ValueChangedData* data = reinterpret_cast<OpStorageValueChangedEvent::ValueChangedData*>(data_buffer);
	data_buffer += sizeof(OpStorageValueChangedEvent::ValueChangedData);
	op_memset(data,0,sizeof(OpStorageValueChangedEvent::ValueChangedData));

	if (key != NULL)
	{
		data->m_key = reinterpret_cast<uni_char*>(data_buffer);
		data_buffer += ALIGNED_UNI_LENGTH(key_length);
		op_memcpy(data->m_key, key, UNICODE_SIZE(key_length));
		data->m_key[key_length] = 0;
		data->m_key_length = key_length;
	}
	else
	{
		data->m_key = NULL;
		data->m_key_length = 0;
	}

	if (new_value != NULL)
	{
		data->m_new_value = reinterpret_cast<uni_char*>(data_buffer);
		data_buffer += ALIGNED_UNI_LENGTH(new_value_length);
		op_memcpy(data->m_new_value, new_value, UNICODE_SIZE(new_value_length));
		data->m_new_value[new_value_length] = 0;
		data->m_new_value_length = new_value_length;
	}
	else
	{
		data->m_new_value = NULL;
		data->m_new_value_length = 0;
	}

	if (old_value != NULL)
	{
		data->m_old_value = reinterpret_cast<uni_char*>(data_buffer);
		data_buffer += ALIGNED_UNI_LENGTH(old_value_length);
		op_memcpy(data->m_old_value, old_value, UNICODE_SIZE(old_value_length));
		data->m_old_value[old_value_length] = 0;
		data->m_old_value_length = old_value_length;
	}
	else
	{
		data->m_old_value = NULL;
		data->m_old_value_length = 0;
	}

	event->m_context = event_ctx;
	event->m_storage_type = storage_type;
	event->m_event_data = data;
	event->m_is_clear = false;

	return OpStatus::OK;
}

#undef ALIGNED_UNI_LENGTH

/*static*/
OP_STATUS
OpStorageValueChangedEvent::Create(WebStorageType storage_type, const OpStorageEventContext& event_ctx, OpStorageValueChangedEvent*& event)
{
	byte* data_buffer = OP_NEWA(byte, sizeof(OpStorageValueChangedEvent));
	if (data_buffer == NULL)
		return SIGNAL_OP_STATUS_ERROR(OpStatus::ERR_NO_MEMORY);

	event = new (data_buffer) OpStorageValueChangedEvent();
	OP_ASSERT(event->GetRefCount() == 0);//checks proper initialization

	event->m_context = event_ctx;
	event->m_storage_type = storage_type;
	event->m_event_data = NULL;
	event->m_is_clear = true;

	return OpStatus::OK;
}


/*************
 * OpStorageEventMessageHandler
 */

OpStorageEventMessageHandler::~OpStorageEventMessageHandler()
{
	INTEGRITY_CHECK();
	OpStorageValueChangedEvent* event = DequeueEventObject();
	while (event != NULL)
	{
		event->Release();
		event = DequeueEventObject();
	}
	m_storage_event_listeners.RemoveAll();
	g_opstorage_globals->GetMessageHandler()->UnsetCallBack(this, MSG_OPSTORAGE_VALUE_CHANGED, GetMessageQueueId());
}

OP_STATUS
OpStorageEventMessageHandler::Init()
{
	INTEGRITY_CHECK();

	if (!GetFlag(INITIALIZED_MSG_QUEUE))
	{
		OP_STATUS status = g_opstorage_globals->GetMessageHandler()->SetCallBack(this, MSG_OPSTORAGE_VALUE_CHANGED, GetMessageQueueId());
		if (OpStatus::IsError(status))
		{
			SetFlag(OBJ_INITIALIZED_ERROR);
			return SIGNAL_OP_STATUS_ERROR(status);
		}
		SetFlag(INITIALIZED_MSG_QUEUE);
	}
	return OpStatus::OK;
}

OP_STATUS
OpStorageEventMessageHandler::EnqueueEventObject(OpStorageValueChangedEvent* e)
{
	INTEGRITY_CHECK();

	EventQueueElement* q_el = OP_NEW(EventQueueElement,(e));
	RETURN_OOM_IF_NULL(q_el);
	q_el->Into(&m_storage_event_objects);
	return OpStatus::OK;
}

OpStorageValueChangedEvent*
OpStorageEventMessageHandler::DequeueEventObject()
{
	INTEGRITY_CHECK();

	EventQueueElement* queue_elem = m_storage_event_objects.First();
	if (queue_elem == NULL)
		return NULL;
	OpStorageValueChangedEvent* event = queue_elem->m_event;
	OP_ASSERT(event != NULL);
	queue_elem->Out();
	OP_DELETE(queue_elem);

	return event;
}

OpStorageValueChangedEvent*
OpStorageEventMessageHandler::PopEventObject()
{
	INTEGRITY_CHECK();

	EventQueueElement* queue_elem = m_storage_event_objects.Last();
	if (queue_elem == NULL)
		return NULL;
	OpStorageValueChangedEvent* event = queue_elem->m_event;
	OP_ASSERT(event != NULL);
	queue_elem->Out();
	OP_DELETE(queue_elem);

	return event;
}

void
OpStorageEventMessageHandler::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	INTEGRITY_CHECK();

	switch(msg)
	{
	case MSG_OPSTORAGE_VALUE_CHANGED:
	{
		OP_ASSERT(reinterpret_cast<OpStorageEventMessageHandler*>(par1) == this);

		OpStorageValueChangedEvent* event = DequeueEventObject();
		if (event == NULL)
			return;

		g_opstorage_globals->DecNumberOfUnprocessedEventMessages();

		OP_STATUS status = OpStatus::OK;
		OpStorageEventListener *next, *list = m_storage_event_listeners.First();

		//event will be owned by the callback object, and it'll have to call Release()
		//for each message posted previously, the ref counter was incremented once
		//because prior to posting the message, the code that handles the message posting is not aware
		//of the number of listeners in the OpStorage object.
		//therefore the ref counter need to be incremented if there is MORE THAN ONE listener
		if (list != NULL)
		{
			OP_ASSERT(event->GetRefCount() == 1);
			while (list != NULL)
			{
				// We get 'next' here because the callback
				// method might clear 'list' from the listeners
				// list, then Suc() returns nothing.
				next = list->Suc();

				if (next != NULL)
					event->IncRefCount();

				if (list->HasListeners())
					status = OpDbUtils::GetOpStatusError(list->HandleEvent(event),status);
				else
					event->Release();

				list = next;
			}
		}
		else
			event->Release();

		ReportCondition(status);

		break;
	}
	default:
		OP_ASSERT(!"Warning: unhandled message received");
	}
}

//if the data object was not posted to any message, then we
//must delete it else it'll leak
//if it was posted then it'll have an apropriate ref count
//and the message handler will take care of it
OP_STATUS
OpStorageEventMessageHandler::FireValueChangedListeners(const WebStorageValue* key,
		const WebStorageValue* old_value, const WebStorageValue* new_value, const OpStorageEventContext& event_ctx)
{
	INTEGRITY_CHECK();

	OP_ASSERT(key != NULL);

	if (HasStorageEventListeners())
	{
		OpStorageValueChangedEvent *event_obj = NULL;
		RETURN_IF_ERROR(OpStorageValueChangedEvent::Create(m_storage_entry->m_type,
				WSV_STR(key), WSV_LENGTH(key),
				WSV_STR(old_value), WSV_LENGTH(old_value),
				WSV_STR(new_value), WSV_LENGTH(new_value),
				event_ctx, event_obj));

		OP_ASSERT(event_obj != NULL);
		RETURN_IF_ERROR(g_opstorage_globals->PostStorageEventMessage(this, event_obj));
	}
	return OpStatus::OK;
}

OP_STATUS
OpStorageEventMessageHandler::FireClearListeners(const OpStorageEventContext& event_ctx)
{
	INTEGRITY_CHECK();

	if (HasStorageEventListeners())
	{
		OpStorageValueChangedEvent *event_obj = NULL;
		RETURN_IF_ERROR(OpStorageValueChangedEvent::Create(m_storage_entry->m_type, event_ctx, event_obj));

		OP_ASSERT(event_obj != NULL);
		RETURN_IF_ERROR(g_opstorage_globals->PostStorageEventMessage(this, event_obj));
	}
	return OpStatus::OK;
}

/*virtual*/
BOOL
OpStorageEventMessageHandler::HasStorageEventListeners()
{
	OpStorageEventListener *n = m_storage_event_listeners.First();

	while(n != NULL && !n->HasListeners())
		n = n->Suc();

	if (n == NULL)
		return FALSE;

	OP_ASSERT(n->HasListeners());

	if (n != m_storage_event_listeners.First())
	{
		//move to beginning so the next time this is called, it returns faster
		//order of callbacks is undefined in the API, so it does not matter
		n->Out();
		n->IntoStart(&m_storage_event_listeners);
	}

	return TRUE;
}

/*virtual*/
unsigned
OpStorageEventMessageHandler::GetNumberOfListeners() const
{
	OpStorageEventListener *n = m_storage_event_listeners.First();
	unsigned count = 0;
	while(n != NULL)
	{
		if (n->HasListeners())
			count++;
		n = n->Suc();
	}
	return count;
}


//needed for the system clock
#include "modules/pi/OpSystemInfo.h"

OP_STATUS
OpStorageGlobals::PostStorageEventMessage(OpStorageEventMessageHandler *message_handler, OpStorageValueChangedEvent* event)
{
#if OPSTORAGE_EVENTS_MAX_AMOUNT_EVENTS > 0
	if (GetNumberOfUnprocessedEventMessages() >= OPSTORAGE_EVENTS_MAX_AMOUNT_EVENTS)
	{
		//ignore this event and quit
		//signal error so everything can unwind back to ecmascript
		event->Release();
		return SIGNAL_OP_STATUS_ERROR(OpStatus::ERR);
	}
	IncNumberOfUnprocessedEventMessages();
#endif

#if OPSTORAGE_EVENTS_MIN_INTERVAL_MS > 0
	unsigned msg_timeout = 0;
	double current_time = g_op_time_info->GetWallClockMS();

	if (m_last_event_time <= current_time)
	{
		msg_timeout = OPSTORAGE_EVENTS_MIN_INTERVAL_MS;
	}
	else
	{
		msg_timeout = static_cast<unsigned>(m_last_event_time-current_time)+OPSTORAGE_EVENTS_MIN_INTERVAL_MS;
	}

	m_last_event_time = current_time + msg_timeout;
#else
# define msg_timeout 1
#endif

	event->IncRefCount();
	if (OpStatus::IsError(message_handler->EnqueueEventObject(event)))
	{
		event->Release();
		return SIGNAL_OP_STATUS_ERROR(OpStatus::ERR_NO_MEMORY);
	}

	if (!GetMessageHandler()->PostMessage(MSG_OPSTORAGE_VALUE_CHANGED,
			reinterpret_cast<MH_PARAM_1>(message_handler), 0, msg_timeout))
	{
		message_handler->PopEventObject();
		event->Release();
		return SIGNAL_OP_STATUS_ERROR(OpStatus::ERR_NO_MEMORY);
	}
	return OpStatus::OK;
#undef msg_timeout
}


#endif // OPSTORAGE_SUPPORT
