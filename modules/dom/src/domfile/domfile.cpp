/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/dom/src/domfile/domfile.h"
#include "modules/dom/src/domfile/domblob.h"
#include "modules/dom/src/domfile/domfilereader.h"

#include "modules/viewers/viewers.h"
#include "modules/util/opfile/opfile.h"
#include "modules/upload/upload.h"

/* static */ OP_STATUS
DOM_File::Make(DOM_File *&file, const uni_char *path, BOOL disguise_as_blob, BOOL delete_on_destruct, DOM_Runtime *runtime)
{
	OP_ASSERT(path);
	RETURN_IF_ERROR(DOMSetObjectRuntime(file = OP_NEW(DOM_File, ()), runtime, runtime->GetPrototype(disguise_as_blob ? DOM_Runtime::BLOB_PROTOTYPE : DOM_Runtime::FILE_PROTOTYPE), disguise_as_blob ? "Blob" : "File"));
	file->m_file = UniSetNewStr(path);
	if (!file->m_file)
		return OpStatus::ERR_NO_MEMORY;
	file->m_delete_on_destruct = delete_on_destruct;
	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_File::Make(DOM_File *&file, DOM_File *source, BOOL disguise_as_blob, DOM_Runtime *runtime)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(file = OP_NEW(DOM_File, ()), runtime, runtime->GetPrototype(disguise_as_blob ? DOM_Runtime::BLOB_PROTOTYPE : DOM_Runtime::FILE_PROTOTYPE), disguise_as_blob ? "Blob" : "File"));
	file->m_sliced_from = source;
	return OpStatus::OK;
}


DOM_File::~DOM_File()
{
	if (m_file && m_delete_on_destruct)
	{
		OpFile file;
		if (OpStatus::IsSuccess(file.Construct(m_file)))
			OpStatus::Ignore(file.Delete(FALSE));
	}
	OP_DELETEA(m_file);
}

/* virtual */ void
DOM_File::GCTrace()
{
	GCMark(m_sliced_from);
}

/* virtual */ OP_STATUS
DOM_File::MakeSlice(DOM_Blob *&sliced_blob, double slice_start, double slice_end)
{
	DOM_File *sliced_file;
	RETURN_IF_ERROR(Make(sliced_file, this, TRUE, GetRuntime()));

	double slice_length = MAX(slice_end - slice_start, 0);

	sliced_file->m_sliced_start = slice_start + m_sliced_start;
	if (stdlib_intpart(sliced_file->m_sliced_start) != sliced_file->m_sliced_start) // Overflow in the addition.
		return OpStatus::ERR;

	if (m_sliced_length != -1.0 && slice_start >= m_sliced_length)
		sliced_file->m_sliced_length = 0;
	else if (m_sliced_length == -1.0)
		sliced_file->m_sliced_length = slice_length;
	else
	{
		double orig_available = m_sliced_length - slice_start;
		sliced_file->m_sliced_length = MIN(orig_available, slice_length);
	}

	sliced_blob = sliced_file;
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
DOM_File::MakeReadView(DOM_BlobReadView *&blob_view, OpFileLength start_position, OpFileLength view_length)
{
	double length = GetSize();
	length = static_cast<double>(start_position) > length ? 0 : (length - static_cast<double>(start_position));
	OpFileLength start_pos = static_cast<OpFileLength>(m_sliced_start) + MIN(static_cast<OpFileLength>(length), start_position);

	DOM_FileReadView *file_view;
	RETURN_IF_ERROR(DOM_FileReadView::Make(file_view, this, start_pos));
	if (m_sliced_length >= 0)
		length = MIN(m_sliced_length, length);

	if (view_length != FILE_LENGTH_NONE)
		length = MIN(length, static_cast<double>(view_length));
	file_view->length_remaining = length;

	blob_view = file_view;
	return OpStatus::OK;
}

const uni_char *
DOM_File::GetContentType()
{
	if (GetForcedContentType())
		return GetForcedContentType();

	if (m_content_type.IsEmpty())
		if (Viewer *viewer = g_viewers->FindViewerByFilename(GetFilePath()))
			OpStatus::Ignore(m_content_type.Set(viewer->GetContentTypeString8()));

	return m_content_type.CStr();
}

double
DOM_File::GetSize()
{
	OpFile file;
	double len = 0;
	if (OpStatus::IsSuccess(file.Construct(GetFilePath())))
	{
		len = static_cast<double>(file.GetFileLength());
		if (len <= m_sliced_start)
			len = 0;
		else
		{
			len -= m_sliced_start;
			if (len > m_sliced_length && m_sliced_length >= 0.)
				len = m_sliced_length;
		}
	}
	return len;
}

/* virtual */ ES_GetState
DOM_File::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_name:
		if (IsSlice())
			return GET_FAILED;
		// The file name without a path.
		if (value)
		{
			TempBuffer *buf = GetEmptyTempBuf();
			GET_FAILED_IF_ERROR(GetFilename(buf));
			DOMSetString(value, buf);
		}
		return GET_SUCCESS;

	case OP_ATOM_lastModifiedDate:
		if (IsSlice())
			return GET_FAILED;
		// The last modified date as a Date object or null if unknown.
		// I am not sure this is something we want to expose to a web page and the spec
		// is worded in a way that makes it possible to keep it hidden without violating
		// the spec.
		DOMSetNull(value);
		return GET_SUCCESS;
	}

	return DOM_Blob::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_File::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_name:
	case OP_ATOM_lastModifiedDate:
		if (!IsSlice())
			return PUT_READ_ONLY;
		break;
	}

	return DOM_Blob::PutName(property_name, value, origining_runtime);
}

