/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

 #ifndef QUICK_OP_WIDGET_BASE_H
#define QUICK_OP_WIDGET_BASE_H

#include "modules/widgets/OpWidget.h"
#include "adjunct/quick_toolkit/widgets/OpToolTipListener.h"
#include "adjunct/desktop_util/settings/DesktopSettings.h"
#include "adjunct/desktop_pi/DesktopDragObject.h"

class QuickOpWidgetBase;
class QuickAnimationWidget;
class OpDelayedTrigger;
class OpScopeWidgetInfo;

class OpWidgetLayout
{
public:

	OpWidgetLayout(INT32 available_width = INT_MAX, INT32 available_height = INT_MAX);

	void SetAvailableWidth(INT32 available_width)	{m_available_width = available_width;}
	void SetAvailableHeight(INT32 available_height)	{m_available_height = available_height;}

	INT32 GetAvailableWidth()		{return m_available_width;}
	INT32 GetAvailableHeight()	{return m_available_height;}

	void SetConstraintsFromWidget(QuickOpWidgetBase* widget);

	void SetFixedWidth(INT32 width);
	void SetFixedHeight(INT32 height);
	void SetFixedSize(INT32 width, INT32 height);
	void SetFixedFullSize();

	void SetMinWidth(INT32 min_width)			{m_min_width = min_width;}
	void SetMinHeight(INT32 min_height)			{m_min_height = min_height;}

	void SetMaxWidth(INT32 max_width)			{m_max_width = max_width;}
	void SetMaxHeight(INT32 max_height)			{m_max_height = max_height;}

	void SetPreferedWidth(INT32 prefered_width)		{m_prefered_width = prefered_width;}
	void SetPreferedHeight(INT32 prefered_height)	{m_prefered_height = prefered_height;}

	INT32 GetMinWidth()  {return m_min_width;}
	INT32 GetMinHeight() {return m_min_height;}

	INT32 GetMaxWidth()	 {return m_max_width;}
	INT32 GetMaxHeight() {return m_max_height;}

	INT32 GetPreferedWidth()	{return m_prefered_width;}
	INT32 GetPreferedHeight()	{return m_prefered_height;}

private:

	INT32 m_available_width;
	INT32 m_available_height;

	INT32 m_min_width;
	INT32 m_min_height;

	INT32 m_prefered_width;
	INT32 m_prefered_height;

	INT32 m_max_width;
	INT32 m_max_height;
};

class OpDelayedTriggerListener
{
public:

	virtual ~OpDelayedTriggerListener() {}

	virtual void OnTrigger(OpDelayedTrigger*) = 0;
};

class OpDelayedTrigger : public OpTimerListener
{
public:

	enum InvokeType
	{
		INVOKE_DELAY,  // default, if timer is running, just return, trigger will happen at timeout
		INVOKE_REDELAY,// if timer is running, stop it and do a new delay (commonly used for edit fields)
		INVOKE_NOW,	   // force an update now and stop any timer running
	};

	OpDelayedTrigger();
	~OpDelayedTrigger();

	void SetDelayedTriggerListener(OpDelayedTriggerListener* listener) {m_listener = listener;}
	void SetTriggerDelay(INT32 initial_delay, INT32 subsequent_delay = 0) {m_initial_delay = initial_delay; m_subsequent_delay = subsequent_delay;}

	INT32 GetInitialDelay() {return m_initial_delay;}

	void InvokeTrigger(InvokeType invoke_type = INVOKE_DELAY);

	void CancelTrigger();	// cancel any trigger operation

	virtual void OnTimeOut(OpTimer* timer);

private:

	void InvokeListener();

	OpDelayedTriggerListener * m_listener;
	INT32                      m_initial_delay;
	INT32                      m_subsequent_delay;
	OpTimer *                  m_timer;
	double                     m_previous_invoke;
	BOOL                       m_is_timer_running;
};

/** QuickOpWidgetBase is various extended functionality for the OpWidget class, needed by the QUICK UI.
 */

class QuickOpWidgetBase : public OpWidgetListener, public OpToolTipListener, public OpInputStateListener
{
	friend class OpWidget;

private: 
	// Private constructor to enforce that no one else than OpWidget
	// can make instances of this class.
	QuickOpWidgetBase();

public:

	virtual ~QuickOpWidgetBase();

	template <typename T> static OP_STATUS Construct(T** obj)
	{
		*obj = OP_NEW(T, ());
		if (!(*obj))
			return OpStatus::ERR_NO_MEMORY;

		if (OpStatus::IsError((*obj)->init_status))
		{
			delete *obj;
			return OpStatus::ERR_NO_MEMORY;
		}

		return OpStatus::OK;
	}

	DesktopWindow * GetParentDesktopWindow();
	OpTypedObject::Type GetParentDesktopWindowType();
	OpWorkspace* GetParentWorkspace();
	OpWorkspace* GetWorkspace();

	virtual BOOL IsAllowedInExtenderMenu() { return FALSE; }

	virtual BOOL IsActionForMe(OpInputAction* action);

