/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#if defined DRAG_SUPPORT || defined USE_OP_CLIPBOARD

#include "modules/dom/src/domdatatransfer/domdatatransfer.h"
#ifdef DRAG_SUPPORT
#include "modules/dragdrop/dragdrop_manager.h"
#endif // DRAG_SUPPORT
#ifdef USE_OP_CLIPBOARD
#include "modules/dragdrop/clipboard_manager.h"
#endif // USE_OP_CLIPBOARD
#include "modules/dragdrop/dragdrop_data_utils.h"
#include "modules/pi/OpDragObject.h"
#include "modules/dom/src/domfile/domfile.h"
#include "modules/layout/box/box.h"
#include "modules/doc/html_doc.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/dom/src/domcore/element.h"
#include "modules/dom/src/domcore/domdoc.h"

#define IS_READ_WRITE(data_store_elm, runtime)                                  \
(data_store_elm->HasProtectionModeOverridden() ?                                \
(data_store_elm->GetOverriddenProtectionMode() == DATA_STORE_MODE_READ_WRITE) : \
(DOM_DataTransfer::GetDataStoreMode(runtime) == DATA_STORE_MODE_READ_WRITE))

#define IS_PROTECTED(data_store_elm, runtime)                                   \
(data_store_elm->HasProtectionModeOverridden() ?                                \
(data_store_elm->GetOverriddenProtectionMode() == DATA_STORE_MODE_PROTECTED) :  \
(DOM_DataTransfer::GetDataStoreMode(runtime) == DATA_STORE_MODE_PROTECTED))

#define IS_FILE(item) (item->GetKind() == DOM_DataTransferItem::DATA_TRANSFER_ITEM_KIND_FILE)
#define IS_STRING(item) (item->GetKind() == DOM_DataTransferItem::DATA_TRANSFER_ITEM_KIND_TEXT)
#define IS_VALID(runtime) (DOM_DataTransfer::IsDataStoreValid(runtime))

/* static */ OP_STATUS
DOM_DataTransferItem::Make(DOM_DataTransferItem*& data_transfer_item, DOM_DataTransferItems* parent, DOM_File* file, DOM_Runtime* runtime, BOOL set_underying_data /* = TRUE */)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(data_transfer_item = OP_NEW(DOM_DataTransferItem, ()), runtime, runtime->GetPrototype(DOM_Runtime::DATATRANSFERITEM_PROTOTYPE), "DataTransferItem"));
	data_transfer_item->m_store.m_kind = DATA_TRANSFER_ITEM_KIND_FILE;
	OP_ASSERT(parent);
	data_transfer_item->m_parent = parent;
	OP_ASSERT(data_transfer_item->GetDataStore());
	RETURN_IF_ERROR(data_transfer_item->m_type.Set(file->GetContentType() ? file->GetContentType() : UNI_L("application/octet-stream")));
	data_transfer_item->m_type.MakeLower();
	if (set_underying_data)
		RETURN_IF_ERROR(data_transfer_item->GetDataStore()->SetData(file->GetPath(), data_transfer_item->m_type.CStr(), TRUE, FALSE));
	RETURN_IF_ERROR(DOM_File::Make(data_transfer_item->m_store.m_data.file, file->GetPath(), FALSE, FALSE, runtime));
	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_DataTransferItem::Make(DOM_DataTransferItem*& data_transfer_item, DOM_DataTransferItems* parent, const uni_char* format, const uni_char* data, DOM_Runtime* runtime, BOOL set_underying_data /* = TRUE */)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(data_transfer_item = OP_NEW(DOM_DataTransferItem, ()), runtime, runtime->GetPrototype(DOM_Runtime::DATATRANSFERITEM_PROTOTYPE), "DataTransferItem"));
	data_transfer_item->m_store.m_kind = DATA_TRANSFER_ITEM_KIND_TEXT;
	OP_ASSERT(parent);
	data_transfer_item->m_parent = parent;
	OP_ASSERT(data_transfer_item->GetDataStore());
	RETURN_IF_ERROR(data_transfer_item->m_type.Set(format));
	data_transfer_item->m_type.MakeLower();
	data_transfer_item->m_store.m_data.data = NULL;
	if (data)
		if (!(data_transfer_item->m_store.m_data.data = UniSetNewStr(data)))
			return OpStatus::ERR_NO_MEMORY;
	if (set_underying_data)
		RETURN_IF_ERROR(data_transfer_item->GetDataStore()->SetData(data_transfer_item->m_store.m_data.data, data_transfer_item->m_type.CStr(), FALSE, TRUE));
	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_DataTransferItem::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
		case OP_ATOM_kind:
			if (value)
			{
				if (IS_VALID(origining_runtime) && m_parent->Contains(this))
					DOMSetString(value, IS_STRING(this) ? UNI_L("string") : UNI_L("file"));
				else
					DOMSetString(value);
			}
			return GET_SUCCESS;

		case OP_ATOM_type:
			if (value)
			{
				if (IS_VALID(origining_runtime) && m_parent->Contains(this))
					DOMSetString(value, m_type.CStr());
				else
					DOMSetString(value);
			}
			return GET_SUCCESS;

		default:
			return DOM_Object::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_DataTransferItem::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
		case OP_ATOM_kind:
		case OP_ATOM_type:
			return PUT_READ_ONLY;

		default:
			return DOM_Object::PutName(property_name, value, origining_runtime);
	}
}

