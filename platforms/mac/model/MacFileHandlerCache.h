/*
 *  MacFileHandlerCache.h
 *  Opera
 *
 *  Created by Adam Minchinton on 5/14/07.
 *  Copyright 2007 Opera. All rights reserved.
 *
 */

#include "modules/util/adt/opvector.h"
#include "modules/pi/OpBitmap.h"

class MacFileHandlerCache
{
public:
	OP_STATUS CopyInto(OpVector<OpString>& handlers,
						OpVector<OpString>& handler_names,
						OpVector<OpBitmap>& handler_icons,
						UINT32 icon_size);

	OpString m_ext;

	OpAutoVector<OpString>	m_handlers;
	OpAutoVector<OpString>	m_handler_names;
	OpAutoVector<OpBitmap>	m_handler_icons;
};
