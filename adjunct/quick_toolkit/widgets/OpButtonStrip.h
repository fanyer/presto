/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef BUTTON_STRIP_H
#define BUTTON_STRIP_H

#include "modules/widgets/OpButton.h"
#include "adjunct/quick_toolkit/widgets/QuickOpWidgetWrapper.h"

/** @brief Represents the buttons that normally appear at the bottom of a dialog
  * You can use this widget to change the layout of the button strip by using
  * a ButtonStrip element in dialog.ini.
  */
class OpButtonStrip : public OpWidget
{
public:
	/** @param obj Where to construct object
	  * @param reverse_buttons Whether to reverse the buttons (Mac-Style)
	  */
	static OP_STATUS Construct(OpButtonStrip** obj, BOOL reverse_buttons)
		{ *obj = OP_NEW(OpButtonStrip, (reverse_buttons)); return *obj ? OpStatus::OK : OpStatus::ERR_NO_MEMORY; }

	explicit OpButtonStrip(BOOL reverse_buttons);

	~OpButtonStrip();

	/** Set the number of buttons to appear in this button strip
	  * @param button_count Number of buttons
	  */
	OP_STATUS SetButtonCount(int button_count);

	/** Set properties on a certain button, shorthand for the various button setters below
	  * @param id ID of button to change (between 0 and button_count)
	  * @param action Action to set on button
	  * @param text Text to set on button
	  * @param enabled Whether button should be enabled
	  * @param visible Whether button should be visible
	  */
	OP_STATUS SetButtonInfo(int id, OpInputAction* action, const OpStringC& text, BOOL enabled, BOOL visible, const OpStringC8& name);

	/** Enable or disable a button
	  * @param id ID of button to change (between 0 and button_count)
	  * @param enable Whether to enable or disable button
	  */
	void EnableButton(int id, BOOL enable) { m_buttons.Get(id)->SetEnabled(enable); m_buttons.Get(id)->SetUpdateNeeded(TRUE); m_buttons.Get(id)->UpdateActionStateIfNeeded(); }

	/** Show or hide a button
	 * @param id ID of button to change (between 0 and button_count)
	 * @param enable Whether to show or hide button
	 */
	void ShowButton(int id, BOOL show);

	/** Set the text on a button
	  * @param id ID of button to change (between 0 and button_count)
	  * @param text Text to set on button
	  */
	OP_STATUS SetButtonText(int id, const uni_char* text) { return m_buttons.Get(id)->SetText(text); }

	/** Set the name of a button
	 * @param id ID of button to change (between 0 and button_count)
	 * @param name Name to set the button to
	 */
	void SetButtonName(int id, const char* name) { m_buttons.Get(id)->SetName(name); }
	
	/** Set the action for a button
	  * @param id ID of button to change (between 0 and button_count)
	  * @param action Action to set on button
	  */
	void SetButtonAction(int id, OpInputAction* action) { m_buttons.Get(id)->SetAction(action); }

	/** Focus a button
	  * @param id ID of button to focus (between 0 and button_count)
	  */
	void FocusButton(int id) { m_buttons.Get(id)->SetFocus(FOCUS_REASON_OTHER); }

	/** Set the default button in the strip
	  * @param id ID of button to set as default (between 0 and button_count)
	  * @m_big_default_button will increase the width of the default button if TRUE
	  */
	void SetDefaultButton(int id, float default_button_size = 1.0f);

	/** Set the standard size for buttons in this strip
	  * @param width Width of buttons
	  * @param height Height of buttons
	  */
	void SetButtonSize(int width, int height, BOOL is_custom_size = FALSE) { m_width = width; m_height = height; m_is_custom_size = is_custom_size;}

	/** Set margins to use around buttons
	  */
	void SetButtonMargins(int left, int top, int right, int bottom) { m_button_margins = Margin(left, top, right, bottom); }

	/** Set padding to use around buttons
	  */
	void SetButtonPadding(int left, int top, int right, int bottom) { m_button_padding = Margin(left, top, right, bottom); }

	/** Set whether this button strip has back and forward buttons (wizard-style)
	  * @param back_forward
	  */
	void SetHasBackForwardButtons(BOOL back_forward) { m_back_forward_buttons = back_forward; }

	/** Set the skin to use for the border around the buttons
	  * @param skin A skin element to use for the borders
	  */
	OP_STATUS SetBorderSkin(const OpStringC8& skin) { return m_border_skin.Set(skin); }

	/** Get the ID of a button that's associated with a certain widget
	  * @param widget Widget to search for
	  * @return ID of the button associated with widget, or -1 if not found
	  */
	int GetButtonId(OpWidget* widget);

	/** Calculate initial positions and sizes of the buttons
	  * @return Width of all buttons together
	  */
	int CalculatePositions();

	/** Set the positions and sizes of the buttons, based on the current bounds of this widget
	  */
	void PlaceButtons();

	/** Set if the buttons should be centered in the dialog
	  */
	void SetCenteredButtons(BOOL centered) { m_centered_buttons = centered; }

	// From OpWidget
	virtual Type GetType() { return WIDGET_TYPE_BUTTON_STRIP; }

	/** Sets all buttons with lower ID than button_id to be left aligned.
	  * Normally the whole strip is right aligned inside the dialog. This function
	  * will left aligne all buttons with lower ID than button_id, and right align the rest.
	  * @param button_id id of the first button to be right alined
	  */
	void SetDynamicSpaceAfter(int button_id) { m_dynamic_space = (UINT)button_id; OP_ASSERT(button_id >= 0); }

	BOOL IsCustomSize() { return m_is_custom_size; }

	/** Retrieve button size.
	 */
	void GetButtonSize(int &width, int &height) { width = m_width, height= m_height; }

private:
	struct Margin
	{
		int left;
		int top;
		int right;
		int bottom;

		Margin(int _left, int _top, int _right, int _bottom) : left(_left), top(_top), right(_right), bottom(_bottom) {}
	};

	OP_STATUS AddButton();
	void PlaceDialogButton(INT32 button_index, const OpRect& rect);

	const BOOL m_reverse_buttons;
	OpWidgetVector<OpButton> m_buttons;
	OpString8 m_border_skin;
	int m_width;					// Desired width of each button.
	int m_height;					// Desired height of each button.
	BOOL m_resize_buttons;
	BOOL m_centered_buttons;
	BOOL m_back_forward_buttons;
	OpRect* m_button_rects;
	int m_total_width;				// Total width used by the buttons.
	Margin m_button_margins;
	Margin m_button_padding;
	UINT m_dynamic_space;			// Left alignes buttons with lower ID (off if value is 0).
	float m_default_button_ratio;	// Will make the default button a different size then the rest. 
	BOOL m_is_custom_size;
};


#endif // BUTTON_STRIP_H
