/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/OpTreeView/Edit.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"

Edit::Edit(OpTreeView* tree_view) : m_tree_view(tree_view)
{
    OpEdit::SetAction(OP_NEW(OpInputAction, (OpInputAction::ACTION_EDIT_ITEM)));
}

void Edit::OnFocus(BOOL focus,FOCUS_REASON reason)
{
    OpEdit::OnFocus(focus,reason);

    if (!focus)
    {
		m_tree_view->FinishEdit();
    }
}

BOOL Edit::OnInputAction(OpInputAction* action)
{
    switch (action->GetAction())
    {
		case OpInputAction::ACTION_LOWLEVEL_KEY_PRESSED:
		{
			if (action->GetActionData() == OP_KEY_ESCAPE)
			{
				m_tree_view->CancelEdit(TRUE);
				return TRUE;
			}
			break;
		}
    }

	BOOL val = OpEdit::OnInputAction(action);

	if (!val)
	{
		// Block all actions we do not want to reach OpTreeView while 
		// the edit field is visible (those will cancel the edit field)
		// Basically all movement actions

		if (action->IsRangeAction() || action->IsMoveAction())
			val = TRUE;
		else
		{
			switch (action->GetAction())
			{
				case OpInputAction::ACTION_GO_TO_PARENT_DIRECTORY:
					// Add specific actions here
					val = TRUE;
				break;
			}
		}
	}

	return val;
}
