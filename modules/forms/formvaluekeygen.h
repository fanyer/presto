/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef FORMVALUEKEYGEN_H
#define FORMVALUEKEYGEN_H

#ifdef FORMS_KEYGEN_SUPPORT

#include "modules/forms/formvalue.h"
#include "modules/forms/tempbuffer8.h"

/**
 * Every form type html element has a form value. The form value can
 * be internal or external. If it's external the value is really
 * stored in a widget. The widget can only be reached through the
 * HTML_Element which is the reason many methods has a HTML_Element as
 * a parameter. We don't want to store that pointer in the FormValue
 * for two reasons. First it cost 4 bytes which is too much and second
 * we don't want to maintain that pointer.
 */
class FormValueKeyGen : public FormValue
{
public:

private:
	/**
	 * Constructor.
	 */
	FormValueKeyGen() : FormValue(VALUE_KEYGEN), m_keysize_idx(-1) {}

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
	static FormValueKeyGen* GetAs(FormValue* val)
	{
		OP_ASSERT(val->GetType() == VALUE_KEYGEN);
		return static_cast<FormValueKeyGen*>(val);
	}

	/**
	 * The one and only right way to create a new FormValue.
	 *
	 * @param he The form element the form value will belong to.
	 *
	 * @param out_value Pointer to the created FormValue.
	 */
	static OP_STATUS ConstructFormValueKeyGen(HTML_Element* he,
		FormValue*& out_value);
	/**
	 * The destructor. Virtual since we will delete objects
	 * by deleting pointers to the base class.
	 */
	virtual ~FormValueKeyGen() {}

	/**
	 * Creates a copy of this form value. XXX Externalized clones.
	 *
	 * Triggered by calls to DOM that clones HTML nodes and by
	 * WEBFORMS REPEAT elements and generated content.
	 */
	virtual FormValue* Clone(HTML_Element *he);

	/**
	 *
	 * Returns the selected index as a string.
	 *
	 * @param he The HTML element this FormValue belongs to.
	 *
	 * @param out_value The returned text.
	 */
	virtual OP_STATUS GetValueAsText(HTML_Element* he,
									 OpString& out_value) const;
	/**
	 *
	 * Returns the selected key size (not index, pure keysize).
	 *
	 * @param he The HTML element this FormValue belongs to.
	 *
	 * @return The keysize. 0 if nothing selected.
	 */
	unsigned int GetSelectedKeySize(HTML_Element* he) const;

	/**
	 * Does nothing.
	 *
	 * @param he The HTML element this FormValue belongs to.
	 *
	 * @param in_value The new value to set. May be NULL if the current value
	 * should be emptied.
	 */
	virtual OP_STATUS SetValueFromText(HTML_Element* he,
									   const uni_char* in_value);

	/**
	 * Does nothing.
	 *
	 * @param he The HTML element this FormValue belongs to.
	 */
	virtual OP_STATUS ResetToDefault(HTML_Element* he);

	// See base class
	virtual BOOL Externalize(FormObject* form_object_to_seed);

	// See base class
	virtual void Unexternalize(FormObject* form_object_to_replace);

	/**
	 * Stores a generated key.
	 *
	 * @param key The key to store or NULL to clear the stored key.
	 *
	 * @returns OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS SetGeneratedKey(const char* key) { return m_generated_key.Set(key); }

	/**
	 * Returns the key stored with SetGeneratedKey.
	 *
	 * @returns the key or NULL
	 */
	const char* GetGeneratedKey() { return m_generated_key.CStr(); }

private:
	/**
	 * The selected index among the available keysizes.
	 */
	int m_keysize_idx;

	OpString8 m_generated_key;
};

#endif // FORMS_KEYGEN_SUPPORT
#endif // FORMVALUEKEYGEN_H
