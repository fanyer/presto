/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2003-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef OP_TEXT_FRAGMENTLIST_H
#define OP_TEXT_FRAGMENTLIST_H

#ifdef COLOURED_MULTIEDIT_SUPPORT
#include "modules/widgets/ColouredMultiEdit.h"
#endif

#include "modules/style/css_all_values.h"

#define BIDI_FRAGMENT_NORMAL           0x01 // 0001	
#define BIDI_FRAGMENT_MIRRORED         0x02 // 0010	
#define BIDI_FRAGMENT_NEUTRAL_NORMAL   0x04 // 0100	
#define BIDI_FRAGMENT_NEUTRAL_MIRRORED 0x08 // 1000	
 	
#define BIDI_FRAGMENT_ANY_NORMAL       0x05 // 0101	
#define BIDI_FRAGMENT_ANY_MIRRORED     0x0A // 1010	
#define BIDI_FRAGMENT_ANY_NEUTRAL      0x0C // 1100

class FramesDocument;
struct FontSupportInfo;
struct OP_TEXT_FRAGMENT;

/**
	Represents a visual offset in a textfragment.
	A visual offset might have 2 logical offsets (where a RTL fragment meets a LTR fragment).
	idx tells which fragment that is prefered.
*/
struct OP_TEXT_FRAGMENT_VISUAL_OFFSET {
	int idx;
	int offset;
};

/**
	Represents a logical offset in a textfragment.
	A logical offset might have 2 visual offsets (where a RTL fragment meets a LTR fragment).
	snap_forward is TRUE if the latter fragment (in logical order) should be prefered.
*/
struct OP_TEXT_FRAGMENT_LOGICAL_OFFSET {
	BOOL snap_forward;
	int offset;
};

/** OpTextFragmentList handles BIDI reordering and fontswitching of a string.
	It does not own a string itself, so it should be updated whenever the string changes.
	The OpTextFragmentList has functions to return the fragments in visual or logical order,
	and convert offsets between visual and logical order.
*/

class OpTextFragmentList
{
private:
	OP_TEXT_FRAGMENT*	m_fragments;		///< The array of textfragments
	UINT32				m_num_fragments;	///< The number of items in the m_fragments array
#ifdef COLOURED_MULTIEDIT_SUPPORT
	UINT32*				m_tag_type_array;
#endif
	BOOL m_has_been_split;	///< @c TRUE iff Split() has been called since last Update()
#ifdef WIDGETS_ELLIPSIS_SUPPORT
	BOOL m_mostly_mirrored;	///< @c TRUE iff more than half of the fragments are mirrored
#endif

public:
	OpTextFragmentList()
		: m_fragments(NULL)
		, m_num_fragments(0)
#ifdef COLOURED_MULTIEDIT_SUPPORT
		, m_tag_type_array(NULL)
#endif
		, m_has_been_split(FALSE)
#ifdef WIDGETS_ELLIPSIS_SUPPORT
		, m_mostly_mirrored(FALSE)
#endif
	{}

	~OpTextFragmentList();

	/** Removes all fragments. */
	void Clear();

	/**
	* Updates the textfragment list. Must be called when the string changes.
	*
	* @param str The string to split into fragments
	* @param len The stringlength
	* @param isRTL If the string is considered right-to-left from the beginning pass TRUE
	* @param preserveSpaces If spaces should be left as-is in the fragments pass TRUE
	* @param treat_nbsp_as_space Pass TRUE if non-breaking space should be treated as space
	* @param framesDocument The FramesDocument, needed for fontswitching
	* @param original_fontnumber The fontnumber of the font that is currently set
	* @param script The preferred script, for fontswitching 
	* @param resolve_order Pass TRUE if order should be calculated.
	*/
	OP_STATUS Update(const uni_char* str, size_t len,
					BOOL isRTL, BOOL isOverride,
					BOOL preserveSpaces,
					BOOL treat_nbsp_as_space,
					FramesDocument* framesDocument, 
					int original_fontnumber, 
					 const WritingSystem::Script script = WritingSystem::Unknown, 
					BOOL resolve_order = TRUE,
					int *start_in_tag = NULL,
					BOOL convert_ampersand = FALSE
					);

