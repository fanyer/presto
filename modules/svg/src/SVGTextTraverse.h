/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#ifndef SVG_TEXTTRAVERSE_H
#define SVG_TEXTTRAVERSE_H

#if defined(SVG_SUPPORT)

#include "modules/svg/src/SVGTraverse.h"
#include "modules/svg/src/SVGMotionPath.h"
#include "modules/svg/src/SVGDocumentContext.h"

#ifdef SVG_SUPPORT_EDITABLE
# include "modules/svg/src/SVGEditable.h"
#endif // SVG_SUPPORT_EDITABLE

#include "modules/svg/src/svgpainter.h"
#include "modules/util/simset.h"
#include "modules/layout/cascade.h"

#ifdef SVG_SUPPORT_TEXTSELECTION
class SVGGlyphScreenBoxCache;
#endif // SVG_SUPPORT_TEXTSELECTION
#ifdef SVG_SUPPORT_EDITABLE
class SVGEditable;
#endif // SVG_SUPPORT_EDITABLE
#ifdef SVG_SUPPORT_TEXTSELECTION
class SelectionElm;
#endif // SVG_SUPPORT_TEXTSELECTION
class FontAtt;

struct TextRun;
class SVGTextCache;
struct SVGTextAreaInfo;
class OpBpath;
class SVGTextContentPaintNode;
class SVGTextAttributePaintNode;
class SVGTextRotate;

struct TextAttributes
{
	TextAttributes()
	{
		deco_paints[0].Reset();
		deco_paints[1].Reset();

		language = NULL;

		glyph_scale = 1;

		writing_mode = SVGWRITINGMODE_LR;

		weight = 4; /* normal */
		decorations = 0;
		italic = 0;
		smallcaps = 0;

		orientation = 0;
		orientation_is_auto = 0;

		geometric_precision = 0;
	}
	~TextAttributes();

	enum
	{
		DECO_UNDERLINE		= 1 << 0,
		DECO_OVERLINE		= 1 << 1,
		DECO_LINETHROUGH	= 1 << 2
	};

	void SetLanguage(const uni_char* lang);

	// Paints to use for text decorations
	SVGPaintDesc deco_paints[2];

	// A potential language identifier to use for glyph selection
	uni_char* language;

	// Font/text related attributes
	SVGNumber letter_spacing;
	SVGNumber word_spacing;
	SVGNumber extra_spacing;
	SVGNumber glyph_scale;
	SVGNumber baseline_shift;
	SVGNumber fontsize;
	COLORREF selection_bg_color;
	COLORREF selection_fg_color;
	unsigned writing_mode:3;
	unsigned weight:4;
	unsigned decorations:3;
	unsigned italic:1;
	unsigned smallcaps:1;
	unsigned orientation:2;
	unsigned orientation_is_auto:1;
	unsigned geometric_precision:1;
};

struct TextPathAttributes
{
	TextPathAttributes()
	{
		path = NULL;
		path_warp = FALSE;
	}
	~TextPathAttributes();

	void SetPath(OpBpath* newpath)
	{
		if (newpath != path)
		{
			SVGObject::IncRef(newpath);
			SVGObject::DecRef(path);
			path = newpath;
		}
	};

	OpBpath*			path;
	SVGMatrix 			path_transform;
	unsigned			path_warp:1;
};

struct GlyphRotationList
{
	GlyphRotationList() : angles(NULL), length(0) {}
	~GlyphRotationList() { OP_DELETEA(angles); }

	SVGNumber* angles;
	unsigned length;
};

struct FragmentPosition
{
	unsigned index;		///< Index in the fragment array
	unsigned offset;	///< Offset in the fragment
};

/**
 * A helper class, to fulfill rules about inheriting x,y,dx,dy-values when drawing text.
 * Each element can add it's own array of values, and they can be differing in length.
 * The spec says use the most specific one, or use parent value if any.
 */
class SVGVectorStack
{
private:
	OpVector<SVGVector> m_internal_vector;
	OpINT32Vector m_offsets;

public:
	const SVGObject* Get(UINT32 glyph_index, BOOL is_rotate = FALSE) const;
	
	OP_STATUS Push(SVGVector* vec, UINT32 offset);
	OP_STATUS Pop();
};

class SVGBBoxStack
{
private:
	OpVector<SVGBoundingBox> m_bbox_vec;

public:
	~SVGBBoxStack() { m_bbox_vec.DeleteAll(); }

	SVGBoundingBox Get()
	{
		SVGBoundingBox* bb = m_bbox_vec.Get(m_bbox_vec.GetCount() - 1);
		if (bb)
			return *bb;
		return SVGBoundingBox();
	}

