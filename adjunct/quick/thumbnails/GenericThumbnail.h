/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */


#ifndef OP_GENERIC_THUMBNAIL_H
#define OP_GENERIC_THUMBNAIL_H

#include "modules/widgets/OpWidget.h"
#include "adjunct/quick/animation/QuickWidgetHoverBlend.h"


class GenericThumbnailContent;
class OpButton;
class OpProgressBar;

/**
 * @author The people who made the original OpThumbnailWidget
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class GenericThumbnail : public OpWidget
{
public:
	struct Config
	{
		Config()
			: m_title_border_image(NULL)
			, m_close_border_image(NULL)
			, m_close_foreground_image(NULL)
			, m_busy_border_image(NULL)
			, m_busy_foreground_image(NULL)
			, m_drag_type(UNKNOWN_TYPE)
		{
		}

		const char* m_title_border_image;
		const char* m_close_border_image;
		const char* m_close_foreground_image;
		const char* m_busy_border_image;
		const char* m_busy_foreground_image;
		Type m_drag_type;
	};

	GenericThumbnail();

	OP_STATUS Init(const Config& config);

	/**
	 * Replaces the current content (if any) with new content.  Any old content
	 * is deleted.
	 *
	 * @param content new content, ownership transferred
	 * @return status
	 */
	OP_STATUS SetContent(GenericThumbnailContent* content);

	void SetSelected(BOOL selected);

	BOOL GetLocked() { return m_locked; }
	void SetLocked(BOOL lock) { m_locked = lock; }

	const uni_char* GetTitle() const;
	int GetNumber() const;

	void GetPadding(INT32& width, INT32& height);

	virtual Type GetType() { return WIDGET_TYPE_THUMBNAIL; }

	virtual void GetRequiredThumbnailSize(INT32& width, INT32& height);

	// OpWidget
	virtual void OnDeleted();
	virtual void OnLayout();
	virtual void GetRequiredSize(INT32& width, INT32& height);
	virtual void OnMouseMove(const OpPoint& point);
	virtual void OnMouseLeave();
	virtual void OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks);
	virtual void OnMouseUp(const OpPoint& point, MouseButton button, UINT8 nclicks);

	// OpWidgetListener
	virtual BOOL OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint &menu_point, const OpRect *avoid_rect, BOOL keyboard_invoked);
	virtual void OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y);
	virtual void OnDragMove(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y);
	virtual void OnDragDrop(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y);

	// OpToolTipListener
	virtual BOOL HasToolTipText(OpToolTip* tooltip);
	virtual void GetToolTipText(OpToolTip* tooltip, OpInfoText& text);

	// OpInputContext
	virtual BOOL OnInputAction(OpInputAction* action);

	// QuickOpWidgetBase
	virtual BOOL ScrollOnFocus() { return TRUE; }

protected:
	GenericThumbnailContent* GetContent() const { return m_content; }
	OP_STATUS OnContentChanged();

private:
	OP_STATUS OnNewContent();

	void SetHovered(BOOL hovered);
	void SetCloseButtonHovered(BOOL close_hovered);

	void DoLayout();
	void ComputeRelativeLayoutRects(OpRect* rects);

	bool HasCloseButton() const;
	bool HasTitle() const { return OpStringC(GetTitle()).HasContent() ? true : false; }

	Config m_config;
	GenericThumbnailContent* m_content;

	OpButton* m_close_button;
	OpButton* m_title_button;
	OpProgressBar* m_busy_spinner;

	BOOL m_hovered;
	BOOL m_locked;
	BOOL m_close_hovered;

	QuickWidgetHoverBlend m_blend;
};

#endif // OP_GENERIC_THUMBNAIL_H
