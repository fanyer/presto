/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef WORDINFOITERATOR_H
#define WORDINFOITERATOR_H

#ifdef LAYOUT_CARET_SUPPORT

class HTML_Element;
class FramesDocument;

#define WI_STACK_DWORDS 8
#define WI_BPU (sizeof(UINT32)*8)
#define WI_STACK_BITS (WI_STACK_DWORDS*WI_BPU)

/**
 * WordInfoIterator is an abstraction for easier access of the array of WordInfo structs in Text_Box.
 * It also contains some logic for determining valid caret-positions inside text-elements.
 */

class WordInfoIterator
{
public:

	/**
	 * Remains from the time when this iterator was interwoven with the documentedit code.
	 * Sometimes we need to ask if the elements are adjacent to an element with certain
	 * properties. Then we use this one.
	 */
	class SurroundingsInformation
	{
	public:
		virtual BOOL HasWsPreservingElmBeside(HTML_Element* helm, BOOL before) = 0;
	};

private:
	/** Returns the "extra length" like trailing whitespace associated with the WordInfo w. */
	int ExtraLength(WordInfo *w);

	BOOL ComputeIsInPreFormatted(HTML_Element *helm);

	OP_STATUS CalculateVisibilityBits();

protected:
	BOOL IsPreFormatted();

private:
	FramesDocument *m_doc;
	HTML_Element *m_helm;
	SurroundingsInformation* m_surroundings;
	int m_word_idx;
	int m_char_ofs;
	int m_word_count;
	WordInfo *m_words;
	int m_full_length;
	const uni_char *m_full_txt;
	UINT32 m_visible_char_stack_bits[WI_STACK_DWORDS];
	UINT32 *m_visible_char_bits;
	int m_un_col_count;
	int m_first_un_col_ofs;
	int m_last_un_col_ofs;
	BOOL m_has_uncollapsed_word;
	BOOL3 m_pre_formatted;

public:

	/** Constructor, helm should be a HE_TEXT element and edit an instance of OpDocumentEdit as some functions like
	    OpDocumentEdit::ReflowAndUpdate and OpDocumentEdit::IsFriends are used. */
	WordInfoIterator(FramesDocument* doc, HTML_Element* helm, SurroundingsInformation* surroundings);

	/** Destructor */
	~WordInfoIterator();

	/** Second stage constructor. */
	OP_STATUS Init();

	/** Get the element */
	HTML_Element* GetElement() { return m_helm; }

	/** Sets the current WordInfo to be the first. */
	void Reset() { m_word_idx = 0; m_char_ofs = 0; }

	/** Returns the number of WordInfo structs for the element. */
	int WordCount() { return m_word_count; }

	/** Return the current index into the WordInfo array. */
	int WordIndex() { return m_word_idx; }

	/** Returns the current character offset into the element's text-content (NOT index into current word). */
	int CharIndex() { return m_char_ofs; }

	/** Step to the next WordInfo. */
	void NextWord() { m_word_idx = MIN(m_word_idx+1,m_word_count); }

	/** Step to the previous WordInfo. */
	void PrevWord() { m_word_idx = MAX(m_word_idx-1,-1); }

	/** Returns the element's text-content length. */
	int GetFullLength() { return m_full_length; }

	/** Returns pointer to WordInfo at index index. */
	WordInfo* GetWordAt(int index);

	/** Returns pointer to the current WordInfo */
	WordInfo* CurrentWord() { return GetWordAt(m_word_idx); }

	/** Returns pointer to the current WordInfo, after that iterates one step forward if possible. */
	WordInfo* CurrentWordAndNext() { WordInfo *w = CurrentWord(); NextWord(); return w; }

	/** Returns pointer to the current WordInfo, after that iterates one step backward if possible. */
	WordInfo* CurrentWordAndPrev() { WordInfo *w = CurrentWord(); PrevWord(); return w; }

	/** Returns the number of uncollapsed characters in the element's text-content. */
	int UnCollapsedCount() { return m_un_col_count; }

	/** Returns TRUE if there are uncollapsed characters in the element's text-content. */
	BOOL HasUnCollapsedChar() { return !!m_un_col_count; }

