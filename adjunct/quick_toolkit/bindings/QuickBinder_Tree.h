/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef QUICK_BINDER_TREE_H
#define QUICK_BINDER_TREE_H

#include "adjunct/quick_toolkit/bindings/QuickBinder.h"
#include "modules/widgets/OpWidget.h"

class OpTreeModel;
class QuickTreeView;

/**
 * A unidirectional (model->view) binding for the tree model of a QuickTreeView.
 *
 * The model displayed by the widget is bound to a an OpTreeModel pointer
 * OpProperty.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class QuickBinder::QuickTreeViewModelBinding : public QuickBinder::Binding
{
public:
	QuickTreeViewModelBinding();
	virtual ~QuickTreeViewModelBinding();

	OP_STATUS Init(QuickTreeView& tree_view, OpProperty<OpTreeModel*>& property);

	void OnPropertyChanged(OpTreeModel* new_value);

	// From Binding
	virtual const QuickWidget* GetBoundWidget() const;

private:
	QuickTreeView* m_tree_view;
	OpProperty<OpTreeModel*>* m_property;
};



/**
 * A bidirectional binding for a single selection in a QuickTreeView.
 *
 * The index of the selected item in the model is bound to an integer
 * OpProperty.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class QuickBinder::QuickTreeViewSelectionBinding
		: public QuickBinder::Binding
		, public OpWidgetListener
{
public:
	QuickTreeViewSelectionBinding();
	virtual ~QuickTreeViewSelectionBinding();

	OP_STATUS Init(QuickTreeView& tree_view, OpProperty<INT32>& property);

	void OnPropertyChanged(INT32 new_value);

	// From Binding
	virtual const QuickWidget* GetBoundWidget() const;

	// OpWidgetListener
	virtual void OnChange(OpWidget* widget, BOOL by_mouse = FALSE);
	virtual void OnSortingChanged(OpWidget* widget);
	virtual void OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y) {}

private:
	QuickTreeView* m_tree_view;
	OpProperty<INT32>* m_property;
};



/**
 * A unidirectional (view->model) binding for a multiple selection in a
 * QuickTreeView.
 *
 * The indices of the selected items in the model are bound to an int-vector
 * OpProperty.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class QuickBinder::QuickTreeViewMultiSelectionBinding
		: public QuickBinder::Binding
		, public OpWidgetListener
{
public:
	QuickTreeViewMultiSelectionBinding();
	virtual ~QuickTreeViewMultiSelectionBinding();

	OP_STATUS Init(QuickTreeView& tree_view, OpProperty<OpINT32Vector>& property);

	// From Binding
	virtual const QuickWidget* GetBoundWidget() const;

	// OpWidgetListener
	virtual void OnChange(OpWidget* widget, BOOL by_mouse = FALSE);

private:
	QuickTreeView* m_tree_view;
	OpProperty<OpINT32Vector>* m_property;
};

#endif // QUICK_BINDER_TREE_H
