/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef SPEED_DIAL_GENERIC_HANDLER_H
#define SPEED_DIAL_GENERIC_HANDLER_H

class DesktopSpeedDial;
class DesktopDragObject;

#include "adjunct/quick/thumbnails/GenericThumbnailContent.h"

class SpeedDialGenericHandler
{
public:
	explicit SpeedDialGenericHandler(const DesktopSpeedDial& entry);

	OP_STATUS GetButtonInfo(GenericThumbnailContent::ButtonInfo& info) const;
	OP_STATUS GetCloseButtonInfo(GenericThumbnailContent::ButtonInfo& info) const;
	GenericThumbnailContent::MenuInfo GetContextMenuInfo(const char* menu_name) const;
	int GetNumber() const;
	OP_STATUS HandleMidClick();
	
private:
	bool IsEntryManaged() const;
	const DesktopSpeedDial* m_entry;
};

#endif // SPEED_DIAL_GENERIC_HANDLER_H
