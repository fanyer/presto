/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/


#include "core/pch.h"
#include "modules/logdoc/dropzone_attribute.h"

#ifdef DRAG_SUPPORT
#define WHITESPACE_L UNI_L(" \t\n\f\r")

OP_STATUS
DropzoneAttribute::Set(const uni_char* attr_str_in, size_t length)
{
	OP_ASSERT(attr_str_in);
	RETURN_IF_ERROR(m_string.Set(attr_str_in, length));

	m_list.DeleteAll();

	const uni_char* attr_str = m_string.CStr();
	BOOL operation_specified = FALSE;
	while (*attr_str)
	{
		// Skip whitespace.
		attr_str += uni_strspn(attr_str, WHITESPACE_L);
		// Find size of this token.
		size_t token_len = uni_strcspn(attr_str, WHITESPACE_L);
		if (token_len >= 3)
		{
			if (!operation_specified && token_len == 4 && uni_strnicmp(attr_str, "copy", 4) == 0)
			{
				operation_specified = TRUE;
				m_operation = OPERATION_COPY;
			}
			else if (!operation_specified && token_len == 4 && uni_strnicmp(attr_str, "move", 4) == 0)
			{
				operation_specified = TRUE;
				m_operation = OPERATION_MOVE;
			}
			else if (!operation_specified && token_len == 4 && uni_strnicmp(attr_str, "link", 4) == 0)
			{
				operation_specified = TRUE;
				m_operation = OPERATION_LINK;
			}
			else if (uni_char *colon = uni_strchr(attr_str, ':'))
			{
				const uni_char* type = attr_str;
				unsigned type_len = colon - attr_str;
				if (type_len != 0 && type_len != token_len - 1) // ':' must not be the first nor last character in the token.
				{
					AcceptedData data_kind;
					data_kind.m_data_kind = AcceptedData::DATA_KIND_UNKNOWN;
					if (((type[0] == 's' || type[0] == 'S') && type_len == 1) || uni_strni_eq(type, "string", type_len))
						data_kind.m_data_kind = AcceptedData::DATA_KIND_STRING;
					else if (((type[0] == 'f' || type[0] == 'F') && type_len == 1) || uni_strni_eq(type, "file", type_len))
						data_kind.m_data_kind = AcceptedData::DATA_KIND_FILE;

					if (data_kind.m_data_kind != AcceptedData::DATA_KIND_UNKNOWN) // Ignore unknown data kinds.
					{
						AcceptedData* new_data = OP_NEW(AcceptedData, ());
						RETURN_OOM_IF_NULL(new_data);
						new_data->m_data_kind = data_kind.m_data_kind;
						OpAutoPtr<AcceptedData> op_data(new_data);
						RETURN_IF_ERROR(new_data->m_data_type.Set(&colon[1], token_len - type_len - 1));
						new_data->m_data_type.MakeLower();
						RETURN_IF_ERROR(m_list.Add(new_data));
						op_data.release();
					}
				}
			}
		}

		// Skip this token.
		attr_str += token_len;
		// Continue populating collection from the next token.
	}

	return OpStatus::OK;
}

/* virtual */ OP_STATUS
DropzoneAttribute::CreateCopy(ComplexAttr **copy_to)
{
	OpAutoPtr<DropzoneAttribute> ap_copy(OP_NEW(DropzoneAttribute, ()));
	RETURN_OOM_IF_NULL(ap_copy.get());

	RETURN_IF_ERROR(ap_copy->m_string.Set(m_string));
	ap_copy->m_operation = m_operation;
	for (UINT32 i_data = 0; i_data < m_list.GetCount(); i_data++)
	{
		OpAutoPtr<DropzoneAttribute::AcceptedData> ap_data(OP_NEW(DropzoneAttribute::AcceptedData, ()));
		RETURN_OOM_IF_NULL(ap_data.get());
		ap_data->m_data_kind = m_list.Get(i_data)->m_data_kind;
		RETURN_IF_ERROR(ap_data->m_data_type.Set(m_list.Get(i_data)->m_data_type.CStr()));
		RETURN_IF_ERROR(ap_copy->m_list.Add(ap_data.get()));
		ap_data.release();
	}

	*copy_to = ap_copy.release();
	return OpStatus::OK;
}

/* static */ DropzoneAttribute*
DropzoneAttribute::Create(const uni_char* attr_str, size_t str_len)
{
	if (!attr_str)
	{
		attr_str = UNI_L("");
		str_len = 0;
	}

	// Create complex attribute.
	DropzoneAttribute* stl = OP_NEW(DropzoneAttribute, ());
	if (!stl || OpStatus::IsError(stl->Set(attr_str, str_len)))
	{
		OP_DELETE(stl);
		return NULL;
	}
	return stl;
}
#endif // DRAG_SUPPORT
