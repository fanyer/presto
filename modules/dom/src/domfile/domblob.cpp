/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/dom/src/domfile/domblob.h"
#include "modules/dom/src/domglobaldata.h"
#include "modules/dom/src/domfile/domfile.h"

/* static */ OP_STATUS
DOM_Blob::Make(DOM_Blob *&blob, ES_Object *array_buffer, BOOL is_array_view, DOM_Runtime *runtime)
{
	DOM_BlobBuffer *blob_buffer;
	RETURN_IF_ERROR(DOM_BlobBuffer::Make(blob_buffer, array_buffer, is_array_view, runtime));
	blob = blob_buffer;
	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_Blob::Make(DOM_Blob *&blob, unsigned char *buffer, unsigned length, DOM_Runtime *runtime)
{
	DOM_BlobBuffer *blob_buffer;
	RETURN_IF_ERROR(DOM_BlobBuffer::Make(blob_buffer, buffer, length, runtime));
	blob = blob_buffer;
	return OpStatus::OK;
}

/* static */ int
DOM_Blob::slice(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(blob, DOM_TYPE_BLOB, DOM_Blob);
	DOM_CHECK_ARGUMENTS("|NNs");
	double size = blob->GetSize();

	double start;
	if (argc == 0)
		start = 0.;
	else if (!ConvertDoubleToLongLong(argv[0].value.number, ModuloRange, start))
		return blob->CallNativeException(ES_Native_TypeError, UNI_L("long long argument expected"), return_value);

	if (start < 0)
		start = MAX(size + start, 0);
	else
		start = MIN(start, size);

	double end;
	if (argc <= 1)
		end = size;
	else if (!ConvertDoubleToLongLong(argv[1].value.number, ModuloRange, end))
		return blob->CallNativeException(ES_Native_TypeError, UNI_L("long long argument expected"), return_value);

	if (end < 0)
		end = MAX(size + end, 0);
	else
		end = MIN(end, size);

	DOM_Blob *sliced_blob;
	OP_STATUS status = blob->MakeSlice(sliced_blob, start, end);
	if (OpStatus::IsMemoryError(status))
		return ES_NO_MEMORY;
	else if (OpStatus::IsSuccess(status) && argc > 2)
	{
		status = sliced_blob->ForceContentType(argv[2].value.string);
		if (OpStatus::IsMemoryError(status))
			return ES_NO_MEMORY;
	}

	if (status == OpStatus::ERR)
		return DOM_CALL_INTERNALEXCEPTION(WRONG_ARGUMENTS_ERR);

	if (blob->GetForcedContentType())
		CALL_FAILED_IF_ERROR(sliced_blob->ForceContentType(blob->GetForcedContentType()));

	DOMSetObject(return_value, sliced_blob);
	return ES_VALUE;
}

OP_STATUS
DOM_Blob::ForceContentType(const char *force_content_type)
{
	OP_ASSERT(force_content_type);
	OP_ASSERT(!m_forced_content_type);

	RETURN_IF_ERROR(m_forced_content_type.SetFromUTF8(force_content_type));

	return OpStatus::OK;
}

OP_STATUS
DOM_Blob::ForceContentType(const uni_char *force_content_type)
{
	OP_ASSERT(force_content_type);

	RETURN_IF_ERROR(m_forced_content_type.Set(force_content_type));

	return OpStatus::OK;
}

/* virtual */ const uni_char *
DOM_Blob::GetContentType()
{
	return GetForcedContentType();
}

/* virtual */ ES_GetState
DOM_Blob::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_size:
		// The size in bytes.
		if (value)
			DOMSetNumber(value, GetSize());
		return GET_SUCCESS;

	case OP_ATOM_type:
		// The content type if known, or the empty string otherwise.
		if (value)
		{
			TempBuffer* buf = GetEmptyTempBuf();
			GET_FAILED_IF_ERROR(buf->Append(GetContentType()));
			DOMSetString(value, buf->GetStorage());
		}
		return GET_SUCCESS;

	}
	return DOM_Object::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_Blob::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_size:
	case OP_ATOM_type:
		return PUT_READ_ONLY;
	}

	return DOM_Object::PutName(property_name, value, origining_runtime);
}

/* static */ OP_STATUS
DOM_Blob::Clone(DOM_Blob *source_object, ES_Runtime *target_runtime, DOM_Blob *&target_object)
{
	return source_object->MakeClone(target_runtime, target_object);
}

#ifdef ES_PERSISTENT_SUPPORT
/* static */ OP_STATUS
DOM_Blob::Clone(DOM_Blob *source_object, ES_PersistentItem *&target_item)
{
	return source_object->MakeClone(target_item);
}

/* static */ OP_STATUS
DOM_Blob::Clone(ES_PersistentItem *&source_item0, ES_Runtime *target_runtime, DOM_Blob *&target_object)
{
	DOM_Blob_PersistentItem *blob_item = static_cast<DOM_Blob_PersistentItem *>(source_item0);
	switch (blob_item->blob_type)
	{
	case BlobFile:
		return DOM_File::MakeClone(source_item0, target_runtime, target_object);
	case BlobString:
		return DOM_BlobString::MakeClone(source_item0, target_runtime, target_object);
	case BlobBuffer:
		return DOM_BlobBuffer::MakeClone(source_item0, target_runtime, target_object);
	case BlobSlice:
		return DOM_BlobSlice::MakeClone(source_item0, target_runtime, target_object);
	case BlobSequence:
		return DOM_BlobSequence::MakeClone(source_item0, target_runtime, target_object);
	default:
		OP_ASSERT(!"Missing case for unknown Blob type");
		return OpStatus::ERR;
	}
}
#endif // ES_PERSISTENT_SUPPORT

/* virtual */ OP_STATUS
DOM_Blob::MakeClone(ES_Runtime *, DOM_Blob *&)
{
	OP_ASSERT(!"Blob subtype is missing a MakeClone() override.");
	return OpStatus::ERR;
}

