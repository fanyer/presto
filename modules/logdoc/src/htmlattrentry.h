#ifndef HTMLATTRENTRY_H
#define HTMLATTRENTRY_H

#include "modules/logdoc/namespace.h" // for NS_IDX_DEFAULT

/**
 * Storage for attributes in markup.
 * When creating HTML_Elements an array of these are used to store the data
 * needed to construct the attributes. A static array of these are used
 * during parsing.
 */
class HtmlAttrEntry
{
public:
	HtmlAttrEntry() : attr(0), ns_idx(NS_IDX_DEFAULT), 
		is_id(FALSE), is_specified(TRUE), is_special(FALSE),
		delete_after_use(FALSE),
		value(NULL), value_len(0), 
		name(NULL), name_len(0) {}

	int				attr; ///< The numeric code for the attribute
	int				ns_idx; ///< The namespace index for the attribute

	BOOL			is_id; ///< Is this an ID attribute
	BOOL			is_specified; ///< Was it specified in the document or defaulted
	BOOL			is_special; ///< Is it a special internal attribute

	BOOL			delete_after_use; ///< Used to indicate that we have allocated a value for this and need to delete it after having processed the attribute

	const uni_char*	value; ///< The string value of the attribute, can be NULL
	UINT			value_len; ///< Length of the value string

	const uni_char*	name;		///< Name of the attribute, only used if ATTR_XML
	UINT			name_len;	///< Length of string name, only used if ATTR_XML
};
#endif // HTMLATTRENTRY_H
