/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */


#ifndef SPEED_DIAL_PLUS_CONTENT_H
#define SPEED_DIAL_PLUS_CONTENT_H

#include "adjunct/quick/speeddial/SpeedDialDragHandler.h"
#include "adjunct/quick/thumbnails/GenericThumbnailContent.h"

class DesktopSpeedDial;

/**
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class SpeedDialPlusContent : public GenericThumbnailContent
{
public:
	explicit SpeedDialPlusContent(const DesktopSpeedDial& entry);
	virtual ~SpeedDialPlusContent();

	OP_STATUS Init();

	virtual OpButton* GetButton() const { return m_button; }
	virtual OP_STATUS GetButtonInfo(ButtonInfo& info) const;

	virtual const uni_char* GetTitle() const { return NULL; }
	virtual int GetNumber() const { return -1; }
	virtual OP_STATUS GetCloseButtonInfo(ButtonInfo& info) const { return OpStatus::OK; }
	virtual BOOL IsBusy() const { return FALSE; }

	virtual MenuInfo GetContextMenuInfo() const { return MenuInfo(); }

	virtual OP_STATUS HandleMidClick();

	virtual bool HandleDragStart(DesktopDragObject& drag_object) const { return false; }
	virtual bool AcceptsDragDrop(DesktopDragObject& drag_object) const;
	virtual void HandleDragDrop(DesktopDragObject& drag_object);

	virtual OP_STATUS Refresh() { return OpStatus::OK; }
	virtual OP_STATUS Zoom() { return OpStatus::OK; }

private:
	const DesktopSpeedDial* m_entry;
	OpButton* m_button;
	SpeedDialDragHandler m_drag_handler;
};

#endif // SPEED_DIAL_PLUS_CONTENT_H
