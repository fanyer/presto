/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Magnus Gasslander
*/

#if !defined UNICODE_BIDIUTILS_H && defined SUPPORT_TEXT_DIRECTION
#define UNICODE_BIDIUTILS_H

class HTML_Element; // defined in htm_elm.h

/**
 * Stack used when laying out bidi text to keep track of the levels used
 * in the bidi algorithm. Paragraph level is the entry level in the
 * paragraph 0 means ltr, 1 means rtl.
 */
class BidiLevelStack
{
private:
	enum
	{
		/** BD2: The maximum explicit depth is level 61. */
		MaximumDepth = 61
	};

	/** Stack used to store BiDi levels and their override status.
	 * Level is stored in the top 7 bits. The LSB is a bool indicating
	 * override. Stack entry 0 contains the paragraph level. */
	UINT8 stack[MaximumDepth + 1];

	static op_force_inline bool SlotOverride(UINT8 stack_val)
	{
		return (stack_val & 1) != 0;
	}
	static op_force_inline unsigned int SlotLevel(UINT8 stack_val)
	{
		return stack_val >> 1;
	}
	static op_force_inline UINT8 EncodeSlot(unsigned int level, bool is_override)
	{
		return static_cast<UINT8>((level << 1) | static_cast<int>(is_override));
	}

	signed char stack_top;

public:
	BidiLevelStack() { Reset(); }

	int GetMaxStackDepth() const { return MaximumDepth; }
	int GetCurrentStackDepth() const { return stack_top; }

	/**
	 * @return current embedding level
	 */
	int GetCurrentLevel() const
	{
		OP_ASSERT(stack_top >= 0 && static_cast<size_t>(stack_top) < ARRAY_SIZE(stack));
		return SlotLevel(stack[stack_top]);
	}


	/**
	 * @return current embedding direction
	 */
	BidiCategory GetEmbeddingDir() const;

	/**
	 * @return current override status
	 */
	BidiCategory GetOverrideStatus() const;

	/**
	 * Get the resolved level based on the current embedding level and the type.
	 * According to chapter 3.3.5 Resolving Implicit Levels
	 *
	 * @param type
	 *
	 * @return the resolved level
	 */
	int GetResolvedLevel(BidiCategory type) const;

	/**
	 * Push a new embedding level with the given properties onto the stack.
	 *
	 * @param direction
	 * @param is_override
	 */
	void PushLevel(int direction, bool is_override);

	/**
	 * Adds one more entry to the stack, without changing anything.
	 */
	void PushLevel()
	{
		if (stack_top >= MaximumDepth)
			return;

		stack_top++;
		stack[stack_top] = stack[stack_top - 1];
	}

	/** 
	 * Pops last embedding level off stack. Caused by a PDF code or a closed bidi-embedding element.
	 */
	void PopLevel()
	{
		if (stack_top > 0)
			stack_top--;

		// The bidi algorithm is well defined here.
		OP_ASSERT(stack_top >= 0);
	}


	/** 
	 * Paragraph level is the entry level in the paragraph 0 means ltr, 1 means rtl. 
	 *
	 * @param type
	 * @param override
	 *
	 */
	void SetParagraphLevel(BidiCategory type, bool override)
	{
		stack[0] = EncodeSlot((type == BIDI_R) ? 1 : 0, override);
	}

	/** Reset the stack */
	void Reset()
	{
		stack_top = 0;
		stack[0] = EncodeSlot(0, false); // Slot 0 is the paragraph level.
	}
};

/**
 * A level run (longest possible segment with the same level) with width
 * and start position information. It also records the level and other
 * user data (HTML_Element).
 *
 * Levels are calculated on a per-paragraph basis, but resolved on a
 * per-line basis.
 */
class ParagraphBidiSegment : public Link
{
public:
	ParagraphBidiSegment(long width, HTML_Element* start_element, unsigned short level, long virtual_position)
			: width(width),
			  start_element(start_element),
			  level(level),
			  virtual_position(virtual_position)
		{}

	/**
	 * @overload Link::Suc()
	 */
	ParagraphBidiSegment* Suc() const { return static_cast<ParagraphBidiSegment*>(Link::Suc()); }

	/**
	 * @overload Link::Pred()
	 */
	ParagraphBidiSegment* Pred() const { return static_cast<ParagraphBidiSegment*>(Link::Pred()); }

	/** Width of the segment */
	long	width;

	/** Segment starts with this element (used specifically in the layout engine */
	HTML_Element*	start_element;

	/** This segment's level, max 63 according to the unicode-bidi standard */
	unsigned short	level;

	/** Logical position in the paragraph where this segment starts. */
	long   virtual_position;
};



/**
 * The engine for calculating bidi levels. 
 *
 * Typically an instance of the bidi calculation object is instantiated
 * (on heap or stack) for each paragraph to calulate the bidi levels
 * for this paragraph. This object can be thrown away when the calculation
 * is done.
 */
class BidiCalculation
{
public:
	BidiCalculation()
		: paragraph_type(BIDI_UNDEFINED),
		  segments(NULL)
		{}

	/**
	 * The segments are owned by Container. This will set the pointer to the segments
	 * 
	 * @param segments
	 *
	 */

	void SetSegments(Head* segments) {this->segments = segments;}


	/**
	 * Add a stretch to the current paragraph.
	 * There are four types that create segments - BIDI_L, BIDI_R, BIDI_AN and BIDI_EN.
	 *   The other types either convert to these types according to the weak type rules, or append a neutral stretch.
	 *   (Maybe BIDI_S should create segments aswell...)
	 *
	 * @param initial_bidi_type
	 * @param width
	 * @param element
	 * @param virtual_position
	 *
	 * @return LONG_MAX if there is a OOM error, LONG_MAX - 1 if the last type was not a neutral - so there was no pending segment created
	 *			otherwise returns the neutral position of the pending neutrals.
	 */


