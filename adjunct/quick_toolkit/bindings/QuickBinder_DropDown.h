/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef QUICK_BINDER_DROP_DOWN_H
#define QUICK_BINDER_DROP_DOWN_H

#include "adjunct/quick_toolkit/bindings/QuickBinder.h"
#include "modules/widgets/OpWidget.h"

class QuickDropDown;
namespace PrefUtils { class IntegerAccessor; }

/**
 * A base class for (potentially) bidirectional bindings for the selection in
 * a QuickDropDown.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class QuickBinder::QuickDropDownSelectionBinding
		: public QuickBinder::Binding
		, public OpWidgetListener
{
public:
	// Binding
	virtual const QuickWidget* GetBoundWidget() const;

	// OpWidgetListener
	virtual void OnChange(OpWidget* widget, BOOL by_mouse = FALSE);

protected:
	QuickDropDownSelectionBinding();
	virtual ~QuickDropDownSelectionBinding();

	OP_STATUS Init(QuickDropDown& drop_down);

	void ToWidget(INT32 new_value);
	virtual OP_STATUS FromWidget(INT32 new_value) = 0;

private:
	QuickDropDown* m_drop_down;
};


/**
 * A bidirectional property binding for the selection in a QuickDropDown.
 *
 * The user data of the selected item is bound to an integer OpProperty.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class QuickBinder::QuickDropDownSelectionPropertyBinding : public QuickDropDownSelectionBinding
{
public:
	QuickDropDownSelectionPropertyBinding();
	virtual ~QuickDropDownSelectionPropertyBinding();

	OP_STATUS Init(QuickDropDown& drop_down, OpProperty<INT32>& property);

	void OnPropertyChanged(INT32 new_value) { ToWidget(new_value); }

protected:
	// QuickDropDownSelectionBinding
	virtual OP_STATUS FromWidget(INT32 new_value);

private:
	OpProperty<INT32>* m_property;
};


/**
 * A unidirectional preference binding for the selection in a QuickDropDown.
 *
 * The user data of the selected item is automatically written to an integer
 * preference.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class QuickBinder::QuickDropDownSelectionPreferenceBinding : public QuickDropDownSelectionBinding
{
public:
	QuickDropDownSelectionPreferenceBinding();
	virtual ~QuickDropDownSelectionPreferenceBinding();

	OP_STATUS Init(QuickDropDown& drop_down, PrefUtils::IntegerAccessor* accessor);

protected:
	// QuickDropDownSelectionBinding
	virtual OP_STATUS FromWidget(INT32 new_value);

private:
	PrefUtils::IntegerAccessor* m_accessor;
};

#endif // QUICK_BINDER_DROP_DOWN_H
