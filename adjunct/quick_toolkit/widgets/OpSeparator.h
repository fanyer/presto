/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 */

#ifndef OP_SEPARATOR_H
#define OP_SEPARATOR_H

#include "modules/widgets/OpWidget.h"

class OpSeparator : public OpWidget
{
public:
	static OP_STATUS Construct(OpSeparator** obj) { *obj = OP_NEW(OpSeparator, ()); return *obj ? OpStatus::OK : OpStatus::ERR_NO_MEMORY; }

	OpSeparator()
	{
		GetBorderSkin()->SetImage ( "Horizontal Separator" );
		SetSkinned ( TRUE );
	}

	virtual Type GetType() { return WIDGET_TYPE_SEPARATOR; }
};

#endif // OP_SEPARATOR_H
