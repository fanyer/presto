/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "modules/logdoc/complex_attr.h"

class FormObject;

/**
 * A wrapper class to store a FormObject in an element's attribute list.
 */
class FormObjectBackup : public ComplexAttr
{
public:
	/**
	 * Takes ownership of a FormObject to store it in an HTML_Element
	 * attribute.
	 */
	FormObjectBackup(FormObject* form_object) : m_form_object(form_object)
	{
		OP_ASSERT(form_object);
	}
	virtual ~FormObjectBackup() { delete m_form_object; }

//	virtual BOOL IsA(int type) { return type == T_UNKNOWN; }

	// See base class
	virtual OP_STATUS	CreateCopy(ComplexAttr **copy_to)
	{
		return OpStatus::ERR_NOT_SUPPORTED; // Not supported
	}

	/**
	 * Releases the form object from this proxy class.
	 */
	FormObject* ReleaseFormObject()
	{
		FormObject* form_object = m_form_object;
		m_form_object = NULL;
		return form_object;
	}

private:
	/**
	 * The form object.
	 */
	FormObject* m_form_object;
};

