/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef DESKTOP_DRAG_OBJECT_H
#define DESKTOP_DRAG_OBJECT_H

#ifdef DRAG_SUPPORT

#include "modules/dragdrop/dragdrop_data_utils.h"
#include "modules/inputmanager/inputaction.h"
#include "modules/util/adt/opvector.h"
#include "modules/util/opstring.h"
#include "modules/pi/OpDragObject.h"

class DesktopDragObject;

class DesktopDragDataIterator : public OpDragDataIterator
{
public:
	DesktopDragDataIterator(DesktopDragObject *obj) : object(obj), current_data_idx(0) {}
	
	BOOL First();
	BOOL Next();
	BOOL IsFileData() const;
	BOOL IsStringData() const;
	const OpFile* GetFileData();
	const uni_char* GetStringData();
	const uni_char* GetMimeType();
	void Remove();

private:
	DesktopDragObject* object;
	unsigned current_data_idx;
};

class DesktopDragObject : public OpDragObject
{
private:
	friend class DesktopDragDataIterator;
	class DragDataElm
	{
	public:
		enum DataType
		{
			DND_DATATYPE_NONE,
			DND_DATATYPE_FILE,
			DND_DATATYPE_STRING
		};
		union
		{
			OpFile* m_file;
			OpString* m_string;
		};
		DataType m_type;
		OpString m_mime;

		OP_STATUS Construct(const uni_char* path_or_data, const uni_char* mime, BOOL is_file = FALSE);

		DragDataElm() : m_type(DND_DATATYPE_NONE) {}

		~DragDataElm()
		{
			if(m_type == DND_DATATYPE_STRING)
				OP_DELETE(m_string);
			else if(m_type == DND_DATATYPE_FILE)
				OP_DELETE(m_file);
		}
	};
	
	DesktopDragDataIterator iterator;
	void RemoveIfExists(const uni_char* format, BOOL is_file);

public:
	// Drop stuff
	enum InsertType
	{
		INSERT_INTO,
		INSERT_BEFORE,
		INSERT_AFTER,
		INSERT_BETWEEN
	};

	DesktopDragObject(OpTypedObject::Type type) :
		iterator(this),
		m_type(type),
		m_action(NULL),
		m_object(NULL),
		m_source(NULL),
		m_bitmap(NULL),
		m_drop_type(DROP_NONE),
		m_visual_drop_type(DROP_NONE),
		m_suggested_drop_type(DROP_UNINITIALIZED),
		m_effects_allowed(DROP_UNINITIALIZED),
		m_protected_mode(FALSE),
		m_read_only(FALSE),
		m_id(0),
		m_insert_type(INSERT_INTO),
		m_suggested_insert_type(INSERT_INTO),
		m_drop_capture_locked(false),
		m_restore_capture_when_lost(false)
		{
		}

	virtual ~DesktopDragObject();

	// OpDragObject overrides
	OpDragDataIterator& GetDataIterator() { return iterator; }
	OpTypedObject::Type GetType() { return m_type; }

	virtual void SetBitmap(OpBitmap* bitmap);
	virtual const OpBitmap* GetBitmap() const { return m_bitmap; }

	virtual void SetBitmapPoint(const OpPoint& bitmap_point) { m_bitmap_point = bitmap_point; }
	virtual const OpPoint& GetBitmapPoint() const { return m_bitmap_point; }

	virtual void SetSource(OpTypedObject* source) { m_source = source; }
	virtual OpTypedObject* GetSource() const { return m_source; }

	virtual OP_STATUS SetData(const uni_char *path_or_data, const uni_char *format, BOOL is_file, BOOL remove_if_exists);
	virtual void ClearData() { m_data.DeleteAll(); }
	virtual unsigned GetDataCount() { return m_data.GetCount(); }

	virtual void SetEffectsAllowed(unsigned effects) { m_effects_allowed = effects; }
	virtual unsigned GetEffectsAllowed() const { return m_effects_allowed; }

	virtual void SetVisualDropType(DropType drop_type);
	virtual DropType GetVisualDropType() const { return m_visual_drop_type; }

	virtual void SetDropType(DropType drop_type) { m_drop_type = drop_type; }
	virtual DropType GetDropType() const { return m_drop_type; }

	virtual void SetProtectedMode(BOOL protected_mode) { m_protected_mode = protected_mode; }
	virtual BOOL IsInProtectedMode() { return m_protected_mode; }

	virtual void SetReadOnly(BOOL read_only) { m_read_only = read_only; }
	virtual BOOL IsReadOnly() { return m_read_only; }

	virtual void		SetSuggestedDropType(DropType drop_type) { m_suggested_drop_type = drop_type; }
	virtual DropType	GetSuggestedDropType() const { return m_suggested_drop_type; }

	// desktop extensions

	// Set BOTH drop types at the same time
	void SetDesktopDropType(DropType drop_type)
	{
		m_drop_type = drop_type;
		m_visual_drop_type = drop_type; // set the visual type as well, to keep the two in sync
	}

	/*
	 * Set/Get a unique id to be associated with this drag object
	*/
	void SetID(int32 id) { m_id = id; }
	INT32 GetID() { return m_id; }

