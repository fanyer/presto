/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#include "core/pch.h"

#ifdef CLIENTSIDE_STORAGE_SUPPORT

#include "modules/dom/src/storage/storage.h"

#include "modules/util/excepts.h"
#include "modules/util/simset.h"
#include "modules/database/opstorage.h"
#include "modules/database/sec_policy.h"
#include "modules/doc/frm_doc.h"
#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/storage/storageutils.h"
#include "modules/dom/src/domglobaldata.h"

#define DOM_WSEV_CTX_VAR_VAL(storage) ,OpStorageEventContext(storage->m_document_url, storage->GetEnvironment())

#ifdef WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
#include "modules/dom/src/userjs/userjsmanager.h"
#endif // WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT

DOM_Storage_OperationRestartObject::DOM_Storage_OperationRestartObject(DOM_Storage_OperationCallback* cb, DOM_Storage_KeyEnumerator *enu, DOM_Storage* storage)
	: ES_RestartObject(OPERA_MODULE_DOM, DOM_RESTART_STORAGE_OPERATION),
	  m_cb(cb),
	  m_enumerator(enu),
	  m_dom_storage(storage),
	  m_thread(NULL)
{
	if (m_cb)
		m_cb->m_restart_object = this;
	if (m_enumerator)
		m_enumerator->m_restart_object = this;
}

DOM_Storage_OperationRestartObject::~DOM_Storage_OperationRestartObject()
{
	if (m_cb)
		m_cb->Delete();
	OP_ASSERT(!m_cb);

	if (m_enumerator)
		m_enumerator->Delete();
}


DOM_Storage_OperationCallback::DOM_Storage_OperationCallback() :
	m_restart_object(NULL),
	m_state(BEFORE),
	m_must_succeed(FALSE)
{
}

DOM_Storage_OperationCallback::~DOM_Storage_OperationCallback()
{
	OP_ASSERT(m_state != WAITING);
	OP_ASSERT(!m_restart_object);
}

/* static */ DOM_Storage_OperationRestartObject *
DOM_Storage_OperationRestartObject::Make(DOM_Storage *storage, ES_PropertyEnumerator *enumerator, DOM_Runtime *runtime)
{
	DOM_Storage_OperationCallback *cb = NULL;
	DOM_Storage_KeyEnumerator *key_enumerator = NULL;
	if (!enumerator)
		cb = OP_NEW(DOM_Storage_OperationCallback, ());
	else
		key_enumerator = OP_NEW(DOM_Storage_KeyEnumerator, (enumerator));

	if (cb || enumerator)
	{
		DOM_Storage_OperationRestartObject *obj = OP_NEW(DOM_Storage_OperationRestartObject, (cb, key_enumerator, storage));
		if (!obj)
		{
			if (cb)
				cb->Delete();
			else
				key_enumerator->Delete();
		}

		return obj;
	}
	return NULL;
}

void
DOM_Storage_OperationCallback::Delete()
{
	if (m_restart_object)
		m_restart_object->m_cb = NULL;
	m_restart_object = NULL;

	if (m_state != WAITING)
		OP_DELETE(this);
}

void
DOM_Storage_OperationCallback::SetState(CallState state)
{
	switch(state)
	{
	case BEFORE:
		OP_ASSERT(m_state != WAITING);
		break;
	case WAITING:
		OP_ASSERT(m_state == BEFORE);
		break;
	case AFTER:
		OP_ASSERT(m_state == WAITING);
		break;
	}
	m_state = state;
}

void
DOM_Storage_OperationRestartObject::PrepareAndBlock(ES_Runtime *origining_runtime)
{
	if (m_cb)
		m_cb->SetState(DOM_Storage_OperationCallback::WAITING);
	else if (m_enumerator)
		m_enumerator->SetState(DOM_Storage_KeyEnumerator::WAITING);

	m_thread = DOM_Object::GetCurrentThread(origining_runtime);
	Push(m_thread, ES_BLOCK_UNSPECIFIED);
}

/* static */ DOM_Storage_OperationRestartObject *
DOM_Storage_OperationRestartObject::PopRestartObject(ES_Runtime *origining_runtime)
{
	ES_Thread *thread = DOM_Object::GetCurrentThread(origining_runtime);
	return static_cast<DOM_Storage_OperationRestartObject *>(ES_RestartObject::Pop(thread, OPERA_MODULE_DOM, DOM_RESTART_STORAGE_OPERATION));
}

void
DOM_Storage_OperationCallback::Reset()
{
	// This is dangerous ! Let the operation complete before changing state.
	OP_ASSERT(m_state != WAITING);

	SetState(BEFORE);
	m_must_succeed = FALSE;
}

