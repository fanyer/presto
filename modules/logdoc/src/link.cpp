/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

// http://www.euronet.nl/~tekelenb/WWW/LINK/
// http://www.w3.org/TR/html401/struct/links.html#h-12.3
// http://www.subotnik.net/html/link
// http://gutfeldt.ch/matthias/translation/LINK/ENaddendum.html
// https://bugs.opera.com/show_bug.cgi?id=78765
// http://www.alistapart.com/stories/alternate/
// http://www.htmlhelp.com/reference/html40/head/link.html

// complete list of rel-values
// http://fantasai.tripod.com/qref/Appendix/LinkTypes/alphindex.html

// FIXME-EE: handle multiple link types within "rel"! this also applies to stuff in htm_ldoc.cpp.

#include "modules/logdoc/link.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/hardcore/unicode/unicode.h"

BOOL
LinkElement::IsStylesheet() const
{
		// note: alternate stylesheets without title are ignored
	return (m_kinds & LINK_TYPE_STYLESHEET) &&
		(!(m_kinds & LINK_TYPE_ALTERNATE) || GetTitle() != NULL);
}

const uni_char*
LinkElement::GetRel() const
{
	return m_elm->GetLinkRel();
}

URL*
LinkElement::GetHRef(LogicalDocument *logdoc/*=NULL*/) const
{
	return m_elm.GetElm()->GetUrlAttr(ATTR_HREF, NS_IDX_HTML, logdoc);
}

const uni_char*
LinkElement::GetCharset() const
{
	return m_elm->GetStringAttr(ATTR_CHARSET);
}

const uni_char*
LinkElement::GetHrefLang() const
{
	return m_elm->GetStringAttr(ATTR_LANG);
}

const uni_char*
LinkElement::GetType() const
{
	return m_elm->GetStringAttr(ATTR_TYPE);
}

short
LinkElement::GetMedia() const
{
	CSS_MediaObject* media_obj = m_elm->GetLinkStyleMediaObject();
	return media_obj ? media_obj->GetMediaTypes() : 0;
}

const uni_char*
LinkElement::GetTitle() const
{
	const uni_char* title = m_elm->GetStringAttr(ATTR_TITLE);
	if (title && !*title)
		title = NULL;
	return title;
}

const uni_char*
LinkElement::GetTarget() const
{
	return m_elm->GetStringAttr(ATTR_TARGET);
}

LinkElement::LinkElement(HTML_Element* elm) : m_elm(elm, this), m_kinds(MatchKind(elm->GetLinkRel()))
{
}

LinkElement::LinkElement(const uni_char* rel, HTML_Element* elm) : m_elm(elm, this), m_kinds(MatchKind(rel))
{
}

#ifdef HAS_COMPLEX_GLOBALS
# define LINK_MAP_START()	const LinkMapEntry g_LinkTypeMap[] = {
# define LINK_MAP_ENTRY(name_str, type) LinkMapEntry(UNI_L(name_str), type),
# define LINK_MAP_END()		};
#else // HAS_COMPLEX_GLOBALS
# define LINK_MAP_START()	void init_LinkTypeMapL() {\
							int i = 0; LinkMapEntry *tmp_map\
								= g_opera->logdoc_module.m_link_type_map;
# define LINK_MAP_ENTRY(name_str, type) tmp_map[i].name = UNI_L(name_str);\
									tmp_map[i++].kind = type;
# define LINK_MAP_END()		};
#endif // HAS_COMPLEX_GLOBALS

/*
 * Don't add or remove anything from this table without updating the size
 * of the array used in <logdoc>/logdoc_module.h
 * vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
 */
LINK_MAP_START()
	LINK_MAP_ENTRY("STYLESHEET",           LINK_TYPE_STYLESHEET           )
	LINK_MAP_ENTRY("ALTERNATE",            LINK_TYPE_ALTERNATE            )
