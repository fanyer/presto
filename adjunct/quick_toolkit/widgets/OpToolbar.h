/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_TOOLBAR_H
#define OP_TOOLBAR_H

#include "adjunct/quick_toolkit/widgets/OpBar.h"
#include "modules/widgets/OpNamedWidgetsCollection.h"

#include "modules/util/adt/opvector.h"
#include "modules/widgets/OpButton.h"

#include "adjunct/quick/animation/QuickAnimation.h"

#include "modules/locale/locale-enum.h"

class PrefsFile;
class PrefsSection;
class OpButton;

/***********************************************************************************
**
**	OpToolbar
**
***********************************************************************************/

class OpToolbar : public OpBar, protected QuickAnimation
{
	protected:

								OpToolbar(PrefsCollectionUI::integerpref prefs_setting = PrefsCollectionUI::DummyLastIntegerPref, PrefsCollectionUI::integerpref autoalignment_prefs_setting = PrefsCollectionUI::DummyLastIntegerPref);
		virtual BOOL			IsExtenderButton( OpWidget* widget ) const { return widget && widget == m_extender_button; }	
		INT32 					GetWidgetPosition( INT32 x, int32 y );

	public:

		class ContentListener
		{
		public:
			// called when reading content. The listener can reject selected widget types
			// Return TRUE to accept the content type, otherwise FALSE
			virtual BOOL OnReadWidgetType(const OpString8& type) = 0;
		};

		enum FixedHeightType
		{
			FIXED_HEIGHT_NONE,
			FIXED_HEIGHT_BUTTON,
			FIXED_HEIGHT_STRETCH
		};

		static OP_STATUS		Construct(OpToolbar** obj, PrefsCollectionUI::integerpref prefs_setting = PrefsCollectionUI::DummyLastIntegerPref, PrefsCollectionUI::integerpref autoalignment_prefs_setting = PrefsCollectionUI::DummyLastIntegerPref);

		virtual					~OpToolbar();

		// Create a button for the toolbar. Override if special buttons are required
		virtual OP_STATUS       CreateButton(Type type, OpButton **button);
		OpButton*				AddButton(const uni_char* label, const char* image = NULL, OpInputAction* action = NULL, void* user_data = NULL, INT32 pos = -1, BOOL double_action = FALSE);
		virtual OpButton*		AddButton(const uni_char* label, const char* image, OpInputAction* action, void* user_data, INT32 pos, BOOL double_action, Str::LocaleString string_id);
		void					AddButton(OpButton* button, const uni_char* label, const char* image = NULL, OpInputAction* action = NULL, void* user_data = NULL, INT32 pos = -1, BOOL double_action = FALSE);
		virtual void			AddButton(OpButton* button, const uni_char* label, const char* image, OpInputAction* action, void* user_data, INT32 pos, BOOL double_action, Str::LocaleString string_id);
		void					AddWidget(OpWidget* widget, INT32 pos = -1);
		
		BOOL					AddButtonFromURL(const uni_char* url_src, const uni_char* url_title, int pos = -1, BOOL force = FALSE);
		void					AddButtonFromURL2(BOOL is_button, OpInputAction * input_action, const uni_char* action_text, const uni_char * url_title, int pos, BOOL force);

		OP_STATUS				RemoveWidget(INT32 index);

		INT32					FindWidgetByUserData(void* user_data);
		INT32					FindWidget(OpWidget* widget, BOOL recursively = FALSE);
		void					MoveWidget(INT32 src, INT32 dst);
		void					AnimateWidgets(int start_index, int stop_index, int ignore_index = -1, double duration = -1);

		BOOL					IsStandardToolbar() {return m_standard;}
		void					SetStandardToolbar(BOOL standard) {m_standard = standard;}

		/** Returns TRUE if something in this toolbar has focus or is being hovered by the mouse. */
		BOOL					IsFocusedOrHovered();


		void					SetContentListener(ContentListener* content_listener) { m_content_listener = content_listener; }

		/** Set alignment with animation.
			If alignment is changed to ALIGNMENT_OFF, it will remain set
			to the current value until the animation is completed. */
		virtual BOOL			SetAlignmentAnimated(Alignment alignment, double animation_duration = -1);

		BOOL					IsAnimatingIn() { return IsAnimating() && m_animation_in; }
		BOOL					IsAnimatingOut() { return IsAnimating() && !m_animation_in; }

		virtual BOOL			SupportsThumbnails() { return FALSE; }

		virtual BOOL			SetSelected(INT32 index, BOOL invoke_listeners = FALSE, BOOL changed_by_mouse = FALSE);

