/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef FORMVALUECOLOR_H
#define FORMVALUECOLOR_H

#include "modules/forms/formvalue.h"

/**
 * Every form type html element has a form value. The form value can
 * be internal or external. If it's external the value is really
 * stored in a widget. The widget can only be reached through the
 * HTML_Element which is the reason many methods has a HTML_Element as
 * a parameter. We don't want to store that pointer in the FormValue
 * for two reasons. First it cost 4 bytes which is too much and second
 * we don't want to maintain that pointer.
 */
class FormValueColor : public FormValue
{
public:

private:
	/**
	 * Constructor.
	 */
	FormValueColor() : FormValue(VALUE_COLOR), m_color(OP_RGB(0, 0, 0)) {}

public:
	/**
	 * "Cast" the FormValue to this class. This asserts in debug
	 * builds if done badly. This should be inlined and be as
	 * cheap as a regular cast. If it turns out it's not we
	 * might have to redo this design.
	 *
	 * @param val The FormValue pointer that really is a pointer to
	 * an object of this class.
	 */
	static FormValueColor* GetAs(FormValue* val)
	{
		OP_ASSERT(val->GetType() == VALUE_COLOR);
		return static_cast<FormValueColor*>(val);
	}

	/**
	 * The one and only right way to create a new FormValueColor.
	 *
	 * @param he The form element the form value will belong to.
	 *
	 * @param out_value Pointer to the created FormValue.
	 */
	static OP_STATUS ConstructFormValueColor(HTML_Element* he,
		FormValue*& out_value);
	/**
	 * The destructor. Virtual since we will delete objects
	 * by deleting pointers to the base class.
	 */
	virtual ~FormValueColor() {}

	// See base class
	virtual FormValue* Clone(HTML_Element *he);

	// See base class
	virtual OP_STATUS GetValueAsText(HTML_Element* he,
									 OpString& out_value) const;
	// See base class
	virtual OP_STATUS SetValueFromText(HTML_Element* he,
									   const uni_char* in_value);

	// See base class
	virtual OP_STATUS ResetToDefault(HTML_Element* he);

	// See base class
	virtual BOOL Externalize(FormObject* form_object_to_seed);

	// See base class
	virtual void Unexternalize(FormObject* form_object_to_replace);
private:

	/**
	 * Sets the color in the widget specified by form_object.
	 */
	static OP_STATUS SetWidgetColor(FormObject* form_object, COLORREF color);

	/**
	 * Gets the color from the widget specified by form_object. Any problem
	 * will result in OP_RGB(0, 0, 0) being returned. The returned color
	 * will have full alpha and will not be named.
	 */
	static COLORREF GetWidgetColor(FormObject* form_object);

	/**
	 * The color.
	 */
	COLORREF m_color;
};

#endif // FORMVALUECOLOR_H
