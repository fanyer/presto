/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if defined DRAG_SUPPORT || defined USE_OP_CLIPBOARD
# include "modules/pi/OpDragObject.h"
# include "modules/dragdrop/dragdrop_data_utils.h"
# include "modules/viewers/viewers.h"

/* static */
BOOL
DragDrop_Data_Utils::HasFiles(OpDragObject* object)
{
	OpDragDataIterator& iter = object->GetDataIterator();
	if (iter.First())
	{
		do
		{
			if (iter.IsFileData())
				return TRUE;
		} while (iter.Next());
	}
	return FALSE;
}


/* static */
OP_STATUS
DragDrop_Data_Utils::AddURL(OpDragObject* object, URL& url)
{
	TempBuffer result;
	OpString url_str;
	RETURN_IF_ERROR(url.GetAttribute(URL::KUniName_With_Fragment_Username_Password_Hidden, url_str));
	return AddURL(object, url_str);
}

/* static */
OP_STATUS
DragDrop_Data_Utils::AddURL(OpDragObject* object, OpStringC url_str)
{
	if(url_str.IsEmpty())
		return OpStatus::ERR;

	TempBuffer result;
	const uni_char* current_url = GetStringData(object, URI_LIST_DATA_FORMAT);
	if (current_url)
	{
		if (uni_strstr(current_url, url_str.CStr()))
			return OpStatus::OK;

		RETURN_IF_ERROR(result.AppendFormat(UNI_L("%s%s\r\n"), current_url, url_str.CStr()));
	}
	else
		RETURN_IF_ERROR(result.AppendFormat(UNI_L("%s\r\n"), url_str.CStr()));
	return object->SetData(result.GetStorage(), URI_LIST_DATA_FORMAT, FALSE, TRUE);
}

/* static */
OP_STATUS
DragDrop_Data_Utils::SetText(OpDragObject* object, const uni_char* text)
{
	return object->SetData(text, TEXT_DATA_FORMAT, FALSE, TRUE);
}

/* static */
OP_STATUS
DragDrop_Data_Utils::SetDescription(OpDragObject* object, const uni_char* description)
{
	return object->SetData(description, DESCRIPTION_DATA_FORMAT, FALSE, TRUE);
}

static BOOL
ResolveFilesTypesSafe(OpDragObject* object)
{
	OpDragDataIterator& iter = object->GetDataIterator();
	if (iter.First())
	{
		do
		{
			const uni_char* real_format = iter.GetMimeType();
			/* If the type of the data is equal to the special opera file type
			   i.e. it's unknown try to find out a proper file's format. */
			if (iter.IsFileData() && uni_stri_eq(real_format, FILE_DATA_FORMAT))
			{
				OpString real_type_utf16;
				const OpFile* file = iter.GetFileData();
				const uni_char* name = file->GetName();
				const uni_char* period = uni_strrchr(name, '.');
				if (period)
				{
					const char* real_type = Viewers::GetDefaultContentTypeStringFromExt(period + 1);
					OpStatus::Ignore(real_type_utf16.SetFromUTF8(real_type));
					real_format = real_type_utf16.CStr() ? real_type_utf16.CStr() : UNI_L("application/octet-stream");
				}
				else
					real_format = UNI_L("application/octet-stream");

				// Set the known mime type.
				if (OpStatus::IsSuccess(object->SetData(file->GetFullPath(), real_format, TRUE, FALSE)))
				{
					iter.Remove();
					return TRUE; // Start over to be more independent on the iterator implementation i.e. better be slower but safer.
				}
			}
		} while (iter.Next());
	}

	return FALSE;
}

static void
ResolveFilesTypes(OpDragObject* object)
{
	BOOL continue_resolving = FALSE;
	do
	{
		continue_resolving = ResolveFilesTypesSafe(object);
	}
	while (continue_resolving);
}

/* static */
BOOL
DragDrop_Data_Utils::HasData(OpDragObject* object, const uni_char* format, BOOL is_file /* = FALSE */)
{
	if (is_file)
		ResolveFilesTypes(object);

	OpDragDataIterator& iter = object->GetDataIterator();
	if (iter.First())
	{
		do
		{
			if (uni_str_eq(iter.GetMimeType(), format) && is_file == iter.IsFileData())
				return TRUE;
		} while (iter.Next());
	}
	return FALSE;
}

/* static */
const uni_char*
DragDrop_Data_Utils::GetStringData(OpDragObject* object, const uni_char* format)
{
	OpDragDataIterator& iter = object->GetDataIterator();
	if (iter.First())
	{
		do
		{
			if (uni_str_eq(iter.GetMimeType(), format) && iter.IsStringData())
				return iter.GetStringData();
		} while (iter.Next());
	}

	return NULL;
}

/* static */
const OpFile*
DragDrop_Data_Utils::GetFileData(OpDragObject* object, const uni_char* format)
{
	ResolveFilesTypes(object);

	OpDragDataIterator& iter = object->GetDataIterator();
	if (iter.First())
	{
		do
		{
			if (uni_str_eq(iter.GetMimeType(), format) && iter.IsFileData())
				return iter.GetFileData();
		} while (iter.Next());
	}
	return NULL;
}

