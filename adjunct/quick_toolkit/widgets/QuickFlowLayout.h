/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef QUICK_FLOW_LAYOUT_H
#define QUICK_FLOW_LAYOUT_H

#include "adjunct/quick_toolkit/widgets/QuickWidget.h"
#include "adjunct/quick_toolkit/widgets/QuickWidgetContainer.h"
#include "adjunct/quick/managers/AnimationManager.h"
#include "modules/util/adt/opvector.h"

/** @brief A grid where rows and columns are determined by available size
  * This grid makes cells that are all the same size (governed by the largest
  * widget inserted).
  */
class QuickFlowLayout : public QuickWidget, public QuickWidgetContainer, public QuickAnimationListener
{
	IMPLEMENT_TYPEDOBJECT(QuickWidget);
public:
	enum TestState
	{
		NORMAL,			///< Use Opera settings
		FORCE_TESTABLE,	///< Force testability properties on
	};

	QuickFlowLayout();
	~QuickFlowLayout();
	
	OP_STATUS Init(TestState teststate = NORMAL);

	/**
	  * Inserts a widget into the layout
	  * @param widget Pointer to the widget to be added (takes ownership)
	  * @param pos The position to add, or -1 to add to the end
	  */
	OP_STATUS InsertWidget(QuickWidget* widget, int pos = -1);

	/** Move widget to new position
	  * @param from_pos Current position of widget
	  * @param to_pos Desired position of widget
	  */
	OP_STATUS MoveWidget(unsigned from_pos, unsigned to_pos);

	/** Finds given widget in the layout
	  * @param widget Widget to be found
	  * 
	  * @return position of widget if found in layout, otherwise -1
	  */
	INT32 FindWidget(QuickWidget* widget) const { return m_cells.Find(widget); }

	/** Remove and destroy a widget
	  * @param pos Position of widget to remove
	  */
	void RemoveWidget(unsigned pos);

	/** Remove and destroy all widgets
	  */
	void RemoveAllWidgets();

	/** Retrieve a widget
	  * @param pos Position of widget to retrieve
	  * @return Widget at position pos
	  */
	QuickWidget* GetWidget(unsigned pos) { return m_cells.Get(pos); }

	/** @return number of widgets in this layout
	  */
	unsigned GetWidgetCount() { return m_cells.GetCount(); }

	/** Set a point before which the flow should never break
	  * @param count Minimum number of items to be used per row, or 0 for no minimum
	  * @note Minimum break should not be set larger than maximum break
	  */
	void SetMinBreak(unsigned count);

	/** Set a point where the flow should always break
	  * @param count Maximum number of items to be used per row, or 0 for no maximum
	  * @note Maximum break should not be set smaller than minimum break
	  */
	void SetMaxBreak(unsigned count);

	/** Set a fixed breaking point for the flow
	  * @param count Number of items to be used per row
	  */
	void SetHardBreak(unsigned count);

	/** Reset any breaking points set earlier.
	 */
	void ResetBreaks() { m_min_break = 0; m_max_break = 0; }

	/** Set the break point so that the preferred width does not exceed @a width
	  * @param width Width to fit into
	  */
	void FitToWidth(unsigned width);

	/** Calculates cell sizes to best fit the window given as parameter.
	  * It looks for the best cells dimensions in the range
	  * [m_min_cell_width, m_preferred_cell_width] and it always keeps cells ratio.
	  * It also respects hard break if it is set.
	  * @param rect window which we want to fit into
	  */
	void FitToRect(const OpRect& rect);

	/** Adds fixed padding to cells width and height when calculating dimensions
	  * in method FitToWindow. It should be used when widgets contain fixed
	  * padding which changes their aspect ratio.
	  * @param width  padding's x value
	  * @param height padding's y value
	  */
	void SetFixedPadding(unsigned width, unsigned height);

