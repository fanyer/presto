/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */


#ifndef SPEED_DIAL_THUMBNAIL_H
#define SPEED_DIAL_THUMBNAIL_H

#include "adjunct/quick/speeddial/SpeedDialListener.h"
#include "adjunct/quick/thumbnails/GenericThumbnail.h"
#include "adjunct/quick/managers/AnimationManager.h"

class DesktopSpeedDial;
class OpSpeedDialView;

class SpeedDialThumbnail :
	public GenericThumbnail,
	public SpeedDialEntryListener,
	public QuickAnimationListener
{
public:
	SpeedDialThumbnail();

	static OP_STATUS Construct(SpeedDialThumbnail** obj);

	OP_STATUS SetEntry(const DesktopSpeedDial* entry);
	const DesktopSpeedDial* GetEntry() const { return m_entry; }

	virtual void AnimateThumbnailIn();
	virtual void AnimateThumbnailOut(bool use_undo);

	virtual void GetRequiredThumbnailSize(INT32& width, INT32& height);

	OpSpeedDialView* GetParentOpSpeedDial() const;

	// SpeedDialEntryListener
	virtual void OnSpeedDialUIChanged(const DesktopSpeedDial &sd) { OpStatus::Ignore(OnContentChanged()); }
	virtual void OnSpeedDialExpired();
	virtual void OnSpeedDialEntryScaleChanged();

	// OpWidget
	virtual void OnDeleted();
	virtual void OnMouseMove(const OpPoint& point);
	virtual void OnMouseLeave();
	virtual void OnMouseDown(const OpPoint& point, MouseButton button, UINT8 nclicks);
	virtual void OnMouseUp(const OpPoint& point, MouseButton button, UINT8 nclicks);
	virtual void OnCaptureLost();

	// OpWidgetListener
	virtual void OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y);
	virtual void OnFocusChanged(OpWidget *widget, FOCUS_REASON reason );

	// OpInputContext
	virtual BOOL OnInputAction(OpInputAction* action);
	virtual void SetFocus(FOCUS_REASON reason);

	// QuickAnimationListener
	virtual void OnAnimationComplete(OpWidget *anim_target, int callback_param);

private:
	/**
	 * Get another SpeedDialThumbnail in the same parent that is intersecting a fair
	 * amount with this one.
	 */
	SpeedDialThumbnail* GetDropThumbnail();

	/**
	 * Start drag'n'drop mode.
	 *
	 * @return @c true iff drag is possible
	 */
	bool StartDragging(INT32 x, INT32 y);

	/**
	 * Stop inline dragging (moving to rearrange dials)
	 */
	void StopDragging();

	bool IsDragging() const { return m_drag_object != NULL; }
	
	const DesktopSpeedDial* m_entry;

	DesktopDragObject* m_drag_object;
	OpPoint m_mousedown_point;
	bool m_mouse_down_active;		// true when MouseDown has been called
};

#endif // SPEED_DIAL_THUMBNAIL_H
