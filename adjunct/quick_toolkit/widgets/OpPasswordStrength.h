/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_PASSWORD_STRENGTH_H
#define OP_PASSWORD_STRENGTH_H

#include "adjunct/desktop_util/passwordstrength/PasswordStrength.h"

#include "modules/widgets/OpWidget.h"

class OpIcon;
class OpButton;
class OpEdit;

/**
 * @brief Password strength widget. Allows to show the perceived strength of the password, 
 *		  when connected to the edit box. 
 *		  For UI concepts see <a href='https://bugs.opera.com/browse/PGDSK-651'>PGSDK-651</a>.
 * 
 * For the widget to work, it must be connected to password
 * edit box, by placing definition in dialog.ini similar to this one:
 *
 * @code
 * Edit,             0, password_edit,             170, 235, 160,  23, Size right
 * PasswordStrength, 0, password_strength_control, 330, 230,  84,  40, Move right
 * @endcode
 *
 * and this logic inside dialog box handling code:
 *
 * @code
 * OpEdit *password_edit = static_cast<OpEdit*>(GetWidgetByName("password_edit"));
 * if (password_edit) password_edit->SetPasswordMode(TRUE);
 * 
 * OpPasswordStrength* password_strength = 
 *     static_cast<OpPasswordStrength*> (GetWidgetByName("password_strength_control"));
 * if (password_strength && password_edit)
 *     password_strength->AttachToEdit(password_edit);
 * @endcode
 *
 * When placing inside dialog box, remember to leave 84 pixel for
 * it's width. Also, remember that the widget will always resize
 * itself to the smallest rect containing the image skin + image skin
 * padding, as well as text size + text skin padding. This is done to
 * ensure, than specific system font used or current locale will not
 * generate labels that will get clipped off.
 *
 * Skin designers are encouraged to create images 80x20 pixels and choose
 * the text padding so that it will be aligned with the first non-shadow
 * element of the image. Also, to avoid scaling problems, all images should
 * have the same size.
 */

class OpPasswordStrength
	: public OpWidget
{
	/// Widget Listener waiting for all editbox events signaled, when text in edit box changes...
	class PassListener
		: public OpWidgetListener
	{
		OpPasswordStrength* parent;
		void OnChange(OpWidget *widget, BOOL changed_by_mouse = FALSE);
		void OnChangeWhenLostFocus(OpWidget *widget);

	public:
		explicit PassListener(OpPasswordStrength* parent): parent(parent) {}
	};

	struct Padding
	{
		INT32 width, height, left, top, right, bottom;
		Padding(OpWidget* obj)
		{
			obj->GetPreferedSize(&width, &height, 1, 1);
			obj->GetForegroundSkin()->GetPadding(&left, &top, &right, &bottom);
		}
		Padding(INT32 width, INT32 height, OpWidget* obj)
		{
			this->width = width;
			this->height = height;
			obj->GetForegroundSkin()->GetPadding(&left, &top, &right, &bottom);
		}
		
		INT32 GetTotalHeight() const { return top + height + bottom; }
		INT32 GetTotalWidth() const { return left + width + right; }
		OpRect GetControlRect(INT32 offx, INT32 offy)
		{
			return OpRect(offx + left, offy + top, width, height);
		}
	};

	OpPasswordStrength();

	/// Helper function to aid with OpWidget creation.
	/// @returns error status of all steps along the creation of the widget:
	///          creation of icon and button (incl. OOM), getting the 
	///          text from language manager or setting it inside the label,
	///          or success otherwise.
	OP_STATUS CreateControls();

	/// Updates the image and label to currently selected strength.
	/// @returns error status of getting the text from language manager or setting
	///          it inside the label, or success otherwise.
	OP_STATUS InternalSetPasswordStrength();

	/// Overrided to disallow title change inside WidgetCreator.
	/// @returns the status of InternalSetPasswordStrength() execution
	OP_STATUS SetText(const uni_char* text) { return OpStatus::ERR_NOT_SUPPORTED; }

	/// Common layouting functinality.
	void InternalLayout(Padding here, Padding leds, Padding text);

	/// On windows XP it is not enough to implement only OnResize, probably because
	/// of some differences in what and how messages are posted there, so to fully
	/// show the widget we need to set its rects also in OnLayout.
	void OnLayout();

	/// Override of OpWidget::OnResize. Will alwyas update the size to currently
	/// calculated rect, based on first image set and currently selected label.
	void OnResize(INT32* new_w, INT32* new_h);

	/// Slot for all change events signaled from attached edit box.
	/// @param edit the source of the change event
	void OnPasswordChanged(OpEdit* edit);

	PasswordStrength::Level			m_pass_strength; /**< Currently set password strength */

	OpIcon*							m_leds; /**< Image part of the widget */
	OpButton*						m_text; /**< Label part of the widget */

	PassListener					m_password_listener; /**< Editbox-listening part of this object */

public:

	/// Used by WidgetCreator when building dialog box.
	/// @returns status of CreateControls() execution
	static OP_STATUS Construct(OpPasswordStrength** obj);

	/// Setter for the "PasswordStrength" property.
	/// When attached to the edit box, this will be called automatically.
	/// @param pass_strength new value for the property
	OP_STATUS SetPasswordStrength(PasswordStrength::Level pass_strength);

	/// Getter for the "PasswordStrength" property.
	/// @returns the current value of the property
	PasswordStrength::Level GetPasswordStrength() const { return m_pass_strength; }

	//Watir binding methods:
	OP_STATUS GetText(OpString& text);
	INT32 GetValue() { return static_cast<INT32> (GetPasswordStrength()); }
	Type GetType() { return WIDGET_TYPE_PASSWORD_STRENGTH; }
	BOOL IsOfType(Type type) { return type == WIDGET_TYPE_PASSWORD_STRENGTH || OpWidget::IsOfType(type); }
	
	/// Connects the password strength widget and edit box widget.
	/// @param edit edit box, which should be observed for text change
	void AttachToEdit(OpEdit* edit);
};

#endif // OP_PASSWORD_STRENGTH_H
