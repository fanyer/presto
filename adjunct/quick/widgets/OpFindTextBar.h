/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_FINDTEXTTOOLBAR_H
#define OP_FINDTEXTTOOLBAR_H

#include "adjunct/quick_toolkit/widgets/OpToolbar.h"
#include "modules/util/adt/opvector.h"

class FindTextManager;

/***********************************************************************************
**
**	OpFindTextBar
**
***********************************************************************************/

class OpFindTextBar : public OpToolbar
{
protected:
	OpFindTextBar();

public:
	static OP_STATUS	Construct(OpFindTextBar** obj);

	void SetInfoLabel(const uni_char *text);
	void ClearStatus();

	void UpdateFindTextManager();

	virtual BOOL SetAlignmentAnimated(Alignment alignment, double animation_duration = -1);

	// == Hooks ======================

	virtual void		OnFocus(BOOL focus,FOCUS_REASON reason);
	virtual void		OnKeyboardInputGained(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason);
	virtual void		OnChange(OpWidget *widget, BOOL changed_by_mouse);
	virtual void		OnAlignmentChanged();
	virtual void		OnReadContent(PrefsSection *section);

	// == OpInputContext ======================

	virtual BOOL			OnInputAction(OpInputAction* action);
	virtual const char*		GetInputContextName() {return "FindTextBar";}

	virtual	Type			GetType() {return WIDGET_TYPE_FINDTEXTBAR;}
private:
	OpWidget *m_match_case;
	OpWidget *m_match_word;
};

#endif // OP_FINDTEXTTOOLBAR_H
