/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef EDIT_H
#define EDIT_H

#include "modules/widgets/OpEdit.h"

class OpTreeView;

/**
 * @brief
 * @author Rikard Gillemyr
 *
 *
 */

class Edit
	: public OpEdit
{
public:

//  -------------------------
//  Public member functions:
//  -------------------------

	/**
	 *
	 * @param tree_view
	 */
    Edit(OpTreeView* tree_view);

	/**
	 *
	 * @param focus
	 * @param reason
	 */
    virtual void OnFocus(BOOL focus,
						 FOCUS_REASON reason);

	/**
	 *
	 * @param action
	 *
	 * @return
	 */
    virtual BOOL OnInputAction(OpInputAction* action);

protected:

//  -------------------------
//  Protected member functions:
//  -------------------------

    virtual ~Edit() {}

private:

//  -------------------------
//  Private member variables:
//  -------------------------

    OpTreeView* m_tree_view;
};

#endif // EDIT_H
