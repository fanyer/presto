/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"
#include "adjunct/desktop_util/file_chooser/file_chooser_fun.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/viewers/viewers.h"


OP_STATUS FileChooserUtil::SavePreferencePath(const DesktopFileChooserRequest::Action action, const DesktopFileChooserResult& result)
{
#ifndef PREFS_NO_WRITE
	if (result.files.GetCount() > 0 && result.files.Get(0)->HasContent())
	{
		OpFileFolder folder = OPFILE_OPEN_FOLDER;
		if (action == DesktopFileChooserRequest::ACTION_FILE_SAVE || 
			action == DesktopFileChooserRequest::ACTION_FILE_SAVE_PROMPT_OVERWRITE)
			folder = OPFILE_SAVE_FOLDER;

		OpFile file;
		RETURN_IF_ERROR(file.Construct(*result.files.Get(0)));

		OpString directory;

		OpFileInfo::Mode mode;
		RETURN_IF_ERROR(file.GetMode(mode));

		if(mode == OpFileInfo::DIRECTORY)
			RETURN_IF_ERROR(directory.Set(file.GetFullPath()));
		else
			RETURN_IF_ERROR(file.GetDirectory(directory));

		TRAPD(rc, g_pcfiles->WriteDirectoryL(folder, directory));
		if (OpStatus::IsError(rc))
			return rc;
	}
#endif
	return OpStatus::OK;
}


OP_STATUS FileChooserUtil::CopySettings(const DesktopFileChooserRequest& src, DesktopFileChooserRequest*& copy)
{
	OpAutoPtr<DesktopFileChooserRequest> tmp(OP_NEW(DesktopFileChooserRequest, ()));
	if (!tmp.get())
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS rc = copy->caption.Set(src.caption);
	if (OpStatus::IsSuccess(rc))
	{
		rc = copy->initial_path.Set(src.initial_path);
		if (OpStatus::IsSuccess(rc))
		{
			for (UINT32 i = 0; i < src.extension_filters.GetCount(); i++)
			{
				OpFileSelectionListener::MediaType* filter = src.extension_filters.Get(i);
				rc = CopyAddMediaType(filter, &tmp.get()->extension_filters, FALSE);
				if (OpStatus::IsError(rc))
					break;
			}
		}
	}

	if (OpStatus::IsError(rc))
		return rc;

	copy = tmp.release();
	copy->action = src.action;
	copy->initial_filter = src.initial_filter;
	copy->get_selected_filter = src.get_selected_filter;
	copy->fixed_filter = src.fixed_filter;

	return OpStatus::OK;
}


INT32 FileChooserUtil::FindExtension(const OpStringC& ext, OpVector<OpFileSelectionListener::MediaType>* filters)
{
	OpString extension;
	extension.Set(ext);
	if (extension.Find(UNI_L("*.")) == KNotFound)
		extension.Insert(0 , UNI_L("*."));

	if (filters)
	{
		for (UINT32 i = 0; i < filters->GetCount(); i++)
		{
			OpFileSelectionListener::MediaType* filter = filters->Get(i);

			for (UINT32 j = 0; j < filter->file_extensions.GetCount(); j++)
			{
				OpString s;
				if (filter->file_extensions.Get(j)->Find(UNI_L("*.")) == KNotFound)
				{
					s.Set(UNI_L("*."));
					s.Append(filter->file_extensions.Get(j)->CStr());
				}
				else
				{
					s.Set(filter->file_extensions.Get(j)->CStr());
				}
				if (extension.CompareI(s) == 0)
				{
					return i;
				}
			}
		}
	}
	return -1;
}


void FileChooserUtil::Reset(DesktopFileChooserRequest& request)
{
	request.action = DesktopFileChooserRequest::ACTION_FILE_OPEN;
	request.caption.Empty();
	request.initial_path.Empty();
	request.extension_filters.DeleteAll();
	request.initial_filter = 0;
	request.get_selected_filter = FALSE;
	request.fixed_filter = FALSE;
}


OP_STATUS FileChooserUtil::AppendExtensionInfo(OpFileSelectionListener::MediaType& media_type)
{
	if (media_type.media_type.Find(UNI_L("(")) != KNotFound)
		return OpStatus::OK;

	RETURN_IF_ERROR(media_type.media_type.Append(UNI_L(" (")));
	if (!media_type.file_extensions.GetCount())
		RETURN_IF_ERROR(media_type.media_type.Append(UNI_L("*.*")));
	else
	{
		for (UINT32 i = 0; i < media_type.file_extensions.GetCount(); i++)
		{
			if (i > 0)
				RETURN_IF_ERROR(media_type.media_type.Append(UNI_L(",")));
			RETURN_IF_ERROR(media_type.media_type.Append(media_type.file_extensions.Get(i)->CStr()));
		}
	}
	return media_type.media_type.Append(UNI_L(")"));
}


OP_STATUS FileChooserUtil::GetInitialExtension(const DesktopFileChooserRequest &request, OpString& initial_extension)
{
	if ((UINT32)request.initial_filter > request.extension_filters.GetCount())
		return OpStatus::ERR;
	else
		return initial_extension.Set(*request.extension_filters.Get(request.initial_filter)->file_extensions.Get(0));
}



