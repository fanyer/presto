/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CSS_PROPERTY_LIST_H
#define CSS_PROPERTY_LIST_H

class TempBuffer;

#include "modules/util/simset.h"

class CSS_decl;

class CSS_property_list
{
	OP_ALLOC_ACCOUNTED_POOLING
	OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT

public:

	/** Flags describing certain features of a declaration block.
		The features for a CSS_property_list is a union of these flags. */
	enum Flag
	{
		/** No flags set. */
		HAS_NONE = 0,
		/** Property list has a content property. */
		HAS_CONTENT = 1,
		/** Property list has a property that takes url() values. */
		HAS_URL_PROPERTY = 2,
		/** Property list has a property that will enable CSS transitions. */
		HAS_TRANSITIONS = 4,
		/** Property list has a property that will enable CSS animations. */
		HAS_ANIMATIONS = 8,

	};

					CSS_property_list() : ref_count(0)
					{
					}
					~CSS_property_list();

	void			Ref() { ref_count++; }
	void			Unref() { OP_ASSERT(ref_count > 0); if (--ref_count == 0) OP_DELETE(this); }

	CSS_decl*		GetFirstDecl() { return (CSS_decl*)decl_list.First(); }
	CSS_decl*		GetLastDecl() { return (CSS_decl*)decl_list.Last(); }
	unsigned		GetLength() { return decl_list.Cardinal(); }
	BOOL			IsEmpty() { return decl_list.Empty(); }

	void			AddDecl(CSS_decl* decl, BOOL important, CSS_decl::DeclarationOrigin origin);

	/** Find and a replace the last occurrence of a declaration.

		This function looks for a declaration with the same property as the
		incoming declaration, searching last to first. If one is found, that
		declaration is removed, and replaced with the incoming declaration.

		If no matching declaration is found, the declaration is added to the
		end of the list.

		@param decl The declaration to find and replace.
		@param important TRUE if 'decl' is "!important", FALSE otherwise.
		@param origin The origin of the declaration.

		@see CSS_decl::DeclarationOrigin
	*/
	void			ReplaceDecl(CSS_decl* decl, BOOL important, CSS_decl::DeclarationOrigin origin);

	void			AddDeclL(int property, const uni_char* value, int value_len, BOOL important, CSS_decl::DeclarationOrigin origin, CSS_string_decl::StringType type, BOOL source_local);
	void			AddDeclL(int property, uni_char* value, BOOL important, CSS_decl::DeclarationOrigin origin, CSS_string_decl::StringType type, BOOL source_local);
	void			AddDeclL(int property, short* val_array, int val_array_len, BOOL important, CSS_decl::DeclarationOrigin origin);
	void			AddDeclL(int property, CSS_generic_value* val_array, int val_array_len, int layer_count, BOOL important, CSS_decl::DeclarationOrigin origin);
	void			AddDeclL(int property, const CSS_generic_value_list& val_list, int layer_count, BOOL important, CSS_decl::DeclarationOrigin origin);
	void			AddDeclL(int property, float val, int val_type, BOOL important, CSS_decl::DeclarationOrigin origin);
	void			AddDeclL(int property, float val1, float val2, int val1_type, int val2_type, BOOL important, CSS_decl::DeclarationOrigin origin);
	void			AddDeclL(int property, short types[4], float values[4], BOOL important, CSS_decl::DeclarationOrigin origin);
	void			AddLongDeclL(int property, long value, BOOL important, CSS_decl::DeclarationOrigin origin);
	void			AddColorDeclL(int property, COLORREF value, BOOL important, CSS_decl::DeclarationOrigin origin);
	void			AddTypeDeclL(int property, CSSValue value, BOOL important, CSS_decl::DeclarationOrigin origin);

	CSS_decl*       RemoveDecl(int property);

	/** Post-process the property list after declarations have been added/removed.
		The actual processing done is decided by the parameters.

		@param delete_duplicates Remove and delete declarations that are overridden
								 by other declarations in the same property list.
		@param get_flags Calculate the set of Flag values for this property list.
		@return The set of Flag values if get_flags is TRUE.
	*/
	unsigned int	PostProcess(BOOL delete_duplicates, BOOL get_flags);

    BOOL            AddList(CSS_property_list* list, BOOL force_important=FALSE);

	CSS_property_list*
					GetCopyL();

	void			AppendPropertiesAsStringL(TempBuffer* tmp_buf);

#ifdef STYLE_PROPNAMEANDVALUE_API
	/** Appends the property name of declaration idx to name, and the property value of
		declaration idx to value, and sets the important flag.

		@param idx Index of the declaration to get the name and value for. First declaration
				   has index 0.
	    @param name TempBuffer that the property name of the declaration will be appended to.
		@param value TempBuffer the the property value of the declaration will be appended to.
		@param important Set to TRUE by this method if the declaration is !important, otherwise
						 set to FALSE.
		@return OpStatus::ERR_NO_MEMORY on OOM.
	*/
	OP_STATUS		GetPropertyNameAndValue(unsigned int idx, TempBuffer* name, TempBuffer* value, BOOL& important);
#endif // STYLE_PROPNAMEANDVALUE_API

private:

	/** Return the property flags for a given declaration.

		@param decl The declaration to get the flags for.
		@return A union of flags from the Flag enum
	*/
	static unsigned int GetPropertyFlags(CSS_decl* decl);

	/** Linked list of CSS_decl objects */
	Head			decl_list;

	/** Reference count */
	int				ref_count;
};

#endif // CSS_PROPERTY_LIST_H
