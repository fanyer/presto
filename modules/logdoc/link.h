/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2001 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef LINK_H
#define LINK_H

#include "modules/logdoc/link_type.h"
#include "modules/util/simset.h"
#include "modules/logdoc/elementref.h"
#include "modules/hardcore/mem/mem_man.h"

class URL;
class HTML_Element;
class LogicalDocument;
class LinkElementDatabase;

/**
 * Class that represents a <link> element in the source. We keep them
 * in a list to make them easy to access without traversing the document
 * tree.
 */
class LinkElement : public ListElement<LinkElement>
{
	OP_ALLOC_ACCOUNTED_POOLING
	OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT

public:
	/**
	 * Bitset of LinkType bits with a parsed version of the rel attribute in the
	 * link.  If LINK_OTHER is included in the bit set then the rel attribute
	 * contained unknown types and you have to do custom parsing of the rel
	 * attribute.
	 */
	unsigned int	GetKinds()     const { return m_kinds; }

	/** The rel attribute, but really, use GetKinds instead. */
	const uni_char*	GetRel()       const;
	/** The charset attribute. */
	const uni_char*	GetCharset()   const;
	/** The lang attribute. */
	const uni_char*	GetHrefLang()  const;
	/** The type attribute. The meaning of this depends on the kind of link. */
	const uni_char*	GetType()      const;
	/** Shortcut to CSS_MediaObject::GetMediaTypes. Only meaningful on LINK_STYLESHEET. */
			  short	GetMedia()     const;
	/** The title attribute.*/
	const uni_char*	GetTitle()     const;
	/** The target attribute.*/
	const uni_char*	GetTarget()    const;
	/** Parsed version of the href attribute or NULL if no such attribute. */
	URL*			GetHRef(LogicalDocument *logdoc)      const;

	/** Checks if the link element is the element supplied. */
	BOOL			IsElm(HTML_Element* elm) const { return elm == m_elm.GetElm(); }
	/** Checks if the link element represents a stylesheet (excluding alternate stylesheets without a title). */
	BOOL			IsStylesheet() const;

	/**
	 * Returns the string representation for a LinkType excepts if the kind is LINK_OTHER in which case
	 * it returns the contents of the rel attribute to allow the caller to do his own parsing.
	 * Should typically not be needed. If there is a meaningful type we've missed, we should add
	 * it to the parser instead.
	 */
	const uni_char* GetStringForKind(LinkType kind) const;

	/** The <link> element. */
	HTML_Element*	GetElement() const { return m_elm.GetElm(); }

	/** Constructor. */
	LinkElement(HTML_Element* elm);
	/** Constructor. */
	LinkElement(const uni_char* rel, HTML_Element* elm);

	/** Called to set the link database this element is associated with */
	void			SetDb(LinkElementDatabase *db) { m_elm.SetDb(db); }

	/** The LinkElements form a list. Use this method to get the next in the list. */
	const LinkElement* Suc() const { return ListElement<LinkElement>::Suc(); }
	LinkElement* Suc() { return ListElement<LinkElement>::Suc(); }

	/** Create a LinkElement object based on an HTML_Element. */
	static OP_STATUS
					CollectLink(HTML_Element* he, LinkElement** target);
	/**
	 * Returns a bit field of link types.
	 */
	static unsigned int MatchKind(const uni_char* rel);

private:

	class LinkElementElmRef : public ElementRef
	{
	public:
		LinkElementElmRef(HTML_Element *elm, LinkElement *link_elm) : ElementRef(elm), m_link_elm(link_elm), m_db(NULL) {}

		void	SetDb(LinkElementDatabase *db) { m_db = db; }

		virtual	void	OnDelete(FramesDocument *document);
		virtual	void	OnRemove(FramesDocument *document) { OnDelete(document); }

	private:
		LinkElement*			m_link_elm;
		LinkElementDatabase*	m_db;
	};

	LinkElementElmRef	m_elm;
	unsigned int		m_kinds;

	friend class LinkElementDatabase;
};

class LinkElementDatabase
{
public:
#ifdef LINK_SUPPORT
	/**
	 * Constructor.
	 */
	LinkElementDatabase() { ResetCache(); }

	/**
	 * Instead of a bitfield per link, this will return every link type for a link
	 * as a separate link. It will count LINK_TYPE_STYLESHEET | LINK_TYPE_ALTERNATE
	 * as one so the calling code must also do so.
	 * This function doesn't support quick random access but will be quick if
	 * called in a loop with ever increasing numbers.
	 *
	 * @returns NULL if nothing found. Else sets sub_index to the relevant
	 * number, starting with zero.
	 *
	 * @param[in] index The index to look for.
	 *
	 * @param[out] sub_index The index of the kind bit in the LinkElement. See note about alternate stylesheets.
	 */
	const LinkElement* GetFromSubLinkIndex(unsigned index, unsigned& sub_index) const;

	/**
	 * Returns how many legal values for index there are in GetFromSubLinkIndex.
	 */
	unsigned GetSubLinkCount() const;
#endif // LINK_SUPPORT

	/** Gets the first LinkElement in the list or NULL. Use Suc() on the element to get the next one. */
	LinkElement* First() { return m_link_list.First(); }
	/** Gets the first LinkElement in the list or NULL. Use Suc() on the element to get the next one. */
	const LinkElement* First() const { return m_link_list.First(); }
	/** Empties the list. */
	void Clear() { m_link_list.Clear(); ResetCache(); }
	/** Add a new element to the end of the list. */
	void AddToList(LinkElement* link) { link->Into(&m_link_list); link->SetDb(this); }
	/** Removes an element from the list. Owned by the caller when this returns. */
	void RemoveFromList(LinkElement* link) { ResetCache(); link->Out(); }

private:

#ifdef LINK_SUPPORT
	void ResetCache() const { m_cached_elm_offset = UINT_MAX; }
	void SetCache(const LinkElement* elm, unsigned offset) const { m_cached_elm = elm; m_cached_elm_offset = offset; }
#else
	void ResetCache() const {}
#endif // LINK_SUPPORT

	List<LinkElement> m_link_list;
#ifdef LINK_SUPPORT
	mutable const LinkElement* m_cached_elm;
	/** UINT_MAX if cache is not used. */
	mutable unsigned m_cached_elm_offset;
#endif // LINK_SUPPORT
};

#endif // LINK_H