void
DOM_Storage_OperationRestartObject::ResumeThread()
{
	// Can only unlock thread, after completing the storage operation.
	OP_ASSERT((m_cb && m_cb->m_state == DOM_Storage_OperationCallback::AFTER) || (m_enumerator && m_enumerator->m_state == DOM_Storage_KeyEnumerator::AFTER));

	if (m_thread)
	{
		m_thread->Unblock();
		m_thread = NULL;
	}
}

/* virtual */ BOOL
DOM_Storage_OperationRestartObject::ThreadCancelled(ES_Thread *thread)
{
	m_thread = NULL;
	return TRUE;
}

/* virtual */ OP_STATUS
DOM_Storage_OperationCallback::HandleOperation(const WebStorageOperation *result)
{
#ifdef OPERA_CONSOLE
	if (result->IsError() && m_restart_object)
		DOM_PSUtils::PostWebStorageErrorToConsole(
			m_restart_object->m_dom_storage->GetRuntime(),
			m_restart_object->m_thread != NULL ? m_restart_object->m_thread->GetInfoString() : NULL,
			m_restart_object->GetStorage(), result->m_error_data.m_error);
#endif //OPERA_CONSOLE

	OP_ASSERT(m_state == WAITING);
	SetState(AFTER);

	DB_DBG(("%p: DOM_Storage::HandleOper(%u)\n", m_restart_object->GetStorage(), result->m_type));

	if (m_restart_object == NULL)
		return OpStatus::OK;

	m_restart_object->ResumeThread();
	OP_STATUS status = result->CloneInto(&m_op);
	if (DOM_PSUtils::IsStorageStatusFileAccessError(m_op.m_error_data.m_error))
		m_op.ClearErrorInfo();
	return status;
}

/* virtual */ Window*
DOM_Storage_OperationCallback::GetWindow()
{
	if (m_restart_object)
		if (FramesDocument* doc = m_restart_object->m_dom_storage->GetFramesDocument())
			return doc->GetWindow();
	return NULL;
}

/* virtual */ void
DOM_Storage_OperationCallback::Discard()
{
	OP_ASSERT(m_state != BEFORE);
	if (m_state == WAITING)
	{
		// Core shutdown or runtime destroyed before operation completed
		SetState(AFTER);
		if (m_restart_object)
			m_restart_object->ResumeThread();
		Reset();
	}
	if (!m_restart_object)
		OP_DELETE(this);
}

OP_STATUS
DOM_Storage_OperationCallback::GetValue(ES_Value *value)
{
	OP_STATUS status = OpStatus::OK;

	OP_ASSERT(m_state != BEFORE);
	OP_ASSERT(m_state != WAITING);

	switch (m_op.m_type)
	{
	case WebStorageOperation::GET_ITEM_BY_KEY:
	case WebStorageOperation::GET_KEY_BY_INDEX:
		if (m_op.IsError())
			status = m_op.m_error_data.m_error;
		else if (m_op.m_data.m_storage.m_value != NULL)
		{
			if (value)
				DOM_Object::DOMSetStringWithLength(value, &(g_DOM_globalData->string_with_length_holder),
					   m_op.m_data.m_storage.m_value->m_value, m_op.m_data.m_storage.m_value->m_value_length);
		}
		else
			status = OpStatus::ERR;

		if (m_must_succeed && OpStatus::IsError(status))
		{
			DOM_Object::DOMSetNull(value);
			status = OpStatus::OK;
		}
		break;

	case WebStorageOperation::GET_ITEM_COUNT:
		DOM_Object::DOMSetNumber(value, m_op.IsError() ? 0 : m_op.m_data.m_item_count);
		break;

	case WebStorageOperation::SET_ITEM:
#ifdef WEBSTORAGE_RO_PAIRS_SUPPORT
	case WebStorageOperation::SET_ITEM_RO:
#endif // WEBSTORAGE_RO_PAIRS_SUPPORT
		if (m_op.IsError()) // Don't ignore the error, we want to throw for quota exceeded and read only.
			status = m_op.m_error_data.m_error;
		break;

	case WebStorageOperation::CLEAR:
#ifdef WEBSTORAGE_RO_PAIRS_SUPPORT
	case WebStorageOperation::CLEAR_RO:
#endif // WEBSTORAGE_RO_PAIRS_SUPPORT
	default:
		status = OpStatus::ERR;
		break;
	}

	Reset();

	return status;
}

/*virtual */ void
DOM_Storage_OperationRestartObject::GCTrace(ES_Runtime *runtime)
{
	runtime->GCMark(m_dom_storage);
}

DOM_Storage_KeyEnumerator::~DOM_Storage_KeyEnumerator()
{
}

void
DOM_Storage_KeyEnumerator::Delete()
{
	if (m_restart_object)
		m_restart_object->m_enumerator = NULL;

	m_restart_object = NULL;
	m_enumerator = NULL;
	if (m_state != WAITING)
		OP_DELETE(this);
}

