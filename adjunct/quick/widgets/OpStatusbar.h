/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_STATUSBAR_H
#define OP_STATUSBAR_H

#include "modules/widgets/OpWidget.h"
#include "adjunct/quick_toolkit/widgets/OpToolbar.h"


/***********************************************************************************
**
**	OpStatusbar
**
**  OpStatusbar is a toolbar with head- and tail-toolbars (added as
**  children, not as widgets) and layouted within OnLayout())
**
***********************************************************************************/

class OpStatusbarHeadTail : public OpToolbar
{
	public:
		static OP_STATUS	Construct(OpStatusbarHeadTail** obj);
		virtual void OnContentSizeChanged();
};

class OpStatusbar : 
	public OpToolbar
{
	protected:
		OpStatusbar();

		OP_STATUS InitStatusbar();

	public:

		static OP_STATUS	Construct(OpStatusbar** obj);

		// Subclassing OpBar

		virtual BOOL		SetAlignment(Alignment alignment, BOOL write_to_prefs = FALSE);

		// Hooks

		virtual void		OnRelayout();
		virtual void		OnLayout();
		virtual void		GetPadding(INT32* left, INT32* top, INT32* right, INT32* bottom);

		// == OpInputContext ======================
		virtual BOOL		OnInputAction(OpInputAction* action);
		virtual const char*	GetInputContextName() {return "Statusbar";}


		virtual void		UpdateHeadTailSize();

		virtual void		EnableTransparentSkin(BOOL enable);
	protected:
		void	SetHeadTailAlignmentFromStatusbar(Alignment pagebar_alignment);

	protected:
		OpStatusbarHeadTail*		m_head_bar;
		OpStatusbarHeadTail*		m_tail_bar;
};

#endif // OP_STATUSBAR_H