#ifdef ES_PERSISTENT_SUPPORT
/* virtual */ OP_STATUS
DOM_Blob::MakeClone(ES_PersistentItem *&)
{
	OP_ASSERT(!"Blob subtype is missing a MakeClone() override.");
	return OpStatus::ERR;
}
#endif // ES_PERSISTENT_SUPPORT

/* ========== DOM_BlobSlice ========== */

/* static */ OP_STATUS
DOM_BlobSlice::Make(DOM_BlobSlice *&blob, DOM_Blob *sliced_from, double slice_start, double slice_end, DOM_Runtime *runtime)
{
	OP_ASSERT(slice_start >= 0. && slice_end >= 0.);
	if (sliced_from->GetType() == BlobSlice)
	{
		DOM_BlobSlice *inner_slice = static_cast<DOM_BlobSlice *>(sliced_from);
		slice_start += inner_slice->GetSliceStart();
		OP_ASSERT(inner_slice->GetSliceLength() != FILE_LENGTH_NONE);
		slice_end = inner_slice->GetSliceStart() + MIN(slice_end, static_cast<double>(inner_slice->GetSliceLength()));
		sliced_from = inner_slice->sliced_from;
		/* By construction, there should be no nested blob slices.. */
		OP_ASSERT(sliced_from && sliced_from->GetType() != BlobSlice);
	}

	DOM_BlobSlice *sliced_blob;
	RETURN_IF_ERROR(DOMSetObjectRuntime(sliced_blob = OP_NEW(DOM_BlobSlice, (sliced_from)), runtime, runtime->GetPrototype(DOM_Runtime::BLOB_PROTOTYPE), "Blob"));

	double slice_length = MAX(slice_end - slice_start, 0);
	sliced_blob->m_sliced_start = slice_start;
	if (stdlib_intpart(sliced_blob->m_sliced_start) != sliced_blob->m_sliced_start) // Overflow in the addition.
		return OpStatus::ERR;

	if (sliced_from->GetSliceLength() != FILE_LENGTH_NONE && slice_start >= sliced_from->GetSliceLength())
		sliced_blob->m_sliced_length = 0;
	else if (sliced_from->GetSliceLength() == FILE_LENGTH_NONE)
		sliced_blob->m_sliced_length = slice_length;
	else
	{
		double orig_available = sliced_from->GetSliceLength() - slice_start;
		sliced_blob->m_sliced_length = MIN(orig_available, slice_length);
	}

	blob = sliced_blob;
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
DOM_BlobSlice::MakeSlice(DOM_Blob *&sliced_result, double slice_start, double slice_end)
{
	return sliced_from->MakeSlice(sliced_result, m_sliced_start + slice_start, MIN(m_sliced_start + slice_end, m_sliced_start + m_sliced_length));
}

/* virtual */ OP_STATUS
DOM_BlobSlice::MakeReadView(DOM_BlobReadView *&blob_view, OpFileLength start_position, OpFileLength view_length)
{
	/* start_position is relative to this slice. */
	OpFileLength length = GetSliceLength();

	/* If the slice isn't to the end, then compute the effective view
	   length on the underlying Blob by subtracting the starting position. */
	if (length != FILE_LENGTH_NONE)
		if (start_position > length)
			length = 0;
		else
			length = length - start_position;

	start_position = start_position + static_cast<OpFileLength>(m_sliced_start);
	view_length = view_length != FILE_LENGTH_NONE ? (length != FILE_LENGTH_NONE ? MIN(length, view_length) : view_length) : length;
	return sliced_from->MakeReadView(blob_view, start_position, view_length);
}

/* virtual */ double
DOM_BlobSlice::GetSize()
{
	return m_sliced_length;
}

/* virtual */ const uni_char *
DOM_BlobSlice::GetContentType()
{
	if (GetForcedContentType())
		return GetForcedContentType();

	return sliced_from->GetContentType();
}

/* virtual */ void
DOM_BlobSlice::GCTrace()
{
	GCMark(sliced_from);
}

/* virtual */ OP_STATUS
DOM_BlobSlice::MakeClone(ES_Runtime *target_runtime, DOM_Blob *&target_object)
{
	DOM_BlobSlice *clone;
	RETURN_IF_ERROR(DOM_BlobSlice::Make(clone, sliced_from, m_sliced_start, m_sliced_start + m_sliced_length, static_cast<DOM_Runtime *>(target_runtime)));

	target_object = clone;
	return OpStatus::OK;
}

#ifdef ES_PERSISTENT_SUPPORT
class DOM_BlobSlice_PersistentItem
	: public DOM_Blob_PersistentItem
{
public:
	DOM_BlobSlice_PersistentItem()
		: DOM_Blob_PersistentItem(DOM_Blob::BlobSlice)
		, sliced_item(NULL)
		, sliced_start(0)
		, sliced_length(-1.0)
	{
	}

	virtual ~DOM_BlobSlice_PersistentItem()
	{
		OP_DELETE(sliced_item);
	}

	ES_PersistentItem *sliced_item;
	double sliced_start;
	double sliced_length;
};

/* virtual */ OP_STATUS
DOM_BlobSlice::MakeClone(ES_PersistentItem *&target_item)
{
	DOM_BlobSlice_PersistentItem *item = OP_NEW(DOM_BlobSlice_PersistentItem, ());
	RETURN_OOM_IF_NULL(item);
	OpAutoPtr<DOM_BlobSlice_PersistentItem> anchor_item(item);
	ES_PersistentItem *sliced_item = NULL;
	RETURN_IF_ERROR(sliced_from->MakeClone(sliced_item));
	item->sliced_item = sliced_item;
	item->sliced_start = m_sliced_start;
	item->sliced_length = m_sliced_length;

	target_item = item;
	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_BlobSlice::MakeClone(ES_PersistentItem *&source_item0, ES_Runtime *target_runtime, DOM_Blob *&target_object)
{
	DOM_BlobSlice_PersistentItem *source_item = static_cast<DOM_BlobSlice_PersistentItem *>(source_item0);
	DOM_BlobSlice *clone;
	DOM_Blob *cloned_blob;
	RETURN_IF_ERROR(DOM_Blob::Clone(source_item->sliced_item, target_runtime, cloned_blob));
	RETURN_IF_ERROR(DOM_BlobSlice::Make(clone, cloned_blob, source_item->sliced_start, source_item->sliced_length, static_cast<DOM_Runtime *>(target_runtime)));

	target_object = clone;
	return OpStatus::OK;
}
#endif // ES_PERSISTENT_SUPPORT

BOOL
DOM_BlobSlice::IsSimpleSlice()
{
	return (sliced_from->GetType() == BlobString || sliced_from->GetType() == BlobBuffer);
}

const unsigned char *
DOM_BlobSlice::GetSliceBuffer()
{
	OP_ASSERT(IsSimpleSlice() || !"GetSliceBuffer() called on a non-simple slice.");
	switch (sliced_from->GetType())
	{
	case BlobString:
		return static_cast<DOM_BlobString *>(sliced_from)->GetString() + static_cast<unsigned>(m_sliced_start);
	case BlobBuffer:
		return static_cast<DOM_BlobString *>(sliced_from)->GetString() + static_cast<unsigned>(m_sliced_start);
	default:
		return NULL;
	}
}

/* ========== DOM_BlobBuffer ========== */

/* static */ OP_STATUS
DOM_BlobBuffer::Make(DOM_BlobBuffer *&blob_buffer, ES_Object *array_buffer, BOOL is_array_view, DOM_Runtime *runtime)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(blob_buffer = OP_NEW(DOM_BlobBuffer, (array_buffer, is_array_view)), runtime, runtime->GetPrototype(DOM_Runtime::BLOB_PROTOTYPE), "Blob"));
	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_BlobBuffer::Make(DOM_BlobBuffer *&blob_buffer, unsigned char *buffer, unsigned length, DOM_Runtime *runtime)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(blob_buffer = OP_NEW(DOM_BlobBuffer, (NULL, length)), runtime, runtime->GetPrototype(DOM_Runtime::BLOB_PROTOTYPE), "Blob"));
	blob_buffer->external_buffer = buffer;
	return OpStatus::OK;
}

