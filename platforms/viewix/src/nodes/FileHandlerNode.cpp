/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#include "core/pch.h"

#include "platforms/viewix/FileHandlerManager.h"
#include "platforms/viewix/src/nodes/FileHandlerNode.h"
#include "platforms/viewix/src/FileHandlerManagerUtilities.h"

FileHandlerNode::FileHandlerNode(const OpStringC & desktop_file_name,
								 const OpStringC & icon)
{
    m_icon.Set(icon);
    m_desktop_file_name.Set(desktop_file_name);

	m_icon_size        = 0;
    m_has_desktop_file = MAYBE;
    m_has_icon         = MAYBE;
}

FileHandlerNode::~FileHandlerNode()
{

}

void FileHandlerNode::SetIconPath(const OpStringC & icon_path,
								  UINT32 icon_size)
{
	m_icon_path.Set(icon_path);
	m_icon_size = icon_size;

	SetHasIcon(m_icon_path.HasContent() ? (BOOL3) TRUE : MAYBE);
}

OpBitmap * FileHandlerNode::GetBitmapIcon(UINT32 icon_size)
{
    FileHandlerManager* manager = FileHandlerManager::GetManager();

    if(m_icon.HasContent() && manager)
    {
		OpString icon_path;
		manager->MakeIconPath(this, icon_path, icon_size);

		OpBitmap * bitmap = 0;

		if(icon_path.HasContent())
		{
			bitmap = FileHandlerManagerUtilities::GetBitmapFromPath(icon_path);
		}

		return bitmap;
    }
    return 0;
}
