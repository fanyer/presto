/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef GADGET_CONTROL_BUTTONS
#define GADGET_CONTROL_BUTTONS


#ifdef GADGET_SUPPORT
#ifdef WIDGET_CONTROL_BUTTONS_SUPPORT
#include "modules/widgets/OpButton.h"
#include "modules/windowcommander/OpWindowCommander.h"

#ifdef WIDGETS_UPDATE_SUPPORT
#include "adjunct/autoupdate_gadgets/GadgetUpdateController.h"
#endif //WIDGETS_UPDATE_SUPPORT

class GadgetButtonMouseListener;
class OpMoveButton;

#define WCB_BUTTON_SIZE 18
#define WCB_PANEL_WIDTH 129
#define WCB_PANEL_WIDTH_UPDATE 40
#define WCB_UPDATE_BUTON_SIZE 28
#define WCB_MARGIN 3
#define WCB_SEPARATOR_WIDTH 2

class GadgetControlButtons: public OpWidget
#ifdef WIDGETS_UPDATE_SUPPORT
	,GadgetUpdateListener
#endif //WIDGETS_UPDATE_SUPPORT
{
public:
	GadgetControlButtons();

	/**
	* Init this control
	* @param update_only - should be set to true for widgets id gadget mode which have WCB off
	*						WCB will work then only with update button displayed 
							in case update is available
	*/
	OP_STATUS Init(BOOL update_only = FALSE);

	virtual const char*		GetInputContextName() {return "Gadget Control Buttons";}
	  
    virtual void OnLayout();
	
	virtual void OnMouseMove(OpWidget *widget, const OpPoint &point);	 
	virtual void OnMouseLeave(OpWidget *widget);
	virtual void OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);
	
	virtual void SetMouseListener(GadgetButtonMouseListener* listener);
	
#ifdef WIDGETS_UPDATE_SUPPORT
	//=================GadgetUpdateListener==================

	/*
	*	Used to display update button in WCB, which indicate resize of WCB panel
	*   in case WCB works in normal mode (when m_update_only is false)
	* 	or displays/hides whole wcb panel, when m_update_only is true
	*/
	void OnGadgetUpdateAvailable(GadgetUpdateInfo* data,BOOL visible);
	void OnGadgetUpdateStarted(GadgetUpdateInfo* data);
	
	void SetUpdateController(WidgetUpdater& ctrl);

#endif //WIDGETS_UPDATE_SUPPORT
	
private:
	virtual BOOL OnInputAction(OpInputAction* action);

	OpButton*	m_close_button;
	OpButton*	m_minimize_button;
	OpButton*	m_menu_button;
	OpMoveButton *m_move_button;
	OpWidget*	m_separator_bar;
	OpWidget*	m_separator_bar1;
	OpWidget*	m_separator_bar2;
	OpPoint		m_mousedown_point;
	GadgetButtonMouseListener* m_mouse_listener;
	OpPoint		m_mouse_position;
	BOOL		m_is_dragging;
	INT32		m_width;

#ifdef WIDGETS_UPDATE_SUPPORT
	BOOL		m_update_only;
	WidgetUpdater* m_updater;
	OpButton*	m_update_button;
	OpWidget*	m_separator_bar3;
	BOOL		m_update_visible;
#endif //WIDGETS_UPDATE_SUPPORT
};


/*
* Class created only to inherit OnSetCursor and set up cursor to CURSOR_MOVE
* for "move" button on WCB panel 
*/
class OpMoveButton : public OpButton
{
 public:
	virtual void OnSetCursor(const OpPoint &point)
	{
		SetCursor(CURSOR_MOVE);
	}   
	
#ifdef SKIN_SUPPORT
	static OP_STATUS Construct(OpMoveButton** obj);
	
	OpMoveButton():OpButton(OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE){}
#endif
	
};
		


class GadgetButtonMouseListener
{
  public: 
	virtual void OnMouseMove() = 0;
	virtual void OnMouseLeave() = 0;
	virtual void OnMinimize() = 0;
	virtual void OnClose() = 0;
	virtual void OnShowMenu() = 0;
	virtual void OnDragMove(INT32 x, INT32 y) = 0;
	virtual void OnPanelSizeChanged() = 0;
	virtual void BlockPanelHide(BOOL block) = 0;
};
#endif //WIDGET_CONTROL_BUTTONS_SUPPORT 

#endif // GADGET_SUPPORT

#endif //GADGET_CONTROL_BUTTONS
