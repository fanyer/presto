/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * @author Tomasz Jamroszczak tjamroszczak@opera.com
 *
 */

#include "core/pch.h"

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/domjilattachment.h"
#include "modules/dom/src/domjil/utils/jilutils.h"
#include "modules/util/opfile/opfile.h"
#include "modules/viewers/viewers.h"

/* static */ OP_STATUS
DOM_JILAttachment::Make(DOM_JILAttachment*& new_obj, DOM_Runtime *origining_runtime, const OpMessaging::Attachment* attachment_contents, const uni_char* full_filename)
{
	new_obj = OP_NEW(DOM_JILAttachment, ());
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_obj, origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::JIL_ATTACHMENT_PROTOTYPE), "Attachment"));
	new_obj->m_undefnull.MIMEType = IS_UNDEF;
	new_obj->m_undefnull.size = IS_UNDEF;
	new_obj->m_undefnull.fileName = IS_UNDEF;
	OP_ASSERT(!(attachment_contents && full_filename));
	if (attachment_contents)
		RETURN_IF_ERROR(new_obj->SetAttachment(origining_runtime, *attachment_contents));
	else if (full_filename)
		RETURN_IF_ERROR(new_obj->SetFile(full_filename));
	return OpStatus::OK;
}

DOM_JILAttachment::DOM_JILAttachment()
	: m_inside_message(FALSE)
	, m_size(0)
{
}

OP_STATUS
DOM_JILAttachment::SetFile(const uni_char* full_filename)
{
	OP_ASSERT(full_filename);
	OpFile file;

	RETURN_IF_ERROR(file.Construct(full_filename));
	OpFileInfo::Mode mode;
	RETURN_IF_ERROR(file.GetMode(mode));
	if (mode != OpFileInfo::FILE)
		return OpStatus::ERR_FILE_NOT_FOUND;
	RETURN_IF_ERROR(file.Open(OPFILE_READ));
	const uni_char* full_path = file.GetFullPath();
	RETURN_IF_ERROR(file.GetFileLength(m_size));
	RETURN_IF_ERROR(m_full_filename.Set(full_path));

	Viewer* viewer = g_viewers->FindViewerByFilename(full_filename);
	if (viewer)
		RETURN_IF_ERROR(m_MIMEType.Set(viewer->GetContentTypeString()));
	else
		RETURN_IF_ERROR(m_MIMEType.Set(UNI_L("application/octet-stream")));

	//TODO: use JILFolderLister::FileName.
	uni_char* suggested_name = uni_strrchr(full_filename, PATHSEPCHAR);
	RETURN_IF_ERROR(m_fileName.Set(suggested_name ? suggested_name + 1 : 0));

	m_undefnull.fileName = m_fileName.HasContent() ? IS_VALUE : IS_NULL;
	m_undefnull.size = IS_VALUE;
	m_undefnull.MIMEType = IS_VALUE;

	return OpStatus::OK;
}

OP_STATUS
DOM_JILAttachment::SetAttachment(DOM_Runtime *origining_runtime, const OpMessaging::Attachment& attachment_contents)
{
	RETURN_IF_ERROR(m_full_filename.Set(attachment_contents.m_full_filename));
	RETURN_IF_ERROR(m_id.Set(attachment_contents.m_id));
	m_inside_message = attachment_contents.m_inside_message;
	RETURN_IF_ERROR(m_fileName.Set(attachment_contents.m_suggested_name));
	RETURN_IF_ERROR(m_MIMEType.Set(attachment_contents.m_mimetype));
	m_size = attachment_contents.m_size;

	m_undefnull.fileName = m_fileName.HasContent() ? IS_VALUE : IS_NULL;
	m_undefnull.MIMEType = m_MIMEType.HasContent() ? IS_VALUE : IS_NULL;
	m_undefnull.size = IS_VALUE;
	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_JILAttachment::InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
		case OP_ATOM_size:
		{
			if (value)
				switch (m_undefnull.size)
				{
					case IS_UNDEF: DOMSetUndefined(value); break;
					case IS_NULL: DOMSetNull(value); break;
					case IS_VALUE: DOMSetNumber(value, static_cast<double>(m_size)); break;
				}
			return GET_SUCCESS;
		}
		case OP_ATOM_MIMEType:
		{
			if (value)
				switch (m_undefnull.MIMEType)
				{
					case IS_UNDEF: DOMSetUndefined(value); break;
					case IS_NULL: DOMSetNull(value); break;
					case IS_VALUE: DOMSetString(value, m_MIMEType.CStr()); break;
				}
			return GET_SUCCESS;
		}
		case OP_ATOM_fileName:
		{
			if (value)
			{
				switch (m_undefnull.fileName)
				{
					case IS_UNDEF: DOMSetUndefined(value); break;
					case IS_NULL: DOMSetNull(value); break;
					case IS_VALUE: DOMSetString(value, m_fileName.CStr()); break;
				}
			}
			return GET_SUCCESS;
		}
	}
	return GET_FAILED;
}