const uni_char *
DOM_File::GetFilePath()
{
	DOM_File *f = this;

	while (!f->m_file)
	{
		f = f->m_sliced_from;
		/* DOM_Files are constructed from a file path or by being a slice
		   of another. If the un-nesting doesn't uncover that path the
		   object is in an invalid state. */
		OP_ASSERT(f || !"Nested file slice without an inner filepath.");
	}
	return f->m_file;
}

OP_STATUS
DOM_File::GetFilename(TempBuffer *buf)
{
	OpFile file;
	if (OpStatus::IsSuccess(file.Construct(GetFilePath())))
		RETURN_IF_ERROR(buf->Append(file.GetName()));

	return OpStatus::OK;
}

OP_STATUS
DOM_File::GetFilename(OpString &result)
{
	OpFile file;
	if (OpStatus::IsSuccess(file.Construct(GetFilePath())))
		RETURN_IF_ERROR(result.Set(file.GetName()));

	return OpStatus::OK;
}

#ifdef _FILE_UPLOAD_SUPPORT_
OP_STATUS
DOM_File::Upload(Upload_Handler *&result, const uni_char *name, const uni_char *filename, const char *mime_type, BOOL as_standalone)
{
	result = NULL;
	TRAPD(status, UploadL(result, name, filename, mime_type, as_standalone));
	RETURN_IF_ERROR(status);

	return result ? OpStatus::OK : OpStatus::ERR;
}

void
DOM_File::UploadL(Upload_Handler *&result, const uni_char *name, const uni_char *filename, const char *mime_type, BOOL as_standalone)
{
	const uni_char *fpath = GetFilePath();
	if (!fpath || !*fpath)
		return;

	OpStackAutoPtr<Upload_URL> upload(OP_NEW_L(Upload_URL, ()));
	if (as_standalone)
	{
		upload->SetForceFileName(FALSE);
		upload->InitL(fpath, UNI_L(""), "", "");
	}
	else
	{
		OpString8 name8;
		name8.SetUTF8FromUTF16L(name);
		OpStringC suggested(filename);

		upload->InitL(fpath, suggested, "form-data", name8);
	}
	if (IsSlice())
	{
		OpFileLength slice_length;
		if (m_sliced_length < 0)
			slice_length = 0;
		else
			slice_length = static_cast<OpFileLength>(m_sliced_length);
		upload->SetUploadRange(static_cast<OpFileLength>(m_sliced_start), slice_length);
	}
	if (as_standalone)
		upload->PrepareUploadL(UPLOAD_BINARY_NO_CONVERSION);

	OpString8 content_type8;
	if (!mime_type)
	{
		content_type8.SetUTF8FromUTF16L(GetContentType());
		mime_type = content_type8.CStr();
	}
	if (mime_type)
		upload->ClearAndSetContentTypeL(mime_type);

	result = upload.release();
}
#endif // _FILE_UPLOAD_SUPPORT_

