/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2001 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MACOPDRAGOBJECT_H
#define MACOPDRAGOBJECT_H

#include "adjunct/desktop_pi/DesktopDragObject.h"

class MacOpDragObject : public DesktopDragObject
{
private:
	MacOpDragObject();

public:
	MacOpDragObject(OpTypedObject::Type type);
	~MacOpDragObject();

	virtual DropType	GetSuggestedDropType() const;

private:

};

#endif // MACOPDRAGOBJECT_H
