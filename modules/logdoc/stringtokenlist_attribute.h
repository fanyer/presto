/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef STRINGTOKENLIST_ATTRIBUTE_H
#define STRINGTOKENLIST_ATTRIBUTE_H

#include "modules/logdoc/complex_attr.h"

/** 
 * This class is used to represent attributes of DOMTokenList and DOMSettableTokenList type.
 *
 * It stores attribute value in two data representations: string and collection
 * of tokens, which came as a result of splitting string on whitespaces.
 **/
class StringTokenListAttribute : public ComplexAttr
{
public:
	virtual ~StringTokenListAttribute() {}

	// Implements ComplexAttr.
	virtual OP_STATUS CreateCopy(ComplexAttr **copy_to);
	virtual OP_STATUS ToString(TempBuffer *buffer) { return buffer->Append(m_string.CStr()); }
	virtual BOOL Equals(ComplexAttr* other) { OP_ASSERT(other); return uni_strcmp(GetAttributeString(), static_cast<StringTokenListAttribute*>(other)->GetAttributeString()) == 0; }

	/** 
	 * Fully set value of attribute.
	 *
	 * @param attr_str Non-null string which has to be set on attribute.  Will
	 * be split on whitespaces.
	 *
	 * @return OpStatus::OK or OOM.
	 */
	OP_STATUS SetValue(const uni_char* attr_str, size_t str_len=static_cast<size_t>(-1));

	/** Get attribute literal string value. **/
	const uni_char* GetAttributeString() const { return m_string.CStr(); }

	/** Get attribute value in form of token collection. **/
	const OpAutoVector<OpString>& GetTokens() const { return m_list; }
	
	int GetTokenCount() const { return m_list.GetCount(); }

	/** 
	 * Check whether this attribute contains given token.
	 *
	 * @param[in] to_find Non-null string with no whitespaces, which is searched for.
	 * 
	 * @return TRUE if |to_find| was found.
	 */
	BOOL Contains(const uni_char* to_find)
	{
		for (UINT32 i = 0; i < m_list.GetCount(); i++)
			if (uni_str_eq(m_list.Get(i)->CStr(), to_find))
				return TRUE;
		return FALSE;
	}

	/** 
	 * Get index-th token from collection.
	 *
	 * @param[in] index Index of element to get, from [0, length-1] range.
	 *
	 * @param[out] contents_out Resulting string.
	 *
	 * @return OpStatus::ERR if index is out of range, otherwise OOM or OK.
	 */
	OP_STATUS Get(UINT32 index, TempBuffer *contents_out)
	{
		if (index < m_list.GetCount())
			return contents_out->Append(m_list.Get(index)->CStr());
		return OpStatus::ERR;
	}

	/** 
	 * Add a token to this attribute.  If token is already present in the attribute, duplicate is not added.
	 *
	 * @param[in] token_to_add Non-empty string with no whitespaces.
	 *
	 * @return OpStatus::OK or OOM.
	 */
	OP_STATUS Add(const uni_char* token_to_add);

	/** 
	 * Remove all the occurences of |token_to_remove|.
	 *
	 * @param[in] token_to_remove Non-empty string with no whitespaces.
	 * 
	 * @return OpStatus::OK or OOM (since underlying string representation needs to be reallocated).
	 */
	OP_STATUS Remove(const uni_char* token_to_remove);

private:
	/** 
	 * Add |what| at the end of |where_to| with at most one extra whitespace.
	 * 
	 * @param[in,out] where_to String to expand.  If it ends with whitespace, |what| is
	 * appended with no addition.  Otherwise, extra space is added.
	 *
	 * @param[in] what String to add.
	 *
	 * @param length Place of truncation of |what| parameter.
	 *
	 * @return OpStatus::OK or OOM.
	 */
	static OP_STATUS AppendWithSpace(TempBuffer* where_to, const uni_char* what, size_t length=static_cast<size_t>(-1))
	{
		OP_ASSERT(where_to);
		if (!length)
			return OpStatus::OK;
		if (where_to->Length())
		{
			uni_char last_character = where_to->GetStorage()[where_to->Length()-1];
			if (!IsHTML5Space(last_character))
				RETURN_IF_ERROR(where_to->Append(' '));
		}
		return where_to->Append(what, length);
	}
	OpAutoVector<OpString> m_list; //< Attribute split on whitespaces.  Can contain duplicates.
	OpString m_string; //< Attribute string, includes all bizarre whitespaces.
};

#endif // STRINGTOKENLIST_ATTRIBUTE_H

