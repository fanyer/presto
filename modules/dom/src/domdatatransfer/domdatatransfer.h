/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DOM_DATATRANSFER_H
#define DOM_DATATRANSFER_H

#if defined(DRAG_SUPPORT) || defined(USE_OP_CLIPBOARD)

#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domfile/domfilelist.h"
#include "modules/dom/src/domcore/domstringlist.h"

class DOM_File;
class DOM_DataTransfer;
class DOM_DataTransferItems;
class OpDragObject;

enum DataStoreMode
{
	DATA_STORE_MODE_READ_WRITE,
	DATA_STORE_MODE_READ_ONLY,
	DATA_STORE_MODE_PROTECTED
};

class DOM_DataTransferItem
	: public DOM_Object
{
public:
	enum DataTransferItemKind
	{
		DATA_TRANSFER_ITEM_KIND_NONE,
		DATA_TRANSFER_ITEM_KIND_TEXT,
		DATA_TRANSFER_ITEM_KIND_FILE
	};
	DOM_DataTransferItem()
		: m_parent(NULL)
	{
		m_store.m_kind = DATA_TRANSFER_ITEM_KIND_NONE;
	}
	virtual ~DOM_DataTransferItem();

	static OP_STATUS Make(DOM_DataTransferItem*& data_transfer_item, DOM_DataTransferItems* parent, DOM_File* data, DOM_Runtime* runtime, BOOL set_underying_data = TRUE);
	static OP_STATUS Make(DOM_DataTransferItem*& data_transfer_item, DOM_DataTransferItems* parent, const uni_char* format, const uni_char* data, DOM_Runtime* runtime, BOOL set_underying_data = TRUE);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_DATA_TRANSFER_ITEM || DOM_Object::IsA(type); }
	virtual void GCTrace();

	const OpString& GetType() { return m_type; }
	DataTransferItemKind GetKind() { return m_store.m_kind; }

	const uni_char* GetStringData()
	{
		OP_ASSERT(m_store.m_kind == DATA_TRANSFER_ITEM_KIND_TEXT);
		return m_store.m_data.data;
	}

	DOM_File* GetFileData()
	{
		OP_ASSERT(m_store.m_kind == DATA_TRANSFER_ITEM_KIND_FILE);
		return m_store.m_data.file;
	}

	OpDragObject* GetDataStore();
	BOOL HasProtectionModeOverridden();
	DataStoreMode GetOverriddenProtectionMode();

	DOM_DECLARE_FUNCTION_WITH_DATA(get);
	enum { FUNCTIONS_WITH_DATA_ARRAY_SIZE = 3 };

private:
	OpString m_type;
	struct Store
	{
		DataTransferItemKind m_kind;
		union
		{
			DOM_File* file;
			uni_char* data;
		} m_data;
	} m_store;
	DOM_DataTransferItems* m_parent;
};