	/** When set to true, keep the width/height ratio used in Get*Height()
	  * calculations equal to the ratio of the calculated minimum width/height
	  * of the cells
	  * @param keep_ratio Whether to take the ratio of minimum size into account
	  * during size calculations
	  */
	void KeepRatio(bool keep_ratio) { m_keep_ratio = keep_ratio; }

	/** Set a name for this widget (used in testability context)
	  * @param name Name to use
	  */
	void SetName(const OpStringC8& name);

	// Override QuickWidget
	virtual OP_STATUS Layout(const OpRect& rect);
	virtual BOOL HeightDependsOnWidth() { return TRUE; }
	virtual void SetParentOpWidget(OpWidget* parent);
	virtual void Show();
	virtual void Hide();
	virtual BOOL IsVisible() { return m_visible; }
	virtual void SetEnabled(BOOL enabled);

	// Override QuickWidgetContainer
	virtual void OnContentsChanged() { Invalidate(); }

	// Override QuickAnimationListener
	virtual void OnAnimationComplete(OpWidget *anim_target, int callback_param) { UpdateZOrder(); }

protected:
	// Override QuickWidget
	virtual unsigned GetDefaultMinimumWidth();
	virtual unsigned GetDefaultMinimumHeight(unsigned width);
	virtual unsigned GetDefaultNominalWidth();
	virtual unsigned GetDefaultNominalHeight(unsigned width);
	virtual unsigned GetDefaultPreferredWidth();
	virtual unsigned GetDefaultPreferredHeight(unsigned width);
	virtual void GetDefaultMargins(WidgetSizes::Margins& margins);

private:
	class BackgroundWidget;
	class ItemWidget;

	struct Dimensions
	{
		Dimensions() : width(0), height(0), fit_the_window(false) {}
		unsigned width;
		unsigned height;
		bool     fit_the_window;
	};

	unsigned GetHeight(unsigned cell_height, unsigned width);
	void CalculateSizes();
	void CalculateSizesToFitRect(const OpRect& rect);
	void CalculateNextCellRect(OpRect& cellrect, unsigned& cellpos, unsigned colbreak, const OpRect& outer_rect) const;
	unsigned CalculateColumnBreak(unsigned width);
	Dimensions CalculateDimensionsAndFillLevel(const OpRect& rect, unsigned cols);
	void UpdateSizes(QuickWidget& cell);
	unsigned GetRowCount(unsigned colbreak) { return (m_visible_cell_count / colbreak) + (m_visible_cell_count % colbreak ? 1 : 0); }
	void Invalidate() { m_valid = false; BroadcastContentsChanged(); }
	unsigned GetMaxBreak() { return max(m_max_break > 0 ? min(m_max_break, m_visible_cell_count) : m_visible_cell_count, 1u); }
	unsigned GetMinBreak() { return max(m_min_break > 0 ? min(m_min_break, m_visible_cell_count) : 1u, 1u); }
	void UpdateZOrder();
	OP_STATUS DoLayout(const OpRect& rect, OpRect cellrect);
	OP_STATUS DoTestLayout(const OpRect& rect, OpRect cellrect);
	OP_STATUS PrepareTestWidget(QuickWidget* widget, int pos);
	OP_STATUS RemoveTestWidget(unsigned pos);
	void RemoveAllTestWidgets();
	OP_STATUS UpdateTestWidgetNames();

	OpAutoVector<QuickWidget> m_cells;
	unsigned m_min_cell_width;
	unsigned m_nominal_cell_width;
	unsigned m_preferred_cell_width;
	unsigned m_min_cell_height;
	unsigned m_nominal_cell_height;
	unsigned m_preferred_cell_height;
	unsigned m_horizontal_margins;
	unsigned m_vertical_margins;
	unsigned m_min_break;
	unsigned m_max_break;
	unsigned m_visible_cell_count;
	unsigned m_fixed_padding_width;
	unsigned m_fixed_padding_height;
	OpWidget* m_parent_op_widget;
	BackgroundWidget* m_background;
	bool m_visible;
	bool m_valid;
	bool m_keep_ratio;
};

#endif // QUICK_FLOW_LAYOUT_H
