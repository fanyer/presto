/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef SPEED_DIAL_DRAG_HANDLER_H
#define SPEED_DIAL_DRAG_HANDLER_H

class DesktopSpeedDial;
class DesktopDragObject;

class SpeedDialDragHandler
{
public:
	explicit SpeedDialDragHandler(const DesktopSpeedDial& entry);

	bool HandleDragStart(DesktopDragObject& drag_object) const;
	bool AcceptsDragDrop(DesktopDragObject& drag_object) const;
	bool HandleDragDrop(DesktopDragObject& drag_object);

private:
	const DesktopSpeedDial* m_entry;
};

#endif // SPEED_DIAL_DRAG_HANDLER_H
