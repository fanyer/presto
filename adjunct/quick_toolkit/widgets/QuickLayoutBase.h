/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef QUICKLAYOUTBASE_H
#define QUICKLAYOUTBASE_H

#include "adjunct/quick_toolkit/widgets/QuickWidget.h"
#include "adjunct/quick_toolkit/widgets/QuickWidgetContainer.h"
#include "modules/util/adt/opvector.h"

/** This class is a simple base class that implements default versions
 * of many of the methods needed by a layout manager.
 *
 * This class holds all the contained widgets in an array, and will
 * delete all the widgets when it is deleted itself.
 *
 *
 * The methods that must be implemented by a real layout manager are:
 *
 * public methods inherited from QuickWidget:
 *
 * - virtual OP_STATUS Layout(const OpRect& rect) = 0;
 * - virtual BOOL HeightDependsOnWidth() = 0;
 *
 * private methods inherited from QuickWidget:
 *
 * - virtual unsigned GetDefaultMinimumWidth();
 * - virtual unsigned GetDefaultMinimumHeight(unsigned width);
 * - virtual unsigned GetDefaultNominalWidth();
 * - virtual unsigned GetDefaultNominalHeight(unsigned width);
 * - virtual unsigned GetDefaultPreferredWidth();
 * - virtual unsigned GetDefaultPreferredHeight(unsigned width);
 * - virtual void GetDefaultMargins(WidgetSizes::Margins& margins);
 */
class QuickLayoutBase : public QuickWidget, public QuickWidgetContainer
{
	IMPLEMENT_TYPEDOBJECT(QuickWidget);
public:
	QuickLayoutBase();

	/** Add a widget to this layout.
	 *
	 * Widgets added with this method will have some minimal
	 * management performed automatically.  This includes in
	 * particular some pass-through calls:
	 *
	 * - When the layout manager is deleted, the widget will be deleted.
	 * - The widget's (OpWidget) parent will be the same as the layout manager's.
	 * - When the layout manager is shown/hidden/enabled/disabled so is the widget.
	 *
	 * Will also call OnContentsChanged() after the widget list has
	 * been updated.
	 */
	virtual OP_STATUS InsertWidget(QuickWidget * widget, int pos = -1);

	/** Remove the widget in position 'pos' from the layout's widget
	 * list.  Will also delete the widget.
	 *
	 * Will also call OnContentsChanged() after the widget list has
	 * been updated.
	 */
	virtual void RemoveWidget(UINT32 pos);

	/** Returns the number of widgets in the layout's widget list.
	 */
	virtual UINT32 GetWidgetCount();

	/** Returns the widget in position 'pos' in the layout's widget
	 * list.
	 */
	virtual QuickWidget * GetWidget(UINT32 pos);


	// Override QuickWidget

	virtual void SetParentOpWidget(class OpWidget* parent);
	virtual void Show();
	virtual void Hide();
	virtual BOOL IsVisible() { return m_visible; }
	virtual void SetEnabled(BOOL enabled);


	// Override QuickWidgetContainer
	virtual void OnContentsChanged() { BroadcastContentsChanged(); }

protected:
	OpWidget* GetParentOpWidget() { return m_parent_op_widget; }

private:
	BOOL m_visible;
	OpWidget * m_parent_op_widget;
	OpAutoVector<QuickWidget> m_widgets;

public:
	// helper methods

	/** Returns the minimum and maximum widths of 'widget'.
	 *
	 * As a rule, the minimum width is the widget's GetMinimumWidth()
	 * and the maximum width is the widget's GetPreferredWidth().
	 * However, if GetPreferredWidth() is larger than 'upper_bound',
	 * or it is Fill or Inifinity, 'upper_bound' is returned as the
	 * maximum width instead.
	 */
	void GetWidthRange(QuickWidget * widget, unsigned upper_bound, unsigned * min_width, unsigned * max_width);

	/** Returns the minimum and maximum heights of 'widget', for a
	 * given width.
	 *
	 *  As a rule, the minimum height is the widget's
	 * GetMinimumHeight() and the maximum height is the widget's
	 * GetPreferredHeight().  However, if GetPreferredHeight() is
	 * larger than 'upper_bound', or it is Fill or Inifinity,
	 * 'upper_bound' is returned as the maximum height instead.
	 *
	 * All calls to Get*Height() uses 'width' as the width parameter.
	 */
	void GetHeightRange(QuickWidget * widget, unsigned width, unsigned upper_bound, unsigned * min_height, unsigned * max_height);
};



#endif
