/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef GADGET_SUPPORT
#ifdef WIDGET_CONTROL_BUTTONS_SUPPORT
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/widgetruntime/widgets/OpGadgetControlButtons.h"

#include "modules/widgets/WidgetContainer.h"


GadgetControlButtons::GadgetControlButtons():
											m_close_button(NULL),
											m_minimize_button(NULL),
											m_menu_button(NULL),
											m_move_button(NULL),
											m_separator_bar(NULL),
											m_separator_bar1(NULL),
											m_separator_bar2(NULL),
											m_mouse_listener(NULL),
											m_is_dragging(FALSE)
#ifdef WIDGETS_UPDATE_SUPPORT
											,m_update_only(FALSE)
											,m_updater(NULL)
											,m_update_button(NULL)
											,m_separator_bar3(NULL)
											,m_update_visible(FALSE)										
#endif //WIDGETS_UPDATE_SUPPORT
											{

											}
OP_STATUS GadgetControlButtons::Init(BOOL update_only)
{
#ifdef WIDGETS_UPDATE_SUPPORT
	m_update_only = update_only;
	m_width = update_only ? WCB_PANEL_WIDTH_UPDATE : WCB_PANEL_WIDTH - WCB_UPDATE_BUTON_SIZE;
#else
	m_width = WCB_PANEL_WIDTH - WCB_UPDATE_BUTON_SIZE;
#endif //WIDGETS_UPDATE_SUPPORT


	GetBorderSkin()->SetImage("Widget Control Buttons Bar");	
	SetSkinned(TRUE);
	SetListener(this);
#ifdef WIDGETS_UPDATE_SUPPORT	
	if (!update_only)
#endif //WIDGETS_UPDATE_SUPPORT
	{
		RETURN_IF_ERROR(OpButton::Construct(&m_close_button, OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE));
		AddChild(m_close_button, TRUE);
		m_close_button->GetBorderSkin()->SetImage("Widget Control Buttons Background");
		m_close_button->GetForegroundSkin()->SetImage("Widget Control Buttons Close");
		m_close_button->SetRestrictImageSize(FALSE);	
		m_close_button->SetListener(this);
		OpInputAction *action = OP_NEW(OpInputAction, (OpInputAction::ACTION_CLOSE_WIDGET));	
		RETURN_OOM_IF_NULL(action);
		m_close_button->SetAction(action);
		
		
		RETURN_IF_ERROR(OpButton::Construct(&m_minimize_button, OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE));
		AddChild(m_minimize_button, TRUE);
		m_minimize_button->GetBorderSkin()->SetImage("Widget Control Buttons Background");
		m_minimize_button->GetForegroundSkin()->SetImage("Widget Control Buttons Minimize");
		m_minimize_button->SetRestrictImageSize(FALSE);
		m_minimize_button->SetListener(this);
		OpInputAction *action_minimize = OP_NEW(OpInputAction, (OpInputAction::ACTION_MINIMIZE_WINDOW));	
		RETURN_OOM_IF_NULL(action_minimize);
		m_minimize_button->SetAction(action_minimize);
		
	 
		RETURN_IF_ERROR(OpButton::Construct(&m_menu_button, OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE));
		AddChild(m_menu_button, TRUE);
		m_menu_button->GetBorderSkin()->SetImage("Widget Control Buttons Background");
		m_menu_button->GetForegroundSkin()->SetImage("Widget Control Buttons Settings");
		m_menu_button->SetRestrictImageSize(FALSE);
		m_menu_button->SetListener(this);
		OpInputAction *action_menu = OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_POPUP_MENU_WITH_MENU_CONTEXT));	
		RETURN_OOM_IF_NULL(action_menu);
		m_menu_button->SetAction(action_menu);
		

		RETURN_IF_ERROR(OpMoveButton::Construct(&m_move_button));		
		AddChild(m_move_button, TRUE);
		m_move_button->GetForegroundSkin()->SetImage("Widget Control Buttons Move");
		m_move_button->SetRestrictImageSize(FALSE);
		m_move_button->SetListener(this);
		OpInputAction *action_move = OP_NEW(OpInputAction, (OpInputAction::ACTION_MOVE_ITEM_DOWN));	
		m_move_button->SetAction(action_move);
		

		RETURN_IF_ERROR(OpWidget::Construct(&m_separator_bar));
		AddChild(m_separator_bar, TRUE);
		RETURN_IF_ERROR(OpWidget::Construct(&m_separator_bar1));
		AddChild(m_separator_bar1, TRUE);			
		RETURN_IF_ERROR(OpWidget::Construct(&m_separator_bar2));
		AddChild(m_separator_bar2, TRUE);	
		
		m_separator_bar->GetBorderSkin()->SetImage("Widget Control Buttons Separator");
		m_separator_bar1->GetBorderSkin()->SetImage("Widget Control Buttons Separator");
		m_separator_bar2->GetBorderSkin()->SetImage("Widget Control Buttons Separator");
		
		m_separator_bar->SetSkinned(TRUE);
		m_separator_bar1->SetSkinned(TRUE);
		m_separator_bar2->SetSkinned(TRUE);
		m_separator_bar->SetListener(this);
		m_separator_bar1->SetListener(this);
		m_separator_bar2->SetListener(this);
		
		
		
	}
