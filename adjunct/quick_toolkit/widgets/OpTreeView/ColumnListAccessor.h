/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef COLUMN_LIST_ACCESSOR_H
#define COLUMN_LIST_ACCESSOR_H

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

class OpTreeView;

/**
 * @brief
 * @author Julien Picalausa
 *
 *
 */

class ColumnListAccessor :
	public OpAccessibleItem
{
public:

//  -------------------------
//  Public member functions:
//  -------------------------

	/**
	 *
	 * @param
	 */
	ColumnListAccessor(OpTreeView* parent)
		: m_parent(parent) {}

	/**
	 *
	 *
	 */
	~ColumnListAccessor() {}

	/**
	 *
	 * @param
	 *
	 * @return
	 */
	virtual BOOL AccessibilityIsReady() const {return TRUE;}

	/**
	 *
	 * @param
	 *
	 * @return
	 */
	virtual OP_STATUS AccessibilityGetText(OpString& str)
		{str.Empty(); return OpStatus::ERR; }

	/**
	 *
	 *
	 * @return
	 */
	virtual OP_STATUS AccessibilityGetAbsolutePosition(OpRect &rect);

	/**
	 *
	 *
	 * @return
	 */
	virtual int	GetAccessibleChildrenCount();

	/**
	 *
	 *
	 * @return
	 */
	virtual Accessibility::ElementKind AccessibilityGetRole() const
		{return Accessibility::kElementKindList;}

	/**
	 *
	 *
	 * @return
	 */
	virtual Accessibility::State AccessibilityGetState();

	/**
	 *
	 * @param
	 *
	 * @return
	 */
	virtual OpAccessibleItem* GetAccessibleParent() {return m_parent;}

	/**
	 *
	 * @param
	 *
	 * @return
	 */
	virtual OpAccessibleItem* GetAccessibleChild(int);

	/**
	 *
	 * @param
	 *
	 * @return
	 */
	virtual int GetAccessibleChildIndex(OpAccessibleItem* child);

	/**
	 *
	 *
	 * @return
	 */
	virtual OpAccessibleItem* GetAccessibleFocusedChildOrSelf();

	/**
	 *
	 * @param
	 * @param
	 *
	 * @return
	 */
	virtual OpAccessibleItem* GetAccessibleChildOrSelfAt(int x, int y);

	/**
	 *
	 *
	 * @return
	 */
	virtual OpAccessibleItem* GetNextAccessibleSibling()
		{ return NULL; }

	/**
	 *
	 *
	 * @return
	 */
	virtual OpAccessibleItem* GetPreviousAccessibleSibling()
		{ return NULL; }

	/**
	 *
	 *
	 * @return
	 */
	virtual OpAccessibleItem* GetLeftAccessibleObject()
		{ return NULL; }

	/**
	 *
	 *
	 * @return
	 */
	virtual OpAccessibleItem* GetRightAccessibleObject()
		{ return NULL; }

	/**
	 *
	 *
	 * @return
	 */
	virtual OpAccessibleItem* GetDownAccessibleObject()
		{ return NULL; }

	/**
	 *
	 *
	 * @return
	 */
	virtual OpAccessibleItem* GetUpAccessibleObject()
		{ return NULL; }

private:

//  -------------------------
//  Private member variables:
//  -------------------------

	OpTreeView* m_parent;
};

#endif //ACCESSIBILITY_EXTENSION_SUPPORT

#endif // COLUMN_LIST_ACCESSOR_H
