/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef DROPZONE_ATTRIBUTE_H
#define DROPZONE_ATTRIBUTE_H

#ifdef DRAG_SUPPORT
#include "modules/logdoc/complex_attr.h"

/**
 * This class is used to represent dropzone attribute.
 *
 * It stores attribute value in two data representations: string and structure containing operation type enum
 * and accepted data types.
 **/
class DropzoneAttribute : public ComplexAttr
{
public:
	enum OperationType
	{
		OPERATION_COPY,
		OPERATION_MOVE,
		OPERATION_LINK
	};

	/** The structure describing data acceptable by a dropzone. */
	struct AcceptedData
	{
	public:
		/** Enumerates all kinds of data which might be accepted by the dropzone.
		 * The d'n'd specification currently defines only 'string' and 'file' data kind as valid. */
		enum
		{
			/** The data kind is unknown i.e. it's neither 'string' nor 'file'. */
			DATA_KIND_UNKNOWN,
			/** The data kind is 'string'. */
			DATA_KIND_FILE,
			/** The data kind is 'file'. */
			DATA_KIND_STRING
		} m_data_kind;

		/** A mime type of the data. */
		OpString m_data_type;
	};

	virtual ~DropzoneAttribute() {}

	static DropzoneAttribute* Create(const uni_char* attr_str, size_t str_len);

	// Implements ComplexAttr.
	virtual OP_STATUS CreateCopy(ComplexAttr **copy_to);
	virtual OP_STATUS ToString(TempBuffer *buffer) { return buffer->Append(m_string.CStr()); }
	virtual BOOL Equals(ComplexAttr* other) { OP_ASSERT(other); return uni_strcmp(GetAttributeString(), static_cast<DropzoneAttribute*>(other)->GetAttributeString()) == 0; }

	/** 
	 * Fully set value of attribute.
	 *
	 * @param attr_str Non-null string which has to be set on attribute.  Will
	 * be split on whitespaces.
	 *
	 * @return OpStatus::OK or OOM.
	 */
	OP_STATUS Set(const uni_char* attr_str, size_t str_len);

	/** Get attribute literal string value. */
	const uni_char* GetAttributeString() const { return m_string.CStr(); }

	/** Get attribute literal string value. */
	OperationType GetOperation() const { return m_operation; }

	/** Gets accepted data types */
	const OpVector<AcceptedData>& GetAcceptedData() const { return m_list; }

private:
	DropzoneAttribute() : m_operation(OPERATION_COPY)
	{}

	OpAutoVector<AcceptedData> m_list; //< A vector of accepted data containing data type and mime.
	OperationType m_operation; //< Allowed operation.
	OpString m_string; //< Attribute string, includes whitespaces.
};

#endif // DRAG_SUPPORT
#endif // DROPZONE_ATTRIBUTE_H

