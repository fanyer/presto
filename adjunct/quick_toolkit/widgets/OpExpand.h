/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * Author: Adam Minchinton
 */

#ifndef OP_EXPAND_H
#define OP_EXPAND_H

#include "modules/widgets/OpButton.h"

class OpLabel;

////////////////////////////////////////////////////////////////////////////////////////

class OpExpand : public OpButton
{
protected:
	OpExpand();
	~OpExpand() {}

public:
	static OP_STATUS	Construct(OpExpand** obj);

	virtual	Type		GetType() { return WIDGET_TYPE_EXPAND; }

	void				SetShowHoverEffect(BOOL show);

	virtual BOOL		OnInputAction(OpInputAction* action);

	virtual const char*	GetInputContextName() {return "Expand Widget";}

	virtual void		OnLayout();

private:
	OpString8	m_original_icon;		// Name of the original icon set before the busy state
};

#endif // OP_EXPAND_H
