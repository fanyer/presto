/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#ifndef SVG_TEXT_ELEMENT_STATE_CONTEXT
#define SVG_TEXT_ELEMENT_STATE_CONTEXT

#ifdef SVG_SUPPORT

#include "modules/unicode/unicode_bidiutils.h"
#include "modules/util/simset.h"

#include "modules/svg/src/SVGElementStateContext.h"
#include "modules/svg/src/SVGGlyphCache.h"

class BidiCalculation;

#ifndef NEEDS_RISC_ALIGNMENT ///< Packing causes memory corruption.
# pragma pack(1)
#endif

struct SVGTextFragment {
	union {
		struct {
			unsigned int wordinfo : 16;		///< The index to the WordInfo for this fragment
			unsigned int next : 16;			///< The index of the next fragment in left-to-right order
		} idx;
		unsigned int idx_init;
	};
	union {
		struct {
			unsigned char bidi : 4;		///< The bidi-direction
			unsigned char level : 4;	///< The bidi-level
		} packed;
		unsigned char packed_init;
	};
};

#ifndef NEEDS_RISC_ALIGNMENT
# pragma pack()
#endif

// Potentially make this (explicitely) single linked
class SVGTextCache : public ListElement<SVGTextCache>
{
private:
	HTML_Element*		text_content_element;
	WordInfo*			word_info_array;
	SVGTextFragment*	text_fragment_list;

	union
	{
		struct
		{
			/** Number of words (and size of word_info_array) */
			unsigned word_count:16;

			/** Word info needs to be recalculated. */
			unsigned remove_word_info:1;

			/** One or more fragments are RTL */
			unsigned has_rtl_frag:1;

			/** Space handling (preserve) */
			unsigned preserve_spaces:1;

			/** Keeps a leading whitespace */
			unsigned keep_leading_ws:1;

			/** Keeps a trailing whitespace */
			unsigned keep_trailing_ws:1;

		} packed; /* 21 bits */
		unsigned int packed_init;
	};

public:
	SVGTextCache() :
		text_content_element(NULL),
		word_info_array(NULL),
		text_fragment_list(NULL),
		packed_init(0) {}
	~SVGTextCache() { Out(); DeleteWordInfoArray(); }

	/** Sets the HTML_Element for which this text cache corresponds */
	void			SetTextContentElement(HTML_Element* text_cont_elm)
	{
		text_content_element = text_cont_elm;
	}

	/** Set whether spaces should be preserved */
	void			SetPreserveSpaces(BOOL preserve_spaces)
	{
		packed.preserve_spaces = preserve_spaces ? 1 : 0;
	}

	/** Set whether a leading whitespace should be preserved */
	void			SetKeepLeadingWS(BOOL keep_leading)
	{
		packed.keep_leading_ws = keep_leading ? 1 : 0;
	}

	/** Set whether a trailing whitespace should be preserved */
	void			SetKeepTrailingWS(BOOL keep_trailing)
	{
		packed.keep_trailing_ws = keep_trailing ? 1 : 0;
	}

	/** Indicate that this text cache has RTL fragments */
	void			SetHasRTLFragments(BOOL has_rtl)
	{
		packed.has_rtl_frag = has_rtl ? 1 : 0;
	}

	/** Allocate word_info_array, and update word_count. Precondition: empty array. */
	BOOL			AllocateWordInfoArray(unsigned int new_word_count);

	/** Delete word_info_array, and update word_count. */
	void			DeleteWordInfoArray();

	/** Mark this text cache as valid (having valid representation) */
	void			SetIsValid() { packed.remove_word_info = 0; }
	BOOL			IsValid() const { return packed.remove_word_info == 0 && word_info_array; }

	/** Remove cached info */
	BOOL			RemoveCachedInfo();

	void			AdoptTextFragmentList(SVGTextFragment* tflist)
	{
		if (text_fragment_list)
			OP_DELETEA(text_fragment_list);
		text_fragment_list = tflist;
	}

	/** Returns if any fragment is RTL */
	BOOL			HasRTLFragments() const { return packed.has_rtl_frag; }

	/** Returns number of words in box. */
	unsigned int	GetWordCount() const { return packed.word_count; }

	/** Returns word array. */
	WordInfo*		GetWords() const { return word_info_array; }

	/** Returns the textfragment list, only used for complex text */
	SVGTextFragment* GetTextFragments() { return text_fragment_list; }

	/** Returns the HTML_Element for which this text cache corresponds */
	HTML_Element*	GetTextContentElement() const { return text_content_element; }

	/** Returns if spaces should be preserved */
	BOOL			GetPreserveSpaces() const { return packed.preserve_spaces; }

	/** Returns if there should be leading whitespace */
	BOOL			GetKeepLeadingWS() const { return packed.keep_leading_ws; }

	/** Returns if there should be trailing whitespace */
	BOOL			GetKeepTrailingWS() const { return packed.keep_trailing_ws; }
};