void
DOM_Storage_KeyEnumerator::SetState(State state)
{
	switch(state)
	{
	case BEFORE:
		OP_ASSERT(m_state != WAITING);
		break;
	case WAITING:
		OP_ASSERT(m_state == BEFORE);
		break;
	case AFTER:
		OP_ASSERT(m_state == WAITING);
		break;
	}
	m_state = state;
}

/* virtual */ void
DOM_Storage_KeyEnumerator::HandleError(OP_STATUS status)
{
	/* Simply stop the enumeration. */
	Discard();
}

/* virtual */ void
DOM_Storage_KeyEnumerator::Discard()
{
	if (m_state == WAITING)
	{
		SetState(AFTER);
		if (m_restart_object)
			m_restart_object->ResumeThread();
	}
	if (!m_restart_object)
		OP_DELETE(this);
}

/* virtual */ OP_STATUS
DOM_Storage_KeyEnumerator::HandleKey(unsigned index, const WebStorageValue* key, const WebStorageValue* value)
{
	OP_ASSERT(!key->m_value && key->m_value_length == 0 || key->m_value[key->m_value_length] == 0);

	if (m_enumerator)
		RETURN_IF_LEAVE(m_enumerator->AddPropertyL(key->m_value));
	return OpStatus::OK;
}

DOM_Storage::DOM_Storage(DOM_Runtime::Prototype proto, WebStorageType type) :
	DOM_BuiltInConstructor(proto),
	m_storage(NULL),
	m_type(type),
	m_has_posted_err_msg(FALSE)
#if defined(WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT) || defined(EXTENSION_SUPPORT)
	, m_override_origin(NULL)
	, m_override_context_id(0)
#endif // WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT || EXTENSION_SUPPORT
{}

/*static*/
void DOM_Storage::FreeStorageResources(Head* h)
{
	OP_ASSERT(h);
	for (Link* o = h->First(); o != NULL; o = o->Suc())
		static_cast<DOM_Storage*>(o)->DropStorageObj();
}

#define CHECK_DOM_STORAGE_ACCESS(o, ret) do{if(!CanRuntimeAccessObject(o->GetStorageType(), origining_runtime, o->GetRuntime())) return ret; }while(0);

BOOL DOM_Storage::CanRuntimeAccessObject(WebStorageType type, ES_Runtime* origining_runtime, DOM_Runtime* object_runtime)
{
	switch(type)
	{
#ifdef WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
	case WEB_STORAGE_USERJS:
	{
# ifdef WEBSTORAGE_ENABLE_SIMPLE_BACKEND
		ES_Thread *thread = GetCurrentThread(origining_runtime);

		if (thread)
		{
			if (ES_Context *context = thread->GetContext())
				if (ES_Runtime::HasPrivilegeLevelAtLeast(context, ES_Runtime::PRIV_LVL_USERJS))
				{
					if (g_database_policies->GetPolicyAttribute(
							PS_ObjectTypes::KUserJsStorage,
							PS_Policy::KAccessToObject,
							object_runtime->GetOriginURL().GetContextId(),
							object_runtime->GetDomain(),
							object_runtime->GetFramesDocument() ? object_runtime->GetFramesDocument()->GetWindow() : NULL))
					return TRUE;
				}
		}
# endif // WEBSTORAGE_ENABLE_SIMPLE_BACKEND
		return FALSE;
	}
#endif // WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
#ifdef WEBSTORAGE_WIDGET_PREFS_SUPPORT
	case WEB_STORAGE_WGT_PREFS:
#endif // WEBSTORAGE_WIDGET_PREFS_SUPPORT
	case WEB_STORAGE_LOCAL:
	case WEB_STORAGE_SESSION:
		return TRUE;
	}
	OP_ASSERT(!"Missed a type");
	return FALSE;
}

OP_STATUS
DOM_Storage::EnsureStorageObj(ES_Runtime *origining_runtime)
{
	if (m_storage != NULL)
		return OpStatus::OK;

	OpVector<Window> vw;
	RETURN_IF_ERROR(GetRuntime()->GetWindows(vw));
	if (vw.GetCount() == 0)
	{
		RETURN_IF_ERROR(origining_runtime->GetWindows(vw));
		if (vw.GetCount() == 0)
			return OpStatus::ERR_NULL_POINTER;
	}
	Window *w = vw.Get(0);

	OpStorage *storage = NULL;

	OpStorageManager *storage_manager = w->DocManager()->GetStorageManager(TRUE);
	RETURN_OOM_IF_NULL(storage_manager);

	DOM_PSUtils::PS_OriginInfo oi;
#if defined(WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT) || defined(EXTENSION_SUPPORT)
	if (m_override_origin != NULL)
	{
		oi.m_origin        = m_override_origin;
		oi.m_context_id    = m_override_context_id;
		// Expose widget.preferences even for private tabs.
		oi.m_is_persistent = TRUE;
	}
	else
#endif // WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT || EXTENSION_SUPPORT
		RETURN_IF_ERROR(DOM_PSUtils::GetPersistentStorageOriginInfo(GetRuntime(), oi));

	RETURN_IF_ERROR(storage_manager->GetStorage(m_type, oi.m_context_id, oi.m_origin, oi.m_is_persistent, &storage));

	OP_ASSERT(storage != NULL);
	m_storage = storage;

	return OpStatus::OK;
}

