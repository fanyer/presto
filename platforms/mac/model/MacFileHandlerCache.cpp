/*
 *  MacFileHandlerCache.h
 *  Opera
 *
 *  Created by Adam Minchinton on 5/14/07.
 *  Copyright 2007 Opera. All rights reserved.
 *
 */

#include "core/pch.h"

#include "platforms/mac/model/MacFileHandlerCache.h"
#include "modules/util/adt/opvector.h"
#include "modules/pi/OpBitmap.h"
#include "platforms/mac/util/MacIcons.h"


OP_STATUS MacFileHandlerCache::CopyInto(OpVector<OpString>& handlers,
										OpVector<OpString>& handler_names,
										OpVector<OpBitmap>& handler_icons,
										UINT32 icon_size)
{
	OpString	*temp_string;
	OpBitmap	*temp_bmp;
	UINT32		index;

	// Found an entry so we'll just be using this
	for (index = 0; index < m_handlers.GetCount(); index++)
	{
		temp_string = new OpString();
		if (!temp_string)
			return OpStatus::ERR_NO_MEMORY;
		temp_string->Set(m_handlers.Get(index)->CStr());
		RETURN_IF_ERROR(handlers.Add(temp_string));

		temp_string = new OpString();
		if (!temp_string)
			return OpStatus::ERR_NO_MEMORY;
		temp_string->Set(m_handler_names.Get(index)->CStr());
		RETURN_IF_ERROR(handler_names.Add(temp_string));

		temp_bmp = GetAttachmentIconOpBitmap(m_handlers.Get(index)->CStr(), FALSE, (UINT32)icon_size);
		if (!temp_bmp)
			return OpStatus::ERR_NO_MEMORY;
		RETURN_IF_ERROR(handler_icons.Add(temp_bmp));
	}

	return OpStatus::OK;
}