/* static */
OP_STATUS
DragDrop_Data_Utils::GetURL(OpDragObject* object, TempBuffer* url, BOOL only_first /* = TRUE */)
{
	url->Clear();
	OpString data;
	RETURN_IF_ERROR(data.Set(GetStringData(object, URI_LIST_DATA_FORMAT)));
	if (only_first)
	{
		uni_char* data_ptr = data.DataPtr();
		if (data_ptr && *data_ptr)
		{
			uni_char* new_line = uni_strstr(data_ptr, UNI_L("\r\n"));
			while (new_line)
			{
				// Strip leading white spaces.
				data_ptr += uni_strspn(data_ptr, UNI_L(" \t\f\v"));

				if (data_ptr[0] != '#')
				{
					*new_line = 0;
					break;
				}
				else
					data_ptr = new_line + 2;

				new_line = uni_strstr(data_ptr, UNI_L("\r\n"));
			}

			if (data_ptr[0] == '#')
				data_ptr = NULL;
			else
			{
				// Skip trailing whitespaces.
				size_t ws = uni_strcspn(data_ptr, UNI_L(" \t\f\v"));
				data_ptr[ws] = 0;
			}

			RETURN_IF_ERROR(url->Append(data_ptr));
		}
	}
	else
		RETURN_IF_ERROR(url->Append(data.CStr()));

	return OpStatus::OK;
}

class UrlsCleaner
{
	OpVector<OpString>* m_urls;
public:
	UrlsCleaner(OpVector<OpString>* urls) : m_urls(urls)
	{}
	~UrlsCleaner()
	{
		if (m_urls)
			m_urls->DeleteAll();
	}

	void Release()
	{
		m_urls = NULL;
	}
};

/* static */
OP_STATUS
DragDrop_Data_Utils::GetURLs(OpDragObject* object, OpVector<OpString> &urls)
{
	urls.DeleteAll();
	OpString data;
	RETURN_IF_ERROR(data.Set(GetStringData(object, URI_LIST_DATA_FORMAT)));

	uni_char* data_ptr = data.DataPtr();
	if (data_ptr && *data_ptr)
	{
		UrlsCleaner cleaner(&urls);
		uni_char* new_line = uni_strstr(data_ptr, UNI_L("\r\n"));
		while (new_line)
		{
			OpString* new_str = OP_NEW(OpString, ());
			RETURN_OOM_IF_NULL(new_str);
			OpAutoPtr<OpString> ap_str(new_str);
			RETURN_IF_ERROR(new_str->Set(data_ptr, new_line - data_ptr));
			new_str->Strip();
			if (new_str->DataPtr()[0] != '#')
			{
				RETURN_IF_ERROR(urls.Add(new_str));
				ap_str.release();
			}
			data_ptr = new_line + 2;
			new_line = uni_strstr(data_ptr, UNI_L("\r\n"));
		}
		cleaner.Release();
	}

	return OpStatus::OK;
}

/* static */
void
DragDrop_Data_Utils::ClearStringData(OpDragObject* object, const uni_char* format)
{
	OpDragDataIterator& iter = object->GetDataIterator();
	if (iter.First())
	{
		do
		{
			if (uni_str_eq(iter.GetMimeType(), format) && iter.IsStringData())
				iter.Remove();
		} while (iter.Next());
	}
}

/* static */
void
DragDrop_Data_Utils::ClearFileData(OpDragObject* object, const uni_char* format)
{
	ResolveFilesTypes(object);

	OpDragDataIterator& iter = object->GetDataIterator();
	if (iter.First())
	{
		do
		{
			if (uni_str_eq(iter.GetMimeType(), format) && iter.IsFileData())
				iter.Remove();
		} while (iter.Next());
	}
}

/* static */
OP_STATUS
DragDrop_Data_Utils::DOMGetFormats(OpDragObject* object, OpVector<OpString>& formats)
{
	BOOL file_entry_added = FALSE;
	OpDragDataIterator& iter = object->GetDataIterator();
	if (iter.First())
	{
		do
		{
			if (iter.IsFileData() && file_entry_added)
				continue;

			OpString* format_str = OP_NEW(OpString, ());
			RETURN_OOM_IF_NULL(format_str);
			OpAutoPtr<OpString> ap_format(format_str);
			if (iter.IsFileData())
			{
				OP_ASSERT(!file_entry_added);
				RETURN_IF_ERROR(format_str->Set(UNI_L("Files")));
				file_entry_added = TRUE;
			}
			else
				RETURN_IF_ERROR(format_str->Set(iter.GetMimeType()));

			RETURN_IF_ERROR(formats.Add(format_str));
			ap_format.release();

		} while (iter.Next());
	}

	return OpStatus::OK;
}

#endif // DRAG_SUPPORT || USE_OP_CLIPBOARD