void
DOM_Storage::DropStorageObj()
{
	m_storage = NULL;
}

/*static*/ OP_STATUS
DOM_Storage::Make(DOM_Storage *&dom_st, WebStorageType type, DOM_Runtime *runtime)
{
	DOM_Storage* new_dom_st = OP_NEW(DOM_Storage, (DOM_Runtime::STORAGE_PROTOTYPE, type));
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_dom_st, runtime, runtime->GetPrototype(DOM_Runtime::STORAGE_PROTOTYPE), "Storage"));
	new_dom_st->SetHasVolatilePropertySet();

	new_dom_st->GetEnvironment()->AddDOMStoragebject(new_dom_st);
	if (runtime->GetFramesDocument() != NULL)
		new_dom_st->m_document_url = runtime->GetFramesDocument()->GetURL();

	dom_st = new_dom_st;
	return OpStatus::OK;
}

#ifdef WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
/*static*/ OP_STATUS
DOM_Storage::MakeScriptStorage(DOM_Storage *&storage, const uni_char* origin, DOM_Runtime *runtime)
{
	if (origin == NULL)
		return OpStatus::ERR_NULL_POINTER;

	DOM_Storage* new_dom_st = OP_NEW(DOM_Storage, (DOM_Runtime::STORAGE_USERJS_PROTOTYPE, WEB_STORAGE_USERJS));
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_dom_st, runtime, runtime->GetPrototype(DOM_Runtime::STORAGE_USERJS_PROTOTYPE), "Storage"));
	new_dom_st->SetHasVolatilePropertySet();

	RETURN_IF_ERROR(new_dom_st->SetOverrideOrigin(origin, runtime->GetOriginURL().GetContextId()));
	new_dom_st->GetEnvironment()->AddDOMStoragebject(new_dom_st);
	storage = new_dom_st;

	return OpStatus::OK;
}
#endif // WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT

/* virtual */
BOOL
DOM_Storage::AllowOperationOnProperty(const uni_char *property_name, PropertyOperation op)
{
	// UserJS storage objects are super protected CORE-28768
	return GetStorageType() != WEB_STORAGE_USERJS;
}

#if defined(WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT) || defined(EXTENSION_SUPPORT)
OP_STATUS
DOM_Storage::SetOverrideOrigin(const uni_char *origin, URL_CONTEXT_ID context_id)
{
	OP_ASSERT(m_storage == NULL);

	uni_char *override = UniSetNewStr(origin); // Will set it to NULL if origin is NULL.
	if (origin && !override)
		return OpStatus::ERR_NO_MEMORY;

	OP_DELETEA(m_override_origin);
	m_override_origin = override;
	m_override_context_id = context_id;

	return OpStatus::OK;
}
#endif // WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT || EXTENSION_SUPPORT

DOM_Storage::~DOM_Storage()
{
	Out();

#if defined(WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT) || defined(EXTENSION_SUPPORT)
	OP_DELETEA(m_override_origin);
#endif // WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT || EXTENSION_SUPPORT
}

/* virtual */ ES_GetState
DOM_Storage::FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime)
{
	/**
	 * If the access check fails, it just returns OK so
	 * the enumeration silently fails. Else if this
	 * returns an error, the es engine will abort the
	 * current ES_Thread.
	 */
	CHECK_DOM_STORAGE_ACCESS(this, GET_SUCCESS);

	GET_FAILED_IF_ERROR(EnsureStorageObj(origining_runtime));
	DOM_Storage_OperationRestartObject *restart = DOM_Storage_OperationRestartObject::PopRestartObject(origining_runtime);

	/* origining_runtimes will have a current thread, but debugger uses
	   break the rules and pass in ones without (and if it has a thread,
	   it's unrelated to the enumeration operation here.) */
	if (!restart && GetCurrentThread(origining_runtime))
	{
		restart = DOM_Storage_OperationRestartObject::Make(this, enumerator, static_cast<DOM_Runtime*>(origining_runtime));
		if (!restart)
			return GET_NO_MEMORY;

		OpAutoPtr<DOM_Storage_OperationRestartObject> restart_anchor(restart);
		GET_FAILED_IF_ERROR(m_storage->EnumerateAllKeys(restart->GetEnumerator()));
		restart->PrepareAndBlock(origining_runtime);
		restart_anchor.release();
		return GET_SUSPEND;
	}
	else
	{
		OpStackAutoPtr<DOM_Storage_OperationRestartObject> restart_anchor(restart);
		return DOM_Object::FetchPropertiesL(enumerator, origining_runtime);
	}
}