	OpWidget * GetWidgetById(INT32 id);
	OpWidget * GetWidgetByType(OpTypedObject::Type type, BOOL only_visible = TRUE) {return GetWidgetByTypeAndIdAndAction(type, -1, NULL, only_visible);}
	OpWidget * GetWidgetByTypeAndId(OpTypedObject::Type type, INT32 id = -1, BOOL only_visible = TRUE) {return GetWidgetByTypeAndIdAndAction(type, id, NULL, only_visible);}
	OpWidget * GetWidgetByTypeAndIdAndAction(OpTypedObject::Type type, INT32 id, OpInputAction* action, BOOL only_visible = TRUE);

	virtual DesktopDragObject* GetDragObject(OpTypedObject::Type type, INT32 x, INT32 y);

	void SetXResizeEffect(RESIZE_EFFECT x_effect)	{ m_x_resize_effect = x_effect;}
	void SetYResizeEffect(RESIZE_EFFECT y_effect)	{ m_y_resize_effect = y_effect;}

	RESIZE_EFFECT GetXResizeEffect() const { return static_cast<RESIZE_EFFECT>(m_x_resize_effect); }
	RESIZE_EFFECT GetYResizeEffect() const { return static_cast<RESIZE_EFFECT>(m_y_resize_effect); }

	void SetMaxWidth(INT32 max_width)	{m_max_width = max_width;}
	void SetMinWidth(INT32 min_width)	{m_min_width = min_width;}
	void SetMaxHeight(INT32 max_height)	{m_max_height = max_height;}
	void SetMinHeight(INT32 min_height)	{m_min_height = min_height;}
	void SetGrowValue(INT32 grow_value)	{m_grow_value = grow_value;}

	INT32 GetMaxWidth() const { return m_max_width; }
	INT32 GetMinWidth() const { return m_min_width; }
	INT32 GetMaxHeight() const { return m_max_height; }
	INT32 GetMinHeight() const { return m_min_height; }
	INT32 GetGrowValue() const { return m_grow_value; }

	void SetFixedMinMaxWidth(BOOL fixed_min_max_width) { m_has_fixed_min_max_width = fixed_min_max_width ? true : false; }
	BOOL HasFixedMinMaxWidth() const { return m_has_fixed_min_max_width; }

#ifdef VEGA_OPPAINTER_SUPPORT
	QuickAnimationWidget *GetAnimation() const { return m_animation; }
	void SetAnimation(QuickAnimationWidget *animation);
	void ResetAnimation() { m_animation = NULL; }
	virtual UINT8 GetOpacity();
#endif

	// used by the splitter to know when it should hide or not
	virtual BOOL GetIsResizable() { return TRUE; }

	void SetUserData(void* user_data) { m_user_data = user_data; }
	void* GetUserData() const { return m_user_data; }

	void SetStringID(Str::LocaleString string_id);
	Str::LocaleString GetStringID() const;

	virtual BOOL MustMoveOutsideToStartDrag() {return FALSE;}

	virtual void Relayout(BOOL relayout = TRUE, BOOL invalidate = TRUE);
	virtual void SyncLayout();
	BOOL IsSyncingLayout(){ return m_syncing_layout; }

	BOOL IsFullscreen();
	BOOL IsCustomizing();

	void SetUpdateNeeded(BOOL update_needed) { m_update_needed = update_needed ? true : false; }
	BOOL IsUpdateNeeded() const { return m_update_needed; }

	void SetUpdateNeededWhen(UpdateNeededWhen update_needed_when);
	UpdateNeededWhen GetUpdateNeededWhen() const { return static_cast<UpdateNeededWhen>(m_update_needed_when); }

	void ListenToActionStateUpdates(BOOL listen) {m_input_state.SetEnabled(listen);}
	void UpdateActionStateIfNeeded();

	bool IsDeleted() const { return m_deleted; }

	// skin / image

	void SetSystemFont(OP_SYSTEM_FONT system_font);
	OP_SYSTEM_FONT GetSystemFont() const { return static_cast<OP_SYSTEM_FONT>(m_standard_font); }

	void SetOriginalRect(const OpRect& rect) {m_original_rect = rect;}
	OpRect GetOriginalRect() const { return m_original_rect; }

	void SetRelativeSystemFontSize(UINT32 percent);
	UINT32 GetRelativeSystemFontSize() const { return m_relative_system_font_size; }

	enum FontWeight
	{
		WEIGHT_DEFAULT = 0,
		WEIGHT_NORMAL = 4,
		WEIGHT_BOLD = 7
	};
	void SetSystemFontWeight(FontWeight weight);
	FontWeight GetSystemFontWeight() const { return static_cast<FontWeight>(m_system_font_weight); }

	//Apply skin padding to widget
	void UpdateSkinPadding();
	void UpdateSkinMargin();

	/**
	 * Adapt the relevant widget properties to UI direction.
	 *
	 * @param rtl if @c TRUE, the new UI direction is RTL
	 */
	void UpdateDirection(BOOL rtl);

