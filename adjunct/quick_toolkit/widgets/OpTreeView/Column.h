/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef COLUMN_H
#define COLUMN_H

#include "modules/widgets/OpButton.h"

class OpTreeView;

/**
 * @brief A class representing a column
 * @author
 *
 *
 */

class OpTreeView::Column :
	public OpButton
{
public:

//  -------------------------
//  Public member functions:
//  -------------------------

	/**
	 *
	 * @param tree_view
	 * @param index
	 */
	Column(OpTreeView* tree_view,
		   INT32 index);

	/**
	 *
	 * @param point
	 */
	virtual void OnMouseMove(const OpPoint &point);

	/**
	 *
	 *
	 */
	virtual void OnMouseLeave();

	/**
	 *
	 * @param point
	 * @param button
	 * @param nclicks
	 */
	virtual void OnMouseDown(const OpPoint &point,
							 MouseButton button,
							 UINT8 nclicks);

	/**
	 *
	 * @param point
	 * @param button
	 * @param nclicks
	 */
	virtual void OnMouseUp(const OpPoint &point,
						   MouseButton button,
						   UINT8 nclicks);

	/**
	 *
	 * @param point
	 */
	virtual void OnSetCursor(const OpPoint &point);

	/**
	 *
	 * @param visible
	 *
	 * @return
	 */
	BOOL SetColumnVisibility(BOOL visible)
		{
			if (m_is_column_visible == visible)
				return FALSE; m_is_column_visible = visible;
			return m_is_column_user_visible;
		}

	/**
	 *
	 * @param visible
	 *
	 * @return
	 */
	BOOL SetColumnUserVisibility(BOOL visible)
		{
			if (m_is_column_user_visible == visible)
				return FALSE;
			m_is_column_user_visible = visible;
			return m_is_column_visible;
		}

	/**
	 *
	 * @param matchability
	 */
	void SetColumnMatchability(BOOL matchability)
		{ m_is_column_matchable = matchability; }

	/**
	 *
	 *
	 * @return
	 */
	BOOL IsColumnVisible()
		{return m_is_column_visible;}

	/**
	 *
	 *
	 * @return
	 */
	BOOL IsColumnUserVisible()
		{return m_is_column_user_visible;}

	/**
	 *
	 *
	 * @return
	 */
	BOOL IsColumnReallyVisible()
		{return m_is_column_visible && m_is_column_user_visible;}

	/**
	 *
	 *
	 * @return
	 */
	BOOL IsColumnMatchable()
		{return m_is_column_matchable;}


//  -------------------------
//  Public member variables:
//  -------------------------

	OpTreeView*		m_tree_view;
	INT32			m_column_index;
	INT32			m_weight;
	INT32			m_fixed_width;
	INT32			m_width;
	BOOL			m_has_dropdown;
	INT32           m_image_fixed_width;
	BOOL            m_fixed_image_drawarea;

private:

//  -------------------------
//  Private member variables:
//  -------------------------

	BOOL			m_is_column_visible;
	BOOL			m_is_column_user_visible;
	BOOL			m_is_column_matchable;

};

#endif // COLUMN_H