BOOL
DOM_JILAttachment::operator==(const DOM_JILAttachment& other)
{
	if (m_inside_message)
	{
		if (!(m_id == other.m_id))
			return FALSE;
	}
	else if (!(m_full_filename == other.m_full_filename))
		return FALSE;

	if (m_undefnull.fileName != IS_UNDEF
		&& (m_undefnull.fileName != other.m_undefnull.fileName || !(m_fileName == other.m_fileName)))
		return FALSE;
	if (m_undefnull.MIMEType != IS_UNDEF
		&& (m_undefnull.MIMEType != other.m_undefnull.MIMEType || !(m_MIMEType == other.m_MIMEType)))
		return FALSE;
	if (m_undefnull.size != IS_UNDEF
		&& (m_undefnull.size != other.m_undefnull.size || m_size != other.m_size))
		return FALSE;

	return TRUE;
}

/* virtual */ ES_PutState
DOM_JILAttachment::InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
		case OP_ATOM_size:
		{
			switch (value->type)
			{
				case VALUE_NULL:
					m_undefnull.size = IS_NULL; break;
				case VALUE_UNDEFINED:
					m_undefnull.size = IS_UNDEF; break;
				case VALUE_NUMBER:
					if (!op_isfinite(value->value.number) || value->value.number < 0)
						return PUT_SUCCESS;
					m_size = static_cast<UINT32>(value->value.number);
					m_undefnull.size = IS_VALUE;
					break;
				default:
					return PUT_NEEDS_NUMBER;
			}
			return PUT_SUCCESS;
		}
		case OP_ATOM_MIMEType:
		{
			switch (value->type)
			{
				case VALUE_NULL:
					m_undefnull.MIMEType = IS_NULL; m_MIMEType.Empty(); break;
				case VALUE_UNDEFINED:
					m_undefnull.MIMEType = IS_UNDEF; m_MIMEType.Empty(); break;
				case VALUE_STRING:
					PUT_FAILED_IF_ERROR(m_MIMEType.Set(value->value.string));
					m_undefnull.MIMEType = IS_VALUE;
					break;
				default:
					return PUT_NEEDS_STRING;
			}
			return PUT_SUCCESS;
		}
		case OP_ATOM_fileName:
		{
			switch (value->type)
			{
				case VALUE_NULL:
					m_undefnull.fileName = IS_NULL; m_fileName.Empty(); break;
				case VALUE_UNDEFINED:
					m_undefnull.fileName = IS_UNDEF; m_fileName.Empty(); break;
				case VALUE_STRING:
					PUT_FAILED_IF_ERROR(m_fileName.Set(value->value.string));
					m_undefnull.fileName = IS_VALUE;
					break;
				default:
					return PUT_NEEDS_STRING;
			}
			return PUT_SUCCESS;
		}
	}
	return PUT_FAILED;
}

/* virtual */
int DOM_JILAttachment_Constructor::Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime)
{
	DOM_JILAttachment* dom_attachment;
	CALL_FAILED_IF_ERROR(DOM_JILAttachment::Make(dom_attachment, static_cast<DOM_Runtime*>(origining_runtime)));
	DOMSetObject(return_value, dom_attachment);
	return ES_VALUE;
}

DOM_JILAttachment_Array::DOM_JILAttachment_Array()
	: m_length(0)
{}

