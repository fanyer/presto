/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_BUTTON_H
#define OP_BUTTON_H

#include "modules/widgets/OpWidget.h"
#include "modules/inputmanager/inputaction.h"
#include "modules/prefs/prefsmanager/prefstypes.h"

#ifdef BUTTON_GROUP_SUPPORT
class OpButtonGroup;
#endif // BUTTON_GROUP_SUPPORT

#ifdef QUICK
class OpGroup;
#endif // QUICK

/** OpButton is a widget acting as some type of button.
	It can represent a normal button, checkbox, radiobutton, tab, and more (See ButtonType).
	With skin support, it can have both image and text as content, layouted in some different ways (See ButtonStyle).
*/

class OpButton : public OpWidget
{
public:

	enum ButtonType
	{
		TYPE_PUSH,
		TYPE_PUSH_DEFAULT,
		TYPE_TOOLBAR,
		TYPE_SELECTOR,
		TYPE_LINK,
		TYPE_TAB,
		TYPE_PAGEBAR,
		TYPE_HEADER,
		TYPE_MENU,		///< Button used for native menu
		TYPE_OMENU,		///< Button used for new opera menu.
		TYPE_START,
		TYPE_RADIO,
		TYPE_CHECKBOX,
		TYPE_CUSTOM
	};

	enum ButtonStyle
	{
		STYLE_IMAGE,
		STYLE_TEXT,
		STYLE_IMAGE_AND_TEXT_BELOW,
		STYLE_IMAGE_AND_TEXT_ON_RIGHT,
		STYLE_IMAGE_AND_TEXT_ON_LEFT,
		STYLE_IMAGE_AND_TEXT_CENTER	///< Both image and text are centered
	};

	enum ButtonExtra
	{
		EXTRA_NONE,
		EXTRA_DROPDOWN,
		EXTRA_HIDDEN_DROPDOWN,
		EXTRA_PLUS_ACTION,
		EXTRA_PLUS_DROPDOWN,
		EXTRA_PLUS_HIDDEN_DROPDOWN,
		EXTRA_SUB_MENU
	};

#ifdef HAS_TAB_BUTTON_POSITION
	enum TabState
	{
		TAB_FIRST         = 0x01,
		TAB_LAST          = 0x02,
		TAB_SELECTED      = 0x04,
		TAB_PREV_SELECTED = 0x08,
		TAB_NEXT_SELECTED = 0x10,
		TAB_STATE_OFFSET  = 24
	};
#endif

protected:

#ifdef SKIN_SUPPORT
	OpButton(ButtonType button_type = TYPE_PUSH, ButtonStyle button_style = STYLE_IMAGE_AND_TEXT_ON_RIGHT);
#else
	OpButton(ButtonType button_type = TYPE_PUSH);
#endif

public:

#ifdef SKIN_SUPPORT
	static OP_STATUS Construct(OpButton** obj, ButtonType button_type = TYPE_PUSH, ButtonStyle button_style = STYLE_IMAGE_AND_TEXT_ON_RIGHT);
#else
	static OP_STATUS Construct(OpButton** obj, ButtonType button_type = TYPE_PUSH);
#endif

#ifdef WIDGETS_CLONE_SUPPORT
	virtual OP_STATUS CreateClone(OpWidget **obj, OpWidget *parent, INT32 font_size, BOOL expanded);
	virtual BOOL IsCloneable() { return TRUE; }
#endif

	virtual OP_STATUS		SetText(const uni_char* text);
	virtual OP_STATUS		GetText(OpString& text);

	void SetValue(INT32 value);
	INT32 GetValue() { return packed2.is_pushed; };
	BOOL IsDown() { return packed2.is_down && !packed2.is_outside; };
	BOOL IsInside() { return !packed2.is_outside; };

	/** Toggle value between 1/0 and trig OnClick and OnChange. */
	void Toggle();

	virtual void Update();

	/** Set "default look" (button that is trigged by default when pressing enter).
		Defaultlook is a special skinstate. If skin isn't used, the default button will get a black border. */
	void SetDefaultLook(BOOL state = TRUE);
	BOOL HasDefaultLook() { return packed2.has_defaultlook; }

	void SetForcedFocusedLook(BOOL state = TRUE) { packed2.has_forced_focuslook = state; }
	BOOL HasForcedFocusedLook() { return packed2.has_forced_focuslook; }

	/** Set 'attention' skin state and Update the button. */
	void SetAttention(BOOL attention = TRUE);

	/** Set if the button should show a action shortcut or not. Disabled by default. */
	void SetShowShortcut(BOOL show) { packed2.show_shortcut = show; }
	BOOL GetShowShortcut() { return packed2.show_shortcut; }

#ifdef SKIN_SUPPORT
	virtual void SetSkinManager(OpSkinManager* skin_manager) {m_dropdown_image.SetSkinManager(skin_manager); OpWidget::SetSkinManager(skin_manager);}

