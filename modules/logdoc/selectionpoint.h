/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef SELECTIONPOINT_H
#define SELECTIONPOINT_H

class HTML_Element;

/**
 * Selection boundary point. Specifies a selected point in a document,
 * using the same format as DOM Ranges (element + offset) with the
 * bonus of a "binding direction" to solve the rendering problem of
 * points of a document that exists at multiple places.
 */
class SelectionBoundaryPoint
{
private:

	HTML_Element* m_element;

	signed int m_offset:31;

	/**
	 * Indicator for what this selection point binds hardest to. In case of line breaks
	 * and BiDi a text like A|B might have the A and B rendered far apart and
	 * then it is important to know whether the point is associated mostly with the
	 * A or the B to determine where to render it. If this is TRUE then we bind
	 * harder forward and in the case of a linebreak the caret would show on the next line.
	 */
	unsigned int m_bind_forward:1;

public:

	/**
	 * @see SetBindDirection()
	 * @see GetBindDirection()
	 */
	enum BindDirection
	{
		BIND_BACKWARD,
		BIND_FORWARD
	};

	/**
	 * Default constructor.
	 */
	SelectionBoundaryPoint()
		: m_element(NULL),
		m_offset(0),
		m_bind_forward(0)
	{}

	/**
	 * Constructor.
	 */
	SelectionBoundaryPoint(HTML_Element* element, int offset)
		: m_element(element),
		m_offset(offset),
		m_bind_forward(0)
	{}

	/**
	 * Other Constructor.
	 */
	SelectionBoundaryPoint(HTML_Element* element, HTML_Element* indicated_child)
		: m_element(element),
		m_offset(CalculateOffset(indicated_child)),
		m_bind_forward(0)
	{}

	/**
	 * Copy constructor.
	 */
	SelectionBoundaryPoint(const SelectionBoundaryPoint& other)
		: m_element(other.m_element),
		m_offset(other.m_offset),
		m_bind_forward(other.m_bind_forward)
	{}

	/**
	 * Empties the selection point.
	 */
	void Reset() { m_element = NULL; m_offset = 0; m_bind_forward = 0; }

	/**
	 * Are these selection points equal? Only position is used in the
	 * camparison. Not bind direction.
	 */
	BOOL operator==(const SelectionBoundaryPoint& other_selection) const;

	/**
	 * Copy constructor.
	 */
	void operator=(const SelectionBoundaryPoint& X);

	/**
	 * Get the element.
	 */
	HTML_Element* GetElement() const { return m_element; }

	/**
	 * Get character offset in text element. backwards compatible
	 * name. Use GetOffset() in new code.
	 */
	int GetTextElementCharacterOffset() const { return GetOffset(); }

	/**
	 * Get the offset. For a text element this is the offset into the
	 * character string. For other elements it's the offset into the
	 * list of ActualStyle visible children. See the
	 * DOM Range specification for a more complete description including
	 * examples.
	 */
	int GetOffset() const { return m_offset; }

	/** Set the logical position. This is an element and an offset
	 * into the element. The offset refers to the children or if
	 * it's a text node, the characters in the text.
	 *
	 * @see GetOffset()
	 */
	void SetLogicalPosition(HTML_Element* element, int offset);

	/** Set the logical position just before an element.
	 *
	 * @param element The element before which selection point should point to.
	 */
	void SetLogicalPositionBeforeElement(HTML_Element* element);

	/** Set the logical position just after an element.
	 *
	 * @param element The element after which selection point should point to.
	 */
	void SetLogicalPositionAfterElement(HTML_Element* element);

	/**
	 * If a selection point has multiple visual positions (BiDi and line breaks
	 * make that happen), then this flag says which is preferred in rendering.
	 *
	 * For a line break, if the rendering should be on the previous line, the
	 * direction should be BIND_BACKWARD. If the rendering should be at the start
	 * of the new line, then the direction should be BIND_FORWARD.
	 *
	 * @param[in] direction The new bind direction.
	 */
	void SetBindDirection(BindDirection direction) { m_bind_forward = (direction == BIND_FORWARD); }

	/**
	 * If a selection point has multiple visual positions (BiDi and line breaks
	 * make that happen), then this flag says which is preferred in rendering.
	 *
	 * @returns The direction. The default is BIND_BACKWARD.
	 */
	BindDirection GetBindDirection() const { return m_bind_forward ? BIND_FORWARD : BIND_BACKWARD; }

	/**
	 * Get character offset in text element. backwards compatible
	 * name. Use GetOffset() in new code.
	 */
	int GetElementCharacterOffset() const { return GetOffset(); }

	/**
	 * Does this text selection point precede other selection point?  Slow
	 * method if called often (for every element or similar).
	 */
	BOOL Precedes(const SelectionBoundaryPoint& other_selection) const;

private:
	static int CalculateOffset(HTML_Element* element);
};

#endif // !SELECTIONPOINT_H
