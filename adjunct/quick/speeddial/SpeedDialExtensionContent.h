/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef SPEED_DIAL_EXTENSION_CONTENT_H
#define SPEED_DIAL_EXTENSION_CONTENT_H

#include "adjunct/quick/thumbnails/GenericThumbnailContent.h"
#include "adjunct/quick/speeddial/SpeedDialDragHandler.h"
#include "adjunct/quick/speeddial/SpeedDialGenericHandler.h"

class BitmapButton;
class BitmapOpWindow;
class DesktopSpeedDial;

class SpeedDialExtensionContent : public GenericThumbnailContent						
{
public:
	explicit SpeedDialExtensionContent(const DesktopSpeedDial& entry);
	virtual ~SpeedDialExtensionContent();
	
	OP_STATUS Init();

	virtual OpButton* GetButton() const;
	virtual OP_STATUS GetButtonInfo(ButtonInfo& info) const;

	virtual const uni_char* GetTitle() const;
	virtual int GetNumber() const;
	virtual OP_STATUS GetCloseButtonInfo(ButtonInfo& info) const;
	virtual BOOL IsBusy() const { return m_busy; }

	virtual MenuInfo GetContextMenuInfo() const;

	virtual OP_STATUS HandleMidClick();

	virtual bool HandleDragStart(DesktopDragObject& drag_object) const;
	virtual bool AcceptsDragDrop(DesktopDragObject& drag_object) const;
	virtual void HandleDragDrop(DesktopDragObject& drag_object);

	virtual OP_STATUS Refresh();
	virtual OP_STATUS Zoom() { return OpStatus::OK; }

private:	
	const DesktopSpeedDial* m_entry;
	BitmapButton* m_button;
	BitmapOpWindow* m_bitmap_window;
	SpeedDialDragHandler m_drag_handler;
	SpeedDialGenericHandler m_generic_handler;
	BOOL m_busy;
};


#endif // SPEED_DIAL_EXTENSION_CONTENT_H