OP_STATUS FileChooserUtil::SetExtensionFilter(const uni_char* string_filter, OpAutoVector<OpFileSelectionListener::MediaType> * extension_filters)
{
	OpFileSelectionListener::MediaType* ext_filter;
	OP_STATUS rc = OpStatus::OK;
	OpString* filter;

	const uni_char * begin_interval = string_filter;
	const uni_char * end_interval = string_filter ? uni_strchr(string_filter, '|') : NULL;

	extension_filters->DeleteAll();

	while(begin_interval && end_interval && end_interval > begin_interval)
	{
		ext_filter = OP_NEW(OpFileSelectionListener::MediaType, ());
		if (!ext_filter)
			return OpStatus::ERR_NO_MEMORY;

		rc = ext_filter->media_type.Append(begin_interval, end_interval - begin_interval);
		if (OpStatus::IsError(rc))
		{
			OP_DELETE(ext_filter);
			return rc;
		}

		begin_interval = end_interval + 1;
		end_interval = uni_strchr(begin_interval, '|');

		if (end_interval && end_interval > begin_interval)
		{
			end_interval = uni_strpbrk(begin_interval, UNI_L("|;"));

			while (begin_interval && end_interval && end_interval > begin_interval)
			{
				filter = OP_NEW(OpString, ());
				if (!filter)
				{
					OP_DELETE(ext_filter);
					return OpStatus::ERR_NO_MEMORY;
				}

				rc = filter->Append(begin_interval, end_interval - begin_interval);
				if (OpStatus::IsError(rc))
				{
					OP_DELETE(ext_filter);
					OP_DELETE(filter);
					return rc;
				}

				rc = ext_filter->file_extensions.Add(filter);
				if (OpStatus::IsError(rc))
				{
					OP_DELETE(ext_filter);
					OP_DELETE(filter);
					return rc;
				}
				begin_interval = end_interval + 1;

				if (*end_interval == '|')
					break;
				else
					end_interval = uni_strpbrk(begin_interval, UNI_L("|;"));
			}
		}

		rc = extension_filters->Add(ext_filter);
		if (OpStatus::IsError(rc))
		{
			OP_DELETE(ext_filter);
			end_interval = NULL;
		}

		end_interval = uni_strchr(begin_interval, '|');
	}
	return rc;
}


OP_STATUS FileChooserUtil::AddMediaType(const OpFileSelectionListener::MediaType* filter, OpAutoVector<OpFileSelectionListener::MediaType>* list, BOOL add_asterix_dot)
{
	if (!filter)
		return OpStatus::OK;

	OpFileSelectionListener::MediaType* filter_copy = OP_NEW(OpFileSelectionListener::MediaType, ());
	if (!filter_copy || OpStatus::IsError(filter_copy->media_type.Set(filter->media_type)))
	{
		OP_DELETE(filter_copy);
		return OpStatus::ERR_NO_MEMORY;
	}

	for (UINT32 i = 0; i < filter->file_extensions.GetCount(); i++)
	{
		OpString* s = OP_NEW(OpString,());
		if (!s || OpStatus::IsError(s->Set(*filter->file_extensions.Get(i))))
		{
			OP_DELETE(filter_copy);
			OP_DELETE(s);
			return OpStatus::ERR_NO_MEMORY;
		}
		if (add_asterix_dot && s->Compare(UNI_L("*")) && s->Compare(UNI_L("*."), 2))
		{
			if (OpStatus::IsError(s->Insert(0, UNI_L("*."))))
			{
				OP_DELETE(filter_copy);
				OP_DELETE(s);
				return OpStatus::ERR_NO_MEMORY;
			}
		}

		if (OpStatus::IsError(filter_copy->file_extensions.Add(s)))
		{
			OP_DELETE(filter_copy);
			OP_DELETE(s);
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	if (OpStatus::IsError(list->Add(filter_copy)))
	{
		OP_DELETE(filter_copy);
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}






INT32 FindExtension(const OpStringC& extension, OpVector<OpFileSelectionListener::MediaType>* filters)
{
	return FileChooserUtil::FindExtension(extension, filters);
}


void ResetDesktopFileChooserRequest(DesktopFileChooserRequest& request)
{
	FileChooserUtil::Reset(request);
}


OP_STATUS ExtendMediaTypeWithExtensionInfo(OpFileSelectionListener::MediaType& media_type)
{
	return FileChooserUtil::AppendExtensionInfo(media_type);
}


OP_STATUS GetInitialExtension(const DesktopFileChooserRequest& request, OpString& extension)
{
	return FileChooserUtil::GetInitialExtension(request, extension);
}


OP_STATUS StringFilterToExtensionFilter(const uni_char* string_filter, OpAutoVector<OpFileSelectionListener::MediaType> * extension_filters)
{
	return FileChooserUtil::SetExtensionFilter(string_filter, extension_filters);
}


OP_STATUS CopyAddMediaType(const OpFileSelectionListener::MediaType* filter, OpAutoVector<OpFileSelectionListener::MediaType>* list, BOOL add_asterix_dot)
{
	return FileChooserUtil::AddMediaType(filter, list, add_asterix_dot);
}