/* static */ int
DOM_DataTransferItem::get(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(data_transfer_item, DOM_TYPE_DATA_TRANSFER_ITEM, DOM_DataTransferItem);

	DataStoreMode dsmode = data_transfer_item->HasProtectionModeOverridden()
	                       ? data_transfer_item->GetOverriddenProtectionMode()
	                       : DOM_DataTransfer::GetDataStoreMode(origining_runtime);

	if (data == 1)
	{
		DOM_CHECK_ARGUMENTS("O");

		if (argv[0].type == VALUE_UNDEFINED || argv[0].type == VALUE_NULL
			|| (dsmode != DATA_STORE_MODE_READ_WRITE && dsmode != DATA_STORE_MODE_READ_ONLY)
			|| !IS_STRING(data_transfer_item))
			return ES_FAILED;

		ES_AsyncInterface* asyncif = origining_runtime->GetEnvironment()->GetAsyncInterface();

		ES_Value argument;
		DOMSetString(&argument, data_transfer_item->m_store.m_data.data);

		asyncif->CallFunction(argv[0].value.object, NULL, 1, &argument, NULL, NULL);
	}
	else
	{
		if ((dsmode == DATA_STORE_MODE_READ_WRITE || dsmode == DATA_STORE_MODE_READ_ONLY)
			&& IS_FILE(data_transfer_item))
			DOMSetObject(return_value, data_transfer_item->m_store.m_data.file);
		else
			DOMSetNull(return_value);
		return ES_VALUE;
	}
	return ES_FAILED;
}

DOM_DataTransferItem::~DOM_DataTransferItem()
{
	if (IS_STRING(this))
		OP_DELETEA(m_store.m_data.data);
}

void DOM_DataTransferItem::GCTrace()
{
	if (IS_FILE(this))
		GCMark(m_store.m_data.file);

	GCMark(m_parent);
}

OpDragObject*
DOM_DataTransferItem::GetDataStore()
{
	return m_parent->GetDataStore();
}

BOOL
DOM_DataTransferItem::HasProtectionModeOverridden()
{
	return m_parent->HasProtectionModeOverridden();
}

DataStoreMode
DOM_DataTransferItem::GetOverriddenProtectionMode()
{
	return m_parent->GetOverriddenProtectionMode();
}


/* static */ OP_STATUS
DOM_DataTransferItems::Make(DOM_DataTransferItems*& data_transfer_items, DOM_DataTransfer* parent, DOM_Runtime* runtime)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(data_transfer_items = OP_NEW(DOM_DataTransferItems, ()), runtime, runtime->GetPrototype(DOM_Runtime::DATATRANSFERITEMS_PROTOTYPE), "DataTransferItemList"));
	RETURN_IF_ERROR(data_transfer_items->Initialize(parent, runtime));
	return OpStatus::OK;
}

OP_STATUS
DOM_DataTransferItems::Initialize(DOM_DataTransfer* parent, DOM_Runtime* runtime)
{
	OP_ASSERT(parent);
	m_parent = parent;

	OpDragObject* drag_object = GetDataStore();
	OP_ASSERT(drag_object);
	OpDragDataIterator& iter = drag_object->GetDataIterator();
	if (!iter.First())
		return OpStatus::OK;
	do
	{
		DOM_DataTransferItem* item;

		if (iter.IsFileData())
		{
			DOM_File* dom_file;
			RETURN_IF_ERROR(DOM_File::Make(dom_file, iter.GetFileData()->GetFullPath(), FALSE, FALSE, runtime));
			RETURN_IF_ERROR(DOM_DataTransferItem::Make(item, this, dom_file, runtime, FALSE));
		}
		else
			RETURN_IF_ERROR(DOM_DataTransferItem::Make(item, this, iter.GetMimeType(), iter.GetStringData(), runtime, FALSE));

		RETURN_IF_ERROR(m_items.Add(item));
	} while (iter.Next());

	return OpStatus::OK;
}

OP_STATUS
DOM_DataTransferItems::GetTypes(DOM_DOMStringList *&types, DOM_Runtime *runtime)
{
	if (!IS_VALID(runtime))
	{
		/* Have to return the same DOMStringList */
		if (!m_types_empty)
			RETURN_IF_ERROR(DOM_DOMStringList::Make(m_types_empty, NULL, NULL, runtime));

		types = m_types_empty;
	}
	else
	{
		if (!m_types)
		{
			if (OpDragObject* data_store = GetDataStore())
				RETURN_IF_ERROR(DragDrop_Data_Utils::DOMGetFormats(data_store, m_generator.GetFormats()));
			RETURN_IF_ERROR(DOM_DOMStringList::Make(m_types, m_parent, &m_generator, runtime));
			m_generator.SetClient(m_types);
		}
		types = m_types;
	}

	return OpStatus::OK;
}