#ifdef WIDGETS_UPDATE_SUPPORT

	RETURN_IF_ERROR(OpButton::Construct(&m_update_button, OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE));
	AddChild(m_update_button, TRUE);
	m_update_button->GetBorderSkin()->SetImage("Widget Control Buttons Background");
	m_update_button->GetForegroundSkin()->SetImage("Widget Control Buttons Close");
	m_update_button->SetRestrictImageSize(FALSE);
	m_update_button->SetListener(this);
	OpInputAction *action_update = OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_GADGET_UPDATE));	
	RETURN_OOM_IF_NULL(action_update);
	m_update_button->SetAction(action_update);
	
	m_update_button->SetVisibility(FALSE);

	if (!update_only)
	{
		RETURN_IF_ERROR(OpWidget::Construct(&m_separator_bar3));
		AddChild(m_separator_bar3, TRUE);		
		m_separator_bar3->GetBorderSkin()->SetImage("Widget Control Buttons Separator");
		m_separator_bar3->SetSkinned(TRUE);
		m_separator_bar3->SetListener(this);
		m_separator_bar3->SetVisibility(FALSE);
	}
	else
	{
		SetVisibility(FALSE); // for update only support, disable visibility until update notification arrives
	}

#endif //WIDGETS_UPDATE_SUPPORT

	m_is_dragging = FALSE;

	return OpStatus::OK;
}


#ifdef WIDGETS_UPDATE_SUPPORT
void GadgetControlButtons::OnGadgetUpdateStarted(GadgetUpdateInfo* data)
{
	if (m_updater)
	{
		m_updater->GetController()->RemoveUpdateListener(this);
	}
}

void GadgetControlButtons::SetUpdateController(WidgetUpdater& ctrl)						
{
	m_updater = &ctrl;
	m_updater->GetController()->AddUpdateListener(this);
}

void GadgetControlButtons::OnGadgetUpdateAvailable(GadgetUpdateInfo* data,BOOL visible)
{
	if (!m_updater->NotificationAllowed() && visible)
		return;

	m_mouse_listener->BlockPanelHide(visible);
	m_update_visible = visible;

	m_update_button->SetVisibility(m_update_visible);
	if (m_update_only)
	{
		// for wcb with only update button, we can enable/disable whole panel
		SetVisibility(visible);	
	}
	else
	{
		// for wcb with other regular buttons enabled, we have to
		// enable / disable update button and resize panel
		m_separator_bar3->SetVisibility(m_update_visible);
		m_width = m_update_visible ? WCB_PANEL_WIDTH : WCB_PANEL_WIDTH - WCB_UPDATE_BUTON_SIZE;

		SetRect(OpRect(0,0,m_width,WCB_BUTTON_SIZE));
		WidgetContainer* cnt = NULL;
		OpWindow* wnd = NULL;
		cnt	= GetWidgetContainer();
		if (cnt && (wnd = cnt->GetWindow()))
		{
			wnd->SetInnerSize(m_width,WCB_BUTTON_SIZE);
			wnd->SetOuterSize(m_width,WCB_BUTTON_SIZE);
		}
		this->GenerateOnRelayout(TRUE);
		m_mouse_listener->OnPanelSizeChanged();
	}
}
#endif //WIDGETS_UPDATE_SUPPORT

void GadgetControlButtons::SetMouseListener(GadgetButtonMouseListener* listener)
{
	 m_mouse_listener = listener;
}


void GadgetControlButtons::OnMouseMove(OpWidget *widget, const OpPoint &point)
{
	if (m_mouse_listener)
	{
	    m_mouse_listener->OnMouseMove();   
	    
	    if (m_is_dragging && widget == m_move_button)
	    {
			INT32 x, y;

			x = point.x - m_mousedown_point.x;
			y = point.y - m_mousedown_point.y;
			m_mouse_listener->OnDragMove(x,y);						
	    }  
	}
}


void GadgetControlButtons::OnMouseLeave(OpWidget *widget) 
{
	  if (m_mouse_listener)
	  {
		 m_mouse_listener->OnMouseLeave();	
		 m_is_dragging = FALSE;	 
	  }
}

BOOL GadgetControlButtons::OnInputAction(OpInputAction* action)
{	 
	switch (action->GetAction())
	{
   		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
		
		   OpInputAction* child_action = action->GetChildAction();
		   
			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_CLOSE_WIDGET:
				{
					  if (m_mouse_listener)
					  {
							child_action->SetEnabled(TRUE);
							return TRUE;	 
					  }
					  break;
				 }
				 case OpInputAction::ACTION_MINIMIZE_WINDOW:
				 {
					  if (m_mouse_listener)
					  {
							child_action->SetEnabled(TRUE);
							return TRUE;	 
					  }
					  break;
				 }
				  case OpInputAction::ACTION_SHOW_POPUP_MENU_WITH_MENU_CONTEXT:
				 {
					  if (m_mouse_listener)
					  {
							child_action->SetEnabled(TRUE);
							return TRUE;	 
					  }
					  break;
				 }
#ifdef WIDGETS_UPDATE_SUPPORT
				  case OpInputAction::ACTION_SHOW_GADGET_UPDATE:
				  {
					  if(m_updater)
					  {
						  child_action->SetEnabled(TRUE);
						  return TRUE;	 
					  }
					  break;
				  }
#endif // WIDGETS_UPDATE_SUPPORT				
				
			}
			return FALSE;
		}

		case OpInputAction::ACTION_CLOSE_WIDGET:
		{
				//TogleUpdateB();
			  if (m_mouse_listener)
			  {
					m_mouse_listener->OnClose();	
					return TRUE;	 
			  }
			  break;
		}
		
		case OpInputAction::ACTION_MINIMIZE_WINDOW:
		{
			if (m_mouse_listener)
			{				
				m_mouse_listener->OnMinimize();
				return TRUE;	 
			}
			break;
		}
		case OpInputAction::ACTION_SHOW_POPUP_MENU_WITH_MENU_CONTEXT:
		 {
			  if (m_mouse_listener)
			  {						
				m_mouse_listener->OnShowMenu();
				return TRUE;	 
			  }
			  break;
		 }
#ifdef WIDGETS_UPDATE_SUPPORT
		case OpInputAction::ACTION_SHOW_GADGET_UPDATE:
		{
			if(m_updater)
			{
				m_updater->ShowUpdateDialog();
			}
			break;
		}
#endif// WIDGETS_UPDATE_SUPPORT

	}
	return OpWidget::OnInputAction(action);
}

void GadgetControlButtons::OnLayout()
{
	OpWidget::OnLayout();

#ifdef WIDGETS_UPDATE_SUPPORT
	if(m_update_only)
	{
		OpRect rect_update(m_width - WCB_UPDATE_BUTON_SIZE - 6 ,0,
			WCB_UPDATE_BUTON_SIZE,WCB_BUTTON_SIZE);
		m_update_button->SetRect(rect_update);

		return;
	}
#endif //WIDGETS_UPDATE_SUPPORT

	OpRect rect(m_width - WCB_BUTTON_SIZE - WCB_MARGIN  ,0,
				WCB_BUTTON_SIZE,WCB_BUTTON_SIZE);					
	m_close_button->SetRect(rect);
	
	OpRect rect_minimize(m_width - 2 * WCB_BUTTON_SIZE - 1 - WCB_MARGIN  ,0,
						 WCB_BUTTON_SIZE,WCB_BUTTON_SIZE);	
	m_minimize_button->SetRect(rect_minimize);
	
	OpRect rect_menu(m_width - 3 * WCB_BUTTON_SIZE - 2 - WCB_MARGIN  ,0,
					 WCB_BUTTON_SIZE,WCB_BUTTON_SIZE);	
	m_menu_button->SetRect(rect_menu);

#ifdef WIDGETS_UPDATE_SUPPORT
	if (!m_update_visible)
	{
#endif //WIDGETS_UPDATE_SUPPORT
		OpRect rect_move(WCB_MARGIN,0,
						 m_width - 3 * WCB_BUTTON_SIZE -8 - WCB_MARGIN ,WCB_BUTTON_SIZE);	
		m_move_button->SetRect(rect_move);

#ifdef WIDGETS_UPDATE_SUPPORT
	}
	else
	{
		OpRect rect_move(WCB_MARGIN,0,
			m_width - 3 * WCB_BUTTON_SIZE -8 - WCB_MARGIN - WCB_UPDATE_BUTON_SIZE -2,WCB_BUTTON_SIZE);	
		m_move_button->SetRect(rect_move);

		OpRect rect_update(m_width - 3 * WCB_BUTTON_SIZE - 3 - WCB_MARGIN - WCB_UPDATE_BUTON_SIZE ,0,
			WCB_UPDATE_BUTON_SIZE,WCB_BUTTON_SIZE);
		m_update_button->SetRect(rect_update);

		OpRect rect_separator3(m_width-3*WCB_BUTTON_SIZE-WCB_MARGIN-3*1 - WCB_UPDATE_BUTON_SIZE -1,0,WCB_SEPARATOR_WIDTH,WCB_BUTTON_SIZE);
		m_separator_bar3->SetRect(rect_separator3);
	}
#endif //WIDGETS_UPDATE_SUPPORT

	OpRect rect_separator(m_width-WCB_BUTTON_SIZE-1-WCB_MARGIN,0,WCB_SEPARATOR_WIDTH,WCB_BUTTON_SIZE);	
	OpRect rect_separator1(m_width-2*WCB_BUTTON_SIZE-WCB_MARGIN-2*1,0,WCB_SEPARATOR_WIDTH,WCB_BUTTON_SIZE);	
	OpRect rect_separator2(m_width-3*WCB_BUTTON_SIZE-WCB_MARGIN-3*1,0,WCB_SEPARATOR_WIDTH,WCB_BUTTON_SIZE);	
	m_separator_bar->SetRect(rect_separator);
	m_separator_bar1->SetRect(rect_separator1);
	m_separator_bar2->SetRect(rect_separator2);

}	


void GadgetControlButtons::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	if (widget == m_move_button)
	{
	   if(button==MOUSE_BUTTON_1 && down)
	   {
		  m_is_dragging = TRUE;
		  m_mousedown_point.x = x;
		  m_mousedown_point.y = y;
	   }
	   else if(button==MOUSE_BUTTON_1 && !down)
	   {
		  m_is_dragging = FALSE;	   
	   } 
	
	}
}

DEFINE_CONSTRUCT(OpMoveButton);

#endif //WIDGET_CONTROL_BUTTONS_SUPPORT
#endif   // GADGET_SUPPORT