/** 'Virtual' context for text nodes */
class SVGTextNode : public SVGElementContext
{
private:
	SVGTextCache	m_textcache;
public:
	SVGTextNode(HTML_Element* element) : SVGElementContext(element) {}

	virtual void AddInvalidState(SVGElementInvalidState state);

	virtual OP_STATUS Enter(SVGTraversalObject* traversal_object, SVGElementInfo& info);
	virtual OP_STATUS HandleContent(SVGTraversalObject* traversal_object, SVGElementInfo& info);
	virtual OP_STATUS Leave(SVGTraversalObject* traversal_object, SVGElementInfo& info);

	virtual SVGTextCache* GetTextCache() { return &m_textcache; }
};

class SVGTBreakElement : public SVGElementContext
{
public:
	SVGTBreakElement(HTML_Element* element) : SVGElementContext(element) {}

	virtual OP_STATUS HandleContent(SVGTraversalObject* traversal_object, SVGElementInfo& info);
};

/** Base class for _any_ text element that can have children */
class SVGTextContainer : public SVGContainer
{
public:
	SVGTextContainer(HTML_Element* element) : SVGContainer(element) {}

	virtual OP_STATUS Enter(SVGTraversalObject* traversal_object, SVGElementInfo& info);
	virtual OP_STATUS HandleContent(SVGTraversalObject* traversal_object, SVGElementInfo& info);
	virtual OP_STATUS Leave(SVGTraversalObject* traversal_object, SVGElementInfo& info);

	virtual BOOL EvaluateChild(HTML_Element* child);

	SVGNumber GetTextExtent() { return m_text_extent; }
	void SetTextExtent(SVGNumber text_extent) { m_text_extent = text_extent; }

	void SetGlyphScale(SVGNumber new_glyph_scale) { m_glyph_scale = new_glyph_scale; }
	SVGNumber GetGlyphScale() { return m_glyph_scale; }

	void SetExtraSpacing(SVGNumber new_extra_spacing) { m_extra_spacing = new_extra_spacing; }
	SVGNumber GetExtraSpacing() { return m_extra_spacing; }

	void ResetCache() { m_text_extent = 0; m_extra_spacing = 0; m_glyph_scale = 1; }

private:
	SVGNumber m_text_extent;
	SVGNumber m_glyph_scale;
	SVGNumber m_extra_spacing;
};

class SVGAltGlyphElement : public SVGTextContainer
{
public:
	SVGAltGlyphElement(HTML_Element* element) : SVGTextContainer(element) {}

	virtual BOOL EvaluateChild(HTML_Element* child);

	OpVector<GlyphInfo>& GetGlyphVector() { return m_glyphs; }

private:
	OpAutoVector<GlyphInfo> m_glyphs;
};

class SVGTRefElement : public SVGTextContainer
{
private:
	SVGTextCache	m_textcache;
public:
	SVGTRefElement(HTML_Element* element) : SVGTextContainer(element) {}

	virtual void AddInvalidState(SVGElementInvalidState state);

	virtual BOOL EvaluateChild(HTML_Element* child) { return FALSE; }
	virtual SVGTextCache* GetTextCache() { return &m_textcache; }
};

/** Base class for elements with a 'transparent' content model - <a>
 *	and <switch>.
 *  Inherits from SVGTextContainer because both these elements are
 *	allowed in text subtrees, and thus need to be able to handle
 *	textnodes. */
class SVGTransparentContainer : public SVGTextContainer
{
public:
	SVGTransparentContainer(HTML_Element* element) : SVGTextContainer(element) {}

	virtual BOOL EvaluateChild(HTML_Element* child);
};

class SVGAElement : public SVGTransparentContainer
{
public:
	SVGAElement(HTML_Element* element) : SVGTransparentContainer(element) {}

	virtual OP_STATUS Enter(SVGTraversalObject* traversal_object, SVGElementInfo& info);
	virtual OP_STATUS HandleContent(SVGTraversalObject* traversal_object, SVGElementInfo& info);
	virtual OP_STATUS Leave(SVGTraversalObject* traversal_object, SVGElementInfo& info);
};

class SVGTextGroupElement : public SVGTransparentContainer
{
public:
	SVGTextGroupElement(HTML_Element* element) : SVGTransparentContainer(element) {}

	virtual OP_STATUS Enter(SVGTraversalObject* traversal_object, SVGElementInfo& info);
	virtual OP_STATUS HandleContent(SVGTraversalObject* traversal_object, SVGElementInfo& info) { return OpStatus::OK; }
	virtual OP_STATUS Leave(SVGTraversalObject* traversal_object, SVGElementInfo& info);
};

class SVGSwitchElement : public SVGTransparentContainer
{
public:
	SVGSwitchElement(HTML_Element* element) : SVGTransparentContainer(element) {}