	/** Moves the is_misspelled flag flag forward steps step (among the OP_TEXT_FRAGMENTs) for all fragments
		that have start_ofs > after_ofs, and resets the flag for the steps fragments before them. */
	void MoveSpellFlagsForwardAfter(UINT32 after_ofs, int steps);
	
	/**
	* Reset the fragment order. The visual order will be the same as the logical.
	*/
	void ResetOrder();

	/**
	* Calculate the order for num fragments, starting with idx. isRTL should be the RTL status for the container.
	* When resolving order for a string wrapped across multiple lines, simply call ResolveOrder multiple times
	* (one time for each line).
	*/
	void ResolveOrder(BOOL isRTL, int idx, int num);

	/**	Get the number of textfragments. */
	UINT32 GetNumFragments() { return m_num_fragments; }

	/** 
	* Get a textfragment.
	*
	* @param idx The wanted index [0..GetNumFragments()]
	* @return The textfragment in logical order.
	*/
	OP_TEXT_FRAGMENT* Get(UINT32 idx);

	/**
	* Get a textfragment in visual order.
	*
	* @param idx The wanted index [0..GetNumFragments()]
	* @return The textfragment in visual order.
	*/
	OP_TEXT_FRAGMENT* GetByVisualOrder(UINT32 idx);

#ifdef COLOURED_MULTIEDIT_SUPPORT
	BOOL	HasColours() { return m_tag_type_array != NULL; }
	UINT32	GetTagType(OP_TEXT_FRAGMENT *frag);
	UINT32	GetTagType(UINT32 idx) { return m_tag_type_array[idx]; }
#endif

	/** Convert a logical offset to a visual */
	OP_TEXT_FRAGMENT_VISUAL_OFFSET LogicalToVisualOffset(OP_TEXT_FRAGMENT_LOGICAL_OFFSET logical_offset);

	/** Convert a visual offset to a logical */
	OP_TEXT_FRAGMENT_LOGICAL_OFFSET VisualToLogicalOffset(OP_TEXT_FRAGMENT_VISUAL_OFFSET visual_offset);

	/**
	   splits text fragment idx so that its width doesn't exceed width px

	   @param info about the text collection
	   @param str the string to be split - this is the entire string, not the substring for the fragment
	   @param idx the index of the fragment to split
	   @param width the desired new width of the fragment after splitting
	   @param next_width returns the width of the next word. Should immediately be 
			set as width on the next word if it fits. But only if the split returned OpStatus::OK

	   @return
		 OpStatus::OK if fragment has been split (next_width will be set)
		 (OP_STATUS)1 if splitting isn't possible (first unit is wider than desired width)
	     OpStatus::ERR if the passed data isn't sane
		 OpStatus::ERR_NO_MEMORY on OOM
	 */
	OP_STATUS Split(struct OP_TCINFO* info, const uni_char* str, const UINT32 idx, const UINT32 width, UINT32& next_width);
	/**
	   splits text fragment idx at position offs, recording its width
	   as new_width. the rest is inserted as a new fragment, with
	   width next_width. last should be the last unicode codepoint
	   before the split, and is used to determine whether the first
	   part of the split segment has trailing whitespace.
	 */
	OP_STATUS Split(const UINT32 idx, const UINT32 offs, const UINT32 new_width, const UINT32 next_width, UnicodePoint last);

	BOOL HasBeenSplit() const { return m_has_been_split; }

#ifdef WIDGETS_ELLIPSIS_SUPPORT
	/**
	 * @return @c TRUE iff more than half of the fragments are mirrored.
	 * 		Neutrals are counted as the direction they inherit.
	 */
	BOOL IsMostlyMirrored() const { return m_mostly_mirrored; }
#endif

private:
	/** Calls the layout-modules GetNextTextFragment. Adds some handling of syntax highlighting. */
	BOOL GetNextTextFragment(const uni_char*& txt, 
							int &length, 
							UnicodePoint& previous_character,
							BOOL leading_whitespace,
							WordInfo& word_info, 
							CSSValue white_space, 
							BOOL stop_at_whitespace_separated_words, 
							BOOL treat_nbsp_as_space, 
							FontSupportInfo* font_info, 
							FramesDocument* doc,
							int *in_tag,
							int *in_tag_out,
							 BOOL convert_ampersand,
							 const WritingSystem::Script script);
};

#endif // OP_TEXT_FRAGMENTLIST_H
