/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- 
 * 
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved. 
 * 
 * This file is part of the Opera web browser.  It may not be distributed 
 * under any circumstances. 
 * 
 * @author Cezary Kulakowski (ckulakowski)
 */ 

#ifndef OP_INDICATOR_BUTTON_H
#define OP_INDICATOR_BUTTON_H

#include "modules/widgets/OpButton.h"

class DesktopWindow;

/**
 * This class represents indicator which informs user about state of
 * permissions for camera and geolocation set for the page. It also informs
 * user if geolocation or camera is currently in use. Indicator consists of three
 * icons: geolocation, camera and separator. Geolocation and camera icons can be
 * in one of four states:
 * 1. disabled (icon is hidden) - permission for corresponding device is not granted
 * 2. enabled and active (black icon) - permission is granted and device is currently in use
 * 3. enabled and inactive (grey icon) - permission is granted but device is not in use
 * 4. enabled and inverted (white icon) - it is used only in tab bar stack button
 *
 * Indicators are located in following places:
 *
 * 1. badge (for current page)
 * 2. Tab button (for background tabs)
 * 3. Tab stack button (for stacked, background tabs)
 *
 * Note: in tab button and tab stack button we don't display inactive icons.
 * In addition in tab stack button we use inverted (white) icons.
 * For details see PGDSK-1013.
 */
class OpIndicatorButton : public OpButton
{
public:
	/**
	  * Values of this enum represent icons types which are the part of
	  * the indicator (see class description).
	  */
	enum IndicatorType
	{
		CAMERA = 1,
		GEOLOCATION,
		SEPARATOR
	};

	/**
	  * Values of this enum represent state of the indicator icons. Every icon
	  * (camera/geolocation/separator) can be in one of the three states:
	  * - ACTIVE - black icon
	  * - INACTIVE - grey icon
	  * - INVERTED - white icon
	  */
	enum IconState
	{
		ACTIVE,
		INACTIVE,
		INVERTED
	};

	/**
	  * Values of this enum determines position of vertical separator in relation
	  * to other icons (camera and/or geolocation).
	  */
	enum SeparatorPosition
	{
		LEFT,
		RIGHT
	};

	OpIndicatorButton(DesktopWindow* desktop_window, SeparatorPosition pos = LEFT)
		: m_desktop_window(desktop_window)
		, m_camera_button(NULL)
		, m_geolocation_button(NULL)
		, m_separator_button(NULL)
		, m_indicator_type(0)
		, m_is_camera_active(false)
		, m_is_geolocation_active(false)
		, m_show_separator(true)
		, m_separator_position(pos)
	{}

	OP_STATUS Init(const char* border_skin = "Toolbar Button Skin");

	// OpWidget
	virtual void OnLayout();
	virtual void OnShow(BOOL show);


	INT32 GetPreferredWidth();
	INT32 GetPreferredHeight();

	/**
	  * Sets state for given indicator part (camera/geolocation/separator).
	  * @param type icon type for which state should be changed
	  * @param state new icon state
	  */
	void SetIconState(IndicatorType type, IconState state);

	bool IsCameraActive() const { return m_is_camera_active; }

	bool IsGeolocationActive() const { return m_is_geolocation_active; }

	/**
	  * Adds (enables) given indicator icon
	  * @param type type of the icon which should be enabled
	  */
	void AddIndicator(IndicatorType type) { UpdateButtons(type, true); }

	/** Removes (disables) given indicator icon
	  * @param type type of the icon which should be disabled
	  */
	void RemoveIndicator(IndicatorType type) { UpdateButtons(type, false); }

	/** Sets indicator type. Parameter @a type is a sum of states
	  * of all device icons. First bit represents state (enabled/disabled)
	  * of camera icon and second bit represents state of gelocation icon.
	  */
	void SetIndicatorType(short type);

	/** Returns indicator type which is a sum of states of all device icons.
	  * First bit represents state (enabled/disabled) of camera icon
	  * and second bit represents state of gelocation icon.
	  */
	short GetIndicatorType() { return m_indicator_type; }

	/** Sets if separator located on right or left side of indicator icons
	  * should be displayed.
	  */
	void ShowSeparator(bool show_separator) { m_show_separator = show_separator; }

private:
	void UpdateButtons(IndicatorType type, bool set);
	void LayoutButton(OpButton* button, OpRect& orig_rect);

	DesktopWindow*	m_desktop_window;
	OpButton*		m_camera_button;
	OpButton*		m_geolocation_button;
	OpButton*		m_separator_button;
	short			m_indicator_type;
	bool			m_is_camera_active;
	bool			m_is_geolocation_active;
	bool			m_show_separator;
	SeparatorPosition m_separator_position;
};


#endif // OP_INDICATOR_BUTTON_H