		virtual bool 			IsDropAvailable(DesktopDragObject* drag_object, int x, int y) { return !!drag_object->IsDropAvailable();}
		virtual void  			UpdateDropAction(DesktopDragObject* drag_object, bool accepted) {}


		void					SetButtonType(OpButton::ButtonType type)		
		{
			if(m_button_type != type) 
			{ 
				m_button_type = type; 
				UpdateAllWidgets(); 
			}
		}
		void					SetButtonStyle(OpButton::ButtonStyle style)		
		{
			if(m_button_style != style)
			{
				m_button_style = style; 
				UpdateAllWidgets();
			}
		}
		void					SetButtonSkinType(SkinType skin_type)			
		{
			if(m_skin_type != skin_type)
			{
				m_skin_type = skin_type; 
				UpdateAllWidgets();
			}
		}
		void					SetButtonSkinSize(SkinSize skin_size)
		{
			if(m_skin_size != skin_size)
			{
				m_skin_size = skin_size; 
				UpdateAllWidgets();
			}
		}
		void					SetSelector(BOOL selector) { m_selector = selector; SetButtonType(OpButton::TYPE_SELECTOR);	}
		void					SetDeselectable(BOOL deselectable)
		{
			if(m_deselectable != deselectable)
			{
				m_deselectable = deselectable; 
				UpdateAllWidgets();
			}
		}
		virtual void			SetWrapping(Wrapping wrapping)
		{
			if(m_wrapping != wrapping)
			{
				m_wrapping = wrapping; 
				Relayout();
			}
		}
		void					SetCentered(BOOL centered)
		{
			if(m_centered != centered)
			{
				m_centered = centered;
				Relayout();
			}
		}
		void					SetFixedMaxWidth(INT32 fixed_max_width)
		{
			if(m_fixed_max_width != fixed_max_width)
			{
				m_fixed_max_width = fixed_max_width; 
				UpdateAllWidgets();
			}
		}
		void					SetFixedMinWidth(INT32 fixed_min_width)
		{
			if(m_fixed_min_width != fixed_min_width)
			{
				m_fixed_min_width = fixed_min_width; 
				UpdateAllWidgets();
			}
		}
		void					SetFixedHeight(FixedHeightType fixed_height)
		{
			if(m_fixed_height != fixed_height)
			{
				m_fixed_height = fixed_height; 
				Relayout();
			}
		}
		void					SetShrinkToFit(BOOL shrink_to_fit)
		{
			if(m_shrink_to_fit != shrink_to_fit)
			{
				m_shrink_to_fit = shrink_to_fit; 
				Relayout();
			}
		}
		void					SetGrowToFit(BOOL grow_to_fit)
		{
			if(m_grow_to_fit != grow_to_fit)
			{
				m_grow_to_fit = grow_to_fit; 
				Relayout();
			}
		}
		void					SetMiniButtons(BOOL mini_buttons)
		{
			if(m_mini_buttons != mini_buttons)
			{
				m_mini_buttons = mini_buttons; 
				Relayout();
			}
		}
		void					SetFillToFit(BOOL fill_to_fit)
		{
			if(m_fill_to_fit != fill_to_fit)
			{
				m_fill_to_fit = fill_to_fit; 
				Relayout();
			}
		}
#ifdef QUICK_FISHEYE_SUPPORT
		void					SetFisheyeShrink(BOOL fisheye_shrink)			
		{
			if(m_fisheye_shrink != fisheye_shrink)
			{
				m_fisheye_shrink = fisheye_shrink; 
				Relayout();
		}
		}
#endif // QUICK_FISHEYE_SUPPORT
		void					SetLocked(BOOL locked)							{m_locked = locked;}
		void					SetDead(BOOL dead)								{m_dead = dead;}
		void					SetAutoUpdateAutoAlignment(BOOL update)			{m_auto_update_auto_alignment = update;}
		OP_STATUS				SetButtonSkin(OpStringC& button_skin)			{OP_STATUS s = m_button_skin.Set(button_skin.CStr()); Relayout(); return s; }
		OP_STATUS				SetButtonSkin(const char* button_skin)			{OP_STATUS s = m_button_skin.Set(button_skin); Relayout(); return s; }