	void SetDropdownImage(const char* image) { m_dropdown_image.SetImage(image); }

	virtual void SetAction(OpInputAction* action);
	virtual OpInputAction* GetAction();

	void SetButtonTypeAndStyle(ButtonType type, ButtonStyle style, BOOL force_style = FALSE);
	void SetButtonType(ButtonType type) {SetButtonTypeAndStyle(type, m_button_style);}
	void SetButtonStyle(ButtonStyle style) {SetButtonTypeAndStyle(m_button_type, style, TRUE);}

	void SetFixedTypeAndStyle(BOOL fixed_type_and_style)	{packed2.has_fixed_type_and_style = fixed_type_and_style;}
	BOOL HasFixedTypeAndStyle()	{return packed2.has_fixed_type_and_style;}

	void SetFixedImage(BOOL fixed_image)	{packed2.has_fixed_image = fixed_image;}
	BOOL HasFixedImage()	{return packed2.has_fixed_image;}

	ButtonType GetButtonType() {return m_button_type;}
	ButtonExtra GetButtonExtra() {return m_button_extra;}

#ifdef HAS_TAB_BUTTON_POSITION
	void SetTabState(int tab_state) { m_tab_state = tab_state; }
	int  GetTabState() const { return m_tab_state; }
#endif

	void SetBold(BOOL bold) { packed2.is_bold = bold;}

	void SetDoubleAction(BOOL double_action = TRUE) { packed2.is_double_action = double_action; };

	virtual void Click(BOOL plus_action = FALSE);

	void AnimateClick();

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	virtual OP_STATUS AccessibilityClicked();
	virtual OP_STATUS AccessibilityGetText(OpString& str);
	virtual Accessibility::ElementKind AccessibilityGetRole() const;
	virtual Accessibility::State AccessibilityGetState();
	virtual OpAccessibleItem*  GetAccessibleParent();
#ifdef PROPERTY_PAGE_CHILDREN_OF_TABS
	virtual int	GetAccessibleChildrenCount();
	virtual OpAccessibleItem* GetAccessibleChild(int childNr);
	virtual int GetAccessibleChildIndex(OpAccessibleItem* child);
	virtual OpAccessibleItem* GetAccessibleChildOrSelfAt(int x, int y);
	virtual OpAccessibleItem* GetAccessibleFocusedChildOrSelf();
#endif // PROPERTY_PAGE_CHILDREN_OF_TABS
	OP_STATUS SetAccessibilityText(const uni_char* text);
#endif // ACCESSIBILITY_EXTENSION_SUPPORT

	virtual void		OnFontChanged() {string.NeedUpdate();}
	virtual void		OnDirectionChanged() { OnFontChanged(); }

	/**
	   documented in OpWidget
	 */
	virtual BOOL FocusRectInMargins() { const Type t = GetType(); return t == WIDGET_TYPE_CHECKBOX || t == WIDGET_TYPE_RADIOBUTTON; }

#ifdef BUTTON_GROUP_SUPPORT
	BOOL RegisterToButtonGroup(OpButtonGroup* button_group);
	virtual void OnRemoved();
#endif // BUTTON_GROUP_SUPPORT

#ifdef QUICK
	virtual void OnUpdateActionState();

	// == OpToolTipListener ======================

	virtual BOOL HasToolTipText(OpToolTip* tooltip);
	virtual void GetToolTipText(OpToolTip* tooltip, OpInfoText& text);
	virtual INT32 GetToolTipDelay(OpToolTip* tooltip);

	void SetPropertyPage(OpGroup* property_page) {m_property_page = property_page;}
	OpGroup *GetPropertyPage() { return m_property_page; }

	// == QuickWidget ============================

	virtual BOOL IsAllowedInExtenderMenu() {return TRUE;}

#endif // QUICK

	BOOL HasDropdown() {return m_button_extra != EXTRA_NONE && m_button_extra != EXTRA_PLUS_ACTION;}

#ifdef WIDGETS_AUTOSCROLL_HIDDEN_CONTENT
	virtual INT32 GetXScroll() { return m_x_scroll; }
#endif // WIDGETS_AUTOSCROLL_HIDDEN_CONTENT

	virtual void UpdateInfoText();
	void GetShortcutString(OpString& string);
#endif // SKIN_SUPPORT

	void PaintContent(OpRect text_rect, UINT32 fcol);

#ifdef WIDGETS_AUTOSCROLL_HIDDEN_CONTENT
	/** This enables "marquee" even if the button is not hovered by the mouse. */
	virtual BOOL AlwaysShowHiddenContent() { return FALSE; }
	BOOL ShowHiddenContent() { return string.GetWidth() > GetBounds().width &&
			(packed2.scroll_content || AlwaysShowHiddenContent()); }
#endif // WIDGETS_AUTOSCROLL_HIDDEN_CONTENT