#ifdef LINK_SUPPORT
	LINK_MAP_ENTRY("NEXT",                 LINK_TYPE_NEXT                 )
	LINK_MAP_ENTRY("PREV",                 LINK_TYPE_PREV                 )
	LINK_MAP_ENTRY("PREVIOUS",             LINK_TYPE_PREV                 )
	LINK_MAP_ENTRY("ICON",                 LINK_TYPE_ICON                 )
	LINK_MAP_ENTRY("START",                LINK_TYPE_START                )
	LINK_MAP_ENTRY("HOME",                 LINK_TYPE_START                )
	LINK_MAP_ENTRY("TOP",                  LINK_TYPE_START                )
	LINK_MAP_ENTRY("CONTENTS",             LINK_TYPE_CONTENTS             )
	LINK_MAP_ENTRY("TOC",                  LINK_TYPE_CONTENTS             )
	LINK_MAP_ENTRY("BEGIN",                LINK_TYPE_FIRST                )
	LINK_MAP_ENTRY("FIRST",                LINK_TYPE_FIRST                )
	LINK_MAP_ENTRY("END",                  LINK_TYPE_END                  )
	LINK_MAP_ENTRY("LAST",                 LINK_TYPE_LAST                 )
	LINK_MAP_ENTRY("UP",                   LINK_TYPE_UP                   )
	LINK_MAP_ENTRY("INDEX",                LINK_TYPE_INDEX                )
	LINK_MAP_ENTRY("FIND",                 LINK_TYPE_FIND                 )
	LINK_MAP_ENTRY("SEARCH",               LINK_TYPE_FIND                 )
	LINK_MAP_ENTRY("GLOSSARY",             LINK_TYPE_GLOSSARY             )
	LINK_MAP_ENTRY("COPYRIGHT",            LINK_TYPE_COPYRIGHT            )
	LINK_MAP_ENTRY("CHAPTER",              LINK_TYPE_CHAPTER              )
	LINK_MAP_ENTRY("SECTION",              LINK_TYPE_SECTION              )
	LINK_MAP_ENTRY("SUBSECTION",           LINK_TYPE_SUBSECTION           )
	LINK_MAP_ENTRY("APPENDIX",             LINK_TYPE_APPENDIX             )
	LINK_MAP_ENTRY("HELP",                 LINK_TYPE_HELP                 )
	LINK_MAP_ENTRY("AUTHOR",               LINK_TYPE_AUTHOR               )
	LINK_MAP_ENTRY("MADE",                 LINK_TYPE_MADE                 )
	LINK_MAP_ENTRY("BOOKMARK",             LINK_TYPE_BOOKMARK             )
	LINK_MAP_ENTRY("ARCHIVES",             LINK_TYPE_ARCHIVES             )
	LINK_MAP_ENTRY("NOFOLLOW",             LINK_TYPE_NOFOLLOW             )
	LINK_MAP_ENTRY("NOREFERRER",           LINK_TYPE_NOREFERRER           )
	LINK_MAP_ENTRY("PINGBACK",             LINK_TYPE_PINGBACK             )
	LINK_MAP_ENTRY("PREFETCH",             LINK_TYPE_PREFETCH             )
	LINK_MAP_ENTRY("APPLE-TOUCH-ICON",     LINK_TYPE_APPLE_TOUCH_ICON     )
	LINK_MAP_ENTRY("IMAGE_SRC",            LINK_TYPE_IMAGE_SRC            )
	LINK_MAP_ENTRY("APPLE-TOUCH-ICON-PRECOMPOSED", LINK_TYPE_APPLE_TOUCH_ICON_PRECOMPOSED )
#endif // LINK_SUPPORT
LINK_MAP_END();
/* ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
 * Don't add or remove anything from this table without updating the size
 * of the array used in <logdoc>/logdoc_module.h
 */

// static
unsigned int
LinkElement::MatchKind(const uni_char* rel)
{
	OP_ASSERT(rel);
	const size_t map_size = sizeof(g_LinkTypeMap) / sizeof(*(g_LinkTypeMap));
	OP_ASSERT(map_size == LINK_TYPE_MAP_SIZE); // see comment above

	uni_char* rel_str = (uni_char*)g_memory_manager->GetTempBuf2();
	int copy_len = MIN(uni_strlen(rel), UNICODE_DOWNSIZE(g_memory_manager->GetTempBuf2Len())-1);
	uni_strncpy(rel_str, rel, copy_len);
	rel_str[copy_len] = 0;
	uni_char* token = uni_strtok(rel_str, UNI_L(" "));

	int result = 0;
	while (token)
	{
		size_t i;
		for (i = 0; i < map_size; i++)
		{
			const uni_char* name_candidate = g_LinkTypeMap[i].name;
			if (uni_stri_eq(token, name_candidate))
			{
				result |= g_LinkTypeMap[i].kind;
				break;
			}
		}
		if (i == map_size)
			result |= LINK_TYPE_OTHER;
		token = uni_strtok(NULL, UNI_L(" "));
	}

	return result;
}