/* virtual */
DOM_BlobBuffer::~DOM_BlobBuffer()
{
	op_free(external_buffer);
}

/* virtual */ void
DOM_BlobBuffer::GCTrace()
{
	GCMark(array_buffer);
}

/* virtual */ OP_STATUS
DOM_BlobBuffer::MakeSlice(DOM_Blob *&sliced_result, double slice_start, double slice_end)
{
	DOM_BlobSlice *blob_slice;
	RETURN_IF_ERROR(DOM_BlobSlice::Make(blob_slice, this, slice_start, slice_end, GetRuntime()));

	sliced_result = blob_slice;
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
DOM_BlobBuffer::MakeReadView(DOM_BlobReadView *&blob_view, OpFileLength start_position, OpFileLength view_length)
{
	OP_ASSERT(FILE_LENGTH_MAX < UINT_MAX || start_position < static_cast<OpFileLength>(UINT_MAX));
	OP_ASSERT(GetSize() >= 0 && GetSize() <= UINT_MAX);
	unsigned length = static_cast<unsigned>(GetSize());
	unsigned start_pos = MIN(length, static_cast<unsigned>(start_position));
	length = view_length != FILE_LENGTH_NONE ? MIN(length, static_cast<unsigned>(view_length)) : length;
	DOM_BlobBuffer::ReadView *read_view = OP_NEW(DOM_BlobBuffer::ReadView, (this, start_pos, length));
	RETURN_OOM_IF_NULL(read_view);

	blob_view = read_view;
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
DOM_BlobBuffer::ReadView::ReadAvailable(DOM_FileReader_Base *reader, const unsigned char *prefix_bytes, unsigned prefix_length, OpFileLength *bytes_read)
{
	if (blob_buffer->array_buffer)
	{
		unsigned to_read = length_remaining;
		const unsigned char *data;
		if (blob_buffer->is_array_view)
			data = static_cast<unsigned char *>(ES_Runtime::GetStaticTypedArrayStorage(blob_buffer->array_buffer));
		else
			data = ES_Runtime::GetArrayBufferStorage(blob_buffer->array_buffer);
		RETURN_IF_ERROR(reader->AppendToResult(prefix_bytes, prefix_length, data + static_cast<unsigned>(blob_buffer->m_sliced_start) + start_position, to_read));
		blob_buffer = NULL;
		length_remaining = 0;
		*bytes_read = to_read;
	}
	else if (blob_buffer->external_buffer)
	{
		unsigned to_read = length_remaining;
		RETURN_IF_ERROR(reader->AppendToResult(prefix_bytes, prefix_length, blob_buffer->external_buffer + static_cast<unsigned>(blob_buffer->m_sliced_start) + start_position, to_read));
		blob_buffer = NULL;
		length_remaining = 0;
		*bytes_read = to_read;
	}
	else
	{
		*bytes_read = 0;
	}
	return OpStatus::OK;
}

/* virtual */ void
DOM_BlobBuffer::ReadView::GCTrace()
{
	if (blob_buffer)
		blob_buffer->GCTrace();
}

/* virtual */ double
DOM_BlobBuffer::GetSize()
{
	OP_ASSERT(!IsSlice());
	if (!array_buffer)
		return external_length;
	else if (is_array_view)
		return ES_Runtime::GetStaticTypedArraySize(array_buffer);
	else
		return ES_Runtime::GetArrayBufferLength(array_buffer);
}

OP_STATUS
DOM_BlobBuffer::SetContentType(const uni_char *t)
{
	return content_type.Set(t);
}

/* virtual */ const uni_char *
DOM_BlobBuffer::GetContentType()
{
	if (GetForcedContentType())
		return GetForcedContentType();

	return content_type.CStr();
}

const unsigned char *
DOM_BlobBuffer::GetBuffer()
{
	if (!array_buffer)
		return external_buffer;
	else if (is_array_view)
		return static_cast<unsigned char *>(ES_Runtime::GetStaticTypedArrayStorage(array_buffer));
	else
		return ES_Runtime::GetArrayBufferStorage(array_buffer);
}

/* virtual */ OP_STATUS
DOM_BlobBuffer::MakeClone(ES_Runtime *target_runtime, DOM_Blob *&target_object)
{
	/* Cloning means copying. */
	DOM_BlobBuffer *clone;
	if (array_buffer)
	{
		ES_Object *copy;
		unsigned length = static_cast<unsigned>(GetSize());
		if (is_array_view)
		{
			ES_Object *buffer;
			unsigned buffer_length = ES_Runtime::GetStaticTypedArraySize(array_buffer);
			RETURN_IF_ERROR(target_runtime->CreateNativeArrayBufferObject(&buffer, buffer_length));
			op_memcpy(ES_Runtime::GetArrayBufferStorage(buffer), ES_Runtime::GetStaticTypedArrayStorage(array_buffer), buffer_length);
			RETURN_IF_ERROR(target_runtime->CreateNativeTypedArrayObject(&copy, ES_Runtime::GetStaticTypedArrayKind(array_buffer), buffer));
		}
		else
		{
			RETURN_IF_ERROR(target_runtime->CreateNativeArrayBufferObject(&copy, length));
			op_memcpy(ES_Runtime::GetArrayBufferStorage(copy), ES_Runtime::GetArrayBufferStorage(array_buffer), length);
		}
		RETURN_IF_ERROR(DOM_BlobBuffer::Make(clone, copy, is_array_view, static_cast<DOM_Runtime *>(target_runtime)));
	}
	else
	{
		OP_ASSERT(external_buffer);
		unsigned char *copy = static_cast<unsigned char *>(op_malloc(external_length));
		if (!copy)
			return OpStatus::ERR_NO_MEMORY;
		op_memcpy(copy, external_buffer, external_length);
		RETURN_IF_ERROR(DOM_BlobBuffer::Make(clone, copy, external_length, static_cast<DOM_Runtime *>(target_runtime)));
	}

	target_object = clone;
	return OpStatus::OK;
}

#ifdef ES_PERSISTENT_SUPPORT
class DOM_BlobBuffer_PersistentItem
	: public DOM_Blob_PersistentItem
{
public:
	DOM_BlobBuffer_PersistentItem()
		: DOM_Blob_PersistentItem(DOM_Blob::BlobBuffer)
		, buffer(NULL)
		, length(0)
	{
	}

	virtual ~DOM_BlobBuffer_PersistentItem()
	{
		op_free(buffer);
	}

	unsigned char *buffer;
	unsigned length;
};

/* virtual */ OP_STATUS
DOM_BlobBuffer::MakeClone(ES_PersistentItem *&target_item)
{
	DOM_BlobBuffer_PersistentItem *item = OP_NEW(DOM_BlobBuffer_PersistentItem, ());
	RETURN_OOM_IF_NULL(item);
	OpAutoPtr<DOM_BlobBuffer_PersistentItem> anchor_item(item);

	item->length = static_cast<unsigned>(GetSize());
	item->buffer = static_cast<unsigned char *>(op_malloc(item->length));
	if (!item->buffer)
		return OpStatus::ERR_NO_MEMORY;

	op_memcpy(item->buffer, GetBuffer(), item->length);

	target_item = item;
	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_BlobBuffer::MakeClone(ES_PersistentItem *&source_item0, ES_Runtime *target_runtime, DOM_Blob *&target_object)
{
	DOM_BlobBuffer_PersistentItem *source_item = static_cast<DOM_BlobBuffer_PersistentItem *>(source_item0);
	DOM_BlobBuffer *clone;
	RETURN_IF_ERROR(DOM_BlobBuffer::Make(clone, source_item->buffer, source_item->length, static_cast<DOM_Runtime *>(target_runtime)));

	/* Hand over ownership. */
	source_item->buffer = NULL;

	target_object = clone;
	return OpStatus::OK;
}
#endif // ES_PERSISTENT_SUPPORT

/* ========== DOM_BlobString ========== */

/* static */ OP_STATUS
DOM_BlobString::Make(DOM_BlobString *&blob, const uni_char *str, unsigned length, DOM_Runtime *runtime)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(blob = OP_NEW(DOM_BlobString, ()), runtime, runtime->GetPrototype(DOM_Runtime::BLOB_PROTOTYPE), "Blob"));
	RETURN_IF_ERROR(blob->SetString(str, length == UINT_MAX ? uni_strlen(str) : length));
	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_BlobString::Make(DOM_BlobString *&blob, const char *str, unsigned length, DOM_Runtime *runtime)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(blob = OP_NEW(DOM_BlobString, ()), runtime, runtime->GetPrototype(DOM_Runtime::BLOB_PROTOTYPE), "Blob"));

	length = length == UINT_MAX ? op_strlen(str) : length;
	if (!blob->string_value.Reserve(length + 1))
		return OpStatus::ERR_NO_MEMORY;

	char *buf = blob->string_value.CStr();
	op_memcpy(buf, str, length);
	buf[length] = 0;
	blob->string_length = length;
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
DOM_BlobString::MakeSlice(DOM_Blob *&sliced_result, double slice_start, double slice_end)
{
	DOM_BlobSlice *blob_slice;
	RETURN_IF_ERROR(DOM_BlobSlice::Make(blob_slice, this, slice_start, slice_end, GetRuntime()));

	sliced_result = blob_slice;
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
DOM_BlobString::MakeReadView(DOM_BlobReadView *&blob_view, OpFileLength start_position, OpFileLength view_length)
{
	OP_ASSERT(FILE_LENGTH_MAX < UINT_MAX || start_position < static_cast<OpFileLength>(UINT_MAX));
	OP_ASSERT(GetSize() >= 0 && GetSize() <= UINT_MAX);
	unsigned length = static_cast<unsigned>(GetSize());
	unsigned start_pos = MIN(length, static_cast<unsigned>(start_position));
	length = view_length != FILE_LENGTH_NONE ? MIN(length, static_cast<unsigned>(view_length)) : length;
	DOM_BlobString::ReadView *read_view = OP_NEW(DOM_BlobString::ReadView, (this, start_pos, length));
	RETURN_OOM_IF_NULL(read_view);

	blob_view = read_view;
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
DOM_BlobString::ReadView::ReadAvailable(DOM_FileReader_Base *reader, const unsigned char *prefix_bytes, unsigned prefix_length, OpFileLength *bytes_read)
{
	unsigned to_read = length_remaining;
	RETURN_IF_ERROR(reader->AppendToResult(prefix_bytes, prefix_length, blob_string->GetString() + start_position, to_read));

	length_remaining = 0;
	*bytes_read = to_read;
	return OpStatus::OK;
}

/* virtual */ void
DOM_BlobString::ReadView::GCTrace()
{
	if (blob_string)
		blob_string->GCTrace();
}

OP_STATUS
DOM_BlobString::SetString(const uni_char *str, unsigned length)
{
	/* Using OpString8 as the UTF-8 conversion conduit, but it doesn't handle
	   embedded NULs very well when later  asking for length. Hence, manually
	   compute the real length of the encoded string by skipping past the
	   number of NULs seen in the source 'str'. */
	unsigned count = 0;
	unsigned real_length = length == UINT_MAX ? uni_strlen(str) : length;
	for (unsigned i = 0; i < real_length; i++)
		if (!str[i])
			count++;

	RETURN_IF_ERROR(string_value.SetUTF8FromUTF16(str, real_length));
	if (count > 0)
	{
		count++;
		const char *str = string_value.CStr();
		while (count > 0)
			if (!*str++)
				count--;

		string_length = str - string_value.CStr() - 1;
	}
	else
		string_length = string_value.Length();

	return OpStatus::OK;
}

const unsigned char *
DOM_BlobString::GetString()
{
	return reinterpret_cast<unsigned char *>(string_value.CStr()) + static_cast<unsigned>(m_sliced_start);
}

/* virtual */ double
DOM_BlobString::GetSize()
{
	OP_ASSERT(!IsSlice());
	return string_length;
}

OP_STATUS
DOM_BlobString::SetContentType(const uni_char *t)
{
	return content_type.Set(t);
}

/* virtual */ const uni_char *
DOM_BlobString::GetContentType()
{
	if (GetForcedContentType())
		return GetForcedContentType();

	return content_type.CStr();
}

/* virtual */ OP_STATUS
DOM_BlobString::MakeClone(ES_Runtime *target_runtime, DOM_Blob *&target_object)
{
	DOM_BlobString *clone;
	RETURN_IF_ERROR(DOM_BlobString::Make(clone, string_value.CStr(), string_length, static_cast<DOM_Runtime *>(target_runtime)));

	target_object = clone;
	return OpStatus::OK;
}

#ifdef ES_PERSISTENT_SUPPORT
class DOM_BlobString_PersistentItem
	: public DOM_Blob_PersistentItem
{
public:
	DOM_BlobString_PersistentItem()
		: DOM_Blob_PersistentItem(DOM_Blob::BlobString)
		, string_value(NULL)
		, string_length(0)
	{
	}

	virtual ~DOM_BlobString_PersistentItem()
	{
		OP_DELETEA(string_value);
	}

	char *string_value;
	unsigned string_length;
};

/* virtual */ OP_STATUS
DOM_BlobString::MakeClone(ES_PersistentItem *&target_item)
{
	DOM_BlobString_PersistentItem *item = OP_NEW(DOM_BlobString_PersistentItem, ());
	if (!item)
		return OpStatus::ERR_NO_MEMORY;
	item->string_value = OP_NEWA(char, string_length);
	if (!item->string_value)
	{
		OP_DELETE(item);
		return OpStatus::ERR_NO_MEMORY;
	}
	op_memcpy(item->string_value, string_value.CStr(), string_length);
	item->string_length = string_length;

	target_item = item;
	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_BlobString::MakeClone(ES_PersistentItem *&source_item0, ES_Runtime *target_runtime, DOM_Blob *&target_object)
{
	DOM_BlobString_PersistentItem *source_item = static_cast<DOM_BlobString_PersistentItem *>(source_item0);
	DOM_BlobString *clone;
	RETURN_IF_ERROR(DOM_BlobString::Make(clone, source_item->string_value, source_item->string_length, static_cast<DOM_Runtime *>(target_runtime)));

	target_object = clone;
	return OpStatus::OK;
}
#endif // ES_PERSISTENT_SUPPORT

/* ========== DOM_BlobSequence ========== */

/* static */ OP_STATUS
DOM_BlobSequence::Make(DOM_BlobSequence *&blob_sequence, DOM_Runtime *runtime)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(blob_sequence = OP_NEW(DOM_BlobSequence, ()), runtime, runtime->GetPrototype(DOM_Runtime::BLOB_PROTOTYPE), "Blob"));
	return OpStatus::OK;
}

OP_STATUS
DOM_BlobSequence::AppendSequence(DOM_BlobSequence *from)
{
	for (unsigned i = 0; i < from->parts.GetCount(); i++)
	{
		DOM_Blob *blob = from->parts.Get(i);
		if (blob->GetType() == BlobSequence)
		{
			DOM_BlobSequence *inner_sequence = static_cast<DOM_BlobSequence *>(blob);
			RETURN_IF_ERROR(AppendSequence(inner_sequence));
		}
		else
			RETURN_IF_ERROR(Append(blob));
	}

	return OpStatus::OK;
}

/* virtual */ OP_STATUS
DOM_BlobSequence::MakeSlice(DOM_Blob *&sliced_result, double slice_start, double slice_end)
{
	/* Compute what sub blobs this touches & use those only. */
	unsigned start_blob_index = UINT_MAX;
	unsigned end_blob_index = UINT_MAX;

	double start_pos = slice_start;
	double slice_length = MAX(0., slice_end - slice_start);
	for (unsigned i = 0; i < parts.GetCount() && slice_length > 0; i++)
	{
		DOM_Blob *blob = parts.Get(i);
		if (start_blob_index == UINT_MAX)
		{
			if (blob->GetSize() > start_pos)
			{
				start_blob_index = end_blob_index = i;
				slice_length -= MIN(slice_length, blob->GetSize() - start_pos);
			}
			else
			{
				start_pos -= blob->GetSize();
				slice_end -= blob->GetSize();
			}
		}
		else
		{
			end_blob_index = i;
			if (blob->GetSize() > slice_length)
				break;
			else
				slice_length -= blob->GetSize();
		}
	}

	DOM_Blob *blob_to_slice;
	if (start_blob_index == UINT_MAX && end_blob_index == UINT_MAX)
	{
		/* The slice is beyond the end of this Blob; return an empty one. */
		DOM_BlobString *empty;
		RETURN_IF_ERROR(DOM_BlobString::Make(empty, UNI_L(""), 0, GetRuntime()));

		sliced_result = empty;
		return OpStatus::OK;
	}
	else if (start_blob_index == end_blob_index)
	{
		slice_start = start_pos;
		blob_to_slice = parts.Get(start_blob_index);
	}
	else
	{
		slice_start = start_pos;

		DOM_BlobSequence *blob_sequence;
		RETURN_IF_ERROR(DOMSetObjectRuntime(blob_sequence = OP_NEW(DOM_BlobSequence, ()), GetRuntime(), GetRuntime()->GetPrototype(DOM_Runtime::BLOB_PROTOTYPE), "Blob"));

		for (unsigned i = start_blob_index; i <= end_blob_index; i++)
		{
			DOM_Blob *blob = parts.Get(i);
			/* Flatten nested sequences. */
			if (blob->GetType() == BlobSequence)
			{
				DOM_BlobSequence *inner_sequence = static_cast<DOM_BlobSequence *>(blob);
				RETURN_IF_ERROR(blob_sequence->AppendSequence(inner_sequence));
			}
			else
				RETURN_IF_ERROR(blob_sequence->Append(blob));
		}
		blob_to_slice = blob_sequence;
	}

	DOM_BlobSlice *blob_slice;
	RETURN_IF_ERROR(DOM_BlobSlice::Make(blob_slice, blob_to_slice, slice_start, slice_end, GetRuntime()));

	sliced_result = blob_slice;
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
DOM_BlobSequence::MakeReadView(DOM_BlobReadView *&blob_view, OpFileLength start_position, OpFileLength view_length)
{
	OpFileLength length = static_cast<OpFileLength>(GetSize());
	start_position = MIN(length, start_position);
	length = view_length != FILE_LENGTH_NONE ? MIN(length, view_length) : length;

	DOM_BlobSequence::ReadView *read_view = OP_NEW(DOM_BlobSequence::ReadView, (this, start_position, length));
	RETURN_OOM_IF_NULL(read_view);

	blob_view = read_view;
	return OpStatus::OK;
}

OP_STATUS
DOM_BlobSequence::SetContentType(const uni_char *t)
{
	return content_type.Set(t);
}

/* virtual */ const uni_char *
DOM_BlobSequence::GetContentType()
{
	if (GetForcedContentType())
		return GetForcedContentType();

	return content_type.CStr();
}

OP_STATUS
DOM_BlobSequence::Append(DOM_Blob *blob)
{
	if (blob->GetType() == BlobSequence)
		return AppendSequence(static_cast<DOM_BlobSequence *>(blob));
	else
		return parts.Add(blob);
}

/* virtual */ void
DOM_BlobSequence::GCTrace()
{
	for (unsigned i = 0; i < parts.GetCount(); i++)
		GCMark(parts.Get(i));
}

/* virtual */ double
DOM_BlobSequence::GetSize()
{
	OP_ASSERT(!IsSlice());

	double size = 0.;
	for (unsigned i = 0; i < parts.GetCount(); i++)
		size += parts.Get(i)->GetSize();

	return size;
}

/* virtual */
DOM_BlobSequence::ReadView::~ReadView()
{
	OP_DELETE(current_blob_view);
}

/* virtual */ OP_STATUS
DOM_BlobSequence::ReadView::ReadAvailable(DOM_FileReader_Base *reader, const unsigned char *prefix_bytes, unsigned prefix_length, OpFileLength *bytes_read)
{
	if (current_blob_view)
	{
		RETURN_IF_ERROR(current_blob_view->ReadAvailable(reader, prefix_bytes, prefix_length, bytes_read));
		length_remaining -= *bytes_read;
		if (current_blob_view->AtEOF())
		{
			OP_DELETE(current_blob_view);
			current_blob_view = NULL;
			current_blob_index++;
		}
		return OpStatus::OK;
	}

	/* Compute the set of non-File blobs we can consume from. */
	double available_size = 0.;
	unsigned end_blob_index = 0;

	/* If the read is sliced and starts some way into the sequence, look past initial elements. */
	for (unsigned i = current_blob_index; i < blob_sequence->parts.GetCount(); i++)
	{
		current_blob_index = i;
		DOM_Blob *blob = blob_sequence->parts.Get(i);
		if (blob->GetType() != BlobFile && !(blob->GetType() == BlobSlice && !static_cast<DOM_BlobSlice *>(blob)->IsSimpleSlice()))
			if (blob->GetSize() <= static_cast<double>(start_position))
				start_position -= static_cast<OpFileLength>(blob->GetSize());
			else
				break;
		else
			break;
	}

	for (unsigned i = current_blob_index; i < blob_sequence->parts.GetCount() && available_size < length_remaining; i++)
	{
		DOM_Blob *blob = blob_sequence->parts.Get(i);
		if (blob->GetType() != BlobFile && !(blob->GetType() == BlobSlice && !static_cast<DOM_BlobSlice *>(blob)->IsSimpleSlice()))
		{
			end_blob_index = i;
			available_size += blob->GetSize() - (i == current_blob_index ? static_cast<double>(start_position) : 0);
		}
		else
			break;
	}

	if (available_size > 0)
	{
		unsigned char short_buffer[10]; // ARRAY OK 2012-03-27 sof
		if (bytes_read)
			*bytes_read = 0;
		for (unsigned i = current_blob_index; i <= end_blob_index && length_remaining > 0; i++)
		{
			DOM_Blob *blob = blob_sequence->parts.Get(i);
			OpFileLength to_read = 0;
			const unsigned char *data = NULL;
			switch (blob->GetType())
			{
			default:
			case BlobFile:
			case BlobSequence:
				OP_ASSERT(!"File or Sequence Blob types cannot appear here.");
				return OpStatus::ERR;
			case BlobSlice:
				data = static_cast<DOM_BlobSlice *>(blob)->GetSliceBuffer();
				to_read = static_cast<OpFileLength>(blob->GetSize()) - start_position;
				break;
			case BlobString:
				data = static_cast<DOM_BlobString *>(blob)->GetString();
				to_read = static_cast<OpFileLength>(blob->GetSize()) - start_position;
				break;
			case BlobBuffer:
				data = static_cast<DOM_BlobBuffer *>(blob)->GetBuffer();
				to_read = static_cast<OpFileLength>(blob->GetSize()) - start_position;
				break;
			}
			to_read = MIN(length_remaining, to_read);
			RETURN_IF_ERROR(reader->AppendToResult(prefix_bytes, prefix_length, data + start_position, static_cast<unsigned>(to_read)));

			if (bytes_read)
				*bytes_read += to_read;

			length_remaining -= to_read;
			if (i + 1 <= end_blob_index)
			{
				/* Can happen if a BOM marker is indicated by an initial one-character
				   sub-Blob or if the encoder receives data that is not modulo the
				   units it works over. */
				prefix_length = reader->ReadOldData(short_buffer);
				prefix_bytes = short_buffer;
			}
			start_position = 0;
		}
		current_blob_index = end_blob_index + 1;
	}
	else if (current_blob_index < blob_sequence->parts.GetCount())
	{
		DOM_Blob *blob = blob_sequence->parts.Get(current_blob_index);
		OP_ASSERT(blob->GetType() == BlobFile || blob->GetType() == BlobSlice);
		if (blob->GetType() == BlobFile)
		{
			DOM_File *file_blob = static_cast<DOM_File *>(blob);
			DOM_BlobReadView *file_view;
			RETURN_IF_ERROR(file_blob->MakeReadView(file_view, start_position, length_remaining));
			OP_DELETE(current_blob_view);
			current_blob_view = file_view;
		}
		else
		{
			DOM_BlobSlice *sliced_blob = static_cast<DOM_BlobSlice *>(blob);
			DOM_BlobReadView *sliced_view;
			RETURN_IF_ERROR(sliced_blob->MakeReadView(sliced_view, start_position, length_remaining));
			OP_DELETE(current_blob_view);
			current_blob_view = sliced_view;
		}
		RETURN_IF_ERROR(current_blob_view->ReadAvailable(reader, prefix_bytes, prefix_length, bytes_read));
		length_remaining -= *bytes_read;
		if (current_blob_view->AtEOF())
		{
			OP_DELETE(current_blob_view);
			current_blob_view = NULL;
			current_blob_index++;
		}
	}
	else
	{
		*bytes_read = 0;
	}

	return OpStatus::OK;
}

/* virtual */ void
DOM_BlobSequence::ReadView::GCTrace()
{
	if (blob_sequence)
		blob_sequence->GCTrace();
}

/* virtual */ OP_STATUS
DOM_BlobSequence::MakeClone(ES_Runtime *target_runtime, DOM_Blob *&target_object)
{
	DOM_BlobSequence *clone;
	RETURN_IF_ERROR(DOM_BlobSequence::Make(clone, static_cast<DOM_Runtime *>(target_runtime)));

	for (unsigned i = 0; i < parts.GetCount(); i++)
		RETURN_IF_ERROR(clone->parts.Add(parts.Get(i)));

	target_object = clone;
	return OpStatus::OK;
}

#ifdef ES_PERSISTENT_SUPPORT
class DOM_BlobSequence_PersistentItem
	: public DOM_Blob_PersistentItem
{
public:
	DOM_BlobSequence_PersistentItem()
		: DOM_Blob_PersistentItem(DOM_Blob::BlobSequence)
	{
	}

	virtual ~DOM_BlobSequence_PersistentItem()
	{
		parts.DeleteAll();
	}

	OpVector<ES_PersistentItem> parts;
};

/* virtual */ OP_STATUS
DOM_BlobSequence::MakeClone(ES_PersistentItem *&target_item)
{
	DOM_BlobSequence_PersistentItem *item = OP_NEW(DOM_BlobSequence_PersistentItem, ());
	if (!item)
		return OpStatus::ERR_NO_MEMORY;

	for (unsigned i = 0; i < parts.GetCount(); i++)
	{
		DOM_Blob *blob = parts.Get(i);

		ES_PersistentItem *o_item;
		RETURN_IF_ERROR(blob->MakeClone(o_item));
		RETURN_IF_ERROR(item->parts.Add(o_item));
	}

	target_item = item;
	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_BlobSequence::MakeClone(ES_PersistentItem *&source_item0, ES_Runtime *target_runtime, DOM_Blob *&target_object)
{
	DOM_BlobSequence_PersistentItem *source_item = static_cast<DOM_BlobSequence_PersistentItem *>(source_item0);
	DOM_BlobSequence *clone;
	RETURN_IF_ERROR(DOM_BlobSequence::Make(clone, static_cast<DOM_Runtime *>(target_runtime)));
	for (unsigned i = 0; i < source_item->parts.GetCount(); i++)
	{
		DOM_Blob *o_item;
		ES_PersistentItem *s_item = source_item->parts.Get(i);
		RETURN_IF_ERROR(DOM_Blob::Clone(s_item, target_runtime, o_item));
		RETURN_IF_ERROR(clone->parts.Add(o_item));
	}

	target_object = clone;
	return OpStatus::OK;
}
#endif // ES_PERSISTENT_SUPPORT

int
DOM_Blob_Constructor::Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime)
{
	const uni_char *content_type = NULL;
	DOM_BlobSequence *combined_blob;
	if (ES_CONVERSION_COMPLETED(argc))
	{
		argc = ES_CONVERSION_ARGC(argc);
		OP_ASSERT(argv[0].type == VALUE_OBJECT && (argc == 1 || argv[1].type == VALUE_OBJECT));
		if (argc > 1)
			content_type = DOMGetDictionaryString(argv[1].value.object, UNI_L("type"), NULL);
	}
	else if (argc > 1 && argv[1].type == VALUE_OBJECT)
	{
		ES_CONVERT_ARGUMENTS_AS(return_value, "[(#ArrayBuffer|#Uint8Array|#Uint8ClampedArray|#Uint16Array|#Uint32Array|#Int8Array|#Int16Array|#Int32Array|#Float32Array|#Float64Array|#DataView|#Blob|#File|s)]{type:s}-");
		return ES_NEEDS_CONVERSION;
	}
	else if (argc == 1 && argv[0].type == VALUE_OBJECT)
	{
		ES_CONVERT_ARGUMENTS_AS(return_value, "[(#ArrayBuffer|#Uint8Array|#Uint8ClampedArray|#Uint16Array|#Uint32Array|#Int8Array|#Int16Array|#Int32Array|#Float32Array|#Float64Array|#DataView|#Blob|#File|s)]-");
		return ES_NEEDS_CONVERSION;
	}
	else if (argc != 0)
		return CallNativeException(ES_Native_TypeError, UNI_L("Expected an array object"), return_value);

	CALL_FAILED_IF_ERROR(DOM_BlobSequence::Make(combined_blob, static_cast<DOM_Runtime *>(origining_runtime)));
	if (content_type)
		CALL_FAILED_IF_ERROR(combined_blob->SetContentType(content_type));

	if (argc >= 1 && argv[0].type == VALUE_OBJECT)
	{
		ES_Object *blob_parts = argv[0].value.object;

		unsigned length = 0;
		if (!DOM_Object::DOMGetArrayLength(blob_parts, length))
			return CallNativeException(ES_Native_TypeError, UNI_L("Expected an array object"), return_value);

		for (unsigned i = 0; i < length; i++)
		{
			ES_Value value;
			OP_BOOLEAN result;
			CALL_FAILED_IF_ERROR(result = origining_runtime->GetIndex(blob_parts, i, &value));
			if (result == OpBoolean::IS_FALSE)
				return CallNativeException(ES_Native_TypeError, UNI_L("Expected a non-sparse array"), return_value);
			else if (value.type == VALUE_STRING)
			{
				unsigned string_length = value.GetStringLength();
				if (string_length > 0)
				{
					DOM_BlobString *blob_string;
					CALL_FAILED_IF_ERROR(DOM_BlobString::Make(blob_string, value.value.string, string_length, static_cast<DOM_Runtime *>(origining_runtime)));
					CALL_FAILED_IF_ERROR(combined_blob->Append(blob_string));

					if (length == 1)
					{
						if (content_type)
							CALL_FAILED_IF_ERROR(blob_string->SetContentType(content_type));
						DOMSetObject(return_value, blob_string);
						return ES_VALUE;
					}
				}
			}
			else if (value.type == VALUE_OBJECT)
			{
				if (ES_Runtime::IsNativeArrayBufferObject(value.value.object))
				{
					if (ES_Runtime::GetArrayBufferLength(value.value.object) > 0)
					{
						DOM_BlobBuffer *blob_buffer;
						CALL_FAILED_IF_ERROR(DOM_BlobBuffer::Make(blob_buffer, value.value.object, FALSE, static_cast<DOM_Runtime *>(origining_runtime)));
						CALL_FAILED_IF_ERROR(combined_blob->Append(blob_buffer));

						if (length == 1)
						{
							if (content_type)
								CALL_FAILED_IF_ERROR(blob_buffer->SetContentType(content_type));
							DOMSetObject(return_value, blob_buffer);
							return ES_VALUE;
						}
					}
				}
				else if (ES_Runtime::IsNativeTypedArrayObject(value.value.object))
				{
					if (ES_Runtime::GetStaticTypedArraySize(value.value.object) > 0)
					{
						DOM_BlobBuffer *blob_buffer;
						CALL_FAILED_IF_ERROR(DOM_BlobBuffer::Make(blob_buffer, value.value.object, TRUE, static_cast<DOM_Runtime *>(origining_runtime)));
						CALL_FAILED_IF_ERROR(combined_blob->Append(blob_buffer));

						if (length == 1)
						{
							if (content_type)
								CALL_FAILED_IF_ERROR(blob_buffer->SetContentType(content_type));
							DOMSetObject(return_value, blob_buffer);
							return ES_VALUE;
						}
					}
				}
				else
				{
					DOM_HOSTOBJECT_SAFE(blob, value.value.object, DOM_TYPE_BLOB, DOM_Blob);
					if (blob)
						CALL_FAILED_IF_ERROR(combined_blob->Append(blob));
					else
						return CallNativeException(ES_Native_TypeError, UNI_L("Unrecognized object type"), return_value);
				}
			}
			else
				return CallNativeException(ES_Native_TypeError, UNI_L("Expected an object"), return_value);
		}
	}

	DOMSetObject(return_value, combined_blob);
	return ES_VALUE;
}

DOM_FUNCTIONS_START(DOM_Blob)
	DOM_FUNCTIONS_FUNCTION(DOM_Blob, DOM_Blob::slice, "slice", "nns-")
DOM_FUNCTIONS_END(DOM_Blob)
