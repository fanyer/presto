/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */


#include "core/pch.h"

#ifdef DRAG_SUPPORT
#include "adjunct/desktop_pi/DesktopDragObject.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "modules/dragdrop/dragdrop_data_utils.h"
#include "modules/pi/OpBitmap.h"


OP_STATUS DesktopDragObject::DragDataElm::Construct(const uni_char* path_or_data, const uni_char* mime, BOOL is_file)
{
	m_file = NULL;

	RETURN_IF_ERROR(m_mime.Set(mime));
	m_mime.MakeLower();
	if (is_file)
	{
		m_file = OP_NEW(OpFile, ());
		RETURN_OOM_IF_NULL(m_file);
		m_type = DND_DATATYPE_FILE;
		RETURN_IF_ERROR(m_file->Construct(path_or_data));
	}
	else
	{
		m_string = OP_NEW(OpString, ());
		RETURN_OOM_IF_NULL(m_string);
		m_type = DND_DATATYPE_STRING;
		RETURN_IF_ERROR(m_string->Set(path_or_data));
	}
	return OpStatus::OK;
}

/* static */
#ifdef DESKTOP_DRAGOBJECT_CREATE	// API_DRAGOBJECT_DESKTOP_CREATE import
OP_STATUS OpDragObject::Create(OpDragObject*& object, OpTypedObject::Type type)
{
	RETURN_OOM_IF_NULL(object = OP_NEW(DesktopDragObject, (type)));
	return OpStatus::OK;
}
#endif // DESKTOP_DRAGOBJECT_CREATE

DesktopDragObject::~DesktopDragObject()
{
	OP_DELETE(m_bitmap);
	OP_DELETE(m_action);
}

OP_STATUS  DesktopDragObject::SynchronizeCoreContent()
{
	return DragDrop_Data_Utils::GetURLs(this, m_urls);
}


void DesktopDragObject::SetBitmap(OpBitmap* bitmap)
{
	OP_DELETE(m_bitmap);
	m_bitmap = bitmap;
}

void DesktopDragObject::SetVisualDropType(DropType drop_type)
{
	 m_visual_drop_type = drop_type;
}

void DesktopDragObject::RemoveIfExists(const uni_char* format, BOOL is_file)
{
	DragDataElm::DataType type = is_file ? DragDataElm::DND_DATATYPE_FILE : DragDataElm::DND_DATATYPE_STRING;
	for (unsigned iter = 0; iter < m_data.GetCount(); ++iter)
	{
		DragDataElm* elm = m_data.Get(iter);
		if (type == elm->m_type && elm->m_mime.CompareI(format) == 0)
			m_data.Delete(iter);
	}
}

OP_STATUS DesktopDragObject::SetData(const uni_char *path_or_data, const uni_char *format, BOOL is_file, BOOL remove_if_exists)
{
	if (remove_if_exists)
		RemoveIfExists(format, is_file);
	DragDataElm* data_elm = OP_NEW(DragDataElm, ());
	RETURN_OOM_IF_NULL(data_elm);
	OpAutoPtr<DragDataElm> ap_data_elm(data_elm);
	RETURN_IF_ERROR(data_elm->Construct(path_or_data, format, is_file));
	RETURN_IF_ERROR(m_data.Add(data_elm));
	ap_data_elm.release();
	if (uni_stri_eq(format, "text/uri-list"))
		SynchronizeCoreContent();
	return OpStatus::OK;
}

//---- Iterator ----

BOOL DesktopDragDataIterator::First()
{
	current_data_idx = 0;
	return object->m_data.GetCount() > 0;
}

BOOL DesktopDragDataIterator::Next()
{
	if (current_data_idx + 1 < object->m_data.GetCount())
	{
		++current_data_idx;
		return TRUE;
	}
	return FALSE;
}

BOOL DesktopDragDataIterator::IsFileData() const
{
	return object->m_data.Get(current_data_idx)->m_type == DesktopDragObject::DragDataElm::DND_DATATYPE_FILE;
}

BOOL DesktopDragDataIterator::IsStringData() const
{
	return object->m_data.Get(current_data_idx)->m_type == DesktopDragObject::DragDataElm::DND_DATATYPE_STRING;
}

const OpFile* DesktopDragDataIterator::GetFileData()
{
	if (IsFileData())
		return object->m_data.Get(current_data_idx)->m_file;
	return NULL;
}

const uni_char* DesktopDragDataIterator::GetStringData()
{
	if (IsStringData())
		return object->m_data.Get(current_data_idx)->m_string->CStr();
	return NULL;
}

const uni_char* DesktopDragDataIterator::GetMimeType()
{
	return object->m_data.Get(current_data_idx)->m_mime.CStr();
}

void DesktopDragDataIterator::Remove()
{
	object->m_data.Delete(current_data_idx);
	--current_data_idx;
}

#endif // DRAG_SUPPORT
