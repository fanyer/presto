/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef GENERIC_GRID_H
#define GENERIC_GRID_H

#include "adjunct/quick_toolkit/widgets/QuickGrid/DefaultGridSizer.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/GridCellContainer.h"
#include "adjunct/quick_toolkit/widgets/QuickWidget.h"
#include "adjunct/quick_toolkit/widgets/QuickWidgetContainer.h"

class GridCell;
class GridLayouter;
class GridSizer;
class OpButtonGroup;

/** @brief Base class for layout widgets that can be represented as a grid
  * For implementations, see QuickGrid, QuickStackLayout and others
  */
class GenericGrid
		: public QuickWidget
		, public QuickWidgetContainer
		, public GridCellContainer
{
	public:
		GenericGrid();
		virtual ~GenericGrid();

		/** Center elements horizontally (false by default)
		  */
		void SetCenterHorizontally(bool center_horizontally) { m_center_horizontally = center_horizontally; }
		/** Center elements vertically (true by default)
		  */
		void SetCenterVertically(bool center_vertically) { m_center_vertically = center_vertically; }

		/** Make all cells the same size or not (false by default).
		  *
		  * All cells will get the width of the widest widget, the height of
		  * the highest widget, and the margins of the widget with the largest
		  * margins.
		  */
		OP_STATUS SetUniformCells(bool uniform_cells);

		// Override QuickWidget
		virtual OP_STATUS Layout(const OpRect& rect);
		virtual void SetParentOpWidget(class OpWidget* parent);
		virtual void Show();
		virtual void Hide();
		virtual BOOL IsVisible();
		virtual void SetEnabled(BOOL enabled);

		// Override QuickWidgetContainer
		virtual void OnContentsChanged();

		OP_STATUS RegisterToButtonGroup(OpButton* button);

	protected:
		/** @return A grid layouter to use when layouting grid */
		virtual GridLayouter* CreateLayouter();

		/** Invalidate the contents of this grid (call when changing contents) */
		void Invalidate(BOOL propagate = TRUE);

		void SetCurrentParentOpWidget(OpWidget* parent) { m_parent_op_widget = parent; }
		OpWidget*    GetParentOpWidget() const { return m_parent_op_widget; }

	private:
		enum SizeType
		{
			MINIMUM,
			NOMINAL,
			PREFERRED,
			SIZE_COUNT
		};

		struct SizeInfo
		{
			unsigned width[SIZE_COUNT];
			unsigned height[SIZE_COUNT];
		};

		// Override QuickWidget
		virtual unsigned GetDefaultMinimumWidth() { return GetDefaultWidth(MINIMUM); }
		virtual unsigned GetDefaultMinimumHeight(unsigned width) { return GetDefaultHeight(MINIMUM, width); }
		virtual unsigned GetDefaultNominalWidth() { return GetDefaultWidth(NOMINAL); }
		virtual unsigned GetDefaultNominalHeight(unsigned width) { return GetDefaultHeight(NOMINAL, width); }
		virtual unsigned GetDefaultPreferredWidth() { return GetDefaultWidth(PREFERRED); }
		virtual unsigned GetDefaultPreferredHeight(unsigned width) { return GetDefaultHeight(PREFERRED, width); }
		virtual void GetDefaultMargins(WidgetSizes::Margins& margins) { if (!m_valid_defaults) CalculateDefaults(); margins = m_default_margins; }
		virtual BOOL HeightDependsOnWidth() { if (!m_valid_defaults) CalculateDefaults(); return m_height_depends_on_width; }

		unsigned GetDefaultWidth(SizeType type) { if (!m_valid_defaults) CalculateDefaults(); return m_default_size.width[type]; }
		unsigned GetDefaultHeight(SizeType type, unsigned width);
		void	 CalculateDefaults();
		bool	 CheckHeightDependsOnWidth();

		/**
		 * Set up a grid border painter provided layout debugging is turned on.
		 *
		 * Feeds an OpRectPainter with the layout results (a series of OpRects)
		 * and attaches it to the grid's parent OpWidget.
		 */
		OP_STATUS PrepareRectPainter(const OpRect& rect, GridLayouter& layouter);
		class OpRectPainter* GetRectPainter();

		bool		m_visible:1;
		bool		m_valid_defaults:1;
		bool		m_height_depends_on_width:1;
		bool		m_center_horizontally:1;
		bool		m_center_vertically:1;
		OpWidget* 	m_parent_op_widget;

		OpButtonGroup* m_button_group;

		DefaultGridSizer m_default_sizer;
		GridSizer* m_sizer;

		WidgetSizes::Margins m_default_margins;		///<Cached margins calculated from cell properties
		SizeInfo	m_default_size;					///<Cached default sizes calculated from cell properties

		static const size_t CACHED_SIZES = 3;
		SizeInfo	m_cached_size[CACHED_SIZES];	///<Cached sizes calculated from cell properties
		unsigned char m_cached_index[SIZE_COUNT];   ///<Index into the buffer m_cached_size for each size type, points to next place to use
};


#endif // GENERIC_GRID_H
