/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef QUICK_BINDER_ENABLED_H
#define QUICK_BINDER_ENABLED_H

#include "adjunct/quick_toolkit/bindings/QuickBinder.h"


/**
 * A unidirectional binding for the "enabled" state of a QuickWidget.
 *
 * The "enabled" state of a QuickWidget is bound to a boolean OpProperty.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class QuickBinder::EnabledBinding : public QuickBinder::Binding
{
public:
	EnabledBinding();
	virtual ~EnabledBinding();

	OP_STATUS Init(QuickWidget& widget, OpProperty<bool>& property);

	void OnPropertyChanged(bool new_value);

	// Binding
	virtual const QuickWidget* GetBoundWidget() const { return m_widget; }

private:
	QuickWidget* m_widget;
	OpProperty<bool>* m_property;
};

#endif // QUICK_BINDER_ENABLED_H
