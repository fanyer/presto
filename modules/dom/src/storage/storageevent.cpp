/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#include "core/pch.h"

#ifdef CLIENTSIDE_STORAGE_SUPPORT

#include "modules/dom/src/storage/storageevent.h"
#include "modules/dom/src/storage/storage.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/doc/frm_doc.h"


DOM_StorageEvent::~DOM_StorageEvent()
{
	if (m_value_changed_event != NULL)
		m_value_changed_event->Release();
}

/* static */ OP_STATUS
DOM_StorageEvent::Make(DOM_StorageEvent *&event, OpStorageValueChangedEvent *event_obj, DOM_Runtime *runtime)
{
	event = OP_NEW(DOM_StorageEvent, ());
	OP_STATUS status = DOMSetObjectRuntime(event, runtime,
			runtime->GetPrototype(DOM_Runtime::STORAGEEVENT_PROTOTYPE), "StorageEvent");

	if (OpStatus::IsError(status))
	{
		if (event_obj != NULL)
			event_obj->Release();
		return status;
	}

	event->m_value_changed_event = event_obj;

	if (!event_obj->m_context.m_url.IsEmpty())
		RETURN_IF_ERROR(event_obj->m_context.m_url.GetAttribute(URL::KUniName_With_Fragment, event->m_origining_url));

	event->InitEvent(STORAGE, runtime->GetEnvironment()->GetWindow(), NULL);

	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_StorageEvent::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_key:
		if (value && m_value_changed_event != NULL)
		{
			if (m_value_changed_event->IsClearEvent() || m_value_changed_event->m_event_data->m_key == NULL)
				DOMSetNull(value);
			else
				DOMSetStringWithLength(value, &m_string_holder,
						m_value_changed_event->m_event_data->m_key,
						m_value_changed_event->m_event_data->m_key_length);
		}
		return GET_SUCCESS;

	case OP_ATOM_oldValue:
		if (value && m_value_changed_event != NULL)
		{
			if (m_value_changed_event->IsClearEvent() || m_value_changed_event->m_event_data->m_old_value == NULL)
				DOMSetNull(value);
			else
				DOMSetStringWithLength(value, &m_string_holder,
						m_value_changed_event->m_event_data->m_old_value,
						m_value_changed_event->m_event_data->m_old_value_length);
		}
		return GET_SUCCESS;

	case OP_ATOM_newValue:
		if (value && m_value_changed_event != NULL)
		{
			if (m_value_changed_event->IsClearEvent() || m_value_changed_event->m_event_data->m_new_value == NULL)
				DOMSetNull(value);
			else
				DOMSetStringWithLength(value, &m_string_holder,
						m_value_changed_event->m_event_data->m_new_value,
						m_value_changed_event->m_event_data->m_new_value_length);
		}
		return GET_SUCCESS;

	case OP_ATOM_url:
		DOMSetString(value, m_origining_url.CStr());
		return GET_SUCCESS;

	case OP_ATOM_storageArea:
		if (m_custom_storage_obj != NULL)
		{
			DOMSetObject(value, m_custom_storage_obj);
		}
		else if (m_value_changed_event != NULL)
		{
			switch (m_value_changed_event->GetType())
			{
			case WEB_STORAGE_LOCAL:
				return GetEnvironment()->GetWindow()->GetName(OP_ATOM_localStorage, value, origining_runtime);
			case WEB_STORAGE_SESSION:
				return GetEnvironment()->GetWindow()->GetName(OP_ATOM_sessionStorage, value, origining_runtime);
#ifdef WEBSTORAGE_WIDGET_PREFS_SUPPORT
			case WEB_STORAGE_WGT_PREFS:
			{
				ES_Value widgets_value;
				if (GetEnvironment()->GetWindow()->GetPrivate(DOM_PRIVATE_widget, &widgets_value) == OpBoolean::IS_TRUE &&
					widgets_value.type == VALUE_OBJECT)
				{
					if (DOM_Object *widgets_object = DOM_GetHostObject(widgets_value.value.object))
						if (widgets_object->IsA(DOM_TYPE_WIDGET))
							return widgets_object->GetName(OP_ATOM_preferences, value, origining_runtime);
				}
				return GET_FAILED;
			}
#endif //WEBSTORAGE_WIDGET_PREFS_SUPPORT
			}
			OP_ASSERT(!"This should never happen");
		}
		return GET_SUCCESS;
	}

	return DOM_Event::GetName(property_name, value, origining_runtime);
}

/* virtual */ void
DOM_StorageEvent::GCTrace()
{
	DOM_Event::GCTrace();
	GCMark(m_custom_storage_obj);
}

/* static */ int
DOM_StorageEvent::initStorageEvent(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(event_obj, DOM_TYPE_STORAGEEVENT, DOM_StorageEvent);
	DOM_CHECK_ARGUMENTS("sbbZZZso");

	//in DOMString typeArg
	//in boolean canBubbleArg
	//in boolean cancelableArg
	//in DOMString keyArg
	//in DOMString oldValueArg
	//in DOMString newValueArg
	//in DOMString urlArg
	//in Storage storageAreaArg

	int result = initEvent(this_object, argv, argc, return_value, origining_runtime);

	if (result != ES_FAILED)
		return result;

	// storage area
	DOM_ARGUMENT_OBJECT_EXISTING(event_obj->m_custom_storage_obj, 7, DOM_TYPE_STORAGE, DOM_Storage);
	OP_ASSERT(event_obj->m_custom_storage_obj != NULL);

	// url
	CALL_FAILED_IF_ERROR(event_obj->m_origining_url.Set(argv[6].value.string));

	if (argv[3].type != VALUE_STRING_WITH_LENGTH &&
		argv[4].type != VALUE_STRING_WITH_LENGTH &&
		argv[5].type != VALUE_STRING_WITH_LENGTH)
	{
		// clear event
		CALL_FAILED_IF_ERROR(OpStorageValueChangedEvent::Create(
				event_obj->m_custom_storage_obj->GetStorageType(),
				OpStorageEventContext(), event_obj->m_value_changed_event));
	}
	else
	{
		const uni_char *key = NULL, *old_value = NULL, *new_value = NULL;
		unsigned key_length = 0, old_value_length = 0, new_value_length = 0;

		if (argv[3].type == VALUE_STRING_WITH_LENGTH)
		{
			key = argv[3].value.string_with_length->string;
			key_length = argv[3].value.string_with_length->length;
		}

		if (argv[4].type == VALUE_STRING_WITH_LENGTH)
		{
			old_value = argv[4].value.string_with_length->string;
			old_value_length = argv[4].value.string_with_length->length;
		}

		if (argv[5].type == VALUE_STRING_WITH_LENGTH)
		{
			new_value = argv[5].value.string_with_length->string;
			new_value_length = argv[5].value.string_with_length->length;
		}

		CALL_FAILED_IF_ERROR(OpStorageValueChangedEvent::Create(
				event_obj->m_custom_storage_obj->GetStorageType(),
				key, key_length,
				old_value, old_value_length,
				new_value, new_value_length,
				OpStorageEventContext(), event_obj->m_value_changed_event));
	}

	OP_ASSERT(event_obj->m_value_changed_event != NULL);
	event_obj->m_value_changed_event->IncRefCount();
#undef ARGUMENT
	return result;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_StorageEvent)
	DOM_FUNCTIONS_FUNCTION(DOM_StorageEvent, DOM_StorageEvent::initStorageEvent, "initStorageEvent", "sbbZZZsoo-")
DOM_FUNCTIONS_END(DOM_StorageEvent)

#endif // CLIENTSIDE_STORAGE_SUPPORT