		BOOL					HasFixedHeight() {return m_fixed_height != FIXED_HEIGHT_NONE;}
		OpButton::ButtonType	GetButtonType() {return m_button_type;}
		OpButton::ButtonStyle	GetButtonStyle() {return m_button_style;}
		SkinType				GetButtonSkinType() {return m_skin_type;}
		SkinSize				GetButtonSkinSize() {return m_skin_size;}
		Wrapping				GetWrapping() {return m_wrapping;}
		OpWidget*				GetWidget(INT32 index) const {return m_widgets.Get(index);}
		INT32					GetWidgetCount() const {return m_widgets.GetCount();}
		BOOL					IsExtenderActive() {return m_extender_button ? m_extender_button->IsVisible() : FALSE;}
		INT32					GetSelected() {return m_selected;}
		INT32					GetFocused() {return m_focused == -1 ? m_selected : m_focused;}
		INT32 					GetComputedColumnCount() const { return m_computed_column_count; }
		BOOL					IsSelector() {return m_selector;}
		BOOL					IsDeselectable() {return m_deselectable;}
		BOOL					IsLocked() {return m_locked;}

		void*					GetUserData(INT32 index);
		virtual int  			GetActiveTabInGroup(int position) { return position; }
		virtual void  			IncDropPosition(int& position, bool step_out_of_group) { position++; }

		void					SelectNext();
		void					SelectPrevious();
		BOOL					HasGrowable();

		// Auto hide
		void					SetAutoHide(BOOL auto_hide = TRUE, UINT deley = 15000/*15s*/);
		void					ResetAutoHideTimer();

		// usage stats
		void					SetReportUsageStats(BOOL report_click_usage) { m_report_click_usage = report_click_usage; }

		// OpTimerListener
		virtual void			OnTimeOut(OpTimer* timer);

		// Hooks

		virtual void			OnReadStyle(PrefsSection *section);
		virtual void			OnReadContent(PrefsSection *section);
		virtual void			OnWriteStyle(PrefsFile* prefs_file, const char* name);
		virtual void			OnWriteContent(PrefsFile* prefs_file, const char* name);
		virtual void			OnAutoAlignmentChanged() {UpdateAllWidgets(); UpdateAutoAlignment();}
		virtual void			OnAlignmentChanged() {if (IsOn()) UpdateTargetToolbar(TRUE);}

		virtual INT32			GetDragSourcePos(DesktopDragObject* drag_object);

		virtual void			OnDeleted();
		virtual void			OnPaintAfterChildren(OpWidgetPainter* widget_painter, const OpRect &paint_rect);
		virtual void			OnLayout(BOOL compute_size_only, INT32 available_width, INT32 available_height, INT32& used_width, INT32& used_height);
		virtual void			OnDragLeave(OpDragObject* drag_object);
		virtual void 			OnDragCancel(OpDragObject* drag_object);
		virtual void			OnDragMove(OpDragObject* drag_object, const OpPoint& point);
		virtual void			OnDragDrop(OpDragObject* drag_object, const OpPoint& point);
		virtual BOOL			OnContextMenu(const OpPoint &menu_point, const OpRect *avoid_rect, BOOL keyboard_invoked) { return OnContextMenu(this, -1, menu_point, NULL, keyboard_invoked); }
		virtual	void			OnFocus(BOOL focus,FOCUS_REASON reason);
		virtual void			OnAdded();
		
		// Close the toolbar when auto hide timer expires
		virtual void			CloseOnTimeOut();

		// called before the extender button is laid out.
		// Override to change its position (like for the pagebar).
		virtual BOOL			OnBeforeExtenderLayout(OpRect& extender_rect) { return FALSE; };

		// called before the available width is used for doing a real layout.
		// Allows overrides to modify how much space the toolbar should use
		virtual INT32			OnBeforeAvailableWidth(INT32 available_width) { return available_width; };

		// called before the layout rect is set on the widget to allow for overriding a layout on a specific widget, if necessary
		virtual BOOL			OnBeforeWidgetLayout(OpWidget *widget, OpRect& layout_rect) { return FALSE; };

		// called when reading content. A subclass can reimplement this and reject selected widget types
		// Return TRUE to accept the content type, otherwise FALSE
		virtual BOOL 			OnReadWidgetType(const OpString8& type) { return TRUE; }

		// Implementing the OpWidgetListener interface