/* static */ OP_STATUS
DOM_JILAttachment_Array::Make(DOM_JILAttachment_Array*& new_obj, DOM_Runtime* origining_runtime, const OpAutoVector<OpMessaging::Attachment>* to_add)
{
	new_obj = OP_NEW(DOM_JILAttachment_Array, ());
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_obj, origining_runtime, origining_runtime->GetArrayPrototype(), "AttachmentArray"));

	if (to_add)
	{
		unsigned int i_len;
		for (i_len = 0; i_len < to_add->GetCount(); i_len++)
		{
			OpMessaging::Attachment* op_attachment = to_add->Get(i_len);
			DOM_JILAttachment* dom_attachment;
			RETURN_IF_ERROR(DOM_JILAttachment::Make(dom_attachment, origining_runtime, op_attachment));
			OpAutoPtr<DOM_JIL_Array_Element> ap_attachment_val(OP_NEW(DOM_JIL_Array_Element, ()));
			RETURN_OOM_IF_NULL(ap_attachment_val.get());
			DOMSetObject(&ap_attachment_val->m_val, dom_attachment);
			ap_attachment_val->m_index = i_len;
			RETURN_IF_ERROR(new_obj->m_array.Add(ap_attachment_val.get()));
			ap_attachment_val.release();
		}
		new_obj->m_length = i_len;
	}
	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_JILAttachment_Array::Make(DOM_JILAttachment_Array*& new_obj, DOM_Runtime* origining_runtime, ES_Object* from_object, int length)
{
	RETURN_IF_ERROR(DOM_JILAttachment_Array::Make(new_obj, origining_runtime));
	for (int i = 0; i < length; i++)
	{
		ES_Value i_val;
		if (origining_runtime->GetIndex(from_object, i, &i_val) != OpBoolean::IS_TRUE || i_val.type != VALUE_OBJECT)
			return OpStatus::ERR;
		DOM_HOSTOBJECT_SAFE(dom_attachment, i_val.value.object, DOM_TYPE_JIL_ATTACHMENT, DOM_JILAttachment);
		if (!dom_attachment)
			return OpStatus::ERR;
		if (new_obj->PutIndex(i, &i_val, origining_runtime) != PUT_SUCCESS)
			return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_JILAttachment_Array::GetName(OpAtom property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_code == OP_ATOM_length)
	{
		DOMSetNumber(value, m_length);
		return GET_SUCCESS;
	}
	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_JILAttachment_Array::PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_code == OP_ATOM_length)
	{
		if (value->type != VALUE_NUMBER)
			return PUT_NEEDS_NUMBER;
		if (op_isintegral(value->value.number) && value->value.number >= 0 && value->value.number <= INT_MAX - 1)
			m_length = static_cast<int>(value->value.number);
		return PUT_SUCCESS;
	}
	return PUT_FAILED;
}

/* virtual */ ES_GetState
DOM_JILAttachment_Array::GetIndex(int property_index, ES_Value* value, ES_Runtime* origining_runtime)
{
	int vector_index = FindIndex(property_index);
	if (vector_index == -1)
		return GET_FAILED;
	ES_Value& elem = m_array.Get(vector_index)->m_val;
	if (elem.type == VALUE_OBJECT)
		DOMSetObject(value, elem.value.object);
	else if (elem.type == VALUE_NULL)
		DOMSetNull(value);
	else if (elem.type == VALUE_UNDEFINED)
		DOMSetUndefined(value);
	else
		OP_ASSERT(!"No other type should be in this typed array.");
	return GET_SUCCESS;
}

int DOM_JILAttachment_Array::FindIndex(int to_find)
{
	for (UINT32 i = 0; i < m_array.GetCount();  i++)
		if (m_array.Get(i)->m_index == to_find)
			return i;
	return -1;
}

/* virtual */ ES_PutState
DOM_JILAttachment_Array::PutIndex(int property_index, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (value->type != VALUE_OBJECT && value->type != VALUE_UNDEFINED && value->type != VALUE_NULL)
		return PUT_SUCCESS;
	// "INT_MAX - 1", because of OpINT32Vector::Find(0) bug I have to add 1 to every index put into m_indices.
	if (property_index < 0 || property_index > INT_MAX - 1)
		return PUT_SUCCESS;
	OpAutoPtr<DOM_JIL_Array_Element> to_insert(OP_NEW(DOM_JIL_Array_Element, ()));
	if (!to_insert.get())
		return PUT_NO_MEMORY;

	if (value->type == VALUE_OBJECT)
	{
		DOM_HOSTOBJECT_SAFE(attachment_to_add, value->value.object, DOM_TYPE_JIL_ATTACHMENT, DOM_JILAttachment);
		RETURN_VALUE_IF_NULL(attachment_to_add, PUT_SUCCESS);
		DOMSetObject(&to_insert->m_val, attachment_to_add);
	}
	else if (value->type == VALUE_NULL)
		DOMSetNull(&to_insert->m_val);
	else
		DOMSetUndefined(&to_insert->m_val);

	DOM_JIL_Array_Element* to_remove = NULL;
	int insertion_place = FindIndex(property_index);
	if (insertion_place == -1)
	{
		insertion_place = 0;
		int end = m_array.GetCount();
		while (end > insertion_place)
		{
			int step = (end - insertion_place) / 2;
			if (m_array.Get(insertion_place + step)->m_index < property_index)
				insertion_place += step + 1;
			else
				end = insertion_place + step;
		}
		if (m_array.GetCount())
			insertion_place++;
		to_insert->m_index = property_index;
	}
	else
		to_remove = m_array.Remove(insertion_place);
	to_insert->m_index = property_index;

	PUT_FAILED_IF_ERROR(m_array.Insert(insertion_place, to_insert.get()));
	to_insert.release();
	OP_DELETE(to_remove);

	if (property_index + 1 > m_length)
		m_length = property_index + 1;
	return PUT_SUCCESS;
}