class DOM_DataTransferItems
	: public DOM_Object
{
public:
	static OP_STATUS Make(DOM_DataTransferItems *&data_transfer_items, DOM_DataTransfer *parent, DOM_Runtime *runtime);

	virtual ES_GetState	GetIndex(int property_index, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_DeleteStatus DeleteIndex(int property_index, ES_Runtime* origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_DATA_TRANSFER_ITEMS || DOM_Object::IsA(type); }
	virtual void GCTrace();

	OP_STATUS Initialize(DOM_DataTransfer* parent, DOM_Runtime* runtime);
	OP_STATUS GetTypes(DOM_DOMStringList*& types, DOM_Runtime* runtime);
	OP_STATUS GetFiles(DOM_FileList*& files, DOM_Runtime* runtime);

	void ClearData(const uni_char* format);
	OP_STATUS SetData(const uni_char* format, const uni_char* data, DOM_Runtime* runtime);
	const uni_char* GetData(const uni_char* format);
	DOM_DataTransfer* GetParent() { return m_parent; }
	BOOL Contains(DOM_DataTransferItem* item);
	OP_STATUS Add(DOM_DataTransferItem* item);
	OpDragObject* GetDataStore();
	BOOL HasProtectionModeOverridden();
	DataStoreMode GetOverriddenProtectionMode();

	DOM_DECLARE_FUNCTION(clear);
	DOM_DECLARE_FUNCTION(add);
	enum { FUNCTIONS_ARRAY_SIZE = 3 };

private:
	DOM_DataTransferItems()
		: m_types(NULL)
		, m_types_empty(NULL)
		, m_files(NULL)
		, m_files_empty(NULL)
		, m_parent(NULL)
	{
	}

	OpVector<DOM_DataTransferItem> m_items;

	DOM_DOMStringList* m_types;
	DOM_DOMStringList* m_types_empty;

	DOM_FileList* m_files;
	DOM_FileList* m_files_empty;

	DOM_DataTransfer* m_parent;

	class FormatsGenerator
		: public DOM_DOMStringList::Generator
	{
	public:
		FormatsGenerator()
			: m_client(NULL)
		{
		}

		OpVector<OpString>& GetFormats() { return m_formats; }
		unsigned StringList_length();
		OP_STATUS StringList_item(int index, const uni_char *&name);
		BOOL StringList_contains(const uni_char *string);
		void SetClient(DOM_DOMStringList* client) { m_client = client; }

	private:
		DOM_DOMStringList* m_client;
		OpAutoVector<OpString> m_formats;
	} m_generator;

};

class DOM_DataTransfer
	: public DOM_Object
{
public:
	DOM_DataTransfer();
	~DOM_DataTransfer();

	/** Enumerates types of the data store. */
	enum DataStoreType
	{
		/** The data store's type is unknown. */
		DataStoreType_Unknown,
		/** The global (real) data store is used. */
		DataStoreType_Global,
		/** The local (fake) data store is used. */
		DataStoreType_Local
	};

	/** Describes the underlying data store. */
	struct DataStoreInfo
	{
		DataStoreInfo() : type(DataStoreType_Unknown), drag_and_drops(TRUE)
		{}

		union
		{
			/** Points at the local data store. Valid only if type == DataStoreType_Local. */
			OpDragObject* object;
			/** The ID of the global data store. Valid only if type == DataStoreType_Global. */
			unsigned id;
		} info;
		/** Type of the data store.
		 * @see DataStoreType.
		 */
		DataStoreType type;
		/** TRUE if this is drag'n'drop's data store. */
		BOOL drag_and_drops;
	};

	static OP_STATUS Make(DOM_DataTransfer*& data_transfer, DOM_Runtime* runtime, BOOL set_origin, DataStoreInfo &data_store_info, ES_Object* inherit_properties = NULL);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_DATA_TRANSFER || DOM_Object::IsA(type); }
	virtual void GCTrace();

	enum
	{
		FUNCTIONS_WITH_DATA_START,
		FUNCTIONS_WITH_DATA_clearData,
		FUNCTIONS_WITH_DATA_setData,
		FUNCTIONS_WITH_DATA_getData,
#ifdef DRAG_SUPPORT
		FUNCTIONS_WITH_DATA_setDragImage,
		FUNCTIONS_WITH_DATA_addElement,
#endif // DRAG_SUPPORT
		FUNCTIONS_WITH_DATA_ARRAY_SIZE
	};

	DOM_DECLARE_FUNCTION_WITH_DATA(handleData); // [clear|set|get]Data
#ifdef DRAG_SUPPORT
	DOM_DECLARE_FUNCTION_WITH_DATA(setFeedbackElement); // setDragImage, addElement
#endif // DRAG_SUPPORT

	enum
	{
		FUNCTIONS_START,
#ifdef DRAG_SUPPORT
		FUNCTIONS_allowTargetOrigin,
#endif // DRAG_SUPPORT
		FUNCTIONS_ARRAY_SIZE
	};

#ifdef DRAG_SUPPORT
	DOM_DECLARE_FUNCTION(allowTargetOrigin);
#endif // DRAG_SUPPORT

	/** Gets current data store's protection mode which depends on current d'n'd event.
	 *
	 * @param origining_runtime the runtime the d'n'd event is triggered in.
	 * @param search_start_drop_up - If TRUE it's search in the interrupted threads
	 * stack if there is any ONDRAGSTART/ONDROP event in order to take the data store mode
	 * based on them even if any other event is encountered first. This is needed for
	 * overridding th mode frr synthetic events fired from real d'n'd event
	 * (real's event protection mode should be used then).
	 *
	 * @see DataStoreMode
	 */
	static DataStoreMode GetDataStoreMode(ES_Runtime* origining_runtime, BOOL search_start_drop_up = FALSE);

	/** Checks if the data store is valid. (It's valid only inside drag'n'drop events handelrs) */
	static BOOL IsDataStoreValid(ES_Runtime* origining_runtime);

	/** Checks if the data store is valid. (It's valid only inside drag'n'drop events handelrs) */
	static BOOL IsDataStoreValid(ES_Thread* thread);

	/** Gets the underlying data store object. */
	OpDragObject* GetDataStore();

	/** Check if the data store's protection mode was overridden. */
	BOOL HasProtectionModeOverridden() { return m_has_overridden_data_store_mode; }

	/** Clears the flag indicating whether the data store's protection mode was overridden.
	 * @see m_has_overridden_data_store_mode
	 */
	void ClearProtectionModeOverridden() { m_has_overridden_data_store_mode = FALSE; }

	/** Get's the overridden data store's protection mode. Must not be called if
	 * HasProtectionModeOverridden() returns FALSE.
	 * @see HasProtectionModeOverridden()
	 */
	DataStoreMode GetOverriddenProtectionMode() { OP_ASSERT(HasProtectionModeOverridden()); return m_overridden_data_store_mode; }

	/** Overrides the data store protection mode with the given mode. */
	void OverrideProtectionMode(DataStoreMode mode) { m_has_overridden_data_store_mode = TRUE; m_overridden_data_store_mode = mode; }

	/** Returns information whether this dataTransfer operates on is its own or shared (global) data store. */
	BOOL OperatesOnGlobalDataStore() { return m_data_store_info.type == DataStoreType_Global; }

	/** Returns TRUE if this object was created for purposes of d'n'd. */
	BOOL IsDragAndDrops() { return m_data_store_info.drag_and_drops; }
private:
	DOM_DataTransferItems* m_items;
	/** Overridden data store protection mode (might want to override it for synthetic d'n'd events). */
	DataStoreMode m_overridden_data_store_mode;
	/** A flag indicating whether the data store has its protection mode overriden. */
	BOOL m_has_overridden_data_store_mode;
	/** Info about the underlying data store.
	 * @see DataStoreInfo.
	 */
	DataStoreInfo m_data_store_info;
};

#endif // DRAG_SUPPORT || USE_OP_CLIPBOARD

#endif // DOM_DATATRANSFER_H
