/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef OPERAMENU_DIALOG_H
#define OPERAMENU_DIALOG_H

#ifdef QUICK_NEW_OPERA_MENU

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "adjunct/quick/widgets/OpThumbnailWidget.h"
#include "modules/hardcore/timer/OpTimer.h"

class QuickAnimationWindowObject;

/***********************************************************************************
**
**  OperaMenuDialog
**
***********************************************************************************/

class OperaMenuDialog : public Dialog
{
public:
	OperaMenuDialog();
	~OperaMenuDialog();

	Type				GetType()				{return DIALOG_TYPE_OPERA_MENU;}
	const char*			GetWindowName()			{return "Opera Menu Dialog";}
	BOOL				GetModality()			{return FALSE;}
	BOOL				HideWhenDesktopWindowInActive(){ return TRUE; }
	BOOL				GetDropShadow()			{return TRUE;}
	BOOL				GetIsPopup()			{return TRUE;}
	BOOL				GetShowPagesAsTabs()	{return TRUE;}
	BOOL				GetCenterPages()		{return FALSE;}

	// OpWidgetListener hooks
	void				OnChange(OpWidget *widget, BOOL changed_by_mouse);
	void				OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);
	void				OnMouseMove(OpWidget *widget, const OpPoint &point);
	void				OnClick(OpWidget *widget, UINT32 id);
	
	// OpTimerListener hooks
	void				OnTimeOut(OpTimer* timer);

	// Subclassing hooks of DesktopWindow
	void				OnActivate(BOOL activate, BOOL first_time);

	void				OnInit();

	BOOL				OnInputAction(OpInputAction* action);

	void				GetPlacement(OpRect& initial_placement);
	INT32				GetButtonCount();
	void				GetButtonInfo(INT32 index, OpInputAction*& action, OpString& text, BOOL& is_enabled, BOOL& is_default, OpString8& name);
	
	//public interface

private:
	OpTabs				*m_tabs;
	OpTimer				m_timer;
	INT32				m_hovered_index;
	QuickAnimationWindowObject *m_window_animation;
};

#endif // QUICK_NEW_OPERA_MENU

#endif // OPERAMENU_DIALOG_H
