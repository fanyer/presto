/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef OPSTORAGE_SUPPORT

#include "modules/database/webstorage_data_abstraction.h"

/*static*/
WebStorageValue*
WebStorageValue::Create(const uni_char* value)
{
	OP_ASSERT(value != NULL);
	return Create(value, static_cast<unsigned>(uni_strlen(value)));
}

/*static*/
WebStorageValue*
WebStorageValue::Create(const uni_char* value, unsigned value_length)
{
	OP_ASSERT(value != NULL || value_length == 0);
	WebStorageValue* new_value = OP_NEW(WebStorageValue,());
	if (new_value != NULL && value != NULL)
	{
		if (OpStatus::IsError(new_value->Copy(value, value_length)))
		{
			new_value->m_value = NULL;
			OP_DELETE(new_value);
			new_value = NULL;
		}
	}
	return new_value;
}

/*static*/
WebStorageValue*
WebStorageValue::Create(const WebStorageValue* value)
{
	OP_ASSERT(value != NULL);
	return Create(value->m_value, value->m_value_length);
}

OP_STATUS
WebStorageValue::Copy(const uni_char* value, unsigned value_length)
{
	OP_ASSERT(value != NULL);
	uni_char* new_value_str = OP_NEWA(uni_char,value_length+1);

	RETURN_OOM_IF_NULL(new_value_str);

	op_memcpy(new_value_str, value, UNICODE_SIZE(value_length));
	new_value_str[value_length] = 0;
	m_value = new_value_str;
	m_value_length = value_length;

	return OpStatus::OK;
}

OP_STATUS
WebStorageValue::Copy(const WebStorageValue* value)
{
	OP_ASSERT(value != NULL);
	return Copy(value->m_value, value->m_value_length);
}

BOOL
WebStorageValue::Equals(const uni_char* value, unsigned value_length) const
{
	return m_value_length == value_length &&
			(m_value_length == 0 ||
			m_value == value ||
			(m_value != NULL && value != NULL && op_memcmp(m_value, value, UNICODE_SIZE(value_length)) == 0)
		);
}

WebStorageOperation::~WebStorageOperation()
{
	Clear();
}

OP_STATUS
WebStorageOperation::CloneInto(WebStorageOperation* dest) const
{
	OP_ASSERT(dest != NULL);

	dest->Clear();
	dest->m_type = m_type;

	dest->m_error_data.m_error = m_error_data.m_error;
	if (m_error_data.m_error_message != NULL)
		RETURN_OOM_IF_NULL(dest->m_error_data.m_error_message = UniSetNewStr(m_error_data.m_error_message));

	if (m_type == WebStorageOperation::GET_ITEM_COUNT)
	{
		dest->m_data.m_item_count = m_data.m_item_count;
	}
	else
	{
		if (m_data.m_storage.m_value != NULL)
		{
			if ((dest->m_data.m_storage.m_value = WebStorageValue::Create(m_data.m_storage.m_value)) == NULL)
			{
				dest->m_error_data.m_error = OpStatus::ERR_NO_MEMORY;
				return OpStatus::ERR_NO_MEMORY;
			}
		}
		else
		{
			dest->m_data.m_storage.m_value = NULL;
		}
		dest->m_data.m_storage.m_storage_mutated = m_data.m_storage.m_storage_mutated;
	}
	return OpStatus::OK;
}

void
WebStorageOperation::Clear()
{
	ClearErrorInfo();

	if (m_type == GET_KEY_BY_INDEX ||
		m_type == GET_ITEM_BY_KEY ||
#ifdef WEBSTORAGE_RO_PAIRS_SUPPORT
		m_type == SET_ITEM_RO ||
#endif //WEBSTORAGE_RO_PAIRS_SUPPORT
		m_type == SET_ITEM)
		OP_DELETE(m_data.m_storage.m_value);

	m_type = NO_OP;
	op_memset(&m_data, 0, sizeof(m_data));
}

void
WebStorageOperation::ClearErrorInfo()
{
	if (m_error_data.m_error_message != NULL)
		OP_DELETEA(const_cast<uni_char*>(m_error_data.m_error_message));
	op_memset(&m_error_data, 0, sizeof(m_error_data));
}

WebStorageBackend::WebStorageBackend()
	: m_type(WEB_STORAGE_START)
	, m_origin(NULL)
	, m_context_id(DB_NULL_CONTEXT_ID)
	, m_is_persistent(FALSE)
#ifdef WEBSTORAGE_WIDGET_PREFS_SUPPORT
	, m_default_widget_prefs_set(FALSE)
#endif // WEBSTORAGE_WIDGET_PREFS_SUPPORT
{}

WebStorageBackend::~WebStorageBackend()
{
	//m_state_listeners.Clear();
	//InvokeShutdownListeners also Clears
	InvokeShutdownListeners();

	OP_DELETEA(m_origin);
	m_origin = NULL;
	m_type = WEB_STORAGE_START;
}

class WebStorageBackend::WebStorageStateChangeListenerLink
	: public ListElement<WebStorageBackend::WebStorageStateChangeListenerLink>
{
public:
	WebStorageStateChangeListenerLink(WebStorageStateChangeListener* p)
		: m_listener(p)
	{
	}

	~WebStorageStateChangeListenerLink()
	{
		OP_ASSERT(!InList());
	}

	WebStorageStateChangeListener *m_listener;
};

OP_STATUS
WebStorageBackend::AddListener(WebStorageStateChangeListener* cb)
{
	WebStorageStateChangeListenerLink* new_listener = OP_NEW(WebStorageStateChangeListenerLink, (cb));
	RETURN_OOM_IF_NULL(new_listener);
	new_listener->Into(&m_state_listeners);
	return OpStatus::OK;
}

void
WebStorageBackend::RemoveListener(WebStorageStateChangeListener* cb)
{
	WebStorageStateChangeListenerLink *m, *n = m_state_listeners.First();
	while (n != NULL)
	{
		m = n->Suc();
		if (n->m_listener == cb)
		{
			n->Out();
			OP_DELETE(n);
		}
		n = m;
	}
}

void
WebStorageBackend::InvokeMutationListeners()
{
	WebStorageStateChangeListenerLink *m, *n = m_state_listeners.First();
	while (n != NULL)
	{
		m = n->Suc();
		OP_ASSERT(n->m_listener);
		n->m_listener->HandleStateChange(WebStorageStateChangeListener::STORAGE_MUTATED);
		n = m;
	}
}

void
WebStorageBackend::InvokeShutdownListeners()
{
	WebStorageStateChangeListenerLink *n;
	while ((n = m_state_listeners.First()) != NULL)
	{
		n->Out();
		OP_ASSERT(n->m_listener);
		n->m_listener->HandleStateChange(WebStorageStateChangeListener::SHUTDOWN);
		OP_DELETE(n);
	}
}

/*virtual*/
OP_STATUS
WebStorageBackend::Init(WebStorageType type, const uni_char* origin, BOOL is_persistent, URL_CONTEXT_ID context_id)
{
	OP_ASSERT(WEB_STORAGE_START < type && type < WEB_STORAGE_END);
	OP_ASSERT(origin != NULL);
	m_origin = UniSetNewStr(origin);
	RETURN_OOM_IF_NULL(m_origin);
	m_type = type;
	m_is_persistent = is_persistent;
	m_context_id = context_id;
	return OpStatus::OK;
}

#endif //OPSTORAGE_SUPPORT
