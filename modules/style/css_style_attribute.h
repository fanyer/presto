/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef CSS_STYLE_ATTRIBUTE_H
#define CSS_STYLE_ATTRIBUTE_H

#include "modules/logdoc/complex_attr.h"

class CSS_property_list;
class HLDocProfile;
class URL;

class StyleAttribute : public ComplexAttr
{
public:
	// How to ensure uniqeness?
	enum
	{
		T_STYLE_ATTRIBUTE = 1265326
	};

	/**
	 * @param string The string representation. Will be owned by this
	 * object and removed with delete[] when the attribute is
	 * removed.
	 *
	 * @param prop_list The CSS_property_list. Assumed to be already
	 * refed. Will deref when the attribute is destroyed.
	 */
	StyleAttribute(uni_char* string, CSS_property_list* prop_list) :
		m_prop_list(prop_list),
		m_original_string(string)
	{
		OP_ASSERT(prop_list);	// prop_list must not be NULL
	}

	virtual ~StyleAttribute();

	virtual BOOL IsA(int type) { return type == T_STYLE_ATTRIBUTE; }

	/// Used to clone HTML Elements. Returning OpStatus::ERR_NOT_SUPPORTED here
	/// will prevent the attribute from being cloned with the html element.
	///\param[out] copy_to A pointer to an new'ed instance of the same object with the same content.
	virtual OP_STATUS	CreateCopy(ComplexAttr** copy_to);

	void				CreateCopyL(ComplexAttr** copy_to);

	/// Used to get a string representation of the attribute. The string
	/// value of the attribute must be appended to the buffer. Need not
	/// be implemented.
	///\param[in] buffer A TempBuffer that the serialized version of the obejct will be appended to.
	virtual OP_STATUS	ToString(TempBuffer* buffer);

	CSS_property_list* GetProperties() { return m_prop_list; }

	void SetIsModified() { OP_DELETEA(m_original_string); m_original_string = NULL; }

#ifdef SVG_SUPPORT
	/**
	 * Reparses the style string. Needed in SVG since SVG can add fonts
	 * so font-name -> font_number has to be redone.
	 */
	OP_STATUS ReparseStyleString(HLDocProfile* hld_profile, const URL& base_url);
#endif // SVG_SUPPORT

private:
	CSS_property_list* m_prop_list;
	uni_char* m_original_string;
};

#endif // CSS_STYLE_ATTRIBUTE_H
