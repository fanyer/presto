/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef NULL_WIDGET_H
#define NULL_WIDGET_H

#include "adjunct/quick_toolkit/widgets/QuickWidget.h"

/** @brief An empty widget
 */
class NullWidget : public QuickWidget
{
	IMPLEMENT_TYPEDOBJECT(QuickWidget);
public:
	OP_STATUS Init() { return OpStatus::OK; }
	virtual unsigned GetDefaultMinimumWidth() { return 0; }
	virtual unsigned GetDefaultMinimumHeight(unsigned width) { return 0; }
	virtual unsigned GetDefaultPreferredWidth() { return 0; }
	virtual unsigned GetDefaultPreferredHeight(unsigned width) { return 0; }
	virtual unsigned GetDefaultNominalWidth() { return 0; }
	virtual unsigned GetDefaultNominalHeight(unsigned width) { return 0; }
	virtual void GetDefaultMargins(WidgetSizes::Margins& margins) {}
	virtual unsigned GetDefaultVerticalMargin() { return 0; }
	virtual BOOL HeightDependsOnWidth() { return FALSE; }
	virtual OP_STATUS Layout(const OpRect& rect) { return OpStatus::OK; }
	virtual void SetParentOpWidget(class OpWidget* parent) {}
	virtual void Show() { }
	virtual void Hide() { }
	virtual BOOL IsVisible() { return TRUE; }
	virtual void SetEnabled(BOOL enabled) { }
};

#endif // NULL_WIDGET_H