OP_STATUS
DOM_DataTransferItems::GetFiles(DOM_FileList*& files, DOM_Runtime *runtime)
{
	if (!IS_VALID(runtime) || IS_PROTECTED(this, runtime))
	{
		/* Have to return the same FileList */
		if (!m_files_empty)
			RETURN_IF_ERROR(DOM_FileList::Make(m_files_empty, runtime));

		files = m_files_empty;
	}
	else
	{
		if (!m_files)
		{
			RETURN_IF_ERROR(DOM_FileList::Make(m_files, runtime));
			for (unsigned i = 0; i < m_items.GetCount(); ++i)
			{
				DOM_DataTransferItem *it = m_items.Get(i);
				if (IS_FILE(it))
					RETURN_IF_ERROR(m_files->Add(it->GetFileData()));
			}
		}

		files = m_files;
	}
	return OpStatus::OK;
}

void DOM_DataTransferItems::GCTrace()
{
	for (unsigned index = 0; index < m_items.GetCount(); ++index)
		GCMark(m_items.Get(index));

	GCMark(m_types);
	GCMark(m_types_empty);
	GCMark(m_files);
	GCMark(m_files_empty);
	GCMark(m_parent);
}

BOOL
DOM_DataTransferItems::Contains(DOM_DataTransferItem* item)
{
	return m_items.Find(item) != -1;
}

OP_STATUS
DOM_DataTransferItems::Add(DOM_DataTransferItem* item)
{
	RETURN_IF_ERROR(m_items.Add(item));
	if (m_files && IS_FILE(item))
		RETURN_IF_ERROR(m_files->Add(item->GetFileData()));

	return OpStatus::OK;
}

void
DOM_DataTransferItems::ClearData(const uni_char* format)
{
	OP_ASSERT(GetDataStore());

#ifdef USE_OP_CLIPBOARD
	g_clipboard_manager->OnDataObjectClear(GetDataStore());
#endif // USE_OP_CLIPBOARD

	if (!format)
	{
		m_items.DeleteAll();
		GetDataStore()->ClearData();
		if (m_files)
			m_files->Clear();
	}
	else
	{
		UINT32 count = m_items.GetCount();
		for (UINT32 iter = 0; iter < count;)
		{
			if (IS_STRING(m_items.Get(iter)) && uni_str_eq(m_items.Get(iter)->GetType().CStr(), format))
			{
				DragDrop_Data_Utils::ClearStringData(GetDataStore(), m_items.Get(iter)->GetType().CStr());
				--count;
				m_items.Delete(iter);
			}
			else
				++iter;
		}
	}
}

OP_STATUS
DOM_DataTransferItems::SetData(const uni_char* format, const uni_char* data, DOM_Runtime* runtime)
{
	DOM_DataTransferItem* item;
	for (UINT32 index = 0; index < m_items.GetCount(); ++index)
	{
		item = m_items.Get(index);
		if (IS_STRING(item) &&
			uni_str_eq(item->GetType().CStr(), format))
			m_items.RemoveByItem(item);
	}

	RETURN_IF_ERROR(DOM_DataTransferItem::Make(item, this, format, data, runtime, TRUE));
	RETURN_IF_ERROR(m_items.Add(item));
	return OpStatus::OK;
}

const uni_char*
DOM_DataTransferItems::GetData(const uni_char* format)
{
	for (UINT32 index = 0; index < m_items.GetCount(); ++index)
	{
		DOM_DataTransferItem* item = m_items.Get(index);
		if (IS_STRING(item) &&
			uni_str_eq(item->GetType().CStr(), format))
			return item->GetStringData();
	}

	return NULL;
}

/* virtual */ ES_GetState
DOM_DataTransferItems::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_length)
	{
		DOMSetNumber(value, !IS_VALID(origining_runtime) ? 0 : static_cast<double>(m_items.GetCount()));
		return GET_SUCCESS;
	}

	return DOM_Object::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_DataTransferItems::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_length)
		return PUT_READ_ONLY;

	return DOM_Object::PutName(property_name, value, origining_runtime);
}

/* virtual */ ES_DeleteStatus
DOM_DataTransferItems::DeleteIndex(int property_index, ES_Runtime* origining_runtime)
{
	if (!IS_READ_WRITE(this, origining_runtime))
		return DELETE_REJECT;

	property_index = static_cast<unsigned>(property_index);
	if (static_cast<unsigned>(property_index) < m_items.GetCount())
	{
		ClearData(m_items.Get(property_index)->GetType().CStr());
		return DELETE_OK;
	}

	return DELETE_REJECT;
}