/* virtual */ ES_GetState
DOM_Storage::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	CHECK_DOM_STORAGE_ACCESS(this, GET_SECURITY_VIOLATION);

	if (property_name == OP_ATOM_length)
	{
		DB_DBG(("%p: DOM_Storage::GetName   (OP_ATOM_length)\n", this))

		if (value)
		{
			GET_FAILED_IF_ERROR(EnsureStorageObj(origining_runtime));

			if (m_storage->HasCachedNumberOfItems())
			{
				DOMSetNumber(value, m_storage->GetCachedNumberOfItems());
				return GET_SUCCESS;
			}

			DOM_Storage_OperationRestartObject *restart = DOM_Storage_OperationRestartObject::Make(this, NULL, static_cast<DOM_Runtime*>(origining_runtime));
			if (!restart)
				return GET_NO_MEMORY;

			OpAutoPtr<DOM_Storage_OperationRestartObject> restart_anchor(restart);
			GET_FAILED_IF_ERROR(m_storage->GetNumberOfItems(restart->GetCallback()));

			restart->PrepareAndBlock(origining_runtime);
			restart_anchor.release();
			DOMSetNull(value);
			return GET_SUSPEND;
		}
		return GET_SUCCESS;
	}
	return GET_FAILED;
}

/* virtual */ ES_GetState
DOM_Storage::GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	CHECK_DOM_STORAGE_ACCESS(this, GET_SECURITY_VIOLATION);

	ES_GetState result = DOM_Object::GetName(property_name, property_code, value, origining_runtime);

	if (result != GET_FAILED)
		return result;

	GET_FAILED_IF_ERROR(EnsureStorageObj(origining_runtime));

	DB_DBG(("%p: DOM_Storage::GetName   (%S)\n", this, property_name))

	if (m_storage->HasCachedNumberOfItems() && m_storage->GetCachedNumberOfItems() == 0)
		return GET_FAILED;

	WebStorageValueTemp key(property_name, uni_strlen(property_name));

	DOM_Storage_OperationRestartObject *restart = DOM_Storage_OperationRestartObject::Make(this, NULL, static_cast<DOM_Runtime*>(origining_runtime));
	if (!restart)
		return GET_NO_MEMORY;

	OpAutoPtr<DOM_Storage_OperationRestartObject> restart_anchor(restart);
	DOM_Storage_OperationCallback *op_callback = restart->GetCallback();

	GET_FAILED_IF_ERROR(m_storage->GetItem(&key, op_callback));

#ifdef WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
	if (m_type == WEB_STORAGE_USERJS)
		op_callback->m_must_succeed = uni_strcmp(UNI_L("constructor"), property_name) == 0;
#endif

	restart->PrepareAndBlock(origining_runtime);
	restart_anchor.release();
	DOMSetNull(value);
	return GET_SUSPEND;
}

/* virtual */ ES_GetState
DOM_Storage::GetNameRestart(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object)
{
	DOM_Storage_OperationRestartObject *restart = DOM_Storage_OperationRestartObject::PopRestartObject(origining_runtime);
	OP_ASSERT(restart);
	DOM_Storage_OperationCallback *op_callback = restart->GetCallback();

	OP_ASSERT(op_callback->m_op.m_type == WebStorageOperation::GET_ITEM_BY_KEY ||
	          op_callback->m_op.m_type == WebStorageOperation::GET_ITEM_COUNT);

	OpAutoPtr<DOM_Storage_OperationRestartObject> restart_anchor(restart);
	GET_FAILED_IF_ERROR(op_callback->GetValue(value));

	restart_anchor.release();
	restart->Discard(DOM_Object::GetCurrentThread(origining_runtime));
	return GET_SUCCESS;
}