		virtual void			OnEnabled(OpWidget *widget, BOOL enabled);
		virtual void			OnClick(OpWidget *widget, UINT32 id = 0);
		virtual void			OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y);
		virtual void			OnDragMove(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y);
		virtual void			OnDragDrop(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y);
		virtual void			OnDragLeave(OpWidget* widget, OpDragObject* drag_object) {OnDragLeave((DesktopDragObject*)drag_object);}
		virtual void			OnDragCancel(OpWidget* widget, OpDragObject* drag_object) {OnDragCancel(drag_object);}
		virtual BOOL            OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked);
		virtual void			OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks);
		virtual void			OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks);
		virtual void			OnMouseMove(const OpPoint &point);
		virtual void			OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);

		// Implementing the OpTooltipListener interface
		virtual BOOL HasToolTipText(OpToolTip* tooltip);
		virtual void GetToolTipText(OpToolTip* tooltip, OpInfoText& text);
		// the only tooltip that is currently shown is about help on dragging
		virtual BOOL GetShowWhileDragging(OpToolTip* tooltip) {return TRUE;}
		// almost immediately show the tooltip, because the user tries to drop something where it can't be dropped
		virtual INT32 GetToolTipDelay(OpToolTip* tooltip) {return 200;}

		// == Implementing QuickAnimation ==============

		virtual void OnAnimationStart();
		virtual void OnAnimationUpdate(double position);
		virtual void OnAnimationComplete();

		// Implementing the OpTreeModelItem interface

		virtual Type			GetType() {return WIDGET_TYPE_TOOLBAR;}
		virtual BOOL			IsOfType(Type type){return type == WIDGET_TYPE_TOOLBAR?TRUE:OpBar::IsOfType(type);}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
		virtual Accessibility::ElementKind AccessibilityGetRole() const {return Accessibility::kElementKindToolbar;}
#endif
		// == OpInputContext ======================

		virtual BOOL			OnInputAction(OpInputAction* action);
		virtual const char*		GetInputContextName() {return "Toolbar";}

		virtual void			EnableTransparentSkin(BOOL enable);

		// Used by subclass(Bookmark bar) to disable to position indicator temporarily when d&d
		virtual BOOL			DisableDropPositionIndicator() {return FALSE;}
	protected:

		void					UpdateAllWidgets();
		virtual void			UpdateWidget(OpWidget* widget);
		void					UpdateButton(OpWidget* widget);
		void					UpdateSkin(OpWidget* widget);
		void					GenerateOnDragDrop(DesktopDragObject* drag_object, INT32 pos, INT32 x, INT32 y);
		void					GenerateOnDragMove(DesktopDragObject* drag_object, INT32 pos, INT32 x, INT32 y);
		void					UpdateAutoAlignment();
		virtual void			UpdateTargetToolbar(BOOL is_target_toolbar);
		BOOL					IsTargetToolbar();
		void					UpdateDropPosition(int new_drop_pos);
		OpWidgetVector<OpWidget> m_widgets;

	private:

		void					SetWidgetMiniSize(OpWidget *widget);

		INT32					m_selected;
		INT32					m_focused;
		BOOL					m_selector;
		BOOL					m_deselectable;
		Wrapping				m_wrapping;
		BOOL					m_centered;
		INT32					m_fixed_max_width;
		INT32					m_fixed_min_width;
		FixedHeightType			m_fixed_height;
		BOOL					m_shrink_to_fit;
#ifdef QUICK_FISHEYE_SUPPORT
		BOOL					m_fisheye_shrink;
#endif // QUICK_FISHEYE_SUPPORT
		BOOL					m_grow_to_fit;
		BOOL					m_mini_buttons;
		BOOL					m_mini_edit;
		BOOL					m_fill_to_fit;
		BOOL					m_locked;
		BOOL					m_auto_update_auto_alignment;
		BOOL					m_report_click_usage;	// if TRUE, report button clicks to usage stats
		OpButton::ButtonType	m_button_type;
		OpButton::ButtonStyle	m_button_style;
		SkinType				m_skin_type;
		SkinSize				m_skin_size;
		INT32					m_drop_pos;
		BOOL					m_standard;
		BOOL					m_dead;
		INT32				    m_computed_column_count;	
		OpButton* 				m_extender_button;
		Str::LocaleString		m_drag_tooltip;			///<Tooltip that should be shown while dragging; string depends content type that is dragged
		OpString8				m_button_skin;
		OpButton*               m_dummy_button;         // used for size calculations during layout
#ifdef WINDOW_MOVE_FROM_TOOLBAR
		BOOL					m_left_mouse_down;		// Set when the left mouse button is down and you want to move the whole window
		OpPoint					m_down_point;
		DesktopWindow*			m_desktop_window;		// Pointer to the current desktop window so we don't search for it on every mouse move
#endif // WINDOW_MOVE_FROM_TOOLBAR
		
		// used to automatically hide the toolbar
		OpTimer*				m_auto_hide_timer;
		UINT					m_auto_hide_delay;

		ContentListener*		m_content_listener;

protected:
		double					m_animation_progress;
		BOOL					m_animation_in;			// TRUE if animation to visible state. FALSE if animating to hide.
		INT32					m_animation_size;		// The last calculated used_width or used_height unaffected by animation (depending on toolbar alignment)
};

#endif // OP_TOOLBAR_H
