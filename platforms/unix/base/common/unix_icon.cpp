/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#include "core/pch.h"

#ifdef _MIME_SUPPORT_
#include "modules/url/url2.h"
#include "modules/util/opfile/opfile.h"
#endif
#include "platforms/viewix/FileHandlerManager.h"

URL GetAttachmentIconURL(URL &base, const OpStringC &filename, BOOL big_attachment_icon)
{
	big_attachment_icon = TRUE; //Hey its pretty :)
	URL icon;

	UINT32 pix = big_attachment_icon ? 32 : 16;

	FileHandlerManager* manager = FileHandlerManager::GetManager();
	OpString content_type;
   	const uni_char* icon_filename_only = manager && filename.HasContent() ? manager->GetFileTypeIconPath(filename, content_type, pix) : NULL;

	OpString icon_filename;
	
	if (icon_filename_only &&
		OpStatus::IsSuccess(icon_filename.Set("file://localhost/")) &&
		OpStatus::IsSuccess(icon_filename.Append(icon_filename_only)))
	{
		icon = g_url_api->GetURL(base, icon_filename.CStr(), (uni_char *)NULL);
	}
	
	return icon;
}
