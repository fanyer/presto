/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/desktop_pi/desktop_pi_util.h"
#include "adjunct/desktop_util/image_utils/fileimagecontentprovider.h"

#include "modules/pi/OpBitmap.h"

#include "platforms/viewix/FileHandlerManager.h"

OP_STATUS DesktopPiUtil::CreateIconBitmapForFile(OpBitmap **bitmap, const uni_char* file_path)
{
	OP_ASSERT(file_path);

	if(file_path)
	{
		if (FileHandlerManager* manager = FileHandlerManager::GetManager())
		{
			/*
			 * Conforming to windows behavior where an icon is assocated with a
			 * file (no distinction between file or handler)
			 *
			 * First assume this is a handler - if not assume this is a file
			 */
			const uni_char* icon_filename = manager->GetApplicationIcon(file_path);

			if(!icon_filename)
			{
				OpString content_type;
				icon_filename = manager->GetFileTypeIconPath(file_path, content_type);
			}

			if (icon_filename)
			{
				FileImageContentProvider provider;
				Image img;
				provider.LoadFromFile(icon_filename, img);

				OpBitmap * loaded_bitmap = img.GetBitmap(0);
				if (!loaded_bitmap)
					return OpStatus::ERR_NO_SUCH_RESOURCE;

				int width  = loaded_bitmap->Width();
				int height = loaded_bitmap->Height();

				RETURN_IF_ERROR(OpBitmap::Create(bitmap,
												 width,
												 height,
												 loaded_bitmap->IsTransparent(),
												 loaded_bitmap->HasAlpha(),
												 FALSE,
												 FALSE,
												 FALSE));

				UINT32 *buffer = OP_NEWA(UINT32, width);
				if (buffer == 0)
					return OpStatus::ERR_NO_MEMORY;

				for (int y=0; y<height; y++)
				{
					loaded_bitmap->GetLineData(buffer, y);
					(*bitmap)->AddLine(buffer, y);
				}

				OP_DELETEA(buffer);

				return OpStatus::OK;
			}
		}
	}
	return OpStatus::ERR;
}

OP_STATUS DesktopPiUtil::SetDesktopBackgroundImage(URL& url)
{
	// not implemented (and currently no plan to do so)
	return OpStatus::ERR;
}