	OP_STATUS PushNew()
	{
		SVGBoundingBox* new_bb = OP_NEW(SVGBoundingBox, ());
		if (!new_bb || OpStatus::IsError(m_bbox_vec.Add(new_bb)))
		{
			OP_DELETE(new_bb);
			return OpStatus::ERR_NO_MEMORY;
		}
		return OpStatus::OK;
	}

	void Add(const SVGBoundingBox& bbox)
	{
		SVGBoundingBox* bb = m_bbox_vec.Get(m_bbox_vec.GetCount() - 1);
		if (bb)
			bb->UnionWith(bbox);
	}

	void Add(const SVGRect& bbox_rect)
	{
		SVGBoundingBox* bb = m_bbox_vec.Get(m_bbox_vec.GetCount() - 1);
		if (bb)
			bb->UnionWith(bbox_rect);
	}

	void Pop()
	{
		SVGBoundingBox* bb = m_bbox_vec.Remove(m_bbox_vec.GetCount() - 1);
		if (bb)
			Add(*bb);
		OP_DELETE(bb);
	}
};

class SVGFontDescriptor
{
public:
	SVGFontDescriptor() : m_font(NULL) {}
	~SVGFontDescriptor() { Release(); }

	void SetFont(FontAtt& fa)
	{
		m_fontinfo = fa;
		if (m_fontinfo.GetHeight() == 0)
			m_fontinfo.SetHeight(1);
	}
	void SetFontNumber(int fontnr);
	void SetFontSize(SVGNumber size)
	{
		m_fontinfo.SetHeight(size.GetIntegerValue());
	}
	void SetFontWeight(int opfontweight)
	{
		m_fontinfo.SetWeight(opfontweight);
	}
	void SetScaleChanged(BOOL changed)
	{
		m_scale_might_have_changed = changed;
	}

	FontAtt& GetFontAttributes() { return m_fontinfo; }
	SVGNumber& GetFontToUserScale() { return m_font_to_user_scale; }
	OpFont* GetFont()
	{
		return m_font;
	}
	void Release();

	/**
	 * Checks if the current font is up-to-date, and sets the new current font if it isn't.
	 */
	OP_STATUS Check(SVGDocumentContext* font_doc_ctx, SVGNumber ctm_expansion, const SVGNumber exact_font_size);

	SVGNumber GetDescent() const { return m_font ? m_font_to_user_scale * m_font->Descent() : 0; }

private:
	OpFont*					m_font;
	FontAtt					m_fontinfo;
	SVGNumber				m_font_to_user_scale;
	BOOL					m_scale_might_have_changed;	///< Set to true when the CTM changes the scale
};

class SVGTextArguments
{
public:
	SVGTextArguments();
	~SVGTextArguments();

	OP_STATUS NewChunk(OpVector<SVGTextChunk>& chunk_list);
	void AddExtent(SVGNumber extent);

	const uni_char* GetCurrentLang() const
	{
		return lang_stack.GetCount() > 0 ? lang_stack.Get(lang_stack.GetCount() - 1) : NULL;
	}
	void PushLang(uni_char* lang) { OpStatus::Ignore(lang_stack.Add(lang)); }
	void PopLang() { lang_stack.Remove(lang_stack.GetCount() - 1); }

	void PushSpace(BOOL preserve)
	{
		OpStatus::Ignore(space_stack.Add((INT32)packed.preserve_spaces));
		packed.preserve_spaces = preserve ? 1 : 0;
	}
	void PopSpace()
	{
		unsigned space_stack_depth = space_stack.GetCount();
		if (space_stack_depth == 0)
			return;

		packed.preserve_spaces = space_stack.Remove(space_stack_depth - 1) ? 1 : 0;
	}

	void SetIsLayout(BOOL in_is_layout) { is_layout = in_is_layout; }
	void SetCurrentTextCache(SVGTextCache* text_cache) { current_text_cache = text_cache; }

	OP_STATUS PushAttributeNode(SVGTextAttributePaintNode* attr_paint_node);
	void PopAttributeNode();

	TextPathAttributes* GetCurrentTextPathAttributes();

	BOOL GlyphLimitReached() const
	{
		return (max_glyph_idx > 0) && (current_glyph_idx >= max_glyph_idx);
	}

	BOOL SelectionActive() const { return (packed.selection_active==1 && selected_min_idx != -1); }

#ifdef SVG_SUPPORT_TEXTSELECTION
	static int SelectionPointToOffset(const SelectionBoundaryPoint* selpoint, int leading_ws_comp);
	void SetupTextSelection(SVGDocumentContext* doc_ctx, HTML_Element* layouted_elm);
	void SetupSelectionFromCurrentHighlight(HTML_Element* layouted_elm);

