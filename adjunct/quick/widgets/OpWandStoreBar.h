/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_WANDSTORETOOLBAR_H
#define OP_WANDSTORETOOLBAR_H

#include "adjunct/quick_toolkit/widgets/OpToolbar.h"

class WandInfo;

/***********************************************************************************
**
**	OpWandStoreBar
**
**  Toolbar that asks user if password data show be saved or not.
**  Automatically dissapears after a time if not used.
**
***********************************************************************************/

class OpWandStoreBar : public OpToolbar
{
protected:
	OpWandStoreBar();
	~OpWandStoreBar();

public:
	static OP_STATUS	Construct(OpWandStoreBar** obj);

	void Show(WandInfo* info);
	void Hide(BOOL focus_page = TRUE);

	void RestartHideTimer();

	// == Hooks ======================

	virtual void		OnAlignmentChanged();

	// == OpInputContext ======================

	virtual BOOL			OnInputAction(OpInputAction* action);
	virtual const char*		GetInputContextName() {return "Wand Store Bar";}

	// OpTimerListener

	virtual void			OnTimeOut(OpTimer* timer);
private:
	WandInfo* m_info;
	OpTimer* m_timer;
};

#endif // OP_WANDSTORETOOLBAR_H