	void GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);
	void GetMinimumSize(INT32* minw, INT32* minh);

	virtual void SetEnabled(BOOL enabled);

	// == Hooks ======================

	void OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);
	virtual void OnMouseMove(const OpPoint &point);
	virtual void OnMouseLeave();
	virtual void OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks);
	virtual void OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks);
	void OnFocus(BOOL focus,FOCUS_REASON reason);
	void OnTimer();

	// Implementing the OpTreeModelItem interface

	virtual Type		GetType();
	virtual BOOL		IsOfType(Type type);

	// == OpInputContext ======================

	virtual BOOL		OnInputAction(OpInputAction* action);
	virtual const char*	GetInputContextName();

	virtual void OnScaleChanged() {string.NeedUpdate();}

private:

#ifdef SKIN_SUPPORT
	/**
	 * Determines the action to be invoked on button click.
	 *
	 * @param plus_action use @c TRUE to choose the action after the plus operator
	 * @return the action to be invoked
	 */
	virtual OpInputAction* GetNextClickAction(BOOL plus_action);
#endif // SKIN_SUPPORT

#ifdef QUICK // because it's used by OnUpdateActionState()
	/**
	 * Gets information required to update the button state.
	 *
	 * @param[out] action_to_use the action chosen to base the state on
	 * @param[out] state_to_use the action state of @a action_to_use
	 * @param[out] next_operator_used whether the next operator was used
	 */
	virtual void GetActionState(OpInputAction*& action_to_use,
			INT32& state_to_use, BOOL& next_operator_used);
#endif

public:
	union {
		struct {
			unsigned int has_defaultlook:1;
			unsigned int has_forced_focuslook:1;
			unsigned int is_down:1;
			unsigned int is_outside:1;
			unsigned int is_pushed:1;
			unsigned int has_attention:1;
			unsigned int is_double_action:1;
			unsigned int is_click_animation_on:1;
			unsigned int is_bold:1;
			unsigned int has_fixed_type_and_style:1;
			unsigned int has_fixed_image:1;
			unsigned int show_shortcut:1;
#ifdef WIDGETS_AUTOSCROLL_HIDDEN_CONTENT
			unsigned int scroll_content; //< TRUE when contents should be scrolled due to focus
#endif // WIDGETS_AUTOSCROLL_HIDDEN_CONTENT
		} packed2;
		unsigned int packed2_init;
	};

	ButtonType				m_button_type;

#ifdef SKIN_SUPPORT
	OpWidgetImage			m_dropdown_image;
	ButtonStyle				m_button_style;
	ButtonExtra				m_button_extra;
	INT32					m_hover_counter;

	/* The return type of OpSystemInfo::GetWallClockMS() is double. Its UNIX
	   implementation returns the amount of milliseconds since
	   1970-01-01 00:00:00, which means that we currently get a 13 digit
	   number. When typecasting such big numbers to INT32, it becomes INT_MIN.
	   The following two members used to be declared as INT32. Glow-on-hover
	   effects don't work under UNIX then. */

	double					m_hover_start_time;
	double					m_click_start_time;

	OpInputAction*			m_action_to_use;
	OpInputState			m_input_state;
#endif
#ifdef HAS_TAB_BUTTON_POSITION
	INT32                   m_tab_state;
#endif
#ifdef WIDGETS_AUTOSCROLL_HIDDEN_CONTENT
	INT32 m_x_scroll;
#endif
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	OpString				m_accessibility_text;
#endif

	OpWidgetString string;

#ifdef BUTTON_GROUP_SUPPORT
	OpButtonGroup*			m_button_group;
#endif // BUTTON_GROUP_SUPPORT

#ifdef QUICK
	OpString				m_text;				///< Has some weird function used by QUICK.
	OpGroup*				m_property_page;
#endif //QUICK
};

/** Wrapper class for OpButton with checkbox type. (Widget displaying togglable checkmark) */

class OpCheckBox : public OpButton
{
	protected:

		virtual				~OpCheckBox() {}

	public:

		static OP_STATUS Construct(OpCheckBox** obj);

		OpCheckBox() : OpButton(TYPE_CHECKBOX), m_indeterminate(FALSE)
		{
			font_info.justify = JUSTIFY_LEFT;
		}

		/** Set indeterminate status. If TRUE, the widget will display with indeterminate style. */
		void SetIndeterminate(BOOL indeterminate)	{ if (indeterminate != m_indeterminate) { m_indeterminate = indeterminate; InvalidateAll();} }
		BOOL GetIndeterminate()						{ return m_indeterminate; }
	private:
		BOOL m_indeterminate;
};

/** Wrapper class for OpButton with radiobutton type. (Widget displaying choice between this and other radiobuttons in the same group) */

class OpRadioButton : public OpButton
{
	protected:

		virtual				~OpRadioButton() {}

	public:

		static OP_STATUS Construct(OpRadioButton** obj);

		OpRadioButton() : OpButton(TYPE_RADIO)
		{
			font_info.justify = JUSTIFY_LEFT;
		}
};

#endif // OP_BUTTON_H
