/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef COMPLEX_ATTR_H
#define COMPLEX_ATTR_H

class TempBuffer;

/// Interface for complex datastructures that can be added to
/// the HTML_Element as an attribute.
class ComplexAttr
{
public:
	/// Types used by the IsA() method
	enum
	{
		T_UNKNOWN,
		T_LOGDOC,
		T_SOURCE_ATTR
	};

protected:
	ComplexAttr() {}

public:
	virtual ~ComplexAttr() {}

	/// Used to check the types of complex attr. SHOULD be implemented
	/// by all subclasses and all must match a unique value.
	///\param[in] type A unique type value.
	virtual BOOL IsA(int type) { return type == T_UNKNOWN; }

	/// Used to clone attributes when cloning HTML_Elements. Returning
	/// OpStatus::ERR_NOT_SUPPORTED here
	/// will prevent the attribute from being cloned with the html element.
	///\param[out] copy_to A pointer to an new'ed instance of the same object with the same content.
	virtual OP_STATUS	CreateCopy(ComplexAttr **copy_to) { return OpStatus::ERR_NOT_SUPPORTED; }

	/// Used to get a string representation of the attribute. The string
	/// value of the attribute must be appended to the buffer. Need not
	/// be implemented.
	///\param[in] buffer A TempBuffer that the serialized version of the object will be appended to.
	virtual OP_STATUS	ToString(TempBuffer *buffer) { return OpStatus::OK; }

	/// Called when the element to which the attribute belongs is moved from one
	/// document to another.  If this function returns FALSE, the atttribute is
	/// removed from the element, otherwise the function is assumed to have
	/// updated any document dependent references the attribute might have.
	///\param[in] old_document Document the element is moved from.  Can be NULL.
	///\param[in] new_document Document the element is moved to.  Can be NULL.
	virtual BOOL MoveToOtherDocument(FramesDocument *old_document, FramesDocument *new_document) { return TRUE; }

	/// Compare two ComplexAttrs of the same class.
	/// This method is used to compare two attribute values to detect if the attribute
	/// has been set to the same value as it already had. It is used as an optimization
	/// for skipping operations that would normally be done when the attribute value changes.
	/// It's not strictly necessary to implement this method. In that case, setting the
	/// attribute will always be considered to be an attribute change.
	///\param[in] other The ComplexAttr to compare to.  Never NULL.
	///@returns TRUE if the two attributes are equal, otherwise FALSE.
	virtual BOOL Equals(ComplexAttr *other) { return FALSE; }
};

#endif // COMPLEX_ATTR_H
