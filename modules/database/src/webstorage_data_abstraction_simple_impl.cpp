/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef WEBSTORAGE_ENABLE_SIMPLE_BACKEND

#include "modules/pi/OpSystemInfo.h"
#include "modules/database/src/webstorage_data_abstraction_simple_impl.h"
#include "modules/windowcommander/src/WindowCommander.h"

#ifdef EXTENSION_SUPPORT
# include "modules/gadgets/OpGadgetManager.h"
#endif //  EXTENSION_SUPPORT

/*static*/
OP_STATUS
WebStorageBackend::Create(WebStorageType type, const uni_char* origin, BOOL is_persistent,
		URL_CONTEXT_ID context_id, WebStorageBackend** result)
{
	WebStorageBackend_SimpleImpl* object;
	RETURN_IF_ERROR(WebStorageBackend_SimpleImpl::GetInstance(type, origin, is_persistent, context_id, &object));
	*result = object;

	return OpStatus::OK;
};

/*static*/OP_STATUS
WebStorageBackend_SimpleImpl::GetInstance(WebStorageType type, const uni_char* origin,
			BOOL is_persistent, URL_CONTEXT_ID context_id, WebStorageBackend_SimpleImpl** result)
{
	OP_ASSERT(result != NULL);

	TempBuffer ss_origin;
	if (type == WEB_STORAGE_SESSION)
	{
		//FIXME we need a way to differentiate browsing contexts
		//however, this is kind of good enough given that the storage object
		//is created only once per top level docmanager, but the data will
		//dissapear when restoring stuff from the trash can.
		RETURN_IF_ERROR(ss_origin.Append(origin));
		RETURN_IF_ERROR(ss_origin.Append("/$"));
		RETURN_IF_ERROR(ss_origin.AppendFormat(UNI_L("%x"), op_rand()));
		is_persistent = FALSE;
		origin = ss_origin.GetStorage();
	}

	PS_Object* ps_obj;
	RETURN_IF_ERROR(PS_Object::GetInstance(WebStorageTypeToPSObjectType(type), origin, NULL, is_persistent, context_id, &ps_obj));

	INTEGRITY_CHECK_P(static_cast<WebStorageBackend_SimpleImpl*>(ps_obj));
	*result = static_cast<WebStorageBackend_SimpleImpl*>(ps_obj);

	return OpStatus::OK;
}

/************************************
 * QuickWebStorageShutdownListener
 ************************************/
class QuickWebStorageShutdownListener: public WebStorageStateChangeListener
{
public:
	BOOL m_was_deleted;
	QuickWebStorageShutdownListener(WebStorageBackend* v)
		: m_was_deleted(FALSE)
		, m_ptr(v)
	{
		OP_ASSERT(v != NULL);
		m_ptr->AddListener(this);
	}
	~QuickWebStorageShutdownListener()
	{
		if (m_ptr)
			m_ptr->RemoveListener(this);
	}

	virtual void HandleStateChange(StateChange v)
	{
		if (v == SHUTDOWN)
		{
			m_was_deleted = TRUE;
			m_ptr = NULL;
		}
	}
private:
	WebStorageBackend *m_ptr;
};


/****************************
 * WebStorageValueInfoTable
 ****************************/
/*virtual*/
UINT32 WebStorageValueInfoTable::Hash(const void* key)
{
	return OpGenericStringHashTable::HashString(reinterpret_cast<const WebStorageValue*>(key)->m_value,
				reinterpret_cast<const WebStorageValue*>(key)->m_value_length, TRUE);
}

/*virtual*/
BOOL WebStorageValueInfoTable::KeysAreEqual(const void* key1, const void* key2)
{
	return reinterpret_cast<const WebStorageValue*>(key1)->Equals(reinterpret_cast<const WebStorageValue*>(key2));
}

/*virtual*/
void WebStorageValueInfoTable::Delete(void* data)
{
	OP_ASSERT(data); if(!data) return;

	WebStorageValueInfo* vi = reinterpret_cast<WebStorageValueInfo*>(data);
	OP_DELETE(vi);
}

/****************************
 * WebStorageOperationFixedValue
 ****************************/
WebStorageOperationFixedValue::~WebStorageOperationFixedValue()
{
	m_data.m_storage.m_value = NULL;
}

void
WebStorageOperationFixedValue::SetValue()
{
	OP_ASSERT(!IsError() && (m_type == GET_KEY_BY_INDEX || m_type == GET_ITEM_BY_KEY || IS_OP_SET_ITEM(this)));
	m_data.m_storage.m_value = &m_value_holder;
}

/************************************
 * WebStorageBackend_SimpleImpl_QuotaCallback
 ************************************/
class WebStorageBackend_SimpleImpl_QuotaCallback : public OpDocumentListener::QuotaCallback
{
public:
	WebStorageBackend_SimpleImpl_QuotaCallback(WebStorageBackend_SimpleImpl *wsb) : m_wsb(wsb){}

	virtual void OnQuotaReply(BOOL allow_increase, OpFileLength new_quota_size)
	{
		if (m_wsb != NULL)
		{
			m_wsb->OnQuotaReply(allow_increase, new_quota_size);
		}
		else
		{
			// Object orphaned
			OP_DELETE(this);
		}
	}
	virtual void OnCancel()
	{
		if (m_wsb != NULL)
		{
			m_wsb->OnQuotaCancel();
		}
		else
		{
			// Object orphaned
			OP_DELETE(this);
		}
	}
	WebStorageBackend_SimpleImpl *m_wsb;
};

/************************************
 * WebStorageBackend_SimpleImpl, the simple backend version
 ************************************/
WebStorageBackend_SimpleImpl::WebStorageBackend_SimpleImpl(PS_IndexEntry* entry)
	: PS_Object(entry),
	m_last_operation_time(0),
	m_executed_operation_count(0),
	m_data_loader(NULL),
	m_data_saver(NULL),
	m_init_error(OpStatus::OK),
	m_errors_delayed(0),
	m_quota_callback(NULL)
{}

WebStorageBackend_SimpleImpl::~WebStorageBackend_SimpleImpl()
{
	DB_DBG(("%p: WebStorageBackend_SimpleImpl::~WebStorageBackend_()\n", this));

	if (m_quota_callback != NULL)
	{
		if (GetQuotaHandlingStatus() == PS_IndexEntry::QS_WAITING_FOR_USER)
		{
			OnQuotaCancel();
			m_quota_callback->m_wsb = NULL;
		}
		else
			OP_DELETE(m_quota_callback);
	}

	//no more operations if the core is still running
	OP_ASSERT(!IsOperaRunning() || IsQueueEmpty());

	OP_ASSERT(!GetFlag(BEING_DELETED));
	SetFlag(BEING_DELETED);

	if (IsInitialized())
		ReportCondition(SaveToDiskSync());

	BOOL is_empty = IsEmpty();

	//WebStorageBackend
	InvokeShutdownListeners();
	//PS_ObjDelListener
	FireShutdownCallbacks();

	OP_DELETE(m_data_loader);
	m_data_loader = NULL;
	OP_DELETE(m_data_saver);
	m_data_saver = NULL;

	m_pending_operations.Clear();
	m_storage_area.DeleteAll();

	if (GetFlag(INITED_MESSAGE_QUEUE) && GetMessageHandler() != NULL)
		GetMessageHandler()->UnsetCallBacks(this);

	HandleObjectShutdown(IsInitialized() && is_empty);
}

/**
 * Synchronously evaluates the amount of data spent by this db.
 *
 */
OP_STATUS
WebStorageBackend_SimpleImpl::EvalDataSizeSync(OpFileLength *result)
{
	OP_ASSERT(result != NULL);
	*result = 0;

	if (IsInitialized())
	{
		unsigned length = (unsigned)m_storage_area_sorted.GetCount();
		for (unsigned k = 0; k < length; k++)
		{
			*result += ConvertPairSize(m_storage_area_sorted.Get(k));
		}
		return OpStatus::OK;
	}

	return OpStatus::OK;
}

/*virtual*/ BOOL
WebStorageBackend_SimpleImpl::PreClearData()
{
	BOOL has_data = !IsEmpty();
	DB_DBG(("%p: WebStorageBackend_SimpleImpl::PreClearData       (): %d\n", this, has_data));
	ReportCondition(WEBSTORAGE_CLEAR_ALL(this, NULL));
	return has_data;
}