	virtual OP_STATUS Enter(SVGTraversalObject* traversal_object, SVGElementInfo& info);
	virtual OP_STATUS HandleContent(SVGTraversalObject* traversal_object, SVGElementInfo& info);
	virtual OP_STATUS Leave(SVGTraversalObject* traversal_object, SVGElementInfo& info);

	virtual void AddInvalidState(SVGElementInvalidState state);

	virtual BOOL EvaluateChild(HTML_Element* child);
	virtual void AppendChild(HTML_Element* child, List<SVGElementContext>* childlist);
};

class SVGEditable;

/** Common base for <text> and <textArea> - currently only to carry
 *	state for 'editable', so maybe this entire class should be
 *	ifdeffed? */
class SVGTextRootContainer : public SVGTextContainer
{
public:
	SVGTextRootContainer(HTML_Element* element) :
		SVGTextContainer(element)
#ifdef SVG_SUPPORT_EDITABLE
		, m_edit_context(NULL)
#endif // SVG_SUPPORT_EDITABLE
#ifdef SUPPORT_TEXT_DIRECTION
		, m_bidi_calculation(NULL)
#endif // SUPPORT_TEXT_DIRECTION
	{}
#if defined(SVG_SUPPORT_EDITABLE) || defined(SUPPORT_TEXT_DIRECTION)
	virtual ~SVGTextRootContainer();
#endif // SVG_SUPPORT_EDITABLE || SUPPORT_TEXT_DIRECTION

#ifdef SVG_SUPPORT_EDITABLE
	SVGEditable* GetEditable(BOOL construct = TRUE);
	BOOL IsEditing();
	BOOL IsCaretVisible();
#endif // SVG_SUPPORT_EDITABLE

#ifdef SUPPORT_TEXT_DIRECTION
# ifdef _DEBUG
	void PrintDebugInfo();
# endif // _DEBUG

	void ResetBidi();

	/** Initialize bidi calculation. */
	BOOL InitBidiCalculation(const HTMLayoutProperties* props);

	/** Compute bidi segments for this line. */
	OP_STATUS ResolveBidiSegments(WordInfo* wordinfo_array, int wordinfo_array_len, SVGTextFragment*& tflist);

	/** Push new inline bidi properties onto stack.
	 *
	 * @return FALSE om OOM
	 */
	BOOL			PushBidiProperties(const HTMLayoutProperties* props);

	/** Pop inline bidi properties from stack.
	 *
	 * @return FALSE on OOM
	 */
	BOOL			PopBidiProperties();

	/** Get bidi segments */
	const Head&		GetBidiSegments() const { return m_bidi_segments; }

	// ED: consider protecting this
	BidiCalculation*	GetBidiCalculation() { return m_bidi_calculation; }
#endif

	virtual SVGTextRootContainer* GetAsTextRootContainer() { return this; }

private:
#ifdef SVG_SUPPORT_EDITABLE
	SVGEditable* 		m_edit_context;
#endif // SVG_SUPPORT_EDITABLE
#ifdef SUPPORT_TEXT_DIRECTION
	Head 				m_bidi_segments;
	BidiCalculation* 	m_bidi_calculation;
#endif // SUPPORT_TEXT_DIRECTION
};

#include "modules/svg/src/SVGTextLayout.h" // For SVGLineInfo and SVGTextChunk

/** */
class SVGTextElement : public SVGTextRootContainer
{
public:
	SVGTextElement(HTML_Element* element) : SVGTextRootContainer(element) {}
	virtual ~SVGTextElement() { m_chunk_list.DeleteAll(); }

	virtual OP_STATUS Enter(SVGTraversalObject* traversal_object, SVGElementInfo& info);
	virtual OP_STATUS Leave(SVGTraversalObject* traversal_object, SVGElementInfo& info);

	virtual BOOL EvaluateChild(HTML_Element* child);

	OpVector<SVGTextChunk>& GetChunkList() { return m_chunk_list; }

private:
	OpVector<SVGTextChunk> m_chunk_list;
};

/** */
class SVGTextAreaElement : public SVGTextRootContainer
{
public:
	SVGTextAreaElement(HTML_Element* element) : SVGTextRootContainer(element) {}
	virtual ~SVGTextAreaElement() { ClearLineInfo(); }

	virtual OP_STATUS Enter(SVGTraversalObject* traversal_object, SVGElementInfo& info);
	virtual OP_STATUS Leave(SVGTraversalObject* traversal_object, SVGElementInfo& info);

	virtual BOOL EvaluateChild(HTML_Element* child);

	void ClearLineInfo() { m_lineinfo.DeleteAll(); }
	OpVector<SVGLineInfo>* GetLineInfo() { return &m_lineinfo; }

private:
	OpVector<SVGLineInfo> m_lineinfo;
};

#endif // SVG_SUPPORT
#endif // SVG_TEXT_ELEMENT_STATE_CONTEXT
