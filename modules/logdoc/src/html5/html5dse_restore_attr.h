/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef HTML5DSE_RESTORE_ATTR_H_
#define HTML5DSE_RESTORE_ATTR_H_

#ifdef DELAYED_SCRIPT_EXECUTION

/**
 * With the HTML 5 parser elements may be moved around by the parsing algorithm
 * after they've been inserted (foster parenting).
 * If such a move happens after the point where we store the state, then we need
 * to move the elements back where they were.  This attribute remembers where the
 * element used to be by storing the parent it used to have.
 */
class HTML5DSE_RestoreAttr : public ComplexAttr
{
public:
	HTML5DSE_RestoreAttr(HTML5ELEMENT* parent) : m_parent(parent) {}

	/**
	 * @return The parent the element had before it was moved by the HTML parser
	 */
	HTML5ELEMENT*	GetParent() { return m_parent.GetElm(); } 
private:
	AutoNullElementRef		m_parent;
};

#endif // DELAYED_SCRIPT_EXECUTION

#endif /* HTML5DSE_RESTORE_ATTR_H_ */