/* virtual */ OP_STATUS
DOM_File::MakeClone(ES_Runtime *target_runtime, DOM_Blob *&target_object)
{
	DOM_File *clone;
	RETURN_IF_ERROR(DOM_File::Make(clone, GetFilePath(), FALSE, FALSE, static_cast<DOM_Runtime *>(target_runtime)));
	clone->m_sliced_start = m_sliced_start;
	clone->m_sliced_length = m_sliced_length;

	target_object = clone;
	return OpStatus::OK;
}

#ifdef ES_PERSISTENT_SUPPORT
class DOM_File_PersistentItem
	: public DOM_Blob_PersistentItem
{
public:
	DOM_File_PersistentItem()
		: DOM_Blob_PersistentItem(DOM_Blob::BlobFile)
		, m_file(NULL)
		, m_sliced_start(0)
		, m_sliced_length(-1.0)
	{
	}

	virtual ~DOM_File_PersistentItem()
	{
		OP_DELETEA(m_file);
	}

	const uni_char *m_file;
	double m_sliced_start;
	double m_sliced_length;
};

/* virtual */ OP_STATUS
DOM_File::MakeClone(ES_PersistentItem *&target_item)
{
	DOM_File_PersistentItem *item = OP_NEW(DOM_File_PersistentItem, ());
	RETURN_OOM_IF_NULL(item);
	OpAutoPtr<DOM_File_PersistentItem> anchor_item(item);
	RETURN_OOM_IF_NULL(item->m_file = UniSetNewStr(GetFilePath()));
	item->m_sliced_start = m_sliced_start;
	item->m_sliced_length = m_sliced_length;

	target_item = item;
	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_File::MakeClone(ES_PersistentItem *&source_item0, ES_Runtime *target_runtime, DOM_Blob *&target_object)
{
	DOM_File_PersistentItem *source_item = static_cast<DOM_File_PersistentItem *>(source_item0);
	DOM_File *clone;
	RETURN_IF_ERROR(DOM_File::Make(clone, source_item->m_file, FALSE, FALSE, static_cast<DOM_Runtime *>(target_runtime)));
	clone->m_sliced_start = source_item->m_sliced_start;
	clone->m_sliced_length = source_item->m_sliced_length;

	target_object = clone;
	return OpStatus::OK;
}
#endif // ES_PERSISTENT_SUPPORT

/* static */ OP_STATUS
DOM_FileReadView::Make(DOM_FileReadView *&file_view, DOM_File *file, OpFileLength start_position)
{
	file_view = OP_NEW(DOM_FileReadView, ());
	RETURN_OOM_IF_NULL(file_view);
	OpAutoPtr<DOM_FileReadView> anchor_file(file_view);

	RETURN_IF_ERROR(file_view->file.Construct(file->GetFilePath()));
	RETURN_IF_ERROR(file_view->file.Open(OPFILE_READ));
	RETURN_IF_ERROR(file_view->file.SetFilePos(start_position));

	anchor_file.release();

	return OpStatus::OK;
}

/* virtual */
DOM_FileReadView::~DOM_FileReadView()
{
	OpStatus::Ignore(file.Close());
}

/* virtual */ BOOL
DOM_FileReadView::AtEOF()
{
	return length_remaining == 0 || file.Eof();
}

/* virtual */ OP_STATUS
DOM_FileReadView::ReadAvailable(DOM_FileReader_Base *reader, const unsigned char *prefix_bytes, unsigned prefix_length, OpFileLength *bytes_read)
{
	unsigned char buf[DOM_File::DOM_FILE_READ_BUFFER_SIZE]; // ARRAY OK sof 2012-04-30
	unsigned buf_length = sizeof(buf);

	OpFileLength bytes_to_read = buf_length - prefix_length;
	if (length_remaining >= 0. && bytes_to_read > length_remaining)
		bytes_to_read = static_cast<OpFileLength>(length_remaining);

	OP_STATUS status = file.Read(buf, bytes_to_read, bytes_read);
	RETURN_IF_ERROR(status);

	// Append to result.
	if (*bytes_read > 0)
		status = reader->AppendToResult(prefix_bytes, prefix_length, buf, static_cast<unsigned>(*bytes_read));

	*bytes_read += prefix_length;
	if (length_remaining >= 0.)
		length_remaining -= *bytes_read;

	return status;
}