	void SetupTextSelectionForFragment();
	BOOL GetNextSelection(SVGDocumentContext* doc_ctx, HTML_Element* layouted_elm, const HTMLayoutProperties& props);
	BOOL GetNextSelectionForFragment();
#endif // SVG_SUPPORT_TEXTSELECTION

	BOOL SelectionEmpty() const
	{
		return (selected_min_idx == selected_max_idx);
	}
	BOOL IsVertical() const
	{
		return writing_mode == SVGWRITINGMODE_TB_RL || writing_mode == SVGWRITINGMODE_TB;
	}
#ifdef SVG_SUPPORT_EDITABLE
	BOOL DrawCaret(HTML_Element* layouted_elm) const
	{
		return (!SelectionActive() && 
				editable && 
				layouted_elm == editable->m_caret.m_point.elm &&
				(editable->m_caret.m_point.ofs == current_char_idx ||
				 current_char_idx == editable->m_caret.m_point.ofs-1));
	}
	BOOL DrawCaret() const
	{
		return DrawCaret(current_element);
	}
	BOOL DrawCaret(int len) const
	{
		return !SelectionActive() &&
			editable &&
			editable->m_caret.m_point.elm == current_element &&
			editable->m_caret.m_point.ofs <= current_char_idx + len &&
			editable->m_caret.m_point.ofs >= (int)current_char_idx;
	}
	BOOL DrawCaretOnRightSide() const
	{
		OP_ASSERT(editable); // This method may only be called after DrawCaret has returned TRUE
		return(current_char_idx == editable->m_caret.m_point.ofs-1);
	}
#endif // SVG_SUPPORT_EDITABLE
	void AdvanceCTP(SVGNumber adv);
	/** Add extent and advance CTP by the extent. */
	void AdvanceCTPExtent(SVGNumber extent);
	void AdjustCTP();

	void AddPendingChunk() { force_chunk = TRUE; }

	void AdvanceBlock();

	void AddPendingRun() { pending_run = TRUE; }
	void FlushPendingRun()
	{
		if (!pending_run)
			return;

		if (is_layout)
			EmitRunStart();

		pending_run = FALSE;
	}
	void EmitRunStart();
	void EmitRunEnd();
	void FlushRun() { EmitRunEnd(); }
	void DoEmitRun(BOOL has_ellipsis);

	void EmitEllipsis(unsigned end_offset);

	void EmitRotation(SVGNumber angle);
	void CommitRotations();

	void UpdateTextAreaEllipsis(unsigned end_offset, BOOL ws_only);
	void FlushTextAreaEllipsis();

	void SetRunBBox();

	/**
	 * Checks if the current textchunk needs alignment (due to text-anchor)
	 */
	BOOL NeedsAlignment()
	{
		BOOL is_rtl = packed.rtl;
		return (packed.anchor == SVGTEXTANCHOR_MIDDLE ||
				(packed.anchor == SVGTEXTANCHOR_END && !is_rtl) ||
				(packed.anchor == SVGTEXTANCHOR_START && is_rtl));
	}

	/**
	 * Update traversal context
	 */
	void UpdateContext(SVGDocumentContext* doc_ctx, SVGElementInfo& info)
	{
		OP_ASSERT(doc_ctx && !doc_ctx->IsExternalResource());
		this->doc_ctx = doc_ctx;
		this->current_element = info.layouted;
		this->props = info.props->GetProps();
	}
	void SetContentNode(SVGElementInfo& info);
	void UpdateContentNode(SVGDocumentContext* font_doc_ctx);
	void SetRotationKeeper(SVGTextRotate* rotation_keeper);

	// Common parameters (positioning, font)
	SVGNumberPair		ctp;					///< The current text position (absolute userunits)
	SVGNumberPair		linepos;				///< The position of the current line in a textArea

	SVGVectorStack		xlist;					///< Absolute x-axis positioning values
	SVGVectorStack		ylist;					///< Absolute y-axis positioning values
	SVGVectorStack		dxlist;					///< Relative x-axis positioning values
	SVGVectorStack		dylist;					///< Relative y-axis positioning values
	SVGVectorStack		rotatelist;				///< Supplemental rotation values

	SVGFontDescriptor	font_desc;				///< Font descriptor

	SVGNumber			baseline_shift;			///< Offset from baseline
	SVGWritingMode		writing_mode;			///< Writing mode (LR, RL, TB, ...)
	SVGNumber			word_spacing;			///< Current Word spacing
	SVGNumber			letter_spacing;			///< Current Letter spacing