/* virtual */ BOOL
WebStorageBackend_SimpleImpl::HasPendingActions() const
{
	return !IsQueueEmpty() || GetFlag(HAS_POSTED_DELAYED_FLUSH_MSG);
}

WebStorageBackend_SimpleImpl*
WebStorageBackend_SimpleImpl::Create(PS_IndexEntry* entry)
{
	WebStorageBackend_SimpleImpl *new_wsb = OP_NEW(WebStorageBackend_SimpleImpl,(entry));
	RETURN_VALUE_IF_NULL(new_wsb, NULL);

	OP_STATUS status = new_wsb->Init(PSObjectTypeToWebStorageType(entry->GetType()),
			entry->GetOrigin(), entry->IsPersistent(), entry->GetUrlContextId());

	if (OpStatus::IsError(status))
	{
		OP_DELETE(new_wsb);
		return NULL;
	}

	return new_wsb;
}

OP_STATUS
WebStorageBackend_SimpleImpl::Release()
{
	OP_ASSERT(GetIndexEntry() != NULL);
	OP_ASSERT(GetIndexEntry()->GetRefCount() > 0);

	GetIndexEntry()->DecRefCount();

	DB_DBG(("%p: WebStorageBackend_SimpleImpl::Release            (%d): %d, %d\n",
			this, GetIndexEntry()->GetRefCount(), IsQueueEmpty(), GetFlag(HAS_BEEN_MODIFIED)));

	// Trigger a save for each Release().
	OP_STATUS status = IsOperaRunning() ? SaveToDiskAsync() : OpStatus::OK;

	SafeDelete();

	return status;
}