/* virtual */ ES_GetState
DOM_Storage::GetIndex(int property_index, ES_Value* value, ES_Runtime* origining_runtime)
{
	uni_char property_name[16]; /* ARRAY OK joaoe 2009-10-12 */
	uni_itoa(property_index, property_name, 10);
	return GetName(property_name, OP_ATOM_UNASSIGNED, value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_Storage::GetIndexRestart(int property_index, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object)
{
	return GetNameRestart(NULL, OP_ATOM_UNASSIGNED, value, origining_runtime, restart_object);
}

/* virtual */ ES_PutState
DOM_Storage::PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	CHECK_DOM_STORAGE_ACCESS(this, PUT_SECURITY_VIOLATION);

	/* Note: cannot delegate to DOM_Object::PutName() to handle named
	   properties with known atoms -- it uses GetName(..,NULL,..) for its
	   [[HasProperty]] check, which initiates an async Storage operation.

	   As luck would have it, the Storage object only supports OP_ATOM_length,
	   so we inline DOM_Object::PutName() + OP_ATOM_length handling instead.
	   The Storage security check above takes care of access control.  */

	if (static_cast<OpAtom>(property_code) == OP_ATOM_length)
		return PUT_READ_ONLY;

	if (value->type != VALUE_STRING)
		return PUT_NEEDS_STRING;

	PUT_FAILED_IF_ERROR(EnsureStorageObj(origining_runtime));

	WebStorageValueTemp key(property_name, uni_strlen(property_name));
	WebStorageValueTemp val(value->value.string, uni_strlen(value->value.string));

	DB_DBG(("%p: DOM_Storage::PutName   (%S)\n", this, property_name))

	DOM_Storage_OperationRestartObject *restart = DOM_Storage_OperationRestartObject::Make(this, NULL, static_cast<DOM_Runtime*>(origining_runtime));
	if (!restart)
		return PUT_NO_MEMORY;

	OpAutoPtr<DOM_Storage_OperationRestartObject> restart_anchor(restart);
	PUT_FAILED_IF_ERROR(m_storage->SetItem(&key, &val, restart->GetCallback() DOM_WSEV_CTX_VAR_VAL(this)));

	restart->PrepareAndBlock(origining_runtime);
	restart_anchor.release();
	DOMSetNull(value);
	return PUT_SUSPEND;
}

/* virtual */ ES_PutState
DOM_Storage::PutNameRestart(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object)
{
	DOM_Storage_OperationRestartObject *restart = DOM_Storage_OperationRestartObject::PopRestartObject(origining_runtime);
	DOM_Storage_OperationCallback *op_callback = restart->GetCallback();

	OP_ASSERT(op_callback->m_op.m_type == WebStorageOperation::SET_ITEM);

	OpAutoPtr<DOM_Storage_OperationRestartObject> restart_anchor(restart);
	OP_STATUS status = op_callback->GetValue(value);

	switch(OpStatus::GetIntValue(status))
	{
	case PS_Status::ERR_QUOTA_EXCEEDED:
		return ConvertCallToPutName(CallDOMException(QUOTA_EXCEEDED_ERR, value), value);
	case PS_Status::ERR_READ_ONLY:
		return ConvertCallToPutName(CallDOMException(NO_MODIFICATION_ALLOWED_ERR, value), value);
	}
	PUT_FAILED_IF_ERROR(status);
	return PUT_SUCCESS;
}

/* virtual */ ES_PutState
DOM_Storage::PutIndex(int property_index, ES_Value* value, ES_Runtime* origining_runtime)
{
	uni_char property_name[16]; /* ARRAY OK joaoe 2009-10-12 */
	uni_itoa(property_index, property_name, 10);
	return PutName(property_name, OP_ATOM_UNASSIGNED, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_Storage::PutIndexRestart(int property_index, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object)
{
	return PutNameRestart(NULL, OP_ATOM_UNASSIGNED, value, origining_runtime, restart_object);
}

/* virtual */ ES_DeleteStatus
DOM_Storage::DeleteName(const uni_char* property_name, ES_Runtime* origining_runtime)
{
	CHECK_DOM_STORAGE_ACCESS(this, DELETE_REJECT);

	DB_DBG(("%p: DOM_Storage::DeleteName(%S)\n", this, property_name))

	if (OpStatus::IsSuccess(EnsureStorageObj(origining_runtime)))
	{
		WebStorageValueTemp key(property_name, uni_strlen(property_name));
		DELETE_FAILED_IF_MEMORY_ERROR(m_storage->SetItem(&key, NULL, NULL DOM_WSEV_CTX_VAR_VAL(this)));
	}

	return DELETE_OK;
}

/* static */ int
DOM_Storage::key(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(storage, DOM_TYPE_STORAGE, DOM_Storage);

	CHECK_DOM_STORAGE_ACCESS(storage, ES_EXCEPT_SECURITY);

	DOM_Storage_OperationRestartObject *restart = NULL;

	if (argc >= 0)
	{
		DOM_CHECK_ARGUMENTS("n");
		unsigned index = TruncateDoubleToUInt(argv[0].value.number);

		DB_DBG(("%p: DOM_Storage::key       (%u)\n", storage, index))

		CALL_FAILED_IF_ERROR(storage->EnsureStorageObj(origining_runtime));

		if (storage->m_storage->HasCachedNumberOfItems() && storage->m_storage->GetCachedNumberOfItems() <= index)
			return ES_FAILED;

		restart = DOM_Storage_OperationRestartObject::Make(storage, NULL, origining_runtime);
		if (!restart)
			return ES_NO_MEMORY;

		OpAutoPtr<DOM_Storage_OperationRestartObject> restart_anchor(restart);
		CALL_FAILED_IF_ERROR(storage->m_storage->GetKeyAtIndex(index, restart->GetCallback()));

		restart->PrepareAndBlock(origining_runtime);
		restart_anchor.release();

		OP_ASSERT(restart->GetCallback()->m_state == DOM_Storage_OperationCallback::WAITING);
		return ES_SUSPEND | ES_RESTART;
	}
	else
	{
		DB_DBG(("%p: DOM_Storage::keyRestart()\n", storage))

		restart = DOM_Storage_OperationRestartObject::PopRestartObject(origining_runtime);
		DOM_Storage_OperationCallback *op_callback = restart->GetCallback();
		OP_ASSERT(op_callback->m_op.m_type == WebStorageOperation::GET_KEY_BY_INDEX);

		OpAutoPtr<DOM_Storage_OperationRestartObject> restart_anchor(restart);
		CALL_FAILED_IF_ERROR(op_callback->GetValue(return_value));
		restart_anchor.release();
		restart->Discard(DOM_Object::GetCurrentThread(origining_runtime));
		return ES_VALUE;
	}
}

/* static */ int
DOM_Storage::getItem(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(storage, DOM_TYPE_STORAGE, DOM_Storage);

	CHECK_DOM_STORAGE_ACCESS(storage, ES_EXCEPT_SECURITY);

	DOM_Storage_OperationRestartObject *restart = NULL;

	if (argc >= 0)
	{
		DOM_CHECK_ARGUMENTS("z");

		DB_DBG(("%p: DOM_Storage::getItem   (%S)\n", storage, argv[0].value.string_with_length->string))

		CALL_FAILED_IF_ERROR(storage->EnsureStorageObj(origining_runtime));

		if (storage->m_storage->HasCachedNumberOfItems() && storage->m_storage->GetCachedNumberOfItems() == 0)
		{
			DOMSetNull(return_value);
			return ES_VALUE;
		}

		restart = DOM_Storage_OperationRestartObject::Make(storage, NULL, origining_runtime);
		if (!restart)
			return ES_NO_MEMORY;

		OpAutoPtr<DOM_Storage_OperationRestartObject> restart_anchor(restart);
		WebStorageValueTemp key(argv[0].value.string_with_length->string, argv[0].value.string_with_length->length);

		CALL_FAILED_IF_ERROR(storage->m_storage->GetItem(&key, restart->GetCallback()));

		restart->PrepareAndBlock(origining_runtime);
		restart_anchor.release();

		OP_ASSERT(restart->GetCallback()->m_state == DOM_Storage_OperationCallback::WAITING);
		return ES_SUSPEND | ES_RESTART;
	}
	else
	{
		DB_DBG(("%p: DOM_Storage::getItemRes()\n", storage))

		restart = DOM_Storage_OperationRestartObject::PopRestartObject(origining_runtime);
		DOM_Storage_OperationCallback *op_callback = restart->GetCallback();
		OP_ASSERT(op_callback->m_op.m_type == WebStorageOperation::GET_ITEM_BY_KEY);

		OpAutoPtr<DOM_Storage_OperationRestartObject> restart_anchor(restart);

		OP_STATUS status = op_callback->GetValue(return_value);
		if (status == OpStatus::ERR)
			// special case, return null
			DOMSetNull(return_value);
		else
			CALL_FAILED_IF_ERROR(status);

		if (OpStatus::IsSuccess(status))
		{
			restart_anchor.release();
			restart->Discard(DOM_Object::GetCurrentThread(origining_runtime));
		}

		return ES_VALUE;
	}
}

/* static */ int
DOM_Storage::setItem(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(storage, DOM_TYPE_STORAGE, DOM_Storage);

	CHECK_DOM_STORAGE_ACCESS(storage, ES_EXCEPT_SECURITY);

	DOM_Storage_OperationRestartObject *restart = NULL;

	if (argc >= 0)
	{
		DOM_CHECK_ARGUMENTS("zz");

		DB_DBG(("%p: DOM_Storage::setItem   (%S, %S)\n", storage, argv[0].value.string_with_length->string, argv[1].value.string_with_length->string))

		restart = DOM_Storage_OperationRestartObject::Make(storage, NULL, origining_runtime);
		if (!restart)
			return ES_NO_MEMORY;

		OpAutoPtr<DOM_Storage_OperationRestartObject> restart_anchor(restart);
		CALL_FAILED_IF_ERROR(storage->EnsureStorageObj(origining_runtime));

		WebStorageValueTemp key(argv[0].value.string_with_length->string, argv[0].value.string_with_length->length);
		WebStorageValueTemp value(argv[1].value.string_with_length->string, argv[1].value.string_with_length->length);

		CALL_FAILED_IF_ERROR(storage->m_storage->SetItem(&key, &value, restart->GetCallback() DOM_WSEV_CTX_VAR_VAL(storage)));

		restart->PrepareAndBlock(origining_runtime);
		restart_anchor.release();

		OP_ASSERT(restart->GetCallback()->m_state == DOM_Storage_OperationCallback::WAITING);
		return ES_SUSPEND | ES_RESTART;
	}
	else
	{
		DB_DBG(("%p: DOM_Storage::setItemRes()\n", storage))

		restart = DOM_Storage_OperationRestartObject::PopRestartObject(origining_runtime);
		DOM_Storage_OperationCallback *op_callback = restart->GetCallback();
		OP_ASSERT(op_callback->m_op.m_type == WebStorageOperation::SET_ITEM);

		OpAutoPtr<DOM_Storage_OperationRestartObject> restart_anchor(restart);
		OP_STATUS status = op_callback->GetValue(NULL);

		switch(OpStatus::GetIntValue(status))
		{
		case PS_Status::ERR_QUOTA_EXCEEDED:
			return DOM_CALL_DOMEXCEPTION(QUOTA_EXCEEDED_ERR);
		case PS_Status::ERR_READ_ONLY:
			return DOM_CALL_DOMEXCEPTION(NO_MODIFICATION_ALLOWED_ERR);
		}

		return OpStatus::IsMemoryError(status) ? ES_NO_MEMORY : ES_FAILED;
	}
}

/* static */ int
DOM_Storage::removeItem(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(storage, DOM_TYPE_STORAGE, DOM_Storage);

	CHECK_DOM_STORAGE_ACCESS(storage, ES_EXCEPT_SECURITY);

	DOM_Storage_OperationRestartObject *restart = NULL;

	if (argc >= 0)
	{
		DOM_CHECK_ARGUMENTS("z");

		DB_DBG(("%p: DOM_Storage::remItem   (%s)\n", storage, argv[0].value.string_with_length->string))

		restart = DOM_Storage_OperationRestartObject::Make(storage, NULL, origining_runtime);
		if (!restart)
			return ES_NO_MEMORY;

		OpAutoPtr<DOM_Storage_OperationRestartObject> restart_anchor(restart);
		CALL_FAILED_IF_ERROR(storage->EnsureStorageObj(origining_runtime));

		WebStorageValueTemp key(argv[0].value.string_with_length->string, argv[0].value.string_with_length->length);

		CALL_FAILED_IF_ERROR(storage->m_storage->SetItem(
				&key, NULL, restart->GetCallback() DOM_WSEV_CTX_VAR_VAL(storage)));

		restart->PrepareAndBlock(origining_runtime);
		restart_anchor.release();

		OP_ASSERT(restart->GetCallback()->m_state == DOM_Storage_OperationCallback::WAITING);
		return ES_SUSPEND | ES_RESTART;
	}
	else
	{
		DB_DBG(("%p: DOM_Storage::remItemRes()\n", storage))

		restart = DOM_Storage_OperationRestartObject::PopRestartObject(origining_runtime);
		DOM_Storage_OperationCallback *op_callback = restart->GetCallback();
		OP_ASSERT(op_callback->m_op.m_type == WebStorageOperation::SET_ITEM);

		OpAutoPtr<DOM_Storage_OperationRestartObject> restart_anchor(restart);
		OP_STATUS status = op_callback->GetValue(NULL);

		if (status == PS_Status::ERR_READ_ONLY)
			return DOM_CALL_DOMEXCEPTION(NO_MODIFICATION_ALLOWED_ERR);

		return OpStatus::IsMemoryError(status) ? ES_NO_MEMORY : ES_FAILED;
	}
}

/* static */ int
DOM_Storage::clear(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(storage, DOM_TYPE_STORAGE, DOM_Storage);

	CHECK_DOM_STORAGE_ACCESS(storage, ES_EXCEPT_SECURITY);

	DB_DBG(("%p: DOM_Storage::clear     ()\n", storage))

	CALL_FAILED_IF_ERROR(storage->EnsureStorageObj(origining_runtime));

	CALL_FAILED_IF_ERROR(storage->m_storage->Clear(NULL DOM_WSEV_CTX_VAR_VAL(storage)));

	return ES_FAILED;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_Storage)
	DOM_FUNCTIONS_FUNCTION(DOM_Storage, DOM_Storage::key, "key", "n-")
	DOM_FUNCTIONS_FUNCTION(DOM_Storage, DOM_Storage::getItem, "getItem", "z-")
	DOM_FUNCTIONS_FUNCTION(DOM_Storage, DOM_Storage::setItem, "setItem", "zz-")
	DOM_FUNCTIONS_FUNCTION(DOM_Storage, DOM_Storage::removeItem, "removeItem", "z-")
	DOM_FUNCTIONS_FUNCTION(DOM_Storage, DOM_Storage::clear, "clear", "-")
DOM_FUNCTIONS_END(DOM_Storage)

#endif // CLIENTSIDE_STORAGE_SUPPORT
