/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef HTML5TOKENWRAPPER_H
#define HTML5TOKENWRAPPER_H

#include "modules/logdoc/src/html5/html5token.h"
#include "modules/logdoc/markup.h"

class HTML5AttrCopy;
class HtmlAttrEntry;
class LogicalDocument;
#ifndef HTML5_STANDALONE
class LogdocXmlName;
#endif // !HTML5_STANDALONE

class HTML5TokenWrapper : public HTML5Token
{
public:
	HTML5TokenWrapper()
		: m_elm_type(Markup::HTE_UNKNOWN)
		, m_ns(Markup::HTML)
		, m_skipped_spaces(0)
		, m_attr_copies(NULL)
		, m_attr_copies_length(0)
#ifndef HTML5_STANDALONE
		, m_attr_entries(NULL)
		, m_attr_entries_length(0)
		, m_tag_name(NULL)
#endif // HTML5_STANDALONE
	{}
	virtual ~HTML5TokenWrapper();

	void		CopyL(HTML5TokenWrapper &src_token);

	void		ResetWrapper();

	Markup::Type
				GetElementType() const { return m_elm_type; }
	void		SetElementType(Markup::Type new_type) { m_elm_type = new_type; }

	Markup::Ns	GetElementNs() const { return m_ns; }
	void		SetElementNs(Markup::Ns ns) { m_ns = ns; }

	BOOL		HasAttribute(const uni_char *name, unsigned &attr_idx);
	BOOL		AttributeHasValue(unsigned attr_idx, const uni_char *value);
	void		SetAttributeValueL(unsigned attr_idx, const uni_char *value);
	virtual unsigned	GetAttrCount() const;
	BOOL		GetAttributeL(unsigned index, HTML5TokenBuffer *&name, uni_char *&value, unsigned &value_len);
	BOOL		GetAttributeValueL(const uni_char *name, uni_char *&value, unsigned &value_len, unsigned* attr_idx = NULL);

#ifndef HTML5_STANDALONE
	/**
	 *  @param attribute  the attribute to get value for
	 *  @param attr_idx   If non-NULL will get the index of the attribute (only valid if the return value is non-NULL)
	 *  @return the attribute as an allocated uni_char string, or NULL if the token doesn't have the attribute.  The returned string must be deleted by caller
	 */
	uni_char*	GetUnescapedAttributeValueL(const uni_char* attribute, unsigned* attr_idx = NULL);

	/**
	 * Get the attributes as an array of HtmlAttrEntrys
	 * @param[out] token_attr_count The number of attributes in the document.
	 * @param[out] extra_attr_count Total number of attributes, including special ones
	 * @returns NULL with a token_attr_count > 0 on OOM, otherwise an array of
	 *  entries extra_attr_count long.
	 */
	HtmlAttrEntry*	GetOrCreateAttrEntries(unsigned &token_attr_count, unsigned &extra_attr_count, LogicalDocument *logdoc);

	/// Must be called when finished with the array gotten by GetOrCreateAttrEntries()
	void		DeleteAllocatedAttrEntries();
#endif // HTML5_STANDALONE
	BOOL		HasLocalAttrCopies() { return m_attr_copies != NULL; }
	HTML5AttrCopy*
				GetLocalAttrCopy(unsigned index);
	BOOL		RemoveAttributeL(const uni_char *name);

	unsigned	SkippedSpaces() { return m_skipped_spaces; }
	void		SkipSpaces(unsigned skip) { m_skipped_spaces = skip; }

	BOOL		IsEqual(HTML5TokenWrapper &token);

	/**
	 * The max number of attributes from a token that will be
	 * stored in the element when it is created.
	 */
	static const unsigned	kMaxAttributeCount = 246;

private:
	void		CreateAttributesCopyL(const HTML5TokenWrapper &src_token);

	Markup::Type		m_elm_type;
	Markup::Ns			m_ns;
	unsigned			m_skipped_spaces;
	HTML5AttrCopy*		m_attr_copies;
	unsigned			m_attr_copies_length;
#ifndef HTML5_STANDALONE
	HtmlAttrEntry*		m_attr_entries;
	unsigned			m_attr_entries_length;
	LogdocXmlName*		m_tag_name; // for unknown tags
#endif // HTML5_STANDALONE
};

#endif // HTML5TOKENWRAPPER_H
