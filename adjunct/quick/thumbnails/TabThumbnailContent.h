/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */


#ifndef TAB_THUMBNAIL_CONTENT_H
#define TAB_THUMBNAIL_CONTENT_H

#include "adjunct/quick/thumbnails/GenericThumbnailContent.h"

struct OpToolTipThumbnailPair;

/**
 * @author The people who made the original OpThumbnailWidget
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class TabThumbnailContent : public GenericThumbnailContent
{
public:
	TabThumbnailContent();
	virtual ~TabThumbnailContent();

	OP_STATUS Init(OpToolTipThumbnailPair& data);

	virtual OpButton* GetButton() const { return m_button; }
	virtual OP_STATUS GetButtonInfo(ButtonInfo& info) const;

	virtual const uni_char* GetTitle() const;
	virtual int GetNumber() const { return -1; }
	virtual OP_STATUS GetCloseButtonInfo(ButtonInfo& info) const;
	virtual BOOL IsBusy() const { return FALSE; }

	virtual MenuInfo GetContextMenuInfo() const { return MenuInfo(); }

	virtual OP_STATUS HandleMidClick() { return OpStatus::OK; }

	virtual bool HandleDragStart(DesktopDragObject& drag_object) const;
	virtual bool AcceptsDragDrop(DesktopDragObject& drag_object) const { return false; }
	virtual void HandleDragDrop(DesktopDragObject& drag_object) {}

	virtual OP_STATUS Refresh() { return OpStatus::OK; }
	virtual OP_STATUS Zoom() { return OpStatus::OK; }

private:
	OpToolTipThumbnailPair* m_data;
	OpButton* m_button;
};

#endif // TAB_THUMBNAIL_CONTENT_H