/* virtual */ ES_GetState
DOM_DataTransferItems::GetIndex(int property_index, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (!IS_VALID(origining_runtime))
		return GET_FAILED;

	property_index = static_cast<UINT32>(property_index);
	if (static_cast<UINT32>(property_index) < m_items.GetCount())
	{
		DOMSetObject(value, m_items.Get(property_index));
		return GET_SUCCESS;
	}

	return GET_FAILED;
}

/* static */ int
DOM_DataTransferItems::add(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(data_transfer_items, DOM_TYPE_DATA_TRANSFER_ITEMS, DOM_DataTransferItems);

	if (!IS_READ_WRITE(data_transfer_items, origining_runtime))
	{
		DOMSetNull(return_value);
		return ES_VALUE;
	}

	if (argc > 0)
	{
		DOM_DataTransferItem* item = NULL;

		if (argc > 1)
		{
			if (argv[0].type != VALUE_STRING)
			{
				DOMSetNumber(return_value, ES_CONVERT_ARGUMENT(ES_CALL_NEEDS_STRING, 0));
				return ES_NEEDS_CONVERSION;
			}
			else if (argv[1].type != VALUE_STRING)
			{
				DOMSetNumber(return_value, ES_CONVERT_ARGUMENT(ES_CALL_NEEDS_STRING, 1));
				return ES_NEEDS_CONVERSION;
			}

			DOM_CHECK_ARGUMENTS("ss");

			for (UINT32 iter = 0; iter < data_transfer_items->m_items.GetCount(); ++iter)
			{
				if (IS_STRING(data_transfer_items->m_items.Get(iter)) &&
					data_transfer_items->m_items.Get(iter)->GetType().CompareI(argv[1].value.string) == 0)
					return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);
			}

			OpString type;
			CALL_FAILED_IF_ERROR(type.Set(argv[1].value.string));
			type.MakeLower();
#ifdef DRAG_SUPPORT
			if (!DragDropManager::IsOperaSpecialMimeType(type.CStr())
				|| !data_transfer_items->m_parent->IsDragAndDrops()
				)
				CALL_FAILED_IF_ERROR(DOM_DataTransferItem::Make(item, data_transfer_items, type.CStr(), argv[0].value.string, origining_runtime));
#endif // DRAG_SUPPORT
		}
		else
		{
			DOM_CHECK_ARGUMENTS("o");

			DOM_HOSTOBJECT_SAFE(file, argv[0].value.object, DOM_TYPE_FILE, DOM_File);

			if (!file)
				return ES_FAILED;

			CALL_FAILED_IF_ERROR(DOM_DataTransferItem::Make(item, data_transfer_items, file, origining_runtime));
		}

		if (item)
		{
			CALL_FAILED_IF_ERROR(data_transfer_items->Add(item));
			DOMSetObject(return_value, item);
		}
		else
			DOMSetNull(return_value);
		return ES_VALUE;
	}

	return ES_FAILED;
}

/* static */ int
DOM_DataTransferItems::clear(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(data_transfer_items, DOM_TYPE_DATA_TRANSFER_ITEMS, DOM_DataTransferItems);

	if (!IS_READ_WRITE(data_transfer_items, origining_runtime))
		return GET_FAILED;

	data_transfer_items->ClearData(NULL);

	return ES_FAILED;
}

/* virtual */ unsigned
DOM_DataTransferItems::FormatsGenerator::StringList_length()
{
	return m_formats.GetCount();
}

/* virtual */ OP_STATUS
DOM_DataTransferItems::FormatsGenerator::StringList_item(int index, const uni_char *&name)
{
	OpString *s = m_formats.Get(index);
	name = s->CStr();
	return OpStatus::OK;
}

BOOL
DOM_DataTransferItems::FormatsGenerator::StringList_contains(const uni_char *string)
{

	for (unsigned i = 0, n = m_formats.GetCount(); i < n; i++)
		if (m_formats.Get(i)->CompareI(string) == 0)
			return TRUE;
	return FALSE;
}

OpDragObject*
DOM_DataTransferItems::GetDataStore()
{
	return m_parent->GetDataStore();
}

BOOL
DOM_DataTransferItems::HasProtectionModeOverridden()
{
	return m_parent->HasProtectionModeOverridden();
}

DataStoreMode
DOM_DataTransferItems::GetOverriddenProtectionMode()
{
	return m_parent->GetOverriddenProtectionMode();
}

#ifdef DRAG_SUPPORT
static const uni_char*
GetDropTypeString(unsigned drop_type)
{
	if (drop_type == DROP_NONE)
		return UNI_L("none");
	else if (drop_type == (DROP_COPY | DROP_MOVE | DROP_LINK))
		return UNI_L("all");
	else if (drop_type == DROP_UNINITIALIZED)
		return UNI_L("uninitialized");

	if (drop_type & DROP_COPY)
		return (drop_type & DROP_LINK) ? UNI_L("copyLink") : (drop_type & DROP_MOVE) ? UNI_L("copyMove") : UNI_L("copy");
	else if (drop_type & DROP_LINK)
		return (drop_type & DROP_MOVE) ? UNI_L("linkMove") : UNI_L("link");
	else if (drop_type & DROP_MOVE)
		return UNI_L("move");

	OP_ASSERT(!"Bad drop_type value");
	return UNI_L("none");
}