	/*
	 * Set/Get a object (typically a OpWidget-derived class) to be associated with this drag object
	*/
	void SetObject(OpTypedObject* object) { m_object = object; }
	OpTypedObject* GetObject() const { return m_object; }

	OP_STATUS SetURL(const uni_char* url)
	{
		RETURN_IF_ERROR(DragDrop_Data_Utils::AddURL(this, url));
		return OpStatus::OK;
	}

	const uni_char* GetURL() const
	{
		if (m_urls.GetCount() == 0)
			return NULL;

		return m_urls.Get(0)->CStr();
	}

	OP_STATUS AddURL(OpString* url)
	{
		RETURN_IF_ERROR(DragDrop_Data_Utils::AddURL(this, *url));
		OP_DELETE(url);
		url = NULL;
		return OpStatus::OK;
	}

	const OpVector<OpString>& GetURLs() const { return m_urls; }

	OP_STATUS SetTitle(const uni_char* title)
	{
		return DragDrop_Data_Utils::SetDescription(this, title);
	}
	const uni_char* GetTitle()
	{
		return DragDrop_Data_Utils::GetDescription(this);
	}

	/**
	 * Update the desktop part of the content so that it is the same
	 * as the core part. Desktop code does not always access the core
	 * part so a synchronization is needed. Should be called in the
	 * platform implementation of OpDragManager::SetDragObject()
	 */
	OP_STATUS SynchronizeCoreContent();


	OP_STATUS SetImageURL(const uni_char* image_url) { return m_image_url.Set(image_url); }
	const uni_char* GetImageURL() { return m_image_url.CStr(); }

	OP_STATUS SetAction(OpInputAction* action) { m_action = OpInputAction::CopyInputActions(action); return m_action ? OpStatus::OK : OpStatus::ERR_NO_MEMORY; }
	OpInputAction* GetAction() { return m_action; }
	
	/*
	 * Set/Get multiple unique ids to be associated with this drag object. 
	*/
	OP_STATUS			AddID(INT32 id) { return m_ids.Add(id); }
	INT32				GetIDCount() { return m_ids.GetCount(); }
	INT32				GetID(INT32 index) { return m_ids.Get(index); }
	const OpINT32Vector&	GetIDs() { return m_ids; }

	void				ResetDrop()
	{
		m_drop_type = DROP_NOT_AVAILABLE;
		m_visual_drop_type = DROP_NOT_AVAILABLE;
		m_suggested_drop_type = DROP_NOT_AVAILABLE;
		m_insert_type = INSERT_INTO;
		m_suggested_insert_type = INSERT_INTO;
	}

	void				SetInsertType(InsertType insert_type) { m_insert_type = insert_type; }
	InsertType			GetInsertType() const { return m_insert_type; }

	void				SetSuggestedInsertType(InsertType insert_type) { m_suggested_insert_type = insert_type; }
	InsertType			GetSuggestedInsertType() const { return m_suggested_insert_type; }

	BOOL				IsDropAvailable() const { return m_drop_type != DROP_NOT_AVAILABLE; }
	BOOL				IsDropCopy() const { return m_drop_type == DROP_COPY; }
	BOOL				IsDropMove() const { return m_drop_type == DROP_MOVE; }
	
	BOOL				IsInsertInto() const { return m_insert_type == INSERT_INTO; }
	BOOL				IsInsertBefore() const { return m_insert_type == INSERT_BEFORE; }
	BOOL				IsInsertAfter() const { return m_insert_type == INSERT_AFTER; }

	/**
	 * Some platforms need to know if a lost drag'n'drop event capture should be ignored or not.
	 * They can check on this flag. The flag is set when the drop happens, and cleared when the drop 
	 * has been completed.
	 */
	void				SetDropCaptureLocked(bool capture_locked) { m_drop_capture_locked = capture_locked; }
	bool				HasDropCaptureLock() { return m_drop_capture_locked; }

	void				SetRestoreCaptureWhenLost(bool restore) { m_restore_capture_when_lost = restore; }
	bool				GetRestoreCaptureWhenLost() { return m_restore_capture_when_lost; }

private:
	OpTypedObject::Type		m_type;
	OpInputAction*			m_action;
	OpTypedObject*			m_object;
	OpTypedObject*			m_source;
	OpBitmap*				m_bitmap;
	OpPoint					m_bitmap_point;
	OpAutoVector<OpString>	m_urls;
	OpString				m_image_url;
	OpAutoVector<DragDataElm>	m_data;
	DropType				m_drop_type;
	DropType				m_visual_drop_type;
	DropType				m_suggested_drop_type;
	unsigned				m_effects_allowed;
	BOOL					m_protected_mode;
	BOOL					m_read_only;
	INT32					m_id;
	OpINT32Vector			m_ids;

	InsertType				m_insert_type;
	InsertType				m_suggested_insert_type;

	bool					m_drop_capture_locked;		// true when a capture should not be lost. Used by Windows.
	bool					m_restore_capture_when_lost;	// true when capture should be set back if it changes during a drag
};

#endif // DRAG_SUPPORT
#endif // DESKTOP_DRAG_OBJECT_H
