/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef FORMVALUEEMAIL_H
#define FORMVALUEEMAIL_H

#include "modules/forms/formvaluetext.h"

/**
 * Every form type html element has a form value. The form value can
 * be internal or external. If it's external the value is really
 * stored in a widget. The widget can only be reached through the
 * HTML_Element which is the reason many methods has a HTML_Element as
 * a parameter. We don't want to store that pointer in the FormValue
 * for two reasons. First it cost 4 bytes which is too much and second
 * we don't want to maintain that pointer.
 */
class FormValueEmail : public FormValueText
{
public:

private:
	/**
	 * Constructor.
	 */
	FormValueEmail() {}

public:
	// Use base class documentation.
	virtual FormValue* Clone(HTML_Element *he);

	/**
	 * The one and only right way to create a new FormValueEmail.
	 *
	 * @param he The form element the form value will belong to.
	 *
	 * @param hld_profile The hld_profile of the document with |he|.
	 *
	 * @param out_value Pointer to the created FormValue.
	 */
	static OP_STATUS ConstructFormValueEmail(HTML_Element* he,
											 HLDocProfile* hld_profile,
											 FormValue*& out_value);

	/**
	 * Helper method that does the same as ConvertToIDNA_ACE_L but returns
	 * an OP_STATUS instead of Leaving.
	 *
	 * @see ConvertToIDNA_ACE_L()
	 */
	static OP_STATUS ConvertToIDNA(uni_char *source, uni_char *target,
	                               size_t targetbufsize, BOOL is_mailaddress);

protected:
	// Use base class documentation.
	virtual OP_STATUS ConvertRealValueToDisplayValue(const uni_char* real_value,
	                                                 OpString& display_value) const;
	// Use base class documentation.
	virtual OP_STATUS ConvertDisplayValueToRealValue(OpString& value) const;

private:
	/**
	 * Helper method that does the same as ConvertFromIDNA_ACE_L but returns
	 * an OP_STATUS instead of Leaving.
	 *
	 * @see ConvertFromIDNA_ACE_L()
	 */
	static OP_STATUS ConvertFromIDNA(const char* source, uni_char *target,
	                                 size_t targetbufsize, BOOL& is_mailaddress);

	/**
	 * Helper method that splits mail a list of mail addresses and
	 * converts the domains to or from IDNA form.
	 */
	static OP_STATUS ConvertMailAddressesIDNA(const uni_char* in,
	                                          TempBuffer* result, BOOL from_idna);
};

#endif // FORMVALUEEMAIL_H