	/** Returns TRUE if there are collapsed characters in the element's text-content. */
	BOOL HasCollapsedChar() { return UnCollapsedCount() != m_full_length; }

	/** Returns TRUE if there are uncollapsed words in the element (WordInfo structs without the collapsed-flag set). */
	BOOL HasUnCollapsedWord() { return m_has_uncollapsed_word; }

	/** Returns the index into the element's text-content for the first uncollapsed character. */
	int FirstUnCollapsedOfs() { return m_first_un_col_ofs; }

	/** Returns the index into the element's text-content for the last uncollapsed character. */
	int LastUnCollapsedOfs() { return m_last_un_col_ofs; }

	/** Returns TRUE if the character at offset ofs in the element text-content is collapsed. */
	BOOL IsCharCollapsed(int ofs) { return !(m_words && m_visible_char_bits && ofs >= 0 && ofs < m_full_length) || !(m_visible_char_bits[ofs/WI_BPU]&(1<<(ofs%WI_BPU))); }

	/** Returns TRUE if the current character is collapsed. */
	BOOL IsCurrentCharCollapsed() { return IsCharCollapsed(m_char_ofs); }

	/** Does the text-content for the element ends with collapsed character(s)? */
	BOOL IsCharAfterLastUnCollapsed(int ofs) { return ofs > m_last_un_col_ofs; }

	/** returns TRUE if the WordInfo struct at index index has the collapsed-flag set. */
	BOOL IsWordCollapsed(int index);

	/** returns TRUE if the current WordInfo structs has the collapsed-flag set. */
	BOOL IsCurrentWordCollapsed() { return IsWordCollapsed(m_word_idx); }

	/** Returns TRUE if the character at offset ofs in the element text-content is the trailing part of a surrogate pair. */
	BOOL IsCharInSurrogatePair(int ofs) { return Unicode::IsLowSurrogate(m_full_txt[ofs]); }

	/** Converts an offset into the element's text-content (including collapsed character) into an offset among the uncollapsed
	    characters. Returns FALSE if col_ofs is itself collapsed or if col_ofs is outside of the elements text-content. */
	BOOL CollapsedToUnCollapsedOfs(int col_ofs, int &res_un_col_ofs);

	/**
	 * "Snaps" offset ofs into the element's text-content to an offset into the text-content for an uncollapsed character,
	 * in the direction specified by snap_forward.
	 * @return TRUE if such offset was found.
	 * @parm ofs The offset to start the scan from including ofs itself.
	 * @parm res_ofs The result...
	 * @parm snap_forward If TRUE we'll scan forward, else backward. Notice that only that direction is scanned, so if
	 * ofs is after the last uncollapsed offset FALSE will be returned even though uncollapsed character exists before.
	 */
	BOOL GetOfsSnappedToUnCollapsed(int ofs, int &res_ofs, BOOL snap_forward);

	/**
	 * Retrieves the next of previous uncollapsed character-offset starting from offset ofs.
	 * @return TRUE if such offset was found.
	 * @parm ofs The offset to start from, ofs might be collapsed or uncollapsed, in any case will NOT ofs itself be returned.
	 * @parm res_ofs The result...
	 * @parm TRUE if we should scan forward, else backward.
	 */
	BOOL GetUnCollapsedOfsFrom(int ofs, int &res_ofs, BOOL forward);

	/** Wrapper around GetUnCollapsedOfsFrom with forward == TRUE, see GetUnCollapsedOfsFrom. */
	BOOL GetNextUnCollapsedOfs(int ofs, int &res_ofs) { return GetUnCollapsedOfsFrom(ofs,res_ofs,TRUE); }

	/** Wrapper around GetUnCollapsedOfsFrom with forward == FALSE, see GetUnCollapsedOfsFrom. */
	BOOL GetPrevUnCollapsedOfs(int ofs, int &res_ofs) { return GetUnCollapsedOfsFrom(ofs,res_ofs,FALSE); }
};
#endif // LAYOUT_CARET_API

#endif // WORDINFOITERATOR_H