	long  AppendStretch(BidiCategory initial_bidi_type, int width, HTML_Element* element, long virtual_position);

	/**
	 * Push a new embedding level with the given properties onto the stack.
	 *
	 * @param direction
	 * @param is_override
	 *
	 * @return status
	 *
	 */

	OP_STATUS PushLevel(int direction, BOOL is_override);

	/**
	 * Pops last embedding level off stack. Caused by a PDF code or a
	 * closed bidi-embedding element.
	 *
	 * @return status
	 */
	
	OP_STATUS PopLevel();

	/**
	 * Paragraph level is the entry level in the paragraph 0 means ltr, 1 means rtl.
	 *
	 * @param type
	 * @param override
	 *
	 */

	void SetParagraphLevel(BidiCategory type, BOOL override);


	/**
	 * This is where we handle neutrals after a push/pop by a bidi control code
	 * or an inline element.
	 *
	 * @return status
	 *
	 * Should become private one the new public API HandleParagraphEnd is used instead.
	 */

	OP_STATUS HandleNeutralsAtRunChange();


	/**
	 * Handle paragraph end
	 *
	 * @return OpStatus::OutOfMemory if a potential allocation fails. Otherwise OpStatus::OK;
	 *
     */
	OP_STATUS HandleParagraphEnd() { return PopLevel(); }

	/** 
	 * Reset the entire calculation object, and prepare it for reuse.
	 */

	void Reset();

	/**
	 *
	 * @return status
	 */

	OP_STATUS InsertParagraphEndingSegment() { return CreateSegment(0, NULL, 63, 0); }

	private:

	/**
	 * Convenience method, is this type a neutral type?
	 */
	static bool IsNeutral(BidiCategory type)
	{
		return (type == BIDI_B ||
				type == BIDI_S ||
				type == BIDI_WS ||
				type == BIDI_ON);
	}

	/**
	 * Create a segment of this level
	 *
	 * @param width
	 * @param element
	 * @param level
	 * @param virtual_position
	 * 
	 * @return status
	 */

	OP_STATUS CreateSegment(long width, HTML_Element* element, unsigned short level, long virtual_position);

	/**
	 * Create this segment, or append to the previous one if appropriate
	 *
	 * @param type
	 * @param width
	 * @param start_element
	 * @param virtual_position
	 *
	 * @return status
	 */

	OP_STATUS ExpandCurrentSegmentOrCreateFirst(BidiCategory type, int width, HTML_Element* start_element, long virtual_positiom);

	/**
	 * Create a ParagraphBidiSegment of this level
	 *
	 * @param type
	 * @param width
	 * @param start_element
	 * @param virtual_position
	 * 
	 * @return status
	 */

	OP_STATUS CreateBidiSegment(BidiCategory type, int width, HTML_Element* start_element, long virtual_position);
	
	/**
	 * Expand the last segment with this width
	 *
	 * @param width
	 */

	void ExpandCurrentSegment(int width);

	ParagraphBidiSegment* GetCurrentSegment() const
	{
		return static_cast<ParagraphBidiSegment*>(segments->Last());
	}


	/** Type of the last word. */
	BidiCategory last_type;

	/** Type of the last strong word. This is either BIDI_L or
	 * BIDI_R. If the level has changed the last strong type is the
	 * start-of-level-run (sor) type. */
	BidiCategory last_strong_type;

	/** Used only to determine if the paragraph type has been set. */
	BidiCategory paragraph_type;

	/** Records the type of the last created segment. */
	BidiCategory last_segment_type;

	/** Used to record the end-of-level-run (eor) at level changes. */
	BidiCategory eor_type;

	/** Saves the actual type of the last segment to be used in rule W1. */
	BidiCategory W1_last_type;

	/** Save the last strong type according to rule W2 the definition
	 * of a strong type in this rule differ. */
	BidiCategory W2_last_strong_type;

	/* Save the type of number that has initiated the W4 rule (EN or
	 * AN). If W4 rule is not initialed it should be BIDI_UNDEFINED. */
	BidiCategory W4_pending_type;

	/** Save the type before W5 rule was initiated. If it is neutral,
	 * the neutral width should also be accounted for when the stretch
	 * is laid out.  If it is not neutral, the information should be
	 * used the create a new neutral stretch if the rule should be
	 * cancelled. */
	BidiCategory W5_type_before;

	BidiCategory W5_last_type;

	BidiCategory W7_last_strong_type;

	/** Used to record the previous embedding level. Used in the
	 * border between a push/pop. */
	int previous_embedding_level;

	/** Records any pending neutrals that waits to be allocated to a
	 * segment. */
	int neutral_width;

	/** Records the level of the segment created last. This is used to
	 * determine if it is possible to add a new stretch to this
	 * level. */
	int previous_segment_level;

	/** End-of-level-run (eor) level. */
	int eor_level;

	int W4_width;

	int W5_width;

	/** Start of the virtual position of the element. */
	long neutral_virtual_position_start;

	long W4_virtual_position;

	long W5_virtual_position;

	/** List of the paragraph segments. The list is owned by the
	 * container since the calculated levels are needed when
	 * resolving levels. */
	Head* segments;

	/** Start element of any pending neutrals. */
	HTML_Element* neutral_start_element;

	/** Used when we want to attach a whitespace and we dont have the element. */
	HTML_Element* last_appended_element;

	HTML_Element* W4_start_element;

	HTML_Element* W5_start_element;

	BidiLevelStack level_stack;
};

#endif