	BOOL				force_chunk;			///< Tell the text-layout that we require a new chunk
	BOOL				pending_run;			///< We have a run pending for the current fragment (textArea)
	BOOL				elm_needs_vector_fonts;	///< Saved textnode-wide vectorfont req.

	struct
	{
		FragmentPosition	start;
		FragmentPosition	end;
		SVGNumberPair		position;
	}					current_run;
	TextRun*			last_run;

	GlyphRotationList	rotation_list;
	unsigned			rotation_count;

	// Position in text/string
	int					current_glyph_idx;		///< The current glyph index (for getting values from the above vectors)

	int					current_char_idx;		///< The current char index in the string (range is: [0..layouted_elm->GetTextContentLength()-1])
	int					total_char_sum;			///< The sum of characters processed - _before_ the current element, i.e. total_char_sum should give a kind of 'global' (text element wide) character index

	FragmentPosition	frag;					///< The position in the fragment array, and offset in the current fragment. Valid within traversal of a textnode.

	// Textselection parameters
	int					selected_min_idx;		///< The min logical char index for the selection, -1 disables selection, [0..layouted_elm->GetTextContentLength()-1]
	int					selected_max_idx;		///< The max logical char index for the selection, [0..layouted_elm->GetTextContentLength()-1]

	// Path parameters
	SVGMotionPath*		path;					///< The path the text should follow
	SVGNumber			pathDistance;			///< The current distance on the curve. In the path's coordinate system.
	SVGNumber			pathDisplacement;		///< Displacement of glyphs along the non-major axis

	// Textarea parameters
	SVGTextAreaInfo*	area;					///< Information structure for a textarea

	// Extent and measurement (bbox et.c.) data
	SVGBBoxStack		bbox;					///< Stack of boundingboxes for the text elements

	OpVector<SVGTextChunk>*chunk_list;			///< List of extents for text chunks
	unsigned int		current_chunk;			///< The chunk we are currently at
	SVGNumber			total_extent;			///< Accumulated length of text, in userunits

	SVGTextData*		data;					///< Used for returning some values
	int					max_glyph_idx;			///< Max-index of glyphs to use (-1 for no limit)

	// Textlength parameters
	SVGNumber			extra_spacing;			///< Extra spacing (length adjust)
	SVGNumber			glyph_scale;			///< Scalefactor for glyphs

	// text-overflow
	SVGNumber			max_extent;				///< Ellipsis constraint to use for <text>
	SVGNumber			current_extent;			///< Extent up to but not including the current chunk

#ifdef SVG_SUPPORT_EDITABLE
	SVGEditable*		editable;				///< The editing context of the current text root, or NULL if inactive
#endif // SVG_SUPPORT_EDITABLE

	SVGTextCache*		current_text_cache;		///< The text cache related to the text block currently being processed. Only valid within WalkFragments.

	BOOL				is_layout;				///< Are we in a layout pass?

	OpVector<SVGTextAttributePaintNode> text_attr_stack;///< Stack of text attribute paint nodes
	OpVector<uni_char>	lang_stack;
	OpINT32Vector		space_stack;
#ifdef SVG_SUPPORT_SEARCH_HIGHLIGHTING	
	AutoDeleteList<SelectionElm>
						dummy_selection;		///< Keeps track of any allocated SelectionElm:s
	SelectionElm*		first_selection_elm;	///< The first selection element
	SelectionElm*		current_sel_elm;		///< The current selection element
#endif // SVG_SUPPORT_SEARCH_HIGHLIGHTING

	// This is the current context, only to be used within a text-traversal
	SVGDocumentContext*	doc_ctx;				///< Current document context
	HTML_Element*		current_element;		///< Current layouted element (potentially a textnode/group)
	const HTMLayoutProperties* props;			///< Properties for current_element
	SVGTextContentPaintNode* content_paint_node;///< The current paint node for text content (non-NULL during layout of text node)
	SVGTextRotate* glyph_rotation_keeper;		///< The keeper of the GlyphRotationList
	SVGTextRootContainer*	text_root_container; ///< The current text root container

	union
	{
		struct
		{
			unsigned int preserve_spaces:1;			///< TRUE if spaces should be preserved
			unsigned int rtl:1;						///< TRUE if the principal direction is right-to-left
			unsigned int spacing_auto:1;			///< TRUE if spacing for textPath is auto
			unsigned int method:2;					///< Method for textPath glyph mapping (see SVGMethod)
			unsigned int anchor:3;					///< TextAnchor (see SVGTextAnchor)
#ifndef FOLLOW_THE_SVG_1_1_SPEC_EVEN_WHEN_ITS_UNUSABLE
			unsigned int keep_leading_whitespace:1;	///< FALSE if we should collapse leading whitespace into nothing in the next text part
#endif // !FOLLOW_THE_SVG_1_1_SPEC_EVEN_WHEN_ITS_UNUSABLE
			unsigned int selection_active:1;		///< TRUE if selection is active in the current element, which means we should look at selected_{min,max}_idx
			unsigned int has_painted_caret:1;		///< TRUE if the caret has been painted in this text root
			unsigned int svg_selection:1;
			unsigned int prev_use_traversal_filter:1;	///< Saved value for if the traversal filter was used before entering the current text-root
			unsigned int use_ellipsis:1;			///< TRUE if text-overflow=ellipsis is active
			unsigned int check_ellipsis:1;			///< TRUE if the chunk being processed will trigger ellipsis
			unsigned int clip_runs:1;				///< If TRUE runs will be marked as invisible
		} packed; /* 15 bits */
		unsigned int packed_init;
	};

	/**
	 * Adds given rectange to the selection rectangles list stored in data->selection_rect_list.
	 * A caller must check if data and data->selection_rect_list are not NULLs before calling this.
	 *
	 * @param rect The rectangle to be added to the list.
	 *
	 * @return OpStatus::ERR_NO_MEMORY on OOM. OpStatus::OK otherwise.
	 */
	OP_STATUS AddSelectionRect(const SVGRect& rect);

	/**
	 * Adds given rectange to the selection rectangles list stored in data->selection_rect_list.
	 * A caller must check if data and data->selection_rect_list are not NULLs before calling this.
	 *
	 * @param rect The rectangle to be added to the list.
	 *
	 * @return OpStatus::ERR_NO_MEMORY on OOM. OpStatus::OK otherwise.
	 */
	OP_STATUS AddSelectionRect(const OpRect& rect);
};

class SVGTextMeasurementObject : public SVGVisualTraversalObject
{
public:
	SVGTextMeasurementObject(SVGChildIterator* child_iterator) :
		SVGVisualTraversalObject(child_iterator)
	{
		m_apply_paint = FALSE;
	}
	virtual ~SVGTextMeasurementObject() {}

	virtual OP_STATUS EnterElement(SVGElementInfo& info, const TransformInfo& tinfo) { return OpStatus::OK; }
	virtual OP_STATUS LeaveElement(SVGElementInfo& info) { return OpStatus::OK; }

	virtual OP_STATUS ValidateUse(SVGElementInfo& info) { return OpStatus::OK; }

	virtual OP_STATUS HandlePaintserver(SVGElementInfo& info, HTML_Element* paintserver, BOOL is_fill,
										BOOL& ignore_fallback) { return OpStatus::OK; }
	virtual OP_STATUS HandleRasterImage(SVGElementInfo& info, URL* imageURL, const SVGRect& img_vp, int quality) { return OpStatus::OK; }
#ifdef SVG_SUPPORT_SVG_IN_IMAGE
	virtual OP_STATUS HandleVectorImage(SVGElementInfo& info, const SVGRect& img_vp) { return OpStatus::OK; }
#endif // SVG_SUPPORT_SVG_IN_IMAGE
#ifdef SVG_SUPPORT_FOREIGNOBJECT
	virtual OP_STATUS HandleForeignObject(SVGElementInfo& info, const SVGRect& img_vp, int quality) { return OpStatus::OK; }
#endif // SVG_SUPPORT_FOREIGNOBJECT

	virtual OP_STATUS HandleAltGlyph(SVGElementInfo& info, BOOL have_glyphs);
	virtual OP_STATUS HandleTextNode(SVGElementInfo& info, HTML_Element* text_content_elm,
									 SVGBoundingBox* bbox);
	virtual OP_STATUS HandleForcedLineBreak(SVGElementInfo& info);
	virtual OP_STATUS SetupTextAreaElement(SVGElementInfo& info);
	virtual OP_STATUS PrepareTextAreaElement(SVGElementInfo& info);
	virtual OP_STATUS SetupExtents(SVGElementInfo& info) { return OpStatus::OK; }
#ifdef SVG_SUPPORT_EDITABLE
	virtual void CheckCaret(SVGElementInfo& info) {}
	virtual void FlushCaret(SVGElementInfo& info) {}
#endif // SVG_SUPPORT_EDITABLE

	SVGTextArguments*	GetTextParams() { return m_textinfo; }
};

#endif // SVG_SUPPORT
#endif // SVG_TEXTTRAVERSE_H
