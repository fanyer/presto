/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * Author: Adam Minchinton
 */

#ifndef OP_PAGEBARITEM_H
#define OP_PAGEBARITEM_H

#include "modules/widgets/OpButton.h"

////////////////////////////////////////////////////////////////////////////////////////

class OpPagebarItem : public OpButton
{
protected:
	OpPagebarItem(INT32 id, UINT32 group_number) : m_group_number(group_number) { SetID(id); }
	virtual ~OpPagebarItem() {}

public:
	virtual BOOL	IsOfType(Type type) { return type == WIDGET_TYPE_PAGEBAR_ITEM || OpButton::IsOfType(type); }
	
	virtual UINT32	GetGroupNumber(BOOL prefer_original_number = FALSE) { return m_group_number; }
	virtual void	SetGroupNumber(UINT32 group_number, BOOL update_active_and_hidden = TRUE) { m_group_number = group_number; }

	

private:
	UINT32				m_group_number; // Group using this button
};

#endif // OP_PAGEBARITEM_H
