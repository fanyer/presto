/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_ZOOM_DROPDOWN_H
#define OP_ZOOM_DROPDOWN_H

#include "modules/widgets/OpDropDown.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/quick/managers/SpeedDialManager.h"
#include "adjunct/quick/windows/OpRichMenuWindow.h"
#include "adjunct/quick/speeddial/SpeedDialListener.h"
#include "modules/widgets/OpSlider.h"
#include "modules/widgets/OpButton.h"

class DocumentDesktopWindow;

/***********************************************************************************
**
**	OpZoomDropDown
**
***********************************************************************************/

class OpZoomDropDown : public OpDropDown, public DesktopWindowSpy
{
	protected:
								OpZoomDropDown();

	public:

		static OP_STATUS Construct(OpZoomDropDown** obj);

		// OpWidget

		virtual void            OnDeleted() {SetSpyInputContext(NULL, FALSE); OpDropDown::OnDeleted();}

		// OpWidgetListener

		virtual void			OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y);
		virtual void			OnDragMove(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y) { static_cast<OpWidgetListener *>(GetParent())->OnDragDrop(this, drag_object, pos, x, y);}
		virtual void			OnDragDrop(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y) { static_cast<OpWidgetListener *>(GetParent())->OnDragDrop(this, drag_object, pos, x, y);}
		virtual void			OnDragLeave(OpWidget* widget, OpDragObject* drag_object) { static_cast<OpWidgetListener *>(GetParent())->OnDragLeave(this, drag_object);}
		virtual void			OnDropdownMenu(OpWidget *widget, BOOL show);
		virtual void			OnChange(OpWidget *widget, BOOL changed_by_mouse);
		virtual BOOL            OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked)
		{
			
			OpWidgetListener* listener = GetParent() ? GetParent() : this;
			return listener->OnContextMenu(this, child_index, menu_point, avoid_rect, keyboard_invoked);
		}

		virtual void 			OnCharacterEntered( uni_char& character );
		virtual void			OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);

		// DesktopWindowSpy hooks

		virtual void			OnTargetDesktopWindowChanged(DesktopWindow* target_window);
		virtual void			OnPageScaleChanged(OpWindowCommander* commander, UINT32 newScale);

		// Implementing the OpTreeModelItem interface

		virtual Type			GetType() {return WIDGET_TYPE_ZOOM_DROPDOWN;}

		// == OpInputContext ======================

		virtual BOOL			OnInputAction(OpInputAction* action);
		BOOL					IsInputContextAvailable(FOCUS_REASON reason) { return TRUE; }

	private:
		void					UpdateZoom();
		BOOL					IsSpeedDial();

		static INT32			s_scales[];
};

/***********************************************************************************
**
**	OpZoomSlider
**
***********************************************************************************/
class OpZoomSlider: 
	public OpSlider, 
	public DesktopWindowSpy,
	public SpeedDialConfigurationListener
{
protected:
		OpZoomSlider();

public:
		static OP_STATUS Construct(OpZoomSlider** obj);
		void					SetValues();
		void					UpdateZoom(UINT32 scale);

		// OpWidget
		virtual void			OnDeleted();
		virtual void			OnMouseMove(const OpPoint &point);
		virtual void			OnMouseLeave();
		virtual void			OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks);
		virtual void			OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks);

		// OpSlider
		virtual void			HandleOnChange();
		virtual Type			GetType() {return WIDGET_TYPE_ZOOM_SLIDER;}
		virtual const char*		GetKnobSkin(BOOL horizontal) { return horizontal ? "Zoom Slider Horizontal Knob" : "Zoom Slider Vertical Knob"; }
		virtual const char*		GetTrackSkin(BOOL horizontal) { return horizontal ? "Zoom Slider Horizontal Track" : "Zoom Slider Vertical Track"; }

		// DesktopWindowSpy hooks
		virtual void			OnTargetDesktopWindowChanged(DesktopWindow* target_window) { SetValues(); }
		virtual void			OnPageScaleChanged(OpWindowCommander* commander, UINT32 scale) { UpdateZoom(scale); }
		virtual void			OnDesktopWindowResized(DesktopWindow* desktop_window, INT32 width, INT32 height) { UpdateZoom(); }

		// SpeedDialConfigurationListener
		virtual void OnSpeedDialConfigurationStarted(const DesktopWindow& window) { }
		virtual void OnSpeedDialLayoutChanged() { }
		virtual void OnSpeedDialColumnLayoutChanged() { }
		virtual void OnSpeedDialBackgroundChanged() { }
		virtual void OnSpeedDialScaleChanged() { UpdateZoom(g_speeddial_manager->GetIntegerThumbnailScale()); }

		// OpInputContext
		virtual BOOL			IsInputContextAvailable(FOCUS_REASON reason) 
		{
			// only allow it to get the focus on explicit actions, not for other reasons
			return reason == FOCUS_REASON_KEYBOARD || reason == FOCUS_REASON_MOUSE; 
		}

		// QuickOpWidgetBase hooks
		virtual void			OnUpdateActionState();

private:
		//Constants
		static const double		MinimumZoomLevel;
		static const double		MaximumZoomLevel;
		static const double		ScaleDelta;
		static const OpSlider::TICK_VALUE TickValues[];

private:
		void					UpdateZoom();
		void					UpdateStatusText();

		OpString  m_status_text;
};

/***********************************************************************************
**
**	OpZoomMenuButton
**
***********************************************************************************/

class OpZoomMenuButton 
	: public OpButton
	, public DesktopWindowSpy
	, public OpRichMenuWindow::Listener
	, public SpeedDialConfigurationListener
{
protected:
								OpZoomMenuButton();
public:
		static OP_STATUS Construct(OpZoomMenuButton ** obj);
		
		// OpButton
		virtual void			Click(BOOL plus_action);

		// OpWidget
		virtual void			OnDeleted();
		virtual void			GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);

		// OpRichMenuWindow:Listener hooks
		virtual void OnMenuClosed();

		// DesktopWindowSpy hooks
		virtual void			OnTargetDesktopWindowChanged(DesktopWindow* target_window) { UpdateZoom(); }
		virtual void			OnPageScaleChanged(OpWindowCommander* commander, UINT32 newScale) { UpdateZoom(); }
		virtual void			OnPageUrlChanged(OpWindowCommander* commander, const uni_char* url) { UpdateZoom(); }

		// Implementing the OpTreeModelItem interface
		virtual Type			GetType() {return WIDGET_TYPE_ZOOM_MENU_BUTTON;}

		// OpInputContext
		virtual const char*		GetInputContextName() {return "Zoom Button Widget";}
		BOOL					IsInputContextAvailable(FOCUS_REASON reason) { return TRUE; }
		void					OnKeyboardInputGained(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason);
		void					OnKeyboardInputLost(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason);

		// SpeedDialConfigurationListener
		virtual void OnSpeedDialConfigurationStarted(const DesktopWindow& window) {}
		virtual void OnSpeedDialLayoutChanged() {}
		virtual void OnSpeedDialBackgroundChanged() {}
		virtual void OnSpeedDialScaleChanged(double new_scale) { UpdateZoom(); }
		virtual void OnSpeedDialColumnLayoutChanged() {}
		virtual void OnSpeedDialScaleChanged() {}

		/* Should be called when button's text (zoom value) should be refreshed. Current zoom level can be taken from one of three places:
		 * 1. When current page is speed dial and zoom level is in manual mode (controlled by user) it is read from SpeedDialManager
		 * 2. When current page is speed dial and zoom level is in automatic mode zoom level is read from parameter zoom (it is set by OpSpeedDialView)
		 * 3. When current page is a regular page zoom level is read from browser view
		 */
		void UpdateZoom(INT32 zoom = 0);

private:
		BOOL					IsSpeedDial();
		void					TogglePushedState(bool pushed);

		OpRichMenuWindow		*m_zoom_window;
		OpString8				m_org_img;
};
#endif // OP_ZOOM_DROPDOWN_H
