// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
/** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Owner:    Alexander Remen (alexr)
 */

#ifndef OPUNREADBADGE_H
#define OPUNREADBADGE_H

#include "modules/widgets/OpButton.h"

class OpUnreadBadge: public OpButton
{
public:
	static OP_STATUS Construct(OpUnreadBadge** obj);

	virtual void	GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);

	virtual void OnMouseMove(const OpPoint &point);
	virtual void OnMouseLeave();
	virtual void OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks);
	virtual void OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks);

private:
	OpUnreadBadge();
	
	INT32	m_minimum_width;
};

#endif // OPUNREADBADGE_H