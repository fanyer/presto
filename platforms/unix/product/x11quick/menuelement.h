/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2003-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Morten Stenshorne
*/

#ifndef MENUELEMENT_H
#define MENUELEMENT_H


class MenuElement
{
public:
	enum MouseEvent
	{
		POINTER_MOTION,
		BUTTON_PRESS,
		BUTTON_RELEASE
	};

	virtual ~MenuElement() { }

	
	virtual bool IsMenuBar() const = 0;

	/**
	 * Stop (hide) the popup menu.
	 * @param cancel whether the menu was cancelled or not
	 */
	virtual void Stop(bool cancel) = 0;

	/**
	 * Hide menu if it does not contain coordinates. The parent, grand parent etc
	 * will be hidden in successive order if they do no contain the coordinate
	 * 
	 * @param gx Global x coordinate
	 * @param gy Global y coordinate
	 */
	virtual MenuElement* Hide(int gx, int gy) = 0;

	/**
	 * Hide current child menu if any
	 */
	virtual void CancelSubMenu() = 0;

	/**
	 * Show next menu. Should only open a menu in a menu bar
	 */
	virtual void ShowNext(MenuElement* current, bool next) = 0;

	/** Handle a mouse event
	 * @return If the event was handled (or at least within the view for this
	 * menu element, return true.
	 */
	virtual bool HandleMouseEvent(int gx, int gy, MouseEvent event, bool confine_to_area) = 0;
	
	/**
	 * Show submenu located in an open menu tree
	 *
	 * @param gx Global x coordiante
	 * @param gy Global y coordinate
	 *
	 * @return true if coordinate is inside the menu, otherwise false
	 */
	virtual bool ShowSubmenu(int gx, int gy) = 0;

	/**
	 * Returns the popup menu item that exists a the specfied 
	 * coordinate. The parent, grand parent etc are quieried if there
	 * no match in the current element.
	 *
	 * This function only make sense for popup menus
	 *
	 * @param gx Global x coordiante
	 * @param gy Global y coordinate
	 *
	 * @return Matched popup menu item or NULL
	 */
	virtual class PopupMenuComponent* ItemAt(int gx, int gy) = 0;
};

#endif // MENUELEMENT_H