BOOL
DOM_JILAttachment_Array::GetIndex(UINT32 index, ES_Value*& result)
{
	if (index >= m_array.GetCount())
		return FALSE;
	result = &m_array.Get(index)->m_val;
	return TRUE;
}

OP_STATUS
DOM_JILAttachment_Array::GetCopy(DOM_JILAttachment_Array*& to_array, DOM_Runtime* origining_runtime)
{
	RETURN_IF_ERROR(DOM_JILAttachment_Array::Make(to_array, origining_runtime));
	ES_Value* i_val;
	for (UINT32 i = 0; GetIndex(i, i_val); i++)
		if (to_array->PutIndex(i, i_val, origining_runtime) != PUT_SUCCESS)
			return OpStatus::ERR_NO_MEMORY;
	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_JILAttachment_Array::FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime)
{
	for (unsigned i = 0; i < m_array.GetCount(); i++)
		enumerator->AddPropertiesL(m_array.Get(i)->m_index, 1);
	return GET_SUCCESS;
}

/* virtual */ ES_GetState
DOM_JILAttachment_Array::GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime)
{
	unsigned max_index = 0;
	for (unsigned i = 0; i < m_array.GetCount(); i++)
	{
		unsigned index = m_array.Get(i)->m_index;
		max_index = MAX(index, max_index);
	}
	count = m_array.GetCount() ? max_index + 1 : 0;
	return GET_SUCCESS;
}

/* virtual */
void DOM_JILAttachment_Array::GCTrace()
{
	for (unsigned i = 0; i < m_array.GetCount(); i++)
		GCMark(m_array.Get(i)->m_val);
}

void
DOM_JILAttachment_Array::Empty()
{
	m_length = 0;
	m_array.DeleteAll();
}

OP_STATUS
DOM_JILAttachment_Array::Remove(const DOM_JILAttachment& to_remove)
{
	int shift_by = 0;
	BOOL was_removed = FALSE;
	for (UINT32 i_len = 0; i_len < m_array.GetCount(); i_len++)
	{
		ES_Value& in_array_val = m_array.Get(i_len)->m_val;
		if (in_array_val.type == VALUE_OBJECT)
		{
			DOM_HOSTOBJECT_SAFE(in_array, in_array_val.value.object, DOM_TYPE_JIL_ATTACHMENT, DOM_JILAttachment);
			if (in_array && *in_array == to_remove)
			{
				DOM_JIL_Array_Element* removed = m_array.Remove(i_len);
				OP_DELETE(removed);
				m_length--;
				shift_by++;
				i_len--;
				was_removed = TRUE;
			}
			else if (shift_by)
				m_array.Get(i_len)->m_index -= shift_by;
		}
		else if (shift_by)
			m_array.Get(i_len)->m_index -= shift_by;
	}
	return was_removed ? OpStatus::OK : OpStatus::ERR_NO_SUCH_RESOURCE;
}

OP_STATUS
DOM_JILAttachment::GetCopy(OpMessaging::Attachment*& new_attachment)
{
	new_attachment = OP_NEW(OpMessaging::Attachment, ());
	RETURN_OOM_IF_NULL(new_attachment);
	OpAutoPtr<OpMessaging::Attachment> ap_new_attachment(new_attachment);

	new_attachment->m_inside_message = m_inside_message;
	new_attachment->m_size = m_size;

	RETURN_IF_ERROR(new_attachment->m_full_filename.Set(m_full_filename));
	RETURN_IF_ERROR(new_attachment->m_id.Set(m_id));
	RETURN_IF_ERROR(new_attachment->m_mimetype.Set(m_MIMEType));
	RETURN_IF_ERROR(new_attachment->m_suggested_name.Set(m_fileName));
	ap_new_attachment.release();
	return OpStatus::OK;
}

#endif // DOM_JIL_API_SUPPORT
