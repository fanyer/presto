/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef QUICK_BINDER_SELECTABLE_H
#define QUICK_BINDER_SELECTABLE_H

#include "adjunct/quick_toolkit/bindings/QuickBinder.h"
#include "modules/widgets/OpWidget.h"

class QuickSelectable;

/**
 * A base class of (potentially) bidirectional bindings for the state of
 * a QuickSelectable.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class QuickBinder::QuickSelectableBinding
		: public QuickBinder::Binding
		, public OpWidgetListener
{
public:
	// Binding
	virtual const QuickWidget* GetBoundWidget() const;

	// OpWidgetListener
	virtual void OnChange(OpWidget* widget, BOOL by_mouse = FALSE);

protected:
	QuickSelectableBinding();
	virtual ~QuickSelectableBinding();

	OP_STATUS Init(QuickSelectable& selectable);

	OP_STATUS ToWidget(bool new_value);
	virtual OP_STATUS FromWidget(bool new_value) = 0;

private:
	QuickSelectable* m_selectable;
};


/**
 * A bidirectional binding for the state of a QuickSelectable.
 *
 * The "selected" state of a QuickSelectable is bound to a boolean OpProperty.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class QuickBinder::QuickSelectablePropertyBinding : public QuickSelectableBinding
{
public:
	QuickSelectablePropertyBinding();
	virtual ~QuickSelectablePropertyBinding();

	OP_STATUS Init(QuickSelectable& selectable, OpProperty<bool>& property);

	void OnPropertyChanged(bool new_value);

protected:
	// QuickSelectableBinding
	virtual OP_STATUS FromWidget(bool new_value);

private:
	OpProperty<bool>* m_property;
};


/**
 * A unidirectional preference binding for the state of a QuickSelectable.
 *
 * The "selected" state of a QuickSelectable is automatically written to an
 * integer preference.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class QuickBinder::QuickSelectablePreferenceBinding : public QuickSelectableBinding
{
public:
	QuickSelectablePreferenceBinding();
	virtual ~QuickSelectablePreferenceBinding();

	OP_STATUS Init(QuickSelectable& selectable, PrefUtils::IntegerAccessor* accessor, int selected, int deselected);

protected:
	// QuickSelectableBinding
	virtual OP_STATUS FromWidget(bool new_value);

private:
	PrefUtils::IntegerAccessor* m_accessor;
	int m_selected;
	int m_deselected;
};

#endif // QUICK_BINDER_SELECTABLE_H