// static
OP_STATUS LinkElement::CollectLink(HTML_Element* he, LinkElement** target)
{
	HTML_ElementType type = he->Type();
	const uni_char* rel = NULL;

	switch (type)
	{
		// <LINK REL="Next" HREF="page2.html">
		// <A REL="Next" HREF="page2.html" TITLE="Next page">
		case HE_LINK:
		case HE_A:
			if (he->HasAttr(ATTR_REL) && he->HasAttr(ATTR_HREF))
			{
				*target = OP_NEW(LinkElement, (he));
				if (*target == NULL)
					return OpStatus::ERR_NO_MEMORY;
			}
			return OpStatus::OK;

		case HE_PROCINST:
			// This function only gets called for processing
			// instructions with the target xml-stylesheet, so we need
			// not check the target for HE_PROCINST.
			*target = OP_NEW(LinkElement, (he->HasSpecialAttr(ATTR_ALTERNATE, SpecialNs::NS_LOGDOC) ? UNI_L("alternate stylesheet") : UNI_L("stylesheet"), he));
			if (*target == NULL)
				return OpStatus::ERR_NO_MEMORY;
			return OpStatus::OK;

		case HE_TEXT:
			rel = he->Content();
			break;

		case HE_IMG:
			rel = he->GetStringAttr(ATTR_ALT);
			break;

		default:
			return OpStatus::OK;
	}

	// try to find links extracted from TEXT- and IMG-content
	// by searching for the first parent element with a href.
	//
	// this will catch cases like this:
	//
	// 1) <a href="next.html">
	//      <img src="next.png" alt="Next">
	//    </a>
	//
	// 2) <a href="next.html">
	//      Next
	//    </a>
	//
	// (but not)
	//
	// 3) <a href="next.html">
	//      Next -->
	//    </a>
	//

	if (rel != NULL)
	{
		for (HTML_Element* candidate = he;
		     candidate != NULL && candidate->Type() != HE_A;
			 candidate = candidate->Parent())
		{
			if (candidate->HasAttr(ATTR_HREF))
			{
				// If we have rel, do not create a fake rel!
				if (candidate->HasAttr(ATTR_REL))
				{
					break;
				}

				*target = OP_NEW(LinkElement, (rel, candidate));
				if (*target == NULL)
					return OpStatus::ERR_NO_MEMORY;
			}
		}
	}

	return OpStatus::OK;
}

const uni_char*
LinkElement::GetStringForKind(LinkType kind) const
{
	// See method documentation for why we do this.
	if (kind == LINK_TYPE_OTHER)
		return m_elm->GetStringAttr(ATTR_REL);

	const size_t map_size = sizeof(g_LinkTypeMap) / sizeof(*(g_LinkTypeMap));
	for (size_t i = 0; i < map_size; i++)
	{
		if (g_LinkTypeMap[i].kind == kind)
			return g_LinkTypeMap[i].name;
	}

	return NULL;
}

/*virtual*/ void LinkElement::LinkElementElmRef::OnDelete(FramesDocument *document)
{
	m_db->RemoveFromList(m_link_elm);
	OP_DELETE(m_link_elm);
}

#ifdef LINK_SUPPORT
// You are not meant to understand this. It's coming from
// http://stackoverflow.com/questions/109023/best-algorithm-to-count-the-number-of-set-bits-in-a-32-bit-integer
// http://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel
// http://en.wikipedia.org/wiki/Hamming_weight
static unsigned CountBits(UINT32 i)
{
	i = i - ((i >> 1) & 0x55555555);
	i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
	return ((i + (i >> 4) & 0xF0F0F0F) * 0x1010101) >> 24;
}

static unsigned CountDistinctLinkTypes(unsigned kinds)
{
	OP_ASSERT(CountBits(1) == 1);
	OP_ASSERT(CountBits(2) == 1);
	OP_ASSERT(CountBits(3) == 2);
	OP_ASSERT(CountBits(static_cast<UINT32>(-1)) == 32);
	OP_ASSERT(CountBits(static_cast<UINT32>(-2)) == 31);
	int link_count = CountBits(static_cast<UINT32>(kinds));
	if ((kinds & LINK_GROUP_ALT_STYLESHEET) == LINK_GROUP_ALT_STYLESHEET) // Count as one, not as two
		link_count--;
	return link_count;
}

unsigned LinkElementDatabase::GetSubLinkCount() const
{
	const LinkElement* link = First();

	UINT32 count = 0;

	while (link != NULL)
	{
		count += CountDistinctLinkTypes(link->GetKinds());
		link = link->Suc();
	}
	return count;

}

const LinkElement*
LinkElementDatabase::GetFromSubLinkIndex(unsigned index, unsigned& sub_index) const
{
	unsigned remaining_count = index;
	const LinkElement* link = First();

	if (m_cached_elm_offset <= index)
	{
		remaining_count -= m_cached_elm_offset;
		link = m_cached_elm;
	}

	while (link != NULL)
	{
		unsigned link_count = CountDistinctLinkTypes(link->GetKinds());
		if (link_count > remaining_count)
		{
			sub_index = remaining_count;
			SetCache(link, index - remaining_count);
			break;
		}
		remaining_count -= link_count;
		link = link->Suc();
	}

	return link;
}
#endif // LINK_SUPPORT