static int
GetDropType(OpAtom property_name, const uni_char* s)
{
	int len = uni_strlen(s);
	if (len != 4 && property_name == OP_ATOM_dropEffect)
		return -1;

	if (uni_str_eq(s, UNI_L("none")))
		return DROP_NONE;
	else if (uni_strncmp(s, UNI_L("copy"), 4) == 0)
	{
		if (len == 4)
			return DROP_COPY;
		else if (property_name == OP_ATOM_effectAllowed)
		{
			if (uni_str_eq(s + 4, "Link"))
				return DROP_COPY | DROP_LINK;
			else if (uni_str_eq(s + 4, "Move"))
				return DROP_COPY | DROP_MOVE;
		}
	}
	else if (uni_strncmp(s, UNI_L("link"), 4) == 0)
	{
		if (len == 4)
			return DROP_LINK;
		else if (property_name == OP_ATOM_effectAllowed)
			if (uni_str_eq(s + 4, "Move"))
				return DROP_LINK | DROP_MOVE;
	}
	else if (uni_str_eq(s, UNI_L("move")))
		return DROP_MOVE;
	else if (uni_str_eq(s, UNI_L("all")))
		return (DROP_COPY | DROP_MOVE | DROP_LINK);
	else if (uni_str_eq(s, UNI_L("uninitialized")))
		return DROP_UNINITIALIZED;

	return -1;
}
#endif // DRAG_SUPPORT

DOM_DataTransfer::DOM_DataTransfer()
 : m_items(NULL)
 , m_overridden_data_store_mode(DATA_STORE_MODE_PROTECTED)
 , m_has_overridden_data_store_mode(FALSE)
{
}

DOM_DataTransfer::~DOM_DataTransfer()
{
	OP_ASSERT(m_data_store_info.type > DataStoreType_Unknown);
	if (m_data_store_info.type == DataStoreType_Local)
		OP_DELETE(m_data_store_info.info.object);
}

/* static */ BOOL
DOM_DataTransfer::IsDataStoreValid(ES_Runtime* origining_runtime)
{
	ES_Thread* thread = GetCurrentThread(origining_runtime);
	return IsDataStoreValid(thread);
}

/* static */ BOOL
DOM_DataTransfer::IsDataStoreValid(ES_Thread* thread)
{
	while (thread)
	{
		ES_ThreadInfo info = thread->GetInfo();

		if (info.type == ES_THREAD_EVENT)
		{
#ifdef DRAG_SUPPORT
			if (info.data.event.type == ONDRAG || info.data.event.type == ONDRAGEND ||
			info.data.event.type == ONDRAGENTER || info.data.event.type == ONDRAGLEAVE ||
			info.data.event.type == ONDRAGOVER || info.data.event.type == ONDRAGSTART ||
			info.data.event.type == ONDROP)
				return TRUE;
#endif // DRAG_SUPPORT
#ifdef USE_OP_CLIPBOARD
			if (info.data.event.type == ONCOPY || info.data.event.type == ONCUT ||
			info.data.event.type == ONPASTE)
				return TRUE;
#endif // USE_OP_CLIPBOARD
		}

		thread = thread->GetInterruptedThread();
	}

	return FALSE;
}

/* static */ DataStoreMode
DOM_DataTransfer::GetDataStoreMode(ES_Runtime* origining_runtime, BOOL search_start_drop_up /* = FALSE */)
{
	ES_Thread* thread = GetCurrentThread(origining_runtime);

	while (thread)
	{
		ES_ThreadInfo info = thread->GetInfo();

		if (info.type == ES_THREAD_EVENT)
		{
#ifdef DRAG_SUPPORT
			if(info.data.event.type == ONDRAGSTART)
				return DATA_STORE_MODE_READ_WRITE;
			else if (info.data.event.type == ONDROP)
				return DATA_STORE_MODE_READ_ONLY;
			else if (!search_start_drop_up &&
			        (info.data.event.type == ONDRAGENTER ||
			        info.data.event.type == ONDRAG ||
			        info.data.event.type == ONDRAGOVER ||
			        info.data.event.type == ONDRAGLEAVE ||
			        info.data.event.type == ONDRAGEND))
				return DATA_STORE_MODE_PROTECTED;
			else
#endif // DRAG_SUPPORT
			{
#ifdef USE_OP_CLIPBOARD
				if (info.data.event.type == ONPASTE)
					return DATA_STORE_MODE_READ_ONLY;
				else if (info.data.event.type == ONCUT ||
				         info.data.event.type == ONCOPY)
				return DATA_STORE_MODE_READ_WRITE;
#endif // USE_OP_CLIPBOARD
			}
		}

		thread = thread->GetInterruptedThread();
	}

	return DATA_STORE_MODE_PROTECTED;
}