static OP_STATUS
WebStorageBackend_SimpleImpl_FillInOpValue(WebStorageBackendOperation* op, const WebStorageValue*& dest, const WebStorageValue* src)
{
	if (src == NULL)
		return OpStatus::OK;

	OP_ASSERT(op);
	OP_ASSERT(&op->m_key == &dest || &op->m_value == &dest);

	dest = WebStorageValue::Create(src->m_value, src->m_value_length);
	if (dest == NULL)
	{
		op->Drop();
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

/*virtual*/ OP_STATUS
WebStorageBackend_SimpleImpl::GetNumberOfItems(WebStorageOperationCallback* callback)
{
	//callback might be NULL due to internal use
	WebStorageBackendOperation *op = OP_NEW(WebStorageBackendOperation,(this, WebStorageOperation::GET_ITEM_COUNT, callback));
	RETURN_OOM_IF_NULL(op);

	return ScheduleOperation(op);
}

/*virtual*/ OP_STATUS
WebStorageBackend_SimpleImpl::GetKeyAtIndex(unsigned index, WebStorageOperationCallback* callback)
{
	OP_ASSERT(callback != NULL);
	WebStorageBackendOperation *op = OP_NEW(WebStorageBackendOperation,(this, WebStorageOperation::GET_KEY_BY_INDEX, callback));
	RETURN_OOM_IF_NULL(op);

	op->m_index = index;

	return ScheduleOperation(op);
}

/*virtual*/ OP_STATUS
WebStorageBackend_SimpleImpl::GetItem(const WebStorageValue *key, WebStorageOperationCallback* callback)
{
	OP_ASSERT(key != NULL);
	OP_ASSERT(callback != NULL);
	WebStorageBackendOperation *op = OP_NEW(WebStorageBackendOperation,(this, WebStorageOperation::GET_ITEM_BY_KEY, callback));
	RETURN_OOM_IF_NULL(op);

	RETURN_IF_ERROR(WebStorageBackend_SimpleImpl_FillInOpValue(op, op->m_key, key));

	return ScheduleOperation(op);
}

/*virtual*/ OP_STATUS
WebStorageBackend_SimpleImpl::SetItem(const WebStorageValue *key, const WebStorageValue *value, WebStorageOperationCallback* callback)
{
	OP_ASSERT(key != NULL);
	WebStorageBackendOperation *op = OP_NEW(WebStorageBackendOperation,(this, WebStorageOperation::SET_ITEM, callback));
	RETURN_OOM_IF_NULL(op);

	RETURN_IF_ERROR(WebStorageBackend_SimpleImpl_FillInOpValue(op, op->m_key, key));
	RETURN_IF_ERROR(WebStorageBackend_SimpleImpl_FillInOpValue(op, op->m_value, value));

	return ScheduleOperation(op);
}

#ifdef WEBSTORAGE_RO_PAIRS_SUPPORT
/*virtual*/ OP_STATUS
WebStorageBackend_SimpleImpl::SetItemReadOnly(const WebStorageValue *key, const WebStorageValue *value, BOOL is_readonly, WebStorageOperationCallback* callback)
{
	OP_ASSERT(key != NULL);
	WebStorageBackendOperation *op = OP_NEW(WebStorageBackendOperation,(this, WebStorageOperation::SET_ITEM_RO, callback));
	RETURN_OOM_IF_NULL(op);

	RETURN_IF_ERROR(WebStorageBackend_SimpleImpl_FillInOpValue(op, op->m_key, key));
	RETURN_IF_ERROR(WebStorageBackend_SimpleImpl_FillInOpValue(op, op->m_value, value));

	if (is_readonly)
		op->SetFlag(WebStorageBackendOperation::SET_RO_FLAG_TRUE);

	return ScheduleOperation(op);
}

/*virtual*/ OP_STATUS
WebStorageBackend_SimpleImpl::SetNewItemReadOnly(const WebStorageValue *key, const WebStorageValue *value, BOOL is_readonly, WebStorageOperationCallback* callback)
{
	OP_ASSERT(key != NULL);
	WebStorageBackendOperation *op = OP_NEW(WebStorageBackendOperation,(this, WebStorageOperation::SET_NEW_ITEM_RO, callback));
	RETURN_OOM_IF_NULL(op);

	RETURN_IF_ERROR(WebStorageBackend_SimpleImpl_FillInOpValue(op, op->m_key, key));
	RETURN_IF_ERROR(WebStorageBackend_SimpleImpl_FillInOpValue(op, op->m_value, value));

	if (is_readonly)
		op->SetFlag(WebStorageBackendOperation::SET_RO_FLAG_TRUE);

	return ScheduleOperation(op);
}

#endif //WEBSTORAGE_RO_PAIRS_SUPPORT

/*virtual*/ OP_STATUS
WebStorageBackend_SimpleImpl::EnumerateAllKeys(WebStorageKeyEnumerator* enumerator)
{
	OP_ASSERT(enumerator != NULL);
	WebStorageBackendOperation *op = OP_NEW(WebStorageBackendOperation,(this, WebStorageBackendOperation::ENUMERATE_ITEMS, enumerator));
	RETURN_OOM_IF_NULL(op);

	return ScheduleOperation(op);
}

/*virtual*/ OP_STATUS
WebStorageBackend_SimpleImpl::EnumerateAllKeys_Internal(WebStorageKeyEnumerator* enumerator)
{
	OP_STATUS status = OpStatus::OK;
	unsigned length = (unsigned)m_storage_area_sorted.GetCount();
	WebStorageValueInfo* vi;
	if (enumerator->WantsValues())
	{
		for (unsigned k = 0; k < length; k++)
		{
			vi = m_storage_area_sorted.Get(k);
			OP_ASSERT(vi);
			status = OpDbUtils::GetOpStatusError(status, enumerator->HandleKey(k, &vi->m_key, &vi->m_value));
		}
	}
	else
	{
		for (unsigned k = 0; k < length; k++)
		{
			vi = m_storage_area_sorted.Get(k);
			OP_ASSERT(vi);
			status = OpDbUtils::GetOpStatusError(status, enumerator->HandleKey(k, &vi->m_key, NULL));
		}
	}
	return status;
}

/*virtual*/ OP_STATUS
WebStorageBackend_SimpleImpl::Clear(WebStorageOperationCallback* callback)
{
	WebStorageBackendOperation *op = OP_NEW(WebStorageBackendOperation,(this, WebStorageOperation::CLEAR, callback));
	RETURN_OOM_IF_NULL(op);

	return ScheduleOperation(op);
}

#ifdef WEBSTORAGE_RO_PAIRS_SUPPORT
/*virtual*/ OP_STATUS
WebStorageBackend_SimpleImpl::ClearAll(WebStorageOperationCallback* callback)
{
	WebStorageBackendOperation *op = OP_NEW(WebStorageBackendOperation,(this, WebStorageOperation::CLEAR_RO, callback));
	RETURN_OOM_IF_NULL(op);

	return ScheduleOperation(op);
}
#endif //WEBSTORAGE_RO_PAIRS_SUPPORT

/*virtual*/ void
WebStorageBackend_SimpleImpl::Release(BOOL force)
{
	ReportCondition(Release());
}

#ifdef SELFTEST
/*virtual*/ void
WebStorageBackend_SimpleImpl::CancelAll()
{
	m_pending_operations.Clear();
}
#endif //SELFTEST

OP_STATUS
WebStorageBackend_SimpleImpl::ScheduleOperation(WebStorageBackendOperation* op)
{
	OP_ASSERT(op != NULL);

	OP_STATUS status = PostExecutionMessage();
	if (OpStatus::IsError(status))
		op->Drop();
	else
		op->Into(&m_pending_operations);

	return status;
}

OP_STATUS
WebStorageBackend_SimpleImpl::InitMessageListeners()
{
	OpMessage messages[2] = {MSG_WEBSTORAGE_EXECUTE_OPERATION, MSG_WEBSTORAGE_DELAYED_FLUSH};

	if (!GetFlag(INITED_MESSAGE_QUEUE) && GetMessageHandler() != NULL)
	{
		RETURN_IF_ERROR(GetMessageHandler()->SetCallBackList(this, GetMessageQueueId(), messages, ARRAY_SIZE(messages)));
		SetFlag(INITED_MESSAGE_QUEUE);
	}
	return OpStatus::OK;
}

OP_STATUS
WebStorageBackend_SimpleImpl::PostExecutionMessage(unsigned timeout)
{
	if (!IsOperaRunning() || GetMessageHandler() == NULL)
		return OpStatus::ERR;

	if (GetFlag(HAS_POSTED_EXECUTION_MSG))
		return OpStatus::OK;

	if (GetFlag(WAITING_FOR_DATA_LOAD))
		return OpStatus::OK;

	RETURN_IF_ERROR(InitMessageListeners());

	OP_STATUS status = GetMessageHandler()->PostMessage(MSG_WEBSTORAGE_EXECUTE_OPERATION,
			GetMessageQueueId(), 0, timeout) ? OpStatus::OK : SIGNAL_OP_STATUS_ERROR(OpStatus::ERR_NO_MEMORY);
	if (OpStatus::IsSuccess(status))
		SetFlag(HAS_POSTED_EXECUTION_MSG);

	return status;
}

OP_STATUS
WebStorageBackend_SimpleImpl::PostDelayedFlush(unsigned timeout)
{
	if (!IsOperaRunning() || GetMessageHandler() == NULL)
		return OpStatus::ERR;

	if (GetFlag(HAS_POSTED_DELAYED_FLUSH_MSG) || !HasModifications() || !IsQueueEmpty())
		return OpStatus::OK;

	if (GetFlag(WAITING_FOR_DATA_LOAD))
		return OpStatus::OK;

	if (GetFileNameObj() && GetFileNameObj()->IsBogus())
		// Without this check, the next time the file would be flushed
		// if would fail. The, the modification flag would not be cleared
		// which would cause a new flush message to be posted, so
		// we'd get an infinite cycle of messages telling the data to
		// be flushed.
		return OpStatus::OK;

	DB_DBG(("%p: WebStorageBackend_SimpleImpl::PostDelayedFlush   (Delay: %u, QueueEmpty: %d, Modified: %d)\n", this, timeout, IsQueueEmpty(), GetFlag(HAS_BEEN_MODIFIED)));

	OP_ASSERT(timeout <= WEBSTORAGE_DELAYED_FLUSH_TIMEOUT);

	RETURN_IF_ERROR(InitMessageListeners());

	OP_STATUS status = GetMessageHandler()->PostMessage(MSG_WEBSTORAGE_DELAYED_FLUSH,
			GetMessageQueueId(), m_executed_operation_count, timeout) ?
					OpStatus::OK : SIGNAL_OP_STATUS_ERROR(OpStatus::ERR_NO_MEMORY);

	if (OpStatus::IsSuccess(status))
		SetFlag(HAS_POSTED_DELAYED_FLUSH_MSG);

	return status;
}

void
WebStorageBackend_SimpleImpl::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_ASSERT(par1 == GetMessageQueueId());

	OP_STATUS status = OpStatus::OK;
	switch(msg)
	{
	case MSG_WEBSTORAGE_EXECUTE_OPERATION:
	{
		UnsetFlag(HAS_POSTED_EXECUTION_MSG);

		QuickWebStorageShutdownListener sh_not(this);

		status = ProcessOneOperation();

		if (!sh_not.m_was_deleted && !IsQueueEmpty() && status != OpStatus::ERR_YIELD)
			status = OpDbUtils::GetOpStatusError(PostExecutionMessage(), status);

		break;
	}
	case MSG_WEBSTORAGE_DELAYED_FLUSH:
	{
		UnsetFlag(HAS_POSTED_DELAYED_FLUSH_MSG);

		// A safe guard to prevent useless messages from being posted.
		OP_ASSERT(!GetFileNameObj() || !GetFileNameObj()->IsBogus());

		if (m_executed_operation_count == static_cast<unsigned>(par2))
		{
			// Nothing happened meanwhile
			status = SaveToDiskAsync();
		}
		else if (IsQueueEmpty() && HasModifications())
		{
			// More stuff was executed while message was waiting
			// but queue is again empty, so just reschedule.

			OP_ASSERT(g_op_time_info->GetWallClockMS() >= m_last_operation_time);

			int delta = WEBSTORAGE_DELAYED_FLUSH_TIMEOUT - static_cast<int>(g_op_time_info->GetWallClockMS() - m_last_operation_time);

			status = PostDelayedFlush(MAX(0, delta));
		}
		else
		{
			// Someone added more stuff to the queue. Ignore message because
			// it'll be rescheduled later
		}

		break;
	}
	default:
		OP_ASSERT(!"What are you sending to this class ? Unrecognized MESSAGE !");
	}

	ReportCondition(status);
}

OP_STATUS
WebStorageBackend_SimpleImpl::EnsureInitialization()
{
	OP_ASSERT(!IsInitialized());

	DB_DBG(("%p: WebStorageBackend_SimpleImpl::EnsureInitializatio()\n", this));

	OP_ASSERT(m_storage_area.GetCount() == 0);

	m_storage_area.SetMinimumCount(HASH_TABLE_MIN_SIZE);
	SetDataFileSize(0);

	if (HasDataFile() && !IsMemoryOnly())
	{
		BOOL file_exists;
		RETURN_IF_ERROR(MakeAbsFilePath());
		if (GetOpFile() &&
			OpStatus::IsSuccess(GetOpFile()->Exists(file_exists)) &&
			file_exists)
		{
			OP_ASSERT(m_data_loader == NULL);
			m_data_loader = OP_NEW(FileStorageLoader, (this));
			RETURN_OOM_IF_NULL(m_data_loader);
			OP_STATUS status = m_data_loader->Load(GetFileAbsPath());
			if (OpStatus::IsSuccess(status))
			{
				SetFlag(WAITING_FOR_DATA_LOAD);
				return OpStatus::ERR_YIELD;
			}

			OP_DELETE(m_data_loader);
			m_data_loader = NULL;

			if (status != OpStatus::ERR_FILE_NOT_FOUND)
				return status;
		}
	}

	if (!HasSetDefaultWidgetPrefs())
		// Empty storage file, it's either new, or perhaps data loss ?
		RestoreExtensionDefaultPreferences();

	SetFlag(OBJ_INITIALIZED);

	return OpStatus::OK;
}

#define BREAK_ERROR(expr) { status = (expr); OP_ASSERT(OpStatus::IsError(status)); break; }
#define BREAK_IF_ERROR(expr) if(OpStatus::IsError(status = (expr))) { break; }

OP_STATUS
WebStorageBackend_SimpleImpl::ProcessOneOperation()
{
	if (GetFlag(DELETE_DATA_LOADER))
	{
		OP_ASSERT(!GetFlag(WAITING_FOR_DATA_LOAD));
		OP_DELETE(m_data_loader);
		m_data_loader = NULL;
		UnsetFlag(DELETE_DATA_LOADER);
	}

	if (GetFlag(WAITING_FOR_DATA_LOAD) ||
		GetQuotaHandlingStatus() == PS_IndexEntry::QS_WAITING_FOR_USER)
	{
		// Shouldn't be called from deconstructor.
		OP_ASSERT(!IsBeingDeleted());
		// Need to wait until all data is loaded.
		return OpStatus::ERR_YIELD;
	}

	if (m_data_saver != NULL && m_data_saver->HasCompleted())
	{
		OP_DELETE(m_data_saver);
		m_data_saver = NULL;
	}

	WebStorageBackendOperation *op = GetCurrentOperation();
	if (op == NULL)
		return OpStatus::OK;

	OP_STATUS status = OpStatus::OK;
	if (op->RequiresInit() && !IsInitialized())
	{
		status = EnsureInitialization();
		if (status == OpStatus::ERR_YIELD)
			return status;
		if (OpStatus::IsError(status))
		{
			status = op->Terminate(status);
			OP_DELETE(op);
			return status;
		}
		// Queue might have been remixed by RestoreExtensionDefaultPreferences
		op = GetCurrentOperation();
		if (op == NULL)
			return OpStatus::OK;
	}

	DB_DBG(("%p: WebStorageBackend_SimpleImpl::ProcessOneOperation(): %p(%d)\n", this, op, op->m_type));

	op->m_result.m_type = static_cast<WebStorageOperation::WebStorageOperationEnum>(op->m_type);
	WebStorageValueInfo* pair;
	switch(op->m_type)
	{
	case WebStorageBackendOperation::FLUSH_DATA_TO_FILE:

		if (m_data_saver != NULL)
		{
			OP_ASSERT(!m_data_saver->HasCompleted());
			// Reentrant operation. The queue is being flushed while
			// it is also waiting for the file saver callback.
			// Or the message came from somewhere else, so we need wait further.
			if (op->IsSyncFlush())
			{
				m_data_saver->Flush();
				OP_DELETE(m_data_saver);
				m_data_saver = NULL;
				UnsetFlag(WAITING_FOR_DATA_WRITE);
				break;
			}
			else
				BREAK_ERROR(OpStatus::ERR_YIELD);
		}

		if (!HasModifications())
		{
			// Something flushed meanwhile or we have
			// a reentrant operation because of OnSavingFinished
			status = (op->m_result.IsError() ? op->m_result.m_error_data.m_error : OpStatus::OK);
			break;
		}
		if (m_storage_area_sorted.GetCount() == 0)
		{
			DestroyData();
			UnsetHasModifications();
			break;
		}

		BREAK_IF_ERROR(MakeAbsFilePath());
		BREAK_IF_ERROR(EnsureDataFileFolder());

		//many asserts to make sure we're not creating the :memory: file on disk !
		OP_ASSERT(!IsVolatile());
		OP_ASSERT(GetFileNameObj() != GetPSManager()->GetNonPersistentFileName());
		OP_ASSERT(GetFileAbsPath() && *GetFileAbsPath());
		OP_ASSERT(uni_strstr(GetFileAbsPath(), g_ps_memory_file_name) == NULL);

		DB_DBG(("%p: WebStorageBackend_SimpleImpl::ProcessOneOperation(): %p(%d), to file \"%S\"\n", this, op, op->m_type, GetFileAbsPath()));

		m_data_saver = OP_NEW(FileStorageSaver, (op->GetMessageHandler(), this, op->IsSyncFlush()));
		if (m_data_saver == NULL)
			BREAK_ERROR(OpStatus::ERR_NO_MEMORY);

		SetFlag(WAITING_FOR_DATA_WRITE);

		status = m_data_saver->Save(GetFileAbsPath(), &m_storage_area_sorted);
		if (OpStatus::IsError(status))
		{
			UnsetFlag(WAITING_FOR_DATA_WRITE);
			OP_DELETE(m_data_saver);
			m_data_saver = NULL;
			BREAK_ERROR(status);
		}

		//Save() internally already calls Flush()
		//flush means that callback has been called
		OP_ASSERT(!op->IsSyncFlush() || !GetFlag(WAITING_FOR_DATA_WRITE));

		if (!GetPSManager()->IsBeingDestroyed())
		{
			//new (?) file on disk.. time to flush
			//but not during shutdown, because the manager already handles that
			if (op->IsSyncFlush())
				status = GetPSManager()->FlushIndexToFile(GetUrlContextId());
			else
				status = GetPSManager()->FlushIndexToFileAsync(GetUrlContextId());
		}

		//calback was not called synchronously !
		if (GetFlag(WAITING_FOR_DATA_WRITE))
			BREAK_ERROR(OpStatus::ERR_YIELD);

		break;

	case WebStorageOperation::GET_ITEM_BY_KEY:

		op->m_result.m_data.m_storage.m_storage_mutated = FALSE;
		op->m_result.m_data.m_storage.m_value = NULL;

		if (m_storage_area.GetCount() != 0 &&
			OpStatus::IsSuccess(m_storage_area.GetData(op->m_key, &pair)))
		{
			op->m_result.m_data.m_storage.m_value = &pair->m_value;
		}
		break;

	case WebStorageOperation::GET_KEY_BY_INDEX:
	{
		WebStorageValueInfo* vi = m_storage_area_sorted.Get(op->m_index);
		op->m_result.m_data.m_storage.m_storage_mutated = FALSE;
		op->m_result.m_data.m_storage.m_value = vi != NULL ? &vi->m_key : NULL;

		break;
	}
	case WebStorageOperation::GET_ITEM_COUNT:
		op->m_result.m_data.m_item_count = m_storage_area.GetCount();
		break;

#ifdef WEBSTORAGE_RO_PAIRS_SUPPORT
	case WebStorageOperation::SET_ITEM_RO:
	case WebStorageOperation::SET_NEW_ITEM_RO:
#endif //WEBSTORAGE_RO_PAIRS_SUPPORT
	case WebStorageOperation::SET_ITEM:
	{
		op->m_result.m_data.m_storage.m_storage_mutated = FALSE;
		op->m_result.m_data.m_storage.m_value = NULL;

		if(GetQuotaHandlingStatus() == PS_IndexEntry::QS_USER_REPLIED)
		{
			if (IsOperaRunning())
			{
				BREAK_IF_ERROR(HandleQuotaReply(op));
				OP_ASSERT(GetQuotaHandlingStatus() == PS_IndexEntry::QS_DEFAULT);
			}
			else
			{
				//we get here during shutdown, due to synchronous flushing
				//but can't do anything because the dialog was cancelled
				//so the answer must be ignored
				BREAK_ERROR(PS_Status::ERR_QUOTA_EXCEEDED);
			}
		}

		WebStorageValueInfo* vi;
		if (OpStatus::IsSuccess(m_storage_area.GetData(op->m_key, &vi)))
		{
#ifdef WEBSTORAGE_RO_PAIRS_SUPPORT
			if (op->m_type == WebStorageOperation::SET_ITEM &&
				vi->m_is_readonly)
			{
				BREAK_ERROR(PS_Status::ERR_READ_ONLY);
			}
			if (op->m_type == WebStorageOperation::SET_NEW_ITEM_RO &&
				op->m_value != NULL)
			{
				// Setting a new value when a previous one existed.
				BREAK_ERROR(PS_Status::ERR_READ_ONLY);
			}
			if (op->m_type == WebStorageOperation::SET_ITEM_RO)
			{
				op->m_result.m_data.m_storage.m_storage_mutated = (vi->m_is_readonly != op->GetFlag(WebStorageBackendOperation::SET_RO_FLAG_TRUE));
				vi->m_is_readonly = op->GetFlag(WebStorageBackendOperation::SET_RO_FLAG_TRUE);
			}
#endif //WEBSTORAGE_RO_PAIRS_SUPPORT
			if (vi->m_value.Equals(op->m_value))
			{
				//Same value, just return it back and don't change anything
				op->m_result.SetValue();
				op->m_result.m_value_holder = *op->m_value;
				const_cast<WebStorageValue*>(op->m_value)->m_value = NULL;
			}
			else
			{
				//New value, change it or erase it
				BREAK_IF_ERROR(CheckQuotaOverflow(op->m_key, &vi->m_value, op->m_value));

				op->m_result.SetValue();
				op->m_result.m_value_holder = vi->m_value;

				if (op->m_value != NULL)
				{
					//change value
					vi->m_value = *op->m_value;
					const_cast<WebStorageValue*>(op->m_value)->m_value = NULL;
				}
				else
				{
					//erase value
					vi->m_value.m_value = NULL;
					m_storage_area.Remove(op->m_key, &vi);
					m_storage_area_sorted.RemoveByItem(vi);
					OP_DELETE(vi);

					if (m_storage_area.GetCount() == 0)
						//rounding and arithmetics might have made this value into non-zero
						SetDataFileSize(0);
				}
				op->m_result.m_data.m_storage.m_storage_mutated = TRUE;
			}
		}
		else
		{
			//insert new value
			if (op->m_value != NULL)
			{
				vi = OP_NEW(WebStorageValueInfo,());
				if (vi == NULL)
					BREAK_ERROR(OpStatus::ERR_NO_MEMORY);

				vi->m_key = *op->m_key;

				status = m_storage_area.Add(&vi->m_key, vi);
				if(OpStatus::IsError(status))
				{
					vi->m_key.m_value = NULL;
					OP_DELETE(vi);
					break;
				}
				status = m_storage_area_sorted.Add(vi);
				if(OpStatus::IsError(status))
				{
					WebStorageValueInfo* dummy;
					m_storage_area.Remove(&vi->m_key, &dummy);
					vi->m_key.m_value = NULL;
					OP_DELETE(vi);
					break;
				}

				status = CheckQuotaOverflow(op->m_key, NULL, op->m_value);
				if(OpStatus::IsError(status))
				{
					WebStorageValueInfo* dummy;
					m_storage_area.Remove(&vi->m_key, &dummy);
					m_storage_area_sorted.Remove(m_storage_area_sorted.GetCount()-1,1);
					vi->m_key.m_value = NULL;
					OP_DELETE(vi);
					break;
				}

				vi->m_value = *op->m_value;
#ifdef WEBSTORAGE_RO_PAIRS_SUPPORT
				vi->m_is_readonly = op->GetFlag(WebStorageBackendOperation::SET_RO_FLAG_TRUE);
#endif //WEBSTORAGE_RO_PAIRS_SUPPORT

				//buffers copied above - need to cleanup these pointers
				const_cast<WebStorageValue*>(op->m_key)->m_value = NULL;
				const_cast<WebStorageValue*>(op->m_value)->m_value = NULL;

				op->m_result.m_data.m_storage.m_storage_mutated = TRUE;
			}
		}

		if (op->m_value != NULL)
			// Inserted new value, unmark the object for deletion so the data is preserved.
			GetIndexEntry()->UnmarkDeletion();

		break;
	}
	case WebStorageOperation::CLEAR:
	{
		op->m_result.m_data.m_storage.m_value = NULL;

#ifndef WEBSTORAGE_RO_PAIRS_SUPPORT
		op->m_result.m_data.m_storage.m_storage_mutated = DestroyData();
#else
		op->m_result.m_data.m_storage.m_storage_mutated = FALSE;

		unsigned used_size = 0;

		unsigned length = (unsigned)m_storage_area_sorted.GetCount();
		for (unsigned k = 0; k < length; k++)
		{
			WebStorageValueInfo *vi = m_storage_area_sorted.Get(k);
			OP_ASSERT(vi);
			if (vi->m_is_readonly)
			{
				used_size += ConvertPairSize(vi);
			}
			else
			{
				WebStorageValueInfo *fetched;
				m_storage_area.Remove(&vi->m_key, &fetched);
				m_storage_area_sorted.Remove(k, 1);
				OP_DELETE(fetched);
				op->m_result.m_data.m_storage.m_storage_mutated = TRUE;
				k--;
				length--;
			}
		}
		SetDataFileSize(used_size);
#endif //WEBSTORAGE_RO_PAIRS_SUPPORT
		break;
	}
#ifdef WEBSTORAGE_RO_PAIRS_SUPPORT
	case WebStorageOperation::CLEAR_RO:
		op->m_result.m_data.m_storage.m_value = NULL;
		op->m_result.m_data.m_storage.m_storage_mutated = !!DestroyData();
		break;

#endif //WEBSTORAGE_RO_PAIRS_SUPPORT

	case WebStorageBackendOperation::ENUMERATE_ITEMS:
		status = EnumerateAllKeys_Internal(op->GetEnumerator());
		break;

	default:
		OP_ASSERT(!"Panic ! Invalid type");
	}

	OP_ASSERT(m_storage_area_sorted.GetCount() == (unsigned)m_storage_area.GetCount());

	if (status == PS_Status::ERR_QUOTA_EXCEEDED && !op->GetFlag(WebStorageBackendOperation::FAIL_IF_QUOTA_ERROR) && IsOperaRunning())
	do
	{
		unsigned handling = GetPolicyAttribute(PS_Policy::KOriginExceededHandling, op->GetWindow());

		OP_ASSERT(GetQuotaHandlingStatus() != PS_IndexEntry::QS_WAITING_FOR_USER);

		if (handling == PS_Policy::KQuotaAskUser)
		{
			if (op->GetWindow() != NULL && GetDomain() != NULL)
			{
				WindowCommander* wc = op->GetWindow()->GetWindowCommander();
				OpDocumentListener *listener = wc->GetDocumentListener();

				SetQuotaHandlingStatus(PS_IndexEntry::QS_WAITING_FOR_USER);

				OpFileLength current_size;
				BREAK_IF_ERROR(GetDataFileSize(&current_size));

				if (m_quota_callback == NULL)
				{
					m_quota_callback = OP_NEW(WebStorageBackend_SimpleImpl_QuotaCallback,(this));
					if (m_quota_callback == NULL)
						BREAK_ERROR(OpStatus::ERR_NO_MEMORY);
				}
				listener->OnIncreaseQuota(wc, NULL,
						GetDomain(), GetTypeDesc(),
						GetPolicyAttribute(PS_Policy::KOriginQuota), 0, m_quota_callback);

				//check again in case the listener replied synchronously
				if (GetQuotaHandlingStatus() == PS_IndexEntry::QS_WAITING_FOR_USER)
				{
					BREAK_IF_ERROR(OpStatus::ERR_YIELD);
				}
				else if (GetQuotaHandlingStatus() == PS_IndexEntry::QS_USER_REPLIED)
				{
					// Reenter in the function, jump back up.
					return ProcessOneOperation();
				}
			}
			else
			{
				/**
				 * Some things happened:
				 *  - there is no callback, so don't do fancy quota handling
				 *  - there is no Window. That can be bad API usage, but can
				 *    happen too if a storage operation is is queued to be
				 *    executed and the Window is destroyed meanwhile.
				 *  - there's no domain, which means preferences can't be saved and
				 *    hence the operation should fail.
				 */
			}
		}
		else if (handling == PS_Policy::KQuotaAllow)
		{
			OP_ASSERT(!"This should not have happened!!!");
		}
	} while(0);

	if (status == OpStatus::ERR_YIELD)
		return status;

	SetQuotaHandlingStatus(PS_IndexEntry::QS_DEFAULT);

	OP_ASSERT(!GetFlag(WAITING_FOR_DATA_WRITE));

	status = op->Terminate(status);
	OP_DELETE(op);
	return status;
}

WebStorageBackendOperation*
WebStorageBackend_SimpleImpl::GetCurrentOperation() const
{
	return m_pending_operations.First();
}

BOOL
WebStorageBackend_SimpleImpl::IsQueueEmpty() const
{
	return m_pending_operations.Empty();
}

// From the OpDocumentListener::QuotaCallback API
void
WebStorageBackend_SimpleImpl::OnQuotaReply(BOOL allow_increase, OpFileLength new_quota_size)
{
	OP_ASSERT(GetQuotaHandlingStatus() == PS_IndexEntry::QS_WAITING_FOR_USER);

	SetQuotaHandlingStatus(PS_IndexEntry::QS_USER_REPLIED);
	m_reply_cancelled = FALSE;
	m_reply_allow_increase = allow_increase;
	m_reply_new_quota_size = new_quota_size;

	ReportCondition(PostExecutionMessage());
}

void
WebStorageBackend_SimpleImpl::OnQuotaCancel()
{
	OP_ASSERT(GetQuotaHandlingStatus() == PS_IndexEntry::QS_WAITING_FOR_USER);

	SetQuotaHandlingStatus(PS_IndexEntry::QS_USER_REPLIED);
	m_reply_cancelled = TRUE;

	ReportCondition(PostExecutionMessage());
}

OP_STATUS
WebStorageBackend_SimpleImpl::HandleQuotaReply(WebStorageBackendOperation* op)
{
	OP_ASSERT(op);

	OpFileLength current_size;
	RETURN_IF_ERROR(GetDataFileSize(&current_size));

	OP_ASSERT(GetQuotaHandlingStatus() == PS_IndexEntry::QS_USER_REPLIED);
	SetQuotaHandlingStatus(PS_IndexEntry::QS_DEFAULT);

	DB_DBG(("%p: WebStorageBackend_SimpleImpl::HandleQuotaReply   (): %d, %d, %u\n", this, m_reply_cancelled, m_reply_allow_increase, (unsigned)m_reply_new_quota_size))

	if (m_reply_cancelled)
	{
		op->SetFlag(WebStorageBackendOperation::FAIL_IF_QUOTA_ERROR);
	}
	else if (m_reply_allow_increase && (m_reply_new_quota_size > current_size || m_reply_new_quota_size == 0))
	{
		if (m_reply_new_quota_size == 0) // don't ask again, always increase
		{
			RETURN_IF_ERROR(SetPolicyAttribute(PS_Policy::KOriginExceededHandling, PS_Policy::KQuotaAllow, op->GetWindow()));
		}
		else
		{
			RETURN_IF_ERROR(SetPolicyAttribute(PS_Policy::KOriginQuota, m_reply_new_quota_size, op->GetWindow()));

			if( m_reply_new_quota_size > GetPolicyAttribute(PS_Policy::KGlobalQuota, op->GetWindow()))
				RETURN_IF_ERROR(SetPolicyAttribute(PS_Policy::KGlobalQuota, m_reply_new_quota_size, op->GetWindow()));
		}
	}
	else if (!m_reply_allow_increase)
	{
		RETURN_IF_ERROR(SetPolicyAttribute(PS_Policy::KOriginExceededHandling, PS_Policy::KQuotaDeny, op->GetWindow()));

		if (m_reply_new_quota_size != 0)
		{
			RETURN_IF_ERROR(SetPolicyAttribute(PS_Policy::KOriginQuota, m_reply_new_quota_size, op->GetWindow()));

			if( m_reply_new_quota_size > GetPolicyAttribute(PS_Policy::KGlobalQuota, op->GetWindow()))
				RETURN_IF_ERROR(SetPolicyAttribute(PS_Policy::KGlobalQuota, m_reply_new_quota_size, op->GetWindow()));
		}
	}

	return OpStatus::OK;
}

BOOL
WebStorageBackend_SimpleImpl::DestroyData()
{
	BOOL did_something = FALSE;
	if (m_storage_area_sorted.GetCount() != 0)
	{
		m_storage_area.DeleteAll();
		m_storage_area_sorted.Clear();
		did_something = TRUE;
		SetHasModifications();
	}
	OP_ASSERT(m_storage_area_sorted.GetCount() == 0);
	OP_ASSERT(!GetFlag(WAITING_FOR_DATA_WRITE));
	SetDataFileSize(0);
	SetFlag(OBJ_INITIALIZED);
	UnsetFlag(OBJ_INITIALIZED_ERROR);

	if (GetIndexEntry()->DeleteDataFile())
		did_something = TRUE;

	m_init_error = OpStatus::OK;
	m_errors_delayed = 0;

	DB_DBG(("%p: WebStorageBackend_SimpleImpl::DestroyData        (): %d\n", this, did_something))

	return did_something;
}

OP_STATUS
WebStorageBackend_SimpleImpl::CheckQuotaOverflow(const WebStorageValue* key,
		const WebStorageValue* old_value, const WebStorageValue* new_value)
{
	OpFileLength data_size;
	RETURN_IF_ERROR(GetDataFileSize(&data_size));

	OpFileLength available_size;
	RETURN_IF_ERROR(CalculateAvailableDataSize(&available_size));

	OP_ASSERT(key != NULL);
	OP_ASSERT(old_value != NULL || new_value != NULL);

	if (old_value == NULL && new_value != NULL)
	{
		DB_DBG(("%p: WebStorageBackend_SimpleImpl::CheckQuotaOverflowA(): %u -> %u in %u\n", this,
				(unsigned)data_size, (unsigned)data_size + ConvertValueSize(key) + ConvertValueSize(new_value),
				(unsigned)available_size))

		//Adding a value
		data_size += ConvertValueSize(key) + ConvertValueSize(new_value);

		if (data_size > available_size && available_size != FILE_LENGTH_NONE)
			return PS_Status::ERR_QUOTA_EXCEEDED;
	}
	else if (old_value != NULL && new_value != NULL)
	{
		DB_DBG(("%p: WebStorageBackend_SimpleImpl::CheckQuotaOverflowR(): %u -> %u in %u\n", this,
				(unsigned)data_size, (unsigned)data_size - ConvertValueSize(old_value) + ConvertValueSize(new_value),
				(unsigned)available_size))

		//Changing a value
		data_size -= ConvertValueSize(old_value);
		data_size += ConvertValueSize(new_value);

		if (data_size > available_size && available_size != FILE_LENGTH_NONE)
			return PS_Status::ERR_QUOTA_EXCEEDED;
	}
	else //if (old_value != NULL && new_value == NULL)
	{
		DB_DBG(("%p: WebStorageBackend_SimpleImpl::CheckQuotaOverflowD(): %u -> %u in %u\n", this,
				(unsigned)data_size, (unsigned)data_size - ConvertValueSize(old_value) - ConvertValueSize(key),
				(unsigned)available_size))

		OP_ASSERT(data_size >= ConvertValueSize(old_value) + ConvertValueSize(key));
		//Removing - quota never overflows :)
		data_size -= ConvertValueSize(old_value) + ConvertValueSize(key);
	}

	SetDataFileSize(data_size);

	return OpStatus::OK;
}

OP_STATUS
WebStorageBackend_SimpleImpl::CalculateAvailableDataSize(OpFileLength* available_size)
{
	OP_ASSERT(available_size != NULL);

	OpFileLength global_used_size = 0;
	OpFileLength global_policy_size = 0;

	OpFileLength origin_used_size = 0;
	OpFileLength origin_policy_size = 0;

	PS_IndexEntry* key = GetIndexEntry();

	OP_ASSERT(GetCurrentOperation()); // This function is only called from the quota handling code
	Window* win = GetCurrentOperation()->GetWindow();

	if (PS_Policy::KQuotaAllow == key->GetPolicyAttribute(PS_Policy::KOriginExceededHandling, win))
	{
		*available_size = FILE_LENGTH_NONE;
		return OpStatus::OK;
	}

	origin_policy_size = key->GetPolicyAttribute(PS_Policy::KOriginQuota, win);
	if (origin_policy_size == 0 || origin_policy_size == FILE_LENGTH_NONE)
	{
		*available_size = origin_policy_size;
		return OpStatus::OK;
	}

	global_policy_size = key->GetPolicyAttribute(PS_Policy::KGlobalQuota, win);
	if (global_policy_size == 0)
	{
		*available_size = global_policy_size;
		return OpStatus::OK;
	}

	RETURN_IF_ERROR(key->GetPSManager()->GetGlobalDataSize(&global_used_size, key->GetType(), key->GetUrlContextId(), NULL));
	RETURN_IF_ERROR(GetDataFileSize(&origin_used_size));

	if (global_policy_size == FILE_LENGTH_NONE || global_policy_size < origin_policy_size)
	{
		//if global allowed size is invalid, default to current origin quota
		global_policy_size = origin_policy_size;
		global_used_size = origin_used_size;
	}

#ifdef DEBUG_ENABLE_OPASSERT
	//helper code section to make sure the quota values are being properly stored and calculated (kind of)
	OpFileLength origin_used_size_global = 0;
	RETURN_IF_ERROR(key->GetPSManager()->GetGlobalDataSize(&origin_used_size_global, key->GetType(), key->GetUrlContextId(), key->GetOrigin()));
	if (origin_used_size_global != origin_used_size)
	{
		DB_DBG(("%p: WebStorageBackend_SimpleImpl::CalculateAvailableD(): %u != %u\n", this, (unsigned)origin_used_size_global, (unsigned)origin_used_size))
	}
	if (WebStorageBackend::IsPersistent())
	{
		//persistent adds up on global quota
		OP_ASSERT(origin_used_size_global == origin_used_size);
		OP_ASSERT(global_used_size >= origin_used_size);
	}
	else
	{
		//non-persistent does not add up on global quota
		OP_ASSERT(origin_used_size_global == 0);
	}
#endif // DEBUG_ENABLE_OPASSERT

	//1st get size used by all the other databases, excluding the size of this one
	if (global_used_size >= origin_used_size)
		global_used_size = global_used_size - origin_used_size;
	else
		global_used_size = 0;
	//global_used_size holds size used globally *without* this origin

	//2nd calculate available global size
	if (global_policy_size > global_used_size)
		*available_size  = global_policy_size - global_used_size;
	else
		*available_size  = 0;
	//available_size holds size available globally

	//origin allowed size < available global size
	if (origin_policy_size < *available_size )
		*available_size = origin_policy_size;

	DB_DBG(("%p: WebStorageBackend_SimpleImpl::CalculateAvailableD(): %u (%u,%u)\n", this,
			(unsigned)*available_size, (unsigned)global_used_size, (unsigned)origin_used_size))

	return OpStatus::OK;
}

#define KILL_PTR(p) do{OP_ASSERT(p);OP_DELETE(p);p=NULL;}while(0)

void
WebStorageBackend_SimpleImpl::OnLoadingFinished()
{
	OP_ASSERT(GetFlag(WAITING_FOR_DATA_LOAD));
	UnsetFlag(WAITING_FOR_DATA_LOAD);
	SetFlag(OBJ_INITIALIZED | DELETE_DATA_LOADER);

	// If the file is empty, it should have never been saved
	// but instead deleted. If there is data corruption, OnLoadingFailed
	// is called instead.
	OP_ASSERT(!IsEmpty());

	ReportCondition(PostExecutionMessage());
}

void
WebStorageBackend_SimpleImpl::OnLoadingFailed(OP_STATUS status)
{
	OP_ASSERT(OpStatus::IsError(status));
	UnsetFlag(WAITING_FOR_DATA_LOAD);
	SetFlag(OBJ_INITIALIZED | DELETE_DATA_LOADER);

	//this function can get two errors: OOM and corrupted file.
	//corrupted file is unrecoverable and parsing should be retried
	//only if the file changes.
	//OOM means we can try to delay everything by some time and
	//retry a second time and do a hard fail (notify listener) only then
	if (status == PS_Status::ERR_CORRUPTED_FILE ||
		m_errors_delayed == MAX_ACCEPTABLE_ERRORS)
	{
		//no need to tell later that we had OOM so not to cause unnecessary panic
		if (!OpStatus::IsMemoryError(status))
			m_init_error = status;

		RestoreExtensionDefaultPreferences();

		if (!IsQueueEmpty())
			status = OpDbUtils::GetOpStatusError(PostExecutionMessage(), status);
	}
	else
	{
		m_errors_delayed++;
		if (!IsQueueEmpty())
			status = OpDbUtils::GetOpStatusError(PostExecutionMessage(10000), status);
	}

	ReportCondition(status);
}

void
WebStorageBackend_SimpleImpl::RestoreExtensionDefaultPreferences()
{
#if defined EXTENSION_SUPPORT && defined WEBSTORAGE_WIDGET_PREFS_SUPPORT
	if (GetPSManager()->IsContextExtension(GetUrlContextId()) && PS_Object::GetType() == KWidgetPreferences)
	{
		// Need to refill the storage area with the default values.
		// So, first backup any pending operations, second store the default
		// values, third restore the pending operations so they act normally.
		List<WebStorageBackendOperation> ops_backup;
		ops_backup.Append(&m_pending_operations);
		g_gadget_manager->OnExtensionStorageDataClear(GetUrlContextId(), OpGadget::PREFS_APPLY_DATA_LOST);
		m_pending_operations.Append(&ops_backup);
	}
#endif // defined EXTENSION_SUPPORT && defined WEBSTORAGE_WIDGET_PREFS_SUPPORT
}

void
WebStorageBackend_SimpleImpl::OnSavingFinished()
{
	OnSavingFailed(OpStatus::OK);
}

void
WebStorageBackend_SimpleImpl::OnSavingFailed(OP_STATUS status)
{
	UnsetFlag(WAITING_FOR_DATA_WRITE);
	UnsetHasModifications();
	if (m_data_saver)
		m_data_saver->Clean();

	WebStorageBackendOperation *op = GetCurrentOperation();

	OP_ASSERT(op && op->m_type == WebStorageBackendOperation::FLUSH_DATA_TO_FILE);

	if (op && op->m_type == WebStorageBackendOperation::FLUSH_DATA_TO_FILE)
	{
		if (OpStatus::IsError(status))
		{
			op->m_result.m_error_data.m_error = status;
			op->m_result.m_error_data.m_error_message = NULL;
		}
		if (op->IsSyncFlush())
		{
			ReportCondition(status);
			return;
		}
	}
	ReportCondition(OpDbUtils::GetOpStatusError(PostExecutionMessage(), status));
}

//used when loading from file
OP_STATUS
WebStorageBackend_SimpleImpl::OnValueFound(WebStorageValueInfo* vi)
{
	OpFileLength data_size;
	OP_STATUS status;

	//GetDataFileSize is unlikely to ever fail
	if (OpStatus::IsError(status = GetDataFileSize(&data_size)) ||
		OpStatus::IsError(status = m_storage_area.Add(&vi->m_key, vi)))
	{
		OP_DELETE(vi);
		return status;
	}

	OP_ASSERT(!GetFlag(WAITING_FOR_DATA_WRITE));
	if (OpStatus::IsError(status = m_storage_area_sorted.Add(vi)))
	{
		WebStorageValueInfo *dummy;
		m_storage_area.Remove(&vi->m_key, &dummy);
		OP_DELETE(vi);
		return status;
	}

	data_size += ConvertPairSize(vi);

	SetDataFileSize(data_size);

	return OpStatus::OK;
}

OP_STATUS
WebStorageBackend_SimpleImpl::SaveToDiskAsync()
{
# ifdef SELFTEST
	if (GetPSManager()->m_self_test_instance)
		return OpStatus::OK;
# endif

	if (!HasModifications() && IsQueueEmpty())
		//nothing to do
		return OpStatus::OK;

	if (m_pending_operations.Last() != NULL &&
		m_pending_operations.Last()->m_type == WebStorageBackendOperation::FLUSH_DATA_TO_FILE)
	{
		return OpStatus::OK;
	}

	WebStorageBackendOperation *op = OP_NEW(WebStorageBackendOperation,(this,
			WebStorageBackendOperation::FLUSH_DATA_TO_FILE, (WebStorageOperationCallback*)NULL));
	RETURN_OOM_IF_NULL(op);

	return ScheduleOperation(op);
}

OP_STATUS
WebStorageBackend_SimpleImpl::SaveToDiskSync()
{
	OP_ASSERT(IsBeingDeleted()); // No point in calling this from somewhere else.

	OP_ASSERT(GetQuotaHandlingStatus() != PS_IndexEntry::QS_WAITING_FOR_USER); // Handled elsewhere.

	if (!IsInitialized() || GetFlag(WAITING_FOR_DATA_LOAD))
		return OpStatus::OK; // Nothing to save.

# ifdef SELFTEST
	if (GetPSManager()->m_self_test_instance)
		return OpStatus::OK;
# endif

	if (!HasModifications() && IsQueueEmpty())
		//nothing to do
		return OpStatus::OK;

	if (IsQueueEmpty() && m_data_saver != NULL) {
		// Something was saving already. Enjoy it.
		m_data_saver->Flush();
		return OpStatus::OK;
	}

	WebStorageBackendOperation *op = NULL;

	if (IsQueueEmpty() ||
		m_pending_operations.Last()->m_type != WebStorageBackendOperation::FLUSH_DATA_TO_FILE)
	{
		op = OP_NEW(WebStorageBackendOperation,(this,
				WebStorageBackendOperation::FLUSH_DATA_TO_FILE, (WebStorageOperationCallback*)NULL));
		RETURN_OOM_IF_NULL(op);
		op->SetIsSyncFlush(TRUE);

		op->Into(&m_pending_operations);
	}

	while (!IsQueueEmpty())
	{
		BOOL is_flush;
		/*
		 * Handling file IO operations:
		 * - either content is loading, but the 1st check in this
		 *   function checks that that is not the case
		 * - or content is saving: pick up op and set it to sync
		 *
		 * If last op is a save to disk, use it, else don't waste
		 * and move on to the next one.
		 *
		 * Unlike in PostDelayedFlush, we don't check if the file
		 * is bogus, because this is one last shot to try and flush
		 * it. We might get lucky this time.
		 */
		if (GetCurrentOperation()->m_type == WebStorageBackendOperation::FLUSH_DATA_TO_FILE)
		{
			is_flush = TRUE;
			if (GetCurrentOperation()->Suc() != NULL && !GetFlag(WAITING_FOR_DATA_WRITE))
			{
				// Callbacks are unused in FLUSH_DATA_TO_FILE operations.
				// We'll delete the current flush operation because we have
				// one last in the end of the queue, which we just added above,
				// while we flush all the other operations.
				OP_ASSERT(GetCurrentOperation()->GetCallback() == NULL);
				OP_DELETE(GetCurrentOperation());
				continue;
			}
			else
			{
				GetCurrentOperation()->SetIsSyncFlush(TRUE);
			}
		}
		else
			is_flush = FALSE;

		OP_STATUS status = ProcessOneOperation();

		if (is_flush && status == OpStatus::ERR_NO_ACCESS)
			// Whoops... no point continuing here. File is not accessible.
			break;
		OP_ASSERT(status != OpStatus::ERR_YIELD); // All cases that require yielding are handled elsewhere.
		RETURN_IF_MEMORY_ERROR(status);
	}

	return OpStatus::OK;
}

void
WebStorageBackend_SimpleImpl::OnOperationCompleted()
{
	if (GetFlag(BEING_DELETED))
		return;

	m_last_operation_time = g_op_time_info->GetWallClockMS();
	m_executed_operation_count++;

	if (IsQueueEmpty())
	{
		ReportCondition(PostDelayedFlush());
		SafeDelete();
	}
}

bool
WebStorageBackend_SimpleImpl::SafeDelete()
{
	if (!HasPendingActions() &&
		GetIndexEntry()->GetRefCount() == 0 &&
		!IsBeingDeleted() &&
		!IsIndexBeingDeleted())
	{
		OP_DELETE(this);
		return true;
	}
	return false;
}

unsigned
WebStorageBackend_SimpleImpl::ConvertValueSize(const WebStorageValue* v) const
{
	if (v == NULL)
		return 0;
	if (!IsVolatile())
		//<k></k> -> 7
		return ((UNICODE_SIZE(v->m_value_length)+2)/3)*4 + 7;
	else
		//memory quota
		return sizeof(WebStorageValue) + UNICODE_SIZE(v->m_value_length);
}

unsigned
WebStorageBackend_SimpleImpl::ConvertPairSize(const WebStorageValueInfo* v) const
{
	if (v == NULL)
		return 0;
	if (!IsVolatile())
		//<e><k></k><v></v></e> -> 7 * 3
		return ConvertValueSize(&v->m_key) +
				ConvertValueSize(&v->m_value) + 7;
	else
		//memory quota
		return ConvertValueSize(&v->m_key) +
				ConvertValueSize(&v->m_value);
}

WebStorageBackendOperation::WebStorageBackendOperation(WebStorageBackend_SimpleImpl* impl, unsigned type, WebStorageOperationCallback* callback)
	: m_impl(impl)
	, m_key(NULL)
	, m_value(NULL)
	, m_callback(callback)
	, m_type(type)
{
	OP_ASSERT(impl != NULL);
}

WebStorageBackendOperation::WebStorageBackendOperation(WebStorageBackend_SimpleImpl* impl, unsigned type, WebStorageKeyEnumerator* enumerator)
	: m_impl(impl)
	, m_key(NULL)
	, m_value(NULL)
	, m_enumerator(enumerator)
	, m_type(type)
{
	OP_ASSERT(impl != NULL);
}

WebStorageBackendOperation::~WebStorageBackendOperation()
{
#ifdef _DEBUG
	m_has_called_callback = TRUE;
#endif // _DEBUG
	Out();
	DiscardCallback();
	if (m_impl)
		m_impl->OnOperationCompleted();
	m_impl = NULL;
	OP_DELETE(m_key);
	m_key = NULL;
	OP_DELETE(m_value);
	m_value = NULL;
}

BOOL
WebStorageBackendOperation::RequiresInit() const
{
	return !IS_OP_CLEAR_ALL(this) && m_type != FLUSH_DATA_TO_FILE;
}

void
WebStorageBackendOperation::DiscardCallback()
{
#ifdef _DEBUG
	OP_ASSERT(m_has_called_callback);
#endif // _DEBUG
	if (IsEnumerate())
	{
		if (m_enumerator != NULL)
		{
			WebStorageKeyEnumerator* en = m_enumerator;
			m_enumerator = NULL;
			en->Discard();
		}
	}
	else
	{
		if (m_callback != NULL)
		{
			WebStorageOperationCallback* cb = m_callback;
			m_callback = NULL;
			cb->Discard();
		}
	}
}

void
WebStorageBackendOperation::Drop()
{
	OP_ASSERT(!InList()); //Make sure it's not in the queue

	m_enumerator = NULL;
	m_callback = NULL;
	m_impl = NULL;

	OP_DELETE(this);
}


OP_STATUS
WebStorageBackendOperation::Terminate(OP_STATUS status)
{
	if (OpStatus::IsSuccess(status) && OpStatus::IsError(m_impl->m_init_error))
	{
		if (GetCallback() != NULL || GetEnumerator() != NULL)
		{
			// Propagate initialization error to callback.
			status = m_impl->m_init_error;
			m_impl->m_init_error = OpStatus::OK;
		}
	}
	return OpStatus::IsSuccess(status) ? InvokeCallback() : InvokeErrorCallback(status, NULL);
}

OP_STATUS
WebStorageBackendOperation::InvokeErrorCallback(OP_STATUS error, const uni_char* error_message)
{
	OP_ASSERT(OpStatus::IsError(error));
	if (IsEnumerate())
	{
		OP_DELETEA(const_cast<uni_char*>(error_message));
		OP_ASSERT(m_enumerator != NULL);

		WebStorageKeyEnumerator* en = m_enumerator;
		m_enumerator = NULL;

#ifdef _DEBUG
		m_has_called_callback = TRUE;
#endif // _DEBUG
		en->HandleError(error);
		en->Discard();

		return error;
	}
	else if (m_callback != NULL)
	{
		op_memcpy(&m_result.m_type, &m_type, sizeof(m_result.m_type));
		m_result.m_error_data.m_error = error;
		m_result.m_error_data.m_error_message = error_message;
		return InvokeCallback();
	}
	OP_DELETEA(const_cast<uni_char*>(error_message));
	return error;
}

OP_STATUS
WebStorageBackendOperation::InvokeCallback()
{
	if ((IS_OP_SET_ITEM(this) || IS_OP_CLEAR(this)) && m_result.m_data.m_storage.m_storage_mutated)
	{
		m_impl->InvokeMutationListeners();
		if (IS_OP_SET_ITEM(this))
			m_impl->SetHasModifications();
		else
			// This is a clear -> the DestroyData() function deletes the data file immediately.
			// and this clear deletes the data, so it is automatically synchronized.
			m_impl->UnsetHasModifications();
	}

#ifdef _DEBUG
	m_has_called_callback = TRUE;
#endif // _DEBUG

	if (IsEnumerate() || m_callback == NULL)
	{
		DiscardCallback();
		return OpStatus::OK;
	}

	WebStorageOperationCallback* cb = m_callback;
	m_callback = NULL;
	OP_STATUS status = cb->HandleOperation(&m_result);
	cb->Discard();
	return status;
}

#undef BREAK_ERROR
#undef BREAK_IF_ERROR

#endif //WEBSTORAGE_ENABLE_SIMPLE_BACKEND
