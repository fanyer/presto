/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef QUICK_BINDER_NUMBER_EDIT_H
#define QUICK_BINDER_NUMBER_EDIT_H

#include "adjunct/quick_toolkit/bindings/QuickBinder.h"
#include "modules/widgets/OpWidget.h"

class QuickNumberEdit;
namespace PrefUtils { class IntegerAccessor; }


/**
 * A base class of (potentially) bidirectional bindings for the value in a
 * QuickNumberEdit.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class QuickBinder::QuickNumberEditBinding
		: public QuickBinder::Binding, public OpWidgetListener
{
public:
	// Binding
	virtual const QuickWidget* GetBoundWidget() const;

	// OpWidgetListener
	virtual void OnChange(OpWidget* widget, BOOL by_mouse = FALSE);

protected:
	QuickNumberEditBinding();
	virtual ~QuickNumberEditBinding();

	OP_STATUS Init(QuickNumberEdit& number_edit);

	OP_STATUS ToWidget(UINT32 new_value);
	virtual OP_STATUS FromWidget(UINT32 new_value) = 0;

private:
	QuickNumberEdit* m_number_edit;
};


/**
 * A bidirectional property binding for the value in a QuickNumberEdit.
 *
 * The value entered in a QuickNumberEdit is bound to an integer OpProperty.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class QuickBinder::QuickNumberEditPropertyBinding : public QuickNumberEditBinding
{
public:
	QuickNumberEditPropertyBinding();
	virtual ~QuickNumberEditPropertyBinding();

	OP_STATUS Init(QuickNumberEdit& number_edit, OpProperty<UINT32>& property);

	void OnPropertyChanged(UINT32 new_value);

protected:
	// QuickNumberEditBinding
	virtual OP_STATUS FromWidget(UINT32 new_value);

private:
	OpProperty<UINT32>* m_property;
};


/**
 * A unidirectional preference binding for the value in a QuickNumberEdit.
 *
 * The value entered in a QuickNumberEdit is automatically written to an
 * integer preference.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class QuickBinder::QuickNumberEditPreferenceBinding : public QuickNumberEditBinding
{
public:
	QuickNumberEditPreferenceBinding();
	virtual ~QuickNumberEditPreferenceBinding();

	OP_STATUS Init(QuickNumberEdit& number_edit, PrefUtils::IntegerAccessor* accessor);

protected:
	// QuickNumberEditBinding
	virtual OP_STATUS FromWidget(UINT32 new_value);

private:
	PrefUtils::IntegerAccessor* m_accessor;
};

#endif // QUICK_BINDER_NUMBER_EDIT_H