OpDragObject*
DOM_DataTransfer::GetDataStore()
{
	OP_ASSERT(m_data_store_info.type > DataStoreType_Unknown);
	if (m_data_store_info.type == DataStoreType_Global)
	{
#ifdef DRAG_SUPPORT
		if (m_data_store_info.drag_and_drops)
			return g_drag_manager->GetDragObject();
		else
#endif // DRAG_SUPPORT
#ifdef USE_OP_CLIPBOARD
			return g_clipboard_manager->GetEventObject(m_data_store_info.info.id);
#else
			return NULL;
#endif // USE_OP_CLIPBOARD
	}
	else
		return m_data_store_info.info.object;
}

/* static */ OP_STATUS
DOM_DataTransfer::Make(DOM_DataTransfer*& data_transfer, DOM_Runtime* runtime, BOOL set_origin, DataStoreInfo &data_store_info, ES_Object* inherit_properties /* = NULL */)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(data_transfer = OP_NEW(DOM_DataTransfer, ()), runtime, runtime->GetPrototype(DOM_Runtime::DATATRANSFER_PROTOTYPE), "DataTransfer"));

	OP_ASSERT(data_store_info.type > DOM_DataTransfer::DataStoreType_Unknown);
	data_transfer->m_data_store_info = data_store_info;

#ifdef DRAG_SUPPORT

	OpDragObject* data_store = data_transfer->GetDataStore();
	if (set_origin)
	{
		URL origin_url = runtime->GetOriginURL();
		DragDropManager::SetOriginURL(data_store, origin_url);
	}

	if (inherit_properties)
	{
		const uni_char* property_value = data_transfer->DOMGetDictionaryString(inherit_properties, UNI_L("dropEffect"), NULL);
		if (property_value)
		{
			int drop_type = GetDropType(OP_ATOM_dropEffect, property_value);
			if (drop_type >= 0)
				data_store->SetDropType(static_cast<DropType>(drop_type));
		}

		property_value = data_transfer->DOMGetDictionaryString(inherit_properties, UNI_L("effectAllowed"), NULL);
		if (property_value)
		{
			int effects = GetDropType(OP_ATOM_effectAllowed, property_value);
			if (effects >= 0)
				data_store->SetEffectsAllowed(static_cast<unsigned int>(effects));
		}
	}
#endif // DRAG_SUPPORT
	RETURN_IF_ERROR(DOM_DataTransferItems::Make(data_transfer->m_items, data_transfer, runtime));
	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_DataTransfer::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_types:
		if (value)
		{
			DOM_DOMStringList* types;
			GET_FAILED_IF_ERROR(m_items->GetTypes(types, static_cast<DOM_Runtime *>(origining_runtime)));
			DOMSetObject(value, types);
		}
		break;

	case OP_ATOM_items:
		DOMSetObject(value, m_items);
		break;

	case OP_ATOM_files:
		if (value)
		{
			DOM_FileList* files;
			GET_FAILED_IF_ERROR(m_items->GetFiles(files, static_cast<DOM_Runtime *>(origining_runtime)));
			DOMSetObject(value, files);
		}
		break;

#ifdef DRAG_SUPPORT
	case OP_ATOM_dropEffect:
		DOMSetString(value, GetDropTypeString(static_cast<unsigned>(GetDataStore() ? GetDataStore()->GetDropType() : DROP_NONE)));
		break;

	case OP_ATOM_effectAllowed:
		DOMSetString(value, GetDropTypeString(static_cast<unsigned>(GetDataStore() ? GetDataStore()->GetEffectsAllowed() : static_cast<unsigned>(DROP_UNINITIALIZED))));
		break;

	case OP_ATOM_origin:
		DOMSetString(value, GetDataStore() && DragDropManager::GetOriginURL(GetDataStore()) ? DragDropManager::GetOriginURL(GetDataStore()): UNI_L("null"));
		break;
#endif // DRAG_SUPPORT

	default:
		return DOM_Object::GetName(property_name, value, origining_runtime);
	}

	return GET_SUCCESS;
}

/* virtual */ ES_PutState
DOM_DataTransfer::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
#ifdef DRAG_SUPPORT
	case OP_ATOM_effectAllowed:
		if (!IS_READ_WRITE(this, origining_runtime))
			return PUT_READ_ONLY;
		// Fall through.
	case OP_ATOM_dropEffect:
	{
		if (OpDragObject* data_store = GetDataStore())
		{
			if (value->type != VALUE_STRING)
				return PUT_NEEDS_STRING;
			int d = GetDropType(property_name, value->value.string);
			if (d >= 0)
			{
				if (property_name == OP_ATOM_dropEffect)
					data_store->SetDropType(static_cast<DropType>(d));
				else
					data_store->SetEffectsAllowed(static_cast<unsigned int>(d));
			}
		}

		return PUT_SUCCESS;
	}
	case OP_ATOM_origin:
#endif // DRAG_SUPPORT
	case OP_ATOM_items:
	case OP_ATOM_files:
	case OP_ATOM_types:
		return PUT_READ_ONLY;
	default:
		return DOM_Object::PutName(property_name, value, origining_runtime);
	}
}

/* virtual */ void
DOM_DataTransfer::GCTrace()
{
	GCMark(m_items);
}

/* static */ int
DOM_DataTransfer::handleData(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(data_transfer, DOM_TYPE_DATA_TRANSFER, DOM_DataTransfer);

	if (data < 2 && (!IS_VALID(origining_runtime) || !IS_READ_WRITE(data_transfer, origining_runtime)))
		return ES_FAILED;

	OpString format;
	BOOL convert_to_url = FALSE;

	if (argc > 0 && argv[0].type == VALUE_STRING)
	{
		CALL_FAILED_IF_ERROR(format.Set(argv[0].value.string));
		format.MakeLower();
		if (uni_str_eq(format.CStr(), "url"))
		{
			CALL_FAILED_IF_ERROR(format.Set(UNI_L("text/uri-list")));
			convert_to_url = TRUE;
		}
		else if (uni_str_eq(format.CStr(), "text"))
			CALL_FAILED_IF_ERROR(format.Set(UNI_L("text/plain")));
	}

	BOOL forbidden = FALSE;
#ifdef DRAG_SUPPORT
	if (format.CStr()
		&& data_transfer->IsDragAndDrops()
		)
		forbidden = DragDropManager::IsOperaSpecialMimeType(format.CStr());
#endif // DRAG_SUPPORT

	if (data == 0)
	{
		DOM_CHECK_ARGUMENTS("|s");

		if (argc == 0)
			data_transfer->m_items->ClearData(NULL);
		else
			data_transfer->m_items->ClearData(format);
	}
	else if (data == 1)
	{
		DOM_CHECK_ARGUMENTS("ss");

		if (forbidden)
			return ES_FAILED;

		CALL_FAILED_IF_ERROR(data_transfer->m_items->SetData(format, argv[1].value.string, origining_runtime));

#ifdef USE_OP_CLIPBOARD
		if (!data_transfer->IsDragAndDrops())
			g_clipboard_manager->OnDataObjectSet(data_transfer->GetDataStore());
#endif // USE_OP_CLIPBOARD

	}
	else
	{
		DOM_CHECK_ARGUMENTS("s");

		TempBuffer* data = GetEmptyTempBuf();
		if (IS_VALID(origining_runtime) && !IS_PROTECTED(data_transfer, origining_runtime))
		{
			CALL_FAILED_IF_ERROR(data->Append(data_transfer->m_items->GetData(format)));

			if (convert_to_url)
				CALL_FAILED_IF_ERROR(DragDrop_Data_Utils::GetURL(data_transfer->GetDataStore(), data));
		}

		DOMSetString(return_value, data->GetStorage());
		return ES_VALUE;
	}
	return ES_FAILED;
}

#ifdef DRAG_SUPPORT
/* static */ int
DOM_DataTransfer::setFeedbackElement(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(data_transfer, DOM_TYPE_DATA_TRANSFER, DOM_DataTransfer);

	if (!IS_VALID(origining_runtime) || !IS_READ_WRITE(data_transfer, origining_runtime))
		return ES_FAILED;

	if (data == 0)
	{
		DOM_CHECK_ARGUMENTS("onn");
		DOM_HOSTOBJECT_SAFE(element, argv[0].value.object, DOM_TYPE_ELEMENT, DOM_Element);
		if (element && element->GetThisElement() && element->GetOwnerDocument() && data_transfer->GetFramesDocument())
		{
			DOM_Document* owner_doc = element->GetOwnerDocument();
			HTML_Document* data_transfer_doc = data_transfer->GetFramesDocument()->GetHtmlDocument();
			if (owner_doc && owner_doc->GetFramesDocument() && owner_doc->GetFramesDocument()->GetHtmlDocument() && data_transfer_doc)
			{
				OpPoint point(TruncateDoubleToInt(argv[1].value.number), TruncateDoubleToInt(argv[2].value.number));
				g_drag_manager->SetFeedbackElement(data_transfer_doc, element->GetThisElement(), owner_doc->GetFramesDocument()->GetHtmlDocument(), point);
			}
		}
	}
	else
	{
		DOM_CHECK_ARGUMENTS("o");

		DOM_HOSTOBJECT_SAFE(element, argv[0].value.object, DOM_TYPE_ELEMENT, DOM_Element);
		if (element && element->GetThisElement() && element->GetOwnerDocument() && data_transfer->GetFramesDocument())
		{
			DOM_Document* owner_doc = element->GetOwnerDocument();
			HTML_Document* data_transfer_doc = data_transfer->GetFramesDocument()->GetHtmlDocument();
			if (owner_doc && owner_doc->GetFramesDocument() && owner_doc->GetFramesDocument()->GetHtmlDocument() && data_transfer_doc)
				CALL_FAILED_IF_ERROR(g_drag_manager->AddElement(data_transfer_doc, element->GetThisElement(), owner_doc->GetFramesDocument()->GetHtmlDocument()));
		}
	}

	return ES_FAILED;
}

