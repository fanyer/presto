/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */


#ifndef GENERIC_THUMBNAIL_CONTENT_H
#define GENERIC_THUMBNAIL_CONTENT_H

#include "modules/inputmanager/inputaction.h"
#include "modules/util/OpTypedObject.h"

class OpButton;
class DesktopDragObject;

/**
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class GenericThumbnailContent
{
public:
	struct ButtonInfo
	{
		ButtonInfo() : m_action(OpInputAction::ACTION_UNKNOWN), m_action_data(0) {}

		bool HasContent() const { return m_action != OpInputAction::ACTION_UNKNOWN; }

		OpString8 m_name;
		OpInputAction::Action m_action;
		INTPTR m_action_data;
		OpString m_tooltip_text;
		OpString m_accessibility_text;
	};

	struct MenuInfo
	{
		MenuInfo() : m_name(NULL), m_context(0) {}
		const char* m_name;
		INTPTR m_context;
	};

	virtual ~GenericThumbnailContent() {}

	virtual OpButton* GetButton() const = 0;
	virtual OP_STATUS GetButtonInfo(ButtonInfo& info) const = 0;

	virtual const uni_char* GetTitle() const = 0;
	virtual int GetNumber() const = 0;
	virtual OP_STATUS GetCloseButtonInfo(ButtonInfo& info) const = 0;
	virtual BOOL IsBusy() const = 0;

	virtual MenuInfo GetContextMenuInfo() const = 0;

	virtual OP_STATUS HandleMidClick() = 0;

	/**
	 * @return @c true iff @a drag_object is ready to be dragged
	 */
	virtual bool HandleDragStart(DesktopDragObject& drag_object) const = 0;
	virtual bool AcceptsDragDrop(DesktopDragObject& drag_object) const = 0;
	virtual void HandleDragDrop(DesktopDragObject& drag_object) = 0;

	virtual OP_STATUS Refresh() = 0;
	virtual OP_STATUS Zoom() = 0;
};

#endif // GENERIC_THUMBNAIL_CONTENT_H
