/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef SPEEDDIAL_GLOBAL_CONFIG_DIALOG_H
#define SPEEDDIAL_GLOBAL_CONFIG_DIALOG_H

#if defined(SUPPORT_SPEED_DIAL)

#include "adjunct/quick_toolkit/widgets/Dialog.h"

/***********************************************************************************
**
**  SpeedDialGlobalConfigDialog
**
***********************************************************************************/

class OpButton;
class OpLabel;
class OpZoomSlider;

class SpeedDialGlobalConfigDialog : public Dialog
{
public:
	SpeedDialGlobalConfigDialog();
	~SpeedDialGlobalConfigDialog();

	//dialog hooks
	Type				GetType()				{return DIALOG_TYPE_SPEEDDIAL_GLOBAL_CONFIG;}
	const char*			GetWindowName()			{return "Speed Dial Global Configuration Dialog";}
	BOOL				GetModality()			{return FALSE;}
	BOOL				HasCenteredButtons()	{return FALSE;}
	INT32				GetButtonCount()		{return 0;}
	BOOL				GetOverlayed()			{return TRUE;}
	void				GetOverlayPlacement(OpRect& initial_placement, OpWidget* overlay_layout_widget);
	DialogAnimType		GetPosAnimationType()	{return POSITIONAL_ANIMATION_NONE;}
	BOOL				IsMovable()				{return FALSE;}
	void				QueryZoomSettings(double &min, double &max, double &step, const OpSlider::TICK_VALUE* &tick_values, UINT8 &num_tick_values);
	
	// opwidgetlistener hooks
	void				OnChange(OpWidget *widget, BOOL changed_by_mouse);
	
	void				OnInit();

	//desktopwindow listener hooks
	void				OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated);

	void 				DialogPlacementRelativeWidget(OpWidget* widget) { m_placement_relative_widget = widget; }
	void				OnThumbnailScaleChanged(double scale);

private:
	OP_STATUS			PopulateImageLayout();
	OP_STATUS			PopulateColumnLayout();
	OP_STATUS			PopulateThumbnailSize();

	OpString			m_old_background_filename;
	DesktopWindow*		m_extra_watched_window;
	OpWidget*			m_placement_relative_widget;
	OpZoomSlider*		m_zoom_slider;
	OpLabel*			m_zoom_value;
};

#endif // defined(SUPPORT_SPEED_DIAL)

#endif // SPEEDDIAL_GLOBAL_CONFIG_DIALOG_H
