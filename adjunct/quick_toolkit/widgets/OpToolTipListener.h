/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef OPTOOLTIP_LISTENER_H
#define OPTOOLTIP_LISTENER_H

#include "modules/inputmanager/inputaction.h"

class OpToolTip;

struct OpToolTipThumbnailPair
{
	OpToolTipThumbnailPair() : is_fixed_image(FALSE), show_close_button(FALSE), can_activate_button(FALSE), window_id(0), active(FALSE) {}

	Image		thumbnail;
	OpString	title;
	BOOL		is_fixed_image;
	BOOL		show_close_button;
	BOOL		can_activate_button;
	INT32		window_id;
	BOOL		active;			// is this the active window
};

class OpToolTipListener
{
public:
	virtual ~OpToolTipListener();
	virtual BOOL HasToolTipText(OpToolTip* tooltip) {return TRUE;}
	virtual void GetToolTipText(OpToolTip* tooltip, OpInfoText& text) {}
	virtual void GetToolTipThumbnailText(OpToolTip* tooltip, OpString& title, OpString& url_string, URL& url) {}
	virtual INT32 GetToolTipDelay(OpToolTip* tooltip);
	virtual INT32 GetToolTipWindowID() {return 0;}

	/*
	** This method is used to get a collection of thumbnail and text pairs from the listener. It is only called
	** when tooltip type is TOOLTIP_TYPE_MULTIPLE_THUMBNAILS. Its primary use is to show all pages that are a member
	** of a tab stack in the same mechanism as regular thumbnail tooltips.
	** The ownership of the collection is transferred to the tooltip after this call.
	**
	*/
	virtual void GetToolTipThumbnailCollection(OpToolTip* tooltip, OpVector<OpToolTipThumbnailPair>& thumbnail_collection) {}

	enum TOOLTIP_TYPE 
	{
		TOOLTIP_TYPE_NORMAL,				///< Tooltip containing text.
		TOOLTIP_TYPE_THUMBNAIL,				///< Tooltip that may contain image or text or both.
		TOOLTIP_TYPE_MULTIPLE_THUMBNAILS	///< Tooltip may contain one or more thumbnail/text pairs. The listener is expected to implement GetToolTipThumbnailCollection() when returning this value.
	};
	// Return which type of tooltip.
	virtual TOOLTIP_TYPE GetToolTipType(OpToolTip* tooltip) { return TOOLTIP_TYPE_NORMAL; }

	// Return TRUE if the thumbnail should not be visible (when type is TOOLTIP_TYPE_THUMBNAIL)
	virtual BOOL GetHideThumbnail(OpToolTip* tooltip) {return FALSE;}

	// Returns the hotlist id of the item that has focus
	virtual INT32 GetToolTipItem(OpToolTip* tooltip) {return 0;}

	// Returns the delay between showing different tooltips (when the focus moves).
	// By default it is set to zero, so users can pan a list of items and quickly scan the tooltips.
	// When the list of items is large and 'tooltip panning' is less important it's better to
	// return non-zero (100ms f.i.) so the tooltip isn't annoyingly sticky.
	virtual INT32 GetToolTipUpdateDelay(OpToolTip* tooltip) {return 0;}

	// Return whether the tooltip should shown while the user is dragging something.
	// By default, tooltips are not shown while dragging, but in some cases you might want
	// to explicitly show a tooltip about the dragging.
	virtual BOOL GetShowWhileDragging(OpToolTip* tooltip) {return FALSE;}

	enum PREFERRED_PLACEMENT {
		PREFERRED_PLACEMENT_LEFT,
		PREFERRED_PLACEMENT_RIGHT,
		PREFERRED_PLACEMENT_TOP,
		PREFERRED_PLACEMENT_BOTTOM
	};
	/** Get the preferred placement for the tooltip.
		@param tooltip tooltip in question
		@param ref_rect the rectangle in screen coordinates for the object showing the tooltip.
		@param placement the preferred placement related to ref_rect. If there is no room for the tooltip on
						 that side, the opposite side will be tried.
		*/
	virtual BOOL GetPreferredPlacement(OpToolTip* tooltip, OpRect &ref_rect, PREFERRED_PLACEMENT &placement) { return FALSE; }

	/** Return whether one can hover over the tooltip with the mouse. This method should be overridden by
		listeners that allow mouse hovering over the tooltip and return TRUE.
		@return TRUE if one can hover over the tooltip with the mouse, else return FALSE.
	*/
	virtual BOOL CanHoverToolTip() const { return FALSE; }

};


#endif // OPTOOLTIP_LISTENER_H
