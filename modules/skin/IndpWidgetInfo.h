/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef INDP_WIDGET_INFO_H
#define INDP_WIDGET_INFO_H

#include "modules/widgets/OpWidget.h"

class IndpWidgetInfo : public OpWidgetInfo
{
public:
	virtual void GetPreferedSize(OpWidget* widget, OpTypedObject::Type type, INT32* w, INT32* h, INT32 cols, INT32 rows);
	virtual void GetMinimumSize(OpWidget* widget, OpTypedObject::Type type, INT32* minw, INT32* minh);
	virtual INT32 GetDropdownButtonWidth(OpWidget* widget);
	virtual INT32 GetDropdownLeftButtonWidth(OpWidget* widget);
	virtual void GetBorders(OpWidget* widget, INT32& left, INT32& top, INT32& right, INT32& bottom);
};

#endif // INDP_WIDGET_INFO_H