	virtual void GetSpacing(INT32* spacing);
	virtual void SetRightOffset(INT32 right_offset) { m_right_offset = right_offset; }
	virtual INT32 GetRightOffset() { return m_right_offset; }
	virtual BOOL GetLayout(OpWidgetLayout& layout) {layout.SetConstraintsFromWidget(this); return FALSE;}
	virtual INT32 GetDropArea(INT32 x, INT32 y);
	virtual void GetRequiredSize(INT32& width, INT32& height);
	virtual void ResetRequiredSize() {m_required_width = m_required_height = -1;}

	/** Get skin margins for this widget, possibly overridden by some widgets.
	No quick code cares about updating the margins stored in OpWidget, but usually get them straight from GetBorderSkin()->GetMargins.
	We need a quick way to be able to override the skins margins without changing all quick code, so this is made overridable. */
	virtual void GetSkinMargins(INT32 *left, INT32 *top, INT32 *right, INT32 *bottom);

	virtual void GenerateOnSettingsChanged(DesktopSettings* settings);

	virtual void GenerateOnRelayout(BOOL force);
	virtual void GenerateOnLayout(BOOL force);

	/** Called from child widget if its content change and it thinks the parent (this) might want to resize */
	virtual void OnContentSizeChanged() {}

	virtual void OnUpdateActionState() {}
	virtual void OnSettingsChanged(DesktopSettings* settings) {}
	virtual void OnDragStart(const OpPoint& point);
	virtual void OnDragLeave(OpDragObject* drag_object) {}
	virtual void OnDragMove(OpDragObject* drag_object, const OpPoint& point);
	virtual void OnDragDrop(OpDragObject* drag_object, const OpPoint& point);

	// default implementation when a child wants relayout it to relayout

	virtual void OnRelayout(OpWidget* widget) {Relayout(TRUE, FALSE);}

	// OpInputState

	virtual void OnUpdateInputState(BOOL full_update);

	// Returns if the debug tooltip info should be shown or not
	BOOL IsDebugToolTipActive();

	virtual BOOL HasToolTipText(OpToolTip* tooltip);
	virtual void GetToolTipText(OpToolTip* tooltip, OpInfoText& text);

	void DumpSkinInformation(OpWidget *widget, OpString8& output);

	// notify the child about the available parent sizes, if needed for layout
	virtual void SetAvailableParentSize(INT32 parent_width, INT32 parent_height) {}

	/** UI Automation (scope)
	 * @return Object that can be used to get more information about the widget
	 * NB: ownership will be transfered to the caller
	 */
	virtual OpScopeWidgetInfo* CreateScopeWidgetInfo() { return NULL; }
	
	/** @return whether this widget should be scrolled to if possible when it
	 * receives focus
	 */
	virtual BOOL ScrollOnFocus() { return FALSE; }

	/**
	 * Mirror a child rect or not depending on the RTL property.
	 *
	 * @param child_rect the rectangle with the calculated position of a child
	 * 		widget
	 * @return the rectangle with the calculated position of the child adjusted
	 * 		to account for UI direction
	 */
	OpRect AdjustForDirection(const OpRect& child_rect) const;

	/**
	 * Helper for laying children out.
	 *
	 * Use this instead of calling SetRect() on the child directly to account
	 * for UI direction.  For widgets with RTL set to TRUE, this function will
	 * mirror the rect.  For widgets with RTL set to FALSE, the rect is not
	 * modified.
	 */
	void SetChildRect(OpWidget* child, const OpRect& rect, BOOL invalidate = TRUE, BOOL resize_children = TRUE) const;
	void SetChildOriginalRect(OpWidget* child, const OpRect& rect) const;

protected:
	void MarkDeleted();

	INT32 m_required_width;
	INT32 m_required_height;

private:

	// Get the OpWidget pointer to this QuickOpWidgetBase
	// These functions are only meant to be used internally in QuickOpWidgetBase as a
	// shortcut/prettier way than having to write out the cast for every function call.
	inline OpWidget* GetOpWidget();
	inline const OpWidget* GetOpWidget() const;

	OpInputState m_input_state;

	void* m_user_data;

	INT32 m_max_width;
	INT32 m_min_width;
	INT32 m_max_height;
	INT32 m_min_height;
	INT32 m_grow_value;

	INT32 m_right_offset;

	UINT32 m_relative_system_font_size;

	int m_string_id;
	OpRect m_original_rect;

#ifdef VEGA_OPPAINTER_SUPPORT
	QuickAnimationWidget *m_animation;
#endif

	// simple types that can be represented in < 32 bits stored in bitfield
	bool m_update_needed:1;
	unsigned m_update_needed_when:2; // enum UpdateNeededWhen
	bool m_syncing_layout:1;
	bool m_deleted:1;
	bool m_has_fixed_min_max_width:1;
	unsigned m_x_resize_effect:2; // enum RESIZE_EFFECT
	unsigned m_y_resize_effect:2; // enum RESIZE_EFFECT
	unsigned m_standard_font:5; // enum OP_SYSTEM_FONT
	unsigned m_system_font_weight:3; // enum FontWeight
};

#endif // QUICK_OP_WIDGET_BASE