/* static */ int
DOM_DataTransfer::allowTargetOrigin(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(data_transfer, DOM_TYPE_DATA_TRANSFER, DOM_DataTransfer);

	if (argc < 1 || argv[0].type != VALUE_STRING)
		return DOM_CALL_DOMEXCEPTION(SYNTAX_ERR);

	if (!IS_VALID(origining_runtime) || !IS_READ_WRITE(data_transfer, origining_runtime))
		return DOM_CALL_DOMEXCEPTION(SECURITY_ERR);

	const uni_char* url = argv[0].value.string;
	unsigned int url_len = argv[0].string_length;

	if (url_len < 1)
		return DOM_CALL_DOMEXCEPTION(SYNTAX_ERR);

	URL resolved_url;

	if (url_len == 1 && *url == '/')
		resolved_url = origining_runtime->GetOriginURL();
	else
	{
		resolved_url = g_url_api->GetURL(url, origining_runtime->GetOriginURL().GetContextId());
		if (url_len > 1 || *url != '*')
		{
			if (!resolved_url.IsValid() || resolved_url.Type() == URL_NULL_TYPE || resolved_url.Type() == URL_UNKNOWN || DOM_Utils::IsOperaIllegalURL(resolved_url))
				return DOM_CALL_DOMEXCEPTION(SYNTAX_ERR);

			if (resolved_url.Type() != URL_DATA)
			{
				if (resolved_url.Type() != URL_JAVASCRIPT && (!resolved_url.GetServerName() || !resolved_url.GetServerName()->UniName()))
					return DOM_CALL_DOMEXCEPTION(SYNTAX_ERR);
			}
			else
			{
				const uni_char* scheme_end = uni_strstr(url, ":");
				if (!scheme_end || !(scheme_end + 1) || !(*(scheme_end + 1)))
					return DOM_CALL_DOMEXCEPTION(SYNTAX_ERR);

				// To check if it has e.g. data:text/plain, form (the comma after a mime type and before data).
				const uni_char* comma = uni_strstr(url, ",");
				if (!comma)
					return DOM_CALL_DOMEXCEPTION(SYNTAX_ERR);
			}
		}
	}

	if (url_len == 1 && *url == '*')
		CALL_FAILED_IF_ERROR(DragDropManager::AllowAllURLs(data_transfer->GetDataStore()));
	else
		CALL_FAILED_IF_ERROR(DragDropManager::AddAllowedTargetURL(data_transfer->GetDataStore(), resolved_url));

	return ES_FAILED;
}
#endif // DRAG_SUPPORT
#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_WITH_DATA_START(DOM_DataTransfer)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_DataTransfer, DOM_DataTransfer::handleData, 0, "clearData", "s-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_DataTransfer, DOM_DataTransfer::handleData, 1, "setData", "ss-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_DataTransfer, DOM_DataTransfer::handleData, 2, "getData", "s-")
#ifdef DRAG_SUPPORT
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_DataTransfer, DOM_DataTransfer::setFeedbackElement, 0, "setDragImage", "-nn-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_DataTransfer, DOM_DataTransfer::setFeedbackElement, 1, "addElement", "-")
#endif // DRAG_SUPPORT
DOM_FUNCTIONS_WITH_DATA_END(DOM_DataTransfer)

DOM_FUNCTIONS_START(DOM_DataTransfer)
#ifdef DRAG_SUPPORT
	DOM_FUNCTIONS_FUNCTION(DOM_DataTransfer, DOM_DataTransfer::allowTargetOrigin, "allowTargetOrigin", "s-")
#endif // DRAG_SUPPORT
DOM_FUNCTIONS_END(DOM_DataTransfer)

DOM_FUNCTIONS_START(DOM_DataTransferItems)
	DOM_FUNCTIONS_FUNCTION(DOM_DataTransferItems, DOM_DataTransferItems::clear, "clear", "-")
	DOM_FUNCTIONS_FUNCTION(DOM_DataTransferItems, DOM_DataTransferItems::add, "add", "-")
DOM_FUNCTIONS_END(DOM_DataTransferItems)

DOM_FUNCTIONS_WITH_DATA_START(DOM_DataTransferItem)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_DataTransferItem, DOM_DataTransferItem::get, 1, "getAsString", "-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_DataTransferItem, DOM_DataTransferItem::get, 0, "getAsFile", "-")
DOM_FUNCTIONS_WITH_DATA_END(DOM_DataTransferItem)

#endif // DRAG_SUPPORT || USE_OP_CLIPBOARD
