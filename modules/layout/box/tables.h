/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

/** @file tables.h
 *
 * Table box and content class prototypes for document layout.
 *
 * @author Geir Ivarsøy
 * @author Karl Anders Øygard
 *
 */

#ifndef _TABLES_H_
#define _TABLES_H_

#include "modules/layout/box/blockbox.h"

class TableContent;
class TableRowBox;
class TableCaptionBox;
class TableColGroupBox;
class TableCellBox;
class PaintObject;
class LayoutWorkplace;

enum FixedLayout
{
	FIXED_LAYOUT_OFF = 0,
	FIXED_LAYOUT_ON = 1,
	FIXED_LAYOUT_COMPLETED = 2
};

enum TRUE_TABLE_MODE
{
	UNDETERMINED,
	NOT_TRUE_TABLE,
	TRUE_TABLE,
	SMALL_TABLE
};

/** Information (mostly widths) about one column in a table. */

class TableColumnInfo
{
public:

				TableColumnInfo()
				  : column_box(NULL),
					packed_init(0),
					packed2_init(0),
					desired_width(0),
					max_width(0),
					normal_min_width(0),
					min_width(0),
					width(1000)
				{
					REPORT_MEMMAN_INC(sizeof(TableColumnInfo));
					packed.is_auto = 1;
				}

				~TableColumnInfo() { REPORT_MEMMAN_DEC(sizeof(TableColumnInfo)); }

	/** Reset all width information (min/max/percentage/desired) but the actual width currently used. */

	void		ResetWidths() { packed_init = 0; desired_width = LayoutCoord(0); packed.is_auto = 1; min_width = LayoutCoord(0); normal_min_width = LayoutCoord(0); max_width = LayoutCoord(0); }

	/** Is this a preferred recipient of extra space?

		A preferred expandee is normally a column without desired width or
		percentage.

		@param min_width_required If TRUE, minimum width of the column must be
		non-zero in order to count as a preferred expandee. Typically the case
		for automatic table layout.
		@param sloppy_desired_width If TRUE, don't disqualify as expandee if
		maximum width exceeds desired width. */

	BOOL		IsPreferredExpandee(BOOL min_width_required, BOOL sloppy_desired_width) const;

	/** @return TRUE if visibility of the table-column or table-column-group box defining this column is collapsed. */

	BOOL		IsVisibilityCollapsed() const;

	/** The column/column-group box (if any) defining this column. */

	TableColGroupBox*
				column_box;

	union
	{
		struct
		{
			/** Percent (in layout fixed point format). It is enough to store
				about 5000% currently (2^19 / LAYOUT_FIXED_POINT_BASE).

				Percentage is enabled by has_percent, meaning that percent=0
				and has_percent=1 counts as percentage. */

			unsigned int	percent:19;

			/** Is this a column with percentage? Note that even if it is,
				'percent' may still be 0, due to rounding. */

			unsigned int	has_percent:1;

			/** Is this a preferred expandee, regardless of what the other
				members might suggest? */

			unsigned int	preferred_expandee:1;

			/** Does desired_width = 0 mean 0px or auto? */

			unsigned int	is_auto:1;

			// room for 10 more bits here

		} packed;
		UINT32	packed_init;
	};

	union
	{
		struct
		{
			/** Active rowspans (state information) */

			unsigned int	rowspans:16;

			/** Cells in this column that is a True Table candidate (max 0x03ff) */

			unsigned int	true_table_cells:10;

			/** Cells in this column that is not a True Table candidate (max 0x003f) */

			unsigned int	non_true_table_cells:6;

		} packed2;
		UINT32	packed2_init;
	};

	// NOTE: We can probably join 'desired_width' and 'percent'.

	/** Desired width. 0 means no desired width. */

	LayoutCoord	desired_width;

	/** Maximum width. Always larger than or equal to normal_min_width.

		The column will not need more space than this. This number will
		normally be the width of the widest cell when ignoring implicit line
		breaks, plus the largest padding and border of any cell in this
		column. However, note that for a cell with a maximum width exceeding
		its desired width, its maximum width will be ignored, and only its
		desired width may contribute to column maximum width. */

	LayoutCoord	max_width;

	/** Normal minimum width. Always larger than or equal to min_width.

		In normal layout mode, this will always be equal to min_width, but in
		ERA mode it may be larger, since ERA may squeeze min_width
		aggressively. However, ERA may still need to know what would have been
		the minimum width in normal layout mode. */

	LayoutCoord	normal_min_width;

	/** Minimum width.

		The column cannot be narrower than this. This number will normally be
		the width of the widest unbreakable piece of content of any cell in
		this column, plus the largest padding and border of any cell in this
		column. */

	LayoutCoord	min_width;

	/** Actual width used by this column.

		Calculated during the second/final table layout pass. Depends on
		percentage and desired column width, as well as the table width and
		width requirements of other columns in the table. In automatic table
		layout it also depends on minimum and maximum width. */

	LayoutCoord	width;
};

#define TABLE_FRAME_not_specified	0
#define TABLE_FRAME_void			1
#define TABLE_FRAME_above			2
#define TABLE_FRAME_below			3
#define TABLE_FRAME_hsides			4
#define TABLE_FRAME_vsides			5
#define TABLE_FRAME_lhs				6
#define TABLE_FRAME_rhs				7
#define TABLE_FRAME_border			8

#define TABLE_RULES_not_specified	0
#define TABLE_RULES_none			1
#define TABLE_RULES_groups			2
#define TABLE_RULES_rows			3
#define TABLE_RULES_cols			4
#define TABLE_RULES_all				5

/** Table list element. */

class TableListElement
  : public Link
{
public:

	/** Convenience overloading. */

    TableListElement*
					Suc() const { return (TableListElement*) Link::Suc(); }

	/** Convenience overloading. */

    TableListElement*
					Pred() const { return (TableListElement*) Link::Pred(); }

	/** Is this a caption? */

	virtual BOOL	IsCaption() const { return FALSE; }

	/** Is this a top caption? */

	virtual BOOL	IsTopCaption() const { return FALSE; }

	/** Is this a row group? */

	virtual BOOL	IsRowGroup() const { return FALSE; }

	/** Return the HTML_Element. */

	virtual HTML_Element*
					GetHtmlElement() const = 0;

	/** Get x position. */

	virtual LayoutCoord
					GetPositionedX() const = 0;

	/** Get y position. */

	virtual LayoutCoord
					GetPositionedY() const = 0;

	/** Get y position, ignoring offset caused by relative positioning. */

	virtual LayoutCoord
					GetStaticPositionedY() const = 0;

	/** Increase y position (or decrease, if using negative values). */

	virtual void	IncreaseY(LayoutInfo& info, TableContent* table, LayoutCoord increase) = 0;

	/** Get height. */

	virtual LayoutCoord
					GetHeight() const = 0;

	/** Traverse box. */

	virtual void	Traverse(TraversalObject* traversal_object, LayoutProperties* table_lprops, TableContent* table) = 0;

#ifdef PAGED_MEDIA_SUPPORT

	/** Attempt to break page.

		Will attempt to break page in the subtree established by this element. */

	virtual BREAKST	AttemptPageBreak(LayoutInfo& info, int strength) = 0;

#endif // PAGED_MEDIA_SUPPORT

	/** Get page break policy before this layout element. */

	virtual BREAK_POLICY
					GetPageBreakPolicyBefore() const { return BREAK_ALLOW; }

	/** Get page break policy after this layout element. */

	virtual BREAK_POLICY
					GetPageBreakPolicyAfter() const { return BREAK_ALLOW; }

	/** Get column break policy before this layout element. */

	virtual BREAK_POLICY
					GetColumnBreakPolicyBefore() const { return BREAK_ALLOW; }

	/** Get column break policy after this layout element. */

	virtual BREAK_POLICY
					GetColumnBreakPolicyAfter() const { return BREAK_ALLOW; }

	/** Distribute content into columns.

		@return FALSE on OOM, TRUE otherwise. */

	virtual BOOL	Columnize(Columnizer& columnizer, Container* container) = 0;

	/** Figure out to which column(s) or spanned element a box belongs. */

	virtual void	FindColumn(ColumnFinder& cf) = 0;
};

/** Reflow state information about each column. */

class TableColumnCalculation
{
public:

	/** Maximum width at the start of current reflow pass.

		After this reflow pass, this value may be compared to min_width for the corresponding
		column in TableColumnInfo, and if they differ, another reflow may be required. */

	LayoutCoord		used_max_width;

	/** Minimum width at the start of current reflow pass.

		After this reflow pass, this value may be compared to max_width for the corresponding
		column in TableColumnInfo, and if they differ, another reflow may be required. */

	LayoutCoord		used_min_width;

	/** Desired width at the start of current reflow pass.

		0 means no desired width. */

	LayoutCoord		used_desired_width;

	/** Percentage at the start of current reflow pass.

		Negative value means no percentage. */

	LayoutFixed		used_percent;

	/** Temporary modified percentage. To be ignored if has_percent is false.

		This variable has a very limited scope; typically local to a function that needs temporary
		storage while calculating column widths. */

	LayoutFixed		percent;

	/** Temporary modified width.

		This variable has a very limited scope; typically local to a function that needs temporary
		storage while calculating column widths. */

	LayoutCoord		width;

	/** Temporary modified minimum width.

		This variable has a very limited scope; typically local to a function that needs temporary
		storage while calculating column widths. */

	LayoutCoord		min_width;

	/** Has this column been expanded beyond minimum, percentage and desired width? */

	unsigned int	expanded:1;

	/** Does this column have percentage?

		This variable has a very limited scope; typically local to a function that needs temporary
		storage while calculating column widths. */

	unsigned int	has_percent:1;

	/** Is percentage to be ignored for this column?

		This variable has a very limited scope; typically local to a function that needs temporary
		storage while calculating column widths. */

	unsigned int	ignore_percent:1;

					TableColumnCalculation()
						: used_max_width(LAYOUT_COORD_MAX),
						  used_min_width(LAYOUT_COORD_MAX),
						  used_desired_width(0),
						  used_percent(-1) {}
};

/** Table content reflow state. */

class TableContentReflowState
{
public:

	/** Last topmost caption. */

	TableCaptionBox*
					last_top_caption;

	/** Last bottommost caption. */

	TableCaptionBox*
					last_bottom_caption;

	/** Last row group. */

	TableRowGroupBox*
					last_row_group;

	/** Current layout element. */

	TableListElement*
					reflow_element;

	/** Reflow state information about each column. */

	TableColumnCalculation*
					column_calculation;

	/** Row position. */

	LayoutCoord		row_position;

	/** Margin collapsing state for bottom captions and the table itself. */

	VerticalMargin	margin_state;

	/** Margin collapsing state specially for top captions. */

	VerticalMargin	top_captions_margin_state;

	/** CSS specified height. */

	LayoutCoord		css_height;

	/** Height from previous layout pass. */

	LayoutCoord		old_height;

	/** Minimum height of all captions if the table's maximum width is satisfied. */

	LayoutCoord		captions_minimum_height;

	/** Minimum height of actual table box if the table's maximum width is satisfied.

		This includes all table row groups and the table's vertical border and
		padding (but it does not include captions). */

	LayoutCoord		table_box_minimum_height;

	/** Largest cell colspan value found in this table. */

	unsigned short	max_colspan;

	/** Number that next col will get. */

	int				next_col_number;

	/** Highest column number border-collapsed so far. */

	unsigned short	highest_col_number_collapsed;

	/** Actual horizontal border spacing. */

	short			border_spacing_horizontal;

	/** Actual vertical border spacing. */

	short			border_spacing_vertical;

	/** (Re-)calculate min/max widths in this reflow pass? */

	BOOL			calculate_min_max_widths;

	/** Use the fixed layout algorithm? */

	FixedLayout		fixed_layout;

	/** Is this an SSR true table? */

	TRUE_TABLE_MODE	true_table;

	/** Caption min width at the start of current reflow pass. */

	LayoutCoord		used_caption_min_width;

	/** Current caption min width. */

	LayoutCoord		caption_min_width;

	/** Caption min width at the start of current reflow pass. */

	LayoutCoord		used_caption_max_width;

	/** Current caption max width. */

	LayoutCoord		caption_max_width;

	/** Largest min width of all cells with colspan > 1. Not used in normal layout mode. */

	LayoutCoord		colspanned_min_width;

	/** Total width of columns with visibility:collapse. */

	LayoutCoord		collapsed_width;

	/** Number of columns found in this reflow pass. */

	int				new_column_count;

	/** Table is (being) reflowed.

		If this flag is TRUE, it means that table layout wasn't skipped. In
		other words, if it is TRUE, Layout() was called. The TableContent class
		is special in that a reflow state is created even if layout is skipped
		(it's created in ComputeSize() for TableContent; all other Content
		classes do this in Layout()). */

	BOOL			reflow;

	/** Computed height changed. Need to reflow content. */

	BOOL			css_height_changed;

	/** Width of a row. */

	LayoutCoord		row_width;

					TableContentReflowState()
					  : last_top_caption(NULL),
						last_bottom_caption(NULL),
						last_row_group(NULL),
						reflow_element(NULL),
						column_calculation(NULL),
						row_position(0),
						captions_minimum_height(0),
						table_box_minimum_height(0),
						max_colspan(0),
						next_col_number(0),
						calculate_min_max_widths(FALSE),
						caption_min_width(0),
						caption_max_width(0),
						colspanned_min_width(0),
						new_column_count(0),
						reflow(FALSE),
						css_height_changed(FALSE),
						row_width(0) {}

					~TableContentReflowState() { OP_DELETEA(column_calculation); }

	void*			operator new(size_t nbytes) OP_NOTHROW { return g_tablecontent_reflow_state_pool->New(sizeof(TableContentReflowState)); }
	void			operator delete(void* p, size_t nbytes) { g_tablecontent_reflow_state_pool->Delete(p); }
};

/** Table content. */

class TableContent
  : public Content
{
private:
	/** Space distribution mode: Start size for space distribution algorithm.

		Normally we start at minimum width, but not when colspan requires us to
		expand column normal minimum or maximum width. */

	enum SpaceDistributionMode
	{
		FROM_MIN_WIDTH,
		FROM_NORMAL_MIN_WIDTH,
		FROM_MAX_WIDTH,
	};

	/** Delete reflow state. */

	virtual void	DeleteReflowState() { OP_DELETE(reflow_state); reflow_state = NULL; }

	/** Column widths. */

	TableColumnInfo*
					column_widths;

	union
	{
		struct
		{
			/** Recalculate column widths in the next reflow pass. */

			unsigned int
					recalculate_columns:1;

			/** Set to TRUE when min/max values are calculated and up-to-date. */

			unsigned int
					minmax_calculated:1;

			/** Set when table is finished but has a row with replaced content that has
				percent height and therefore needs an extra reflow without changing row heights. */

			unsigned int
					use_old_row_heights:1;

			/** Table has replaced content with percentage height. */

			unsigned int
					has_content_with_percentage_height:1;

			/** Height is relative (either percentage or depends on top/bottom offsets). */

			unsigned int
					relative_height:1;

			/** Which sides of the frame surrounding a table will be visible. */

			unsigned int
					frame:5;

			/** Which rules will appear between cells within a table. */

			unsigned int
					rules:4;

			/** Has this table any wrapped rows? (AMSR) */

			unsigned int
					is_wrapped:1;

			/** Is this a True Table (ERA)? See TRUE_TABLE_MODE. */

			unsigned int
					true_table:2;

			/** Reflow has been requested, but the element has not been marked dirty yet. */

			unsigned int
					reflow_requested:1;

#ifdef SUPPORT_TEXT_DIRECTION
			/** Is table rtl table?

				Only used under reflow. */

			unsigned int
					rtl:1;
#endif

			/** Avoid page break inside table? */

			unsigned int
					avoid_page_break_inside:1;

			/** Avoid column break inside table? */

			unsigned int
					avoid_column_break_inside:1;

			// room for 11 more bits here
		} packed;
		unsigned int
					packed_init;
	};

	/** Number of columns in table. */

	unsigned short	column_count;

	/** Last referenced/used column for this table, plus one.

		This is the last column in the table that is the first column of at
		least one cell box, plus one. */

	unsigned short	last_used_column;

	/** previous CSS defined height of table. */

	LayoutCoord		old_css_height;

	/** Height of box. */

	LayoutCoord		height;

	/** Distance from table content left edge to a row's left edge. */

	LayoutCoord		row_left_offset;

	/** Width that content can be in box. */

	LayoutCoord		width;

	/** Minimum table width.

		This is the minimal width this table can have without having child
		content overflow it. This is the sum of all columns' minimum widths and
		the table's horizontal border-spacing, padding and border. */

	LayoutCoord		minimum_width;

	/** Normal layout mode minimum table width.

		In normal layout mode, this is equal to minimum_width. In MSR and other
		ERA modes this is the minimum width _as if laid out_ in normal mode;
		i.e. minimum width without applying any ERA tricks. */

	LayoutCoord		normal_minimum_width;

	/** Constrained maximum table width.

		Only used for width propagation to ancestors, and -1 means unset.

		If set, it overrides maximum_width when it comes to width propagation
		to ancestors, while the actual table with calculation will still honor
		maximum_width and ignore this value completely. This difference
		(between actual maximum width and propagated maximum width) is
		unfortunately necessary because the maximum width caused by
		percent-width columns must not be propagated to ancestor tables. All
		thanks to a deliberately vague spec and how various browser engines
		(including Opera) have always done this. */

	LayoutCoord		constrained_maximum_width;

	/** Maximum table width.

		This is the width that this table would need in order to prevent
		implicit line breaks and floats being shifted downwards (due to lack of
		horizontal space) in its cells. */

	LayoutCoord		maximum_width;

	/** Height of box if maximum_width is satisfied. */

	LayoutCoord		minimum_height;

	/** Reset layout state. */

	void			ResetLayout(const HTMLayoutProperties& props);

	/** Compute size of content and return IS_TRUE if size is changed. */

	virtual OP_BOOLEAN
					ComputeSize(LayoutProperties* cascade, LayoutInfo& info);

	/** Check and update column min/max widths based on widths of one cell or column.

		Called when a new cell or column has been added, or when we
		need to recalculate the widths due to colspan. */

	BOOL			UpdateColumnWidths(const LayoutInfo& info, int column, int colspan, LayoutCoord cell_desired_width, LayoutCoord cell_min_width, LayoutCoord cell_normal_min_width, LayoutCoord cell_max_width, BOOL is_cell);

	/** Calculate all column min/max/percentage widths.

		Do the cells with lowest colspan first. */

	void			CalculateColumnMinMaxWidths(const LayoutInfo& info);

	/** Calculate table width. */

	BOOL			CalculateTableWidth(LayoutProperties* cascade, LayoutInfo& info);

	/** Calculate column widths for the given table width.

		Returns TRUE when one ore more columns have changed size. */

	BOOL			CalculateColumnWidths(LayoutInfo& info);

	/** Expand some or all columns to meet some minimum width requirement, if necessary.

		This call will not change column widths directly; results will be put in the width member
		of TableColumnCalculation entries.

		@param available_width Minimum width requirement of the columns combined (border-spacing not included)
		@param first_column First column
		@param last_column Last column
		@param mode Space distribution mode. Normally we start by satisfying column
		minimum widths, followed by percentage, desired and maximum, but some of these
		steps may be skipped when expanding normal minimum or maximum widths due to
		colspan.
		@param scale_down_percentage If TRUE, if total percentage exceeds 100, scale all
		percentages down instead of ignoring excess. */

	void			DistributeSpace(const LayoutInfo& info, LayoutCoord available_width, unsigned int first_column, unsigned int last_column, SpaceDistributionMode mode, BOOL scale_down_percentage);

	/** Initialise reflow state. */

	BOOL			InitialiseReflowState() { if (!reflow_state) reflow_state = new TableContentReflowState; return reflow_state != NULL; }

#ifdef PAGED_MEDIA_SUPPORT

	/** Attempt to break page. */

	BREAKST			PageBreakHelper(LayoutInfo& info, int strength, TableListElement* potential_break);

#endif // PAGED_MEDIA_SUPPORT

	/** Find and propagate explicit (column/page) breaks in table.

		@returns ERR_NO_MEMORY on OOM, otherwise OK. */

	OP_STATUS		FindBreaks(LayoutInfo& info, LayoutProperties* cascade);

	/** Compute minimum and maximum widths for table.

		Return TRUE if min/max values changed. */

	BOOL			ComputeMinMaxWidth(const LayoutInfo& info, LayoutProperties* cascade);

	/** Modify properties for collapsing border content. */

	virtual void	ModifyBorderProps(LayoutInfo& info, LayoutProperties* cascade) {}

	/** Find the largest minimum and maximum width among all captions. */

	void			CalculateCaptionMinMaxWidth(LayoutCoord& min_width, LayoutCoord& max_width) const;

	/** Calculate row width and the distance from table content left edge to a row's left edge. */

	virtual void	CalculateRowDimensions(const HTMLayoutProperties& props, LayoutCoord& row_left_offset, LayoutCoord& row_width) const;

	/** Set X position and width of all TableColGroupBox objects in this table. */

	void			SetColGroupBoxDimensions();

	/** Get the baseline of the specified row, relative to the table top edge.

		@param row_number Row number. Negative values means counting from the bottom of the table. */

	LayoutCoord		LocalGetBaseline(long row_number) const;

protected:

	/** State of reflow in box.

		This is deleted when reflow is complete. */

	TableContentReflowState*
					reflow_state;

	/** Layout stack. */

	Head			layout_stack;

	/** Update height. */

	void			UpdateHeight(const LayoutInfo& info);

	/** Has reflow been forced? */

	virtual BOOL	GetReflowForced() const { return packed.reflow_requested; }

	/** Set reflow force status, which will be checked and reset during SignalChange(). */

	virtual void	SetReflowForced(BOOL force_reflow_on) { packed.reflow_requested = !!force_reflow_on; }

	/** Get the sum of non-percentage based top and bottom border and padding. */

	virtual LayoutCoord
					GetNonPercentVerticalBorderPadding(const HTMLayoutProperties& props) const { return props.GetNonPercentVerticalBorderPadding(); }

public:

					TableContent();
	virtual			~TableContent();

#ifdef LAYOUT_TRAVERSE_DIRTY_TREE

	/** Signal that a TableColGroupBox in this Table is about to be deleted. */

	void			SignalChildDeleted(FramesDocument* doc, TableColGroupBox* child);

#endif // LAYOUT_TRAVERSE_DIRTY_TREE

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Propagate minimum and maximum cell widths to columns. */

	void			PropagateCellWidths(const LayoutInfo& info, unsigned short column, unsigned short colspan, LayoutCoord desired_width, LayoutCoord min_width, LayoutCoord normal_min_width, LayoutCoord max_width);

	/** Propagate minimum and maximum widths from captions. */

	void			PropagateCaptionWidths(const LayoutInfo& info, LayoutCoord min_width, LayoutCoord max_width);

	/** Based on column widths, get table cell width, either calculated width or desired width. */

	LayoutCoord		GetCellWidth(int column, int colspan, BOOL desired, LayoutCoord* collapsed_width = NULL) const;

	/** Get top of first row. */

	long			GetRowsTop() const;

	/** Get height between top of first row and bottom of last row. */

	long			GetRowsHeight() const;

	/** Has table column a specified width? */

	BOOL			HasColumnSpecifiedWidth(int column) const { return column_widths && column < column_count && (column_widths[column].desired_width || column_widths[column].packed.has_percent); }

	/** Does the table column have visibility:collapse? */

	BOOL			IsColumnVisibilityCollapsed(int column) const { return column_widths && column < column_count && column_widths[column].IsVisibilityCollapsed(); }

	/** Get the visible rectangle of a partially visibility:collapsed table cell. */

	BOOL			GetCollapsedCellRect(OpRect& clip_rect, LayoutCoord& offset_x, LayoutCoord& offset_y, LayoutProperties* table_lprops, const TableCellBox* cell, const TableRowBox* first_row) const;

	/** Get table column count. */

	unsigned short	GetColumnCount() const { return column_count; }

	/** Get table used column count (last referenced column plus one). */

	unsigned short	GetLastColumn() const { return last_used_column; }

	/** Get width of table contents. */

	virtual LayoutCoord
					GetWidth() const { return width; }

	/** Get height of container. */

	virtual LayoutCoord
					GetHeight() const { return height; }

	/** Get minimum height (height if maximum width is satisfied) */

	virtual LayoutCoord
					GetMinHeight() const { return minimum_height; }

	/** Get height from previous layout pass. */

	LayoutCoord		GetOldHeight() const { return reflow_state->old_height; }

	/** Add minimum height from a row group. */

	void			AddRowGroupMinHeight(LayoutCoord rowgroup_minimum_height) { reflow_state->table_box_minimum_height += rowgroup_minimum_height; }

	/** Add minimum height from a caption. */

	void			AddCaptionMinHeight(LayoutCoord caption_minimum_height) { reflow_state->captions_minimum_height += caption_minimum_height; }

	/** Does this container have 'auto' height */

	LayoutCoord		GetCSSHeight() const { return reflow_state->css_height; }

	/** Get the row width of this table. */

	LayoutCoord		GetRowWidth() const { return reflow_state->row_width; }

	/** Set desired table column width. */

	void			SetDesiredColumnWidth(const LayoutInfo& info, int column, int colspan, LayoutCoord desired_column_width, BOOL is_cell);

	/** Check if changes require a reflow.

	    Check if the changes made to the table during this reflow pass requires
	    another reflow. Also make sure that all minimum/maximum widths of all
	    columns and the table are up-to-date.

	    @return TRUE if reflow is required */

	BOOL			CheckChange(LayoutInfo& info);

	/** A cell/column/column-group has been added. Adjust number of columns in
		table and update rowspan state for the columns spanned by the
		cell/column/column-group added.

	    Return FALSE if OOM. */

	BOOL			AdjustNumberOfColumns(unsigned short column, unsigned short colspan, unsigned short rowspan, BOOL is_cell, LayoutWorkplace* workplace);

	/** Add caption to table. */

	void			GetNewCaption(LayoutInfo& info, TableCaptionBox* caption, int caption_side);

	/** Add row group to table. */

	void			GetNewRowGroup(LayoutInfo& info, TableRowGroupBox* row_group);

	/** Add columns to table. */

	BOOL			AddColumns(const LayoutInfo& info, short span, LayoutCoord width, TableColGroupBox* column_box);

	/** Skip rowspanned columns.

		Returns whether rowspans are completed. */

	BOOL			SkipRowSpannedColumns(LayoutInfo& info, unsigned short& column, TableRowBox* row, LayoutCoord* next_column_position = NULL, BOOL force_end = FALSE, unsigned int min_rowspan = 0);

	/** Finish all row spans. */

	void			TerminateRowSpan(LayoutInfo& info, TableRowBox* last_row);

	/** Helper function for moving rows. */

	void			MoveElements(LayoutInfo& info, TableListElement* element, LayoutCoord increment);

	/** Layout of this box is finished (or skipped). Propagate changes (bottom margins,
		bounding-box) to parents. This may grow the box, which in turn may cause its parents to be
		grown. Bottom margins may participate in margin collapsing with successive content, but
		only if this box is part of the normal flow. In that case, propagate the bottom margin to
		the reflow state of the container of this box. */

	void			PropagateBottomMargins(LayoutInfo& info, const VerticalMargin* bottom_margin = 0, BOOL has_inflow_content = TRUE);

	/** Recalculate the top margin after a new block box has been added to a container's layout
		stack. Collapse the margin with preceding adjacent margins if appropriate. If the top
		margin of this block is adjacent to an ancestor's top margin, it may cause the ancestor's
		Y position to change. If the top margin of this block is adjacent to a preceding sibling's
		bottom margin, this block may change its Y position.

		@return TRUE if the Y position of any element was changed. */

	BOOL			RecalculateTopMargins(LayoutInfo& info, const VerticalMargin* top_margin);

	/** Expand relevant parent containers and their bounding-boxes to contain absolutely positioned
		boxes. A call to this method is only to be initiated by absolutely positioned boxes.

		@param info Layout information structure
		@param child_bounding_box Bounding-box of the propagating descendant, joined with the
		bounding-boxes of the elements up the parent chain towards this element. Relative to the
		top border-edge of this box.
		@param opts Bounding box propagation options */

	void			PropagateBottom(const LayoutInfo& info, const AbsoluteBoundingBox& child_bounding_box, PropagationOptions opts);

	/** Get horizontal cellspacing. */

	LayoutCoord		GetHorizontalCellSpacing() const { return LayoutCoord(reflow_state->border_spacing_horizontal); }

	/** Does this table use the fixed layout algorithm? */

	FixedLayout		GetFixedLayout() const { return reflow_state->fixed_layout; }

	/** Row completed, decrement rowspans. */

	void			RowCompleted();

	/** Finish reflowing box. */

	virtual LAYST	FinishLayout(LayoutInfo& info);

	/** Update screen. */

	virtual void	UpdateScreen(LayoutInfo& info);

	/** Cast this object to table content, if it is one. */

	virtual TableContent*
					GetTableContent() { return this; }

	/** Get baseline of first or last line.

		@param last_line TRUE if last line baseline search (inline-block case).	*/

	virtual LayoutCoord
					GetBaseline(BOOL last_line = FALSE) const;

	/** Get baseline of inline block or content of inline element that
		has a baseline (e.g. ReplacedContent with baseline, BulletContent). */

	virtual LayoutCoord
					GetBaseline(const HTMLayoutProperties& props) const;

	/** Get the baseline if maximum width is satisfied. */

	virtual LayoutCoord
					GetMinBaseline(const HTMLayoutProperties& props) const;

	/** Is considered a containing element, e.g. container, table or flexbox? */

	virtual BOOL	IsContainingElement() const { return TRUE; }

	/** Calculate bottom margin of layout element, by collapsing with adjacent child margins.
	 * This is only done when skipping layout of a box.
	 * @param cascade properties of this element
	 * @param info Layout information structure
	 * @param bottom_margin Margin state
	 * @param include_this If FALSE, omit margins specified on this element from the calculation
	 * and just collapse adjacent child margins.
	 * @return OpBoolean::IS_FALSE if top and bottom margins of this element are adjacent,
	 * OpBoolean::IS_TRUE otherwise, unless an error occurred.
	 */

	virtual OP_BOOLEAN
					CalculateBottomMargins(LayoutProperties* cascade, LayoutInfo& info, VerticalMargin* bottom_margin, BOOL include_this = TRUE) const;

	/** Calculate top margins of layout element, by collapsing with adjoining child margins.
	 * This is only done when skipping layout of a box.
	 * @param cascade properties of this element
	 * @param info Layout information structure
	 * @param top_margin Margin state
	 * @param include_self_bottom If FALSE, don't collapse top with bottom margin
	 * specified on this element, even if they are adjoining.
	 * @return OpBoolean::IS_FALSE if top and bottom margins of this element are adjoining,
	 * OpBoolean::IS_TRUE otherwise, unless an error occurred.
	 */

	virtual OP_BOOLEAN
					CalculateTopMargins(LayoutProperties* cascade, LayoutInfo& info, VerticalMargin* top_margin, BOOL include_self_bottom) const;

	/** Traverse box. */

	virtual void	Traverse(TraversalObject* traversal_object, LayoutProperties* layout_props);

	/** Get min/max width for box. */

	virtual BOOL	GetMinMaxWidth(LayoutCoord& min_width, LayoutCoord& normal_min_width, LayoutCoord& max_width) const;

	/** Clear min/max width. */

	virtual void	ClearMinMaxWidth();

	/** Does child content need to propagate min/max widths to this table? */

	BOOL			IsCalculatingMinMaxWidths() const { return reflow_state->calculate_min_max_widths; }

#ifdef PAGED_MEDIA_SUPPORT

	/** Attempt to break page. */

	virtual BREAKST	AttemptPageBreak(LayoutInfo& info, int strength, SEARCH_CONSTRAINT constrain);

	/** Insert a page break. */

	BREAKST			InsertPageBreak(LayoutInfo& info, int strength);

#endif // PAGED_MEDIA_SUPPORT

	/** Return TRUE if we are to avoid page breaks inside this table. */

	BOOL			AvoidPageBreakInside() const { return packed.avoid_page_break_inside; }

	/** Return TRUE if we are to avoid column breaks inside this table. */

	BOOL			AvoidColumnBreakInside() const { return packed.avoid_column_break_inside; }

	/** Insert a page break. */

	//BREAKST			RowInsertPageBreak(LayoutInfo& info, int strength);

	/** Collapse borders? */

	virtual BOOL	GetCollapseBorders() const { return FALSE; }

	/** Use old row heights? */

	BOOL			UseOldRowHeights() const { return packed.use_old_row_heights; }

	/** Clear use old row heights mark */

	void			ClearUseOldRowHeights() { packed.use_old_row_heights = 0; }

	/** Request reflow of table. */

	void			RequestReflow(LayoutInfo& info);

	/** Get column box at 'column'. */

	TableColGroupBox*
					GetColumnBox(int column);

	/** Get column group start and stop. */

	void			GetColGroupStartStop(int column, int colspan, BOOL& colgroup_starts_left, BOOL& colgroup_stops_right);

	/** Which sides of the frame surrounding a table will be visible. */

	unsigned int	GetFrame() const { return packed.frame; }

	/** Which rules will appear between cells within a table. */

	unsigned int	GetRules() const { return packed.rules; }

	/** Does this table need special border handling due to html attributes 'frames' and 'rules' ? */

	BOOL			NeedSpecialBorders();

	/** Get table border widths, taking border collapsing into account if applicable. */

	virtual void	GetBorderWidths(const HTMLayoutProperties& props, short& top, short& left, short& right, short& bottom, BOOL inner) const;

	/** Get distance from table content left edge to a row's left edge. */

	LayoutCoord		GetRowLeftOffset() const { return row_left_offset; }

	/** Reset collapsed bottom border. */

	virtual void	ResetCollapsedBottomBorder() {}

	/** Get current rowspan info for column. */

	int				GetCurrentRowspan(int column) { return column_widths[column].packed2.rowspans; }

	/** This table has content with percentage height. */

	void			SetHasContentWithPercentageHeight() { packed.has_content_with_percentage_height = 1; }

	/** Is height relative? */

	virtual BOOL	HeightIsRelative() const { return packed.relative_height == 1; }

	/** Has this table any wrapped rows? (AMSR) */

	BOOL			IsWrapped() const { return packed.is_wrapped; }

	/* Returns TRUE if this is a True Table (in SSR or CSSR mode) */

	BOOL			IsTrueTable() const { return packed.true_table > NOT_TRUE_TABLE; }

	/** Indicate that table column strategy has changed (single column or normal) (ERA) */

	void			SetColumnStrategyChanged() { packed.recalculate_columns = 1; }

#ifdef SUPPORT_TEXT_DIRECTION

	/** Is RTL table? */

	BOOL			IsRTL() const { return packed.rtl; }

#endif

	/** Add True Table condition for column */

	void			AddTrueTableCondition(int column, TRUE_TABLE_CANDIDATE candidate, const LayoutInfo& info);

	/** Is this a True Table candidate? */

	virtual TRUE_TABLE_CANDIDATE
					IsTrueTableCandidate() const { return packed.true_table == TRUE_TABLE ? POSITIVE : DISABLE; }

	/** Get the percent of normal width when page is shrunk in ERA. */

	virtual int		GetPercentOfNormalWidth() const;

	/** Get normal x position of column when page is shrunk in ERA. */

	LayoutCoord		GetNormalColumnX(int column, short horizontal_cell_spacing) const;

	/** Yield layout of this currently reflowing table. */

	virtual void	YieldLayout(LayoutInfo& info) { packed.recalculate_columns = 1; }

	/** Does width propagation from this table affect FlexRoot? */

	BOOL			AffectsFlexRoot() const;

	/** Did the computed height change in this pass? In that case we need to reflow children. */

	BOOL			CssHeightChanged() { OP_ASSERT(reflow_state); if (reflow_state) return reflow_state->css_height_changed; return FALSE; }

	/** Return TRUE if this element can be split into multiple outer columns or pages.

		In general, descendant content of a paged or multicol container is
		columnizable, but this is not the case if the content is or is inside
		of a container with scrollbars, button, replaced content, etc. */

	virtual BOOL	IsColumnizable() const;

	/** Distribute content into columns.

		@return FALSE on OOM, TRUE otherwise. */

	virtual BOOL	Columnize(Columnizer& columnizer, Container* container);

	/** Figure out to which column(s) or spanned element a box belongs. */

	virtual void	FindColumn(ColumnFinder& cf);
};

/** Table content with collapsing borders. */

class TableCollapsingBorderContent
  : public TableContent
{
private:

	/** Collapsed border. This is not the border that is drawn by the table
		but it is used to determine size and position. */

	Border			collapsed_border;

	/** Modify properties for collapsing border content. */

	union
	{
		struct
		{
			/** Are the corners calculated? */

			unsigned int
					corners_calculated:1;

		} packed2;
		unsigned long
					packed2_init;
	};

	virtual void	ModifyBorderProps(LayoutInfo& info, LayoutProperties* cascade);

	/** Calculate row width and the distance from table content left edge to a row's left edge. */

	virtual void	CalculateRowDimensions(const HTMLayoutProperties& props, LayoutCoord& row_left_offset, LayoutCoord& row_width) const;

protected:

	/** Get the sum of non-percentage based top and bottom border and padding. */

	virtual LayoutCoord
					GetNonPercentVerticalBorderPadding(const HTMLayoutProperties& props) const { return LayoutCoord(collapsed_border.top.width + collapsed_border.bottom.width); }

public:

					TableCollapsingBorderContent()
						: TableContent(),
                          packed2_init(0) { collapsed_border.Reset(); }

	/** Collapse borders? */

	virtual BOOL	GetCollapseBorders() const { return TRUE; }

	/** Collapse top border. */

	void			CollapseTopBorder(LayoutInfo& info, BorderEdge& cell_border_edge);

	/** Collapse left border. */

	void			CollapseLeftBorder(LayoutInfo& info, BorderEdge& cell_border_edge, unsigned short end_column);

	/** Collapse right border. */

	void			CollapseRightBorder(LayoutInfo& info, BorderEdge& cell_border_edge, unsigned short end_column);

	/** Collapse bottom border. */

	void			CollapseBottomBorder(LayoutInfo& info, BorderEdge& cell_border_edge);

	/** Reset collapsed bottom border. */

	virtual void	ResetCollapsedBottomBorder();

	/** Clear min/max width. */

	virtual void	ClearMinMaxWidth();

	/** Get border widths, including collapsing table borders */

	virtual void	GetBorderWidths(const HTMLayoutProperties& props, short& top, short& left, short& right, short& bottom, BOOL inner) const;

	/** Are the corners calculated? */

	BOOL			GetCornersCalculated() const { return packed2.corners_calculated; }

	/** Set corners as calculated. */

	void			SetCornersCalculated(BOOL value) { packed2.corners_calculated = value ? 1 : 0; }

	/** Finish reflowing box. */

	virtual LAYST	FinishLayout(LayoutInfo& info);

};

/** Table row box reflow state. */

class TableRowBoxReflowState
  : public ReflowState
{
public:

	/** Table row group to which this row belongs. */

	TableRowGroupBox*
					row_group_box;

	/** Old y position of box. */

	LayoutCoord		old_y;

	/** Old height of box. */

	LayoutCoord		old_height;

	/** Old baseline of box. */

	LayoutCoord		old_baseline;

	/** Height of row if the table's maximum_width is satisfied. */

	LayoutCoord		minimum_height;

	/** Old x position of box. */

	LayoutCoord		old_x;

	/** Previous y root established by positioned box. */

	LayoutCoord		previous_root_y;

	/** Previous x root established by positioned box. */

	LayoutCoord		previous_root_x;

	/** X position of next column. */

	LayoutCoord		reflow_position;

	/** Current baseline of row */

	LayoutCoord		baseline;

	/** Maximum space needed below baseline for a baseline aligned cell */

	LayoutCoord		max_below_baseline;

	/** Use old height. */

	BOOL			use_old_height;

	/** Number that next column will be given. */

	unsigned short	next_column;

					TableRowBoxReflowState()
					  : row_group_box(0),
						old_y(0),
						old_height(0),
						old_baseline(0),
						minimum_height(0),
						old_x(0),
						previous_root_y(0),
						previous_root_x(0),
						reflow_position(0),
						baseline(0),
						max_below_baseline(0),
						use_old_height(FALSE),
						next_column(0) {}

	void*			operator new(size_t nbytes) OP_NOTHROW { return g_tablerow_reflow_state_pool->New(sizeof(TableRowBoxReflowState)); }
	void			operator delete(void* p, size_t nbytes) { g_tablerow_reflow_state_pool->Delete(p); }
};

/** Vertical box reflow state. */

class TableCellBoxReflowState
  : public VerticalBoxReflowState
{
public:

	/** Y position of cell at previous screen update.

		This is the distance between the cell's border edge and the ancestor
		table-row-group (i.e. it is not the distance between the cell and its
		row, which might be more expected. This has to do with handling of
		ROWSPAN attribute values larger than 1). */

	LayoutCoord		old_position;

	/** The previous table cell content height. */

	LayoutCoord		old_content_height;

	/** Desired height of cell. */

	LayoutCoord		desired_height;

	/** Must update entire cell in UpdateScreen(). */

	BOOL			update_entire_cell;

	void*			operator new(size_t nbytes) OP_NOTHROW { return g_tablecell_reflow_state_pool->New(sizeof(TableCellBoxReflowState)); }
	void			operator delete(void* p, size_t nbytes) { g_tablecell_reflow_state_pool->Delete(p); }
};

#define MAX_COLUMNS	0x1fff

/** Table cell box. */

class TableCellBox
  : public VerticalBox,
    public Link
{
private:

	/** Space manager for allocating space. */

	SpaceManager	space_manager;

	enum TableCellVerticalAlignment
	{
		CELL_ALIGN_TOP = 0,
		CELL_ALIGN_BASELINE,
		CELL_ALIGN_MIDDLE,
		CELL_ALIGN_BOTTOM
	};

	union
	{
		struct
		{
			/** Column number */

			unsigned int
					column:13; // has to match MAX_COLUMNS size

			/** Vertical alignment of cell. */

			unsigned int
					vertical_alignment:2;

			/** Cell has replaced content with percentage height. */

			unsigned int
					has_content_with_percentage_height:1;
		} packed;
		unsigned long
					packed_init;
	};

	/** Desired cell width from CSS. Negative if percentage.
		@see HTMLayoutProperties::content_width. */

	LayoutCoord		desired_width;

	/** X position of cell, relative to row. */

	LayoutCoord		x_pos;

	/** Width of cell. */

	LayoutCoord		content_width;

	/** Modify properties for collapsing cell box. */

	virtual void	ModifyBorderProps(LayoutInfo& info, LayoutProperties* cascade, TableRowBox* table_row) {}

protected:

	/** Initialise reflow state. */

	TableCellBoxReflowState*
					InitialiseReflowState();

	/** Increase desired width and min/max widths, and repropagate them. */

	void			IncreaseWidths(const LayoutInfo& info, LayoutCoord increment);

	/** Get bounding box relative to left border edge of this cell and top edge of the row.
		Extends box to include the whole cell border when borders are collapsed (for See CSS2.1 - 17.6.2). */

	void			GetCellBoundingBox(AbsoluteBoundingBox& box, BOOL include_overflow, const Border* collapsed_borders) const;

public:

					TableCellBox(HTML_Element* element, Content* content);
	virtual			~TableCellBox() { Out(); }

	/** Get reflow state. */

	TableCellBoxReflowState*
					GetReflowState() const { return (TableCellBoxReflowState*) VerticalBox::GetReflowState(); }

	/** Delete reflow state.

		This is done by the cell's row box. */

	void			DelayedDeleteReflowState() { DeleteReflowState(); }

	/** Get desired cell width. */

	LayoutCoord		GetDesiredWidth() const { return desired_width; }

	/** Get width. */

	virtual LayoutCoord
					GetWidth() const { return content_width; }

	/** Get height. */

	virtual LayoutCoord
					GetHeight() const;

	/** Get cell content height. */

	LayoutCoord		GetCellContentHeight() const { return content->GetHeight(); }

	/** Get the old cell content height.

		The one before starting the cell's Content layout. */

	LayoutCoord		GetOldCellContentHeight() const { OP_ASSERT(GetReflowState()); return GetReflowState()->old_content_height; }

	/** Get cell height. */

	LayoutCoord		GetCellHeight() const;

	/** Get cell colspan. */

	unsigned short	GetCellColSpan() const;

	/** Get cell rowspan. */

	unsigned short	GetCellRowSpan() const;

	/** Get column number. */

	unsigned short	GetColumn() const { return packed.column; }

	/** Set X position of cell, relative to row, but not including any offset caused by relative positioning. */

	void			SetStaticPositionedX(LayoutCoord new_x) { LayoutCoord x_offs, y_offs; GetPositionedOffset(x_offs, y_offs); x_pos = new_x + x_offs; }

	/** Get X position of cell, relative to row, but not including any offset caused by relative positioning. */

	LayoutCoord		GetStaticPositionedX() const { LayoutCoord x_offs, y_offs; GetPositionedOffset(x_offs, y_offs); return x_pos - x_offs; }

	/** Get X position of cell, relative to row. */

	LayoutCoord		GetPositionedX() const { return x_pos; }

	/** Get Y position of cell, relative to row. */

	LayoutCoord		GetPositionedY() const { LayoutCoord x_offs, y_offs; GetPositionedOffset(x_offs, y_offs); return y_offs; }

	/** Get offset caused by relative positioning. */

	virtual void	GetPositionedOffset(LayoutCoord& x, LayoutCoord& y) const { x = LayoutCoord(0); y = LayoutCoord(0); }

	/** Is this box one of the table boxes or a box with table content? */

	virtual BOOL	IsTableBox() const { return TRUE; }

	/** Is this box a table cell? */

	virtual BOOL	IsTableCell() const { return TRUE; }

	/** Is this box (and potentially also its children) columnizable?

		A box may only be columnizable if it lives in a paged or multicol container.

		A descendant of a box that isn't columnizable can never be columnizable (unless
		there's a new paged or multicol container between that box and its descendant).

		Table-rows, table-captions, table-row-groups and block-level descendant boxes of
		a paged or multicol container are typically columnizable. Being columnizable
		means that they need to be taken into account by the ancestor paged or multicol
		container somehow. They may become a start or stop element of a column or
		page. It may also be possible to split them over multiple columns or pages, but
		that depends on details about the box type, and that is where the
		"require_children_columnizable" parameter comes into play.

		@param require_children_columnizable If TRUE, only return TRUE if this box is of
		such a type that not only the box itself is columnizable, but also that it
		doesn't prevent its children from becoming columnizable. A table-row, block with
		scrollbars, absolutely-positioned box or float prevents children from being
		columnizable (FALSE will be returned it this parameter is TRUE), but the box
		itself may be very well be columnizable. Example: an absolutely positioned box
		may live inside of a paged or multicol container (so that its Y position is
		stored as a virtual Y position within the paged or multicol container), but its
		descendants do not have to worry about the multi-columnness or multi-pagedness.

		@return TRUE if this box (and possibly its children too) is columnizable. Return
		FALSE if this box isn't columnizable, or if require_children_columnizable is set
		and the children aren't columnizable. */

	virtual BOOL	IsColumnizable(BOOL require_children_columnizable = FALSE) const;

	/** If cell has vertical alignment 'baseline' */

	BOOL			IsBaselineAligned() const { return packed.vertical_alignment == CELL_ALIGN_BASELINE; }

	/** Compute y translation of table cell. */
	LayoutCoord		ComputeCellY(BOOL include_wrapped_offset = TRUE) const { return ComputeCellY(GetTableRowBox(), include_wrapped_offset); }
	LayoutCoord		ComputeCellY(const TableRowBox* row, BOOL include_wrapped_offset = TRUE) const { return ComputeCellY(row, include_wrapped_offset, GetHeight()); }
	LayoutCoord		ComputeCellY(const TableRowBox* row, BOOL include_wrapped_offset, LayoutCoord cell_height) const;

	/** Get vertical offset if cell is wrapped (AMSR) */

	LayoutCoord		GetCellYOffset() const;

	/** Clear vertical table cell alignment.

	    Used only in paged media, when table row spans more than
	    one page. */

	void			ClearVerticalAlignment() { packed.vertical_alignment = CELL_ALIGN_TOP; }

	/** Layout of this box is finished (or skipped). Propagate changes (bottom margins,
		bounding-box) to parents. This may grow the box, which in turn may cause its parents to be
		grown. Bottom margins may participate in margin collapsing with successive content, but
		only if this box is part of the normal flow. In that case, propagate the bottom margin to
		the reflow state of the container of this box. */

	virtual void	PropagateBottomMargins(LayoutInfo& info, const VerticalMargin* bottom_margin = 0, BOOL has_inflow_content = TRUE);

	/** Expand relevant parent containers and their bounding-boxes to contain floating and
		absolutely positioned boxes. A call to this method is only to be initiated by floating or
		absolutely positioned boxes.

		@param info Layout information structure
		@param bottom Only relevant for floats: The bottom margin edge of the float, relative to
		top border-edge of this box.
		@param min_bottom Only relevant for floats: The bottom margin edge of the float if
		maximum width of its container is satisfied
		@param child_bounding_box Bounding-box of the propagating descendant, joined with the
		bounding-boxes of the elements up the parent chain towards this element. Relative to the
		top border-edge of this box.
		@param opts Bounding box propagation options */

	virtual void	PropagateBottom(const LayoutInfo& info, LayoutCoord bottom, LayoutCoord min_bottom, const AbsoluteBoundingBox& child_bounding_box, PropagationOptions opts);

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Finish reflowing box. */

	virtual LAYST	FinishLayout(LayoutInfo& info);

	/** Update screen. */

	virtual void	UpdateScreen(LayoutInfo& info);

	/** Update cell. */

	void			Update(LayoutInfo& info, TableRowBox* row, LayoutCoord height, BOOL updated_all);

	/** Get baseline of cell. */

	LayoutCoord		GetBaseline() const;

	/** Do we need to calculate min/max widths of this box's content? */

	virtual BOOL	NeedMinMaxWidthCalculation(LayoutProperties* cascade) const;

	/** Propagate widths to container. */

	virtual void	PropagateWidths(const LayoutInfo& info, LayoutCoord min_width, LayoutCoord normal_min_width, LayoutCoord max_width);

	/** Traverse box with children.

		This method will traverse the contents of this box and its children, recursively. */

	virtual void	TraverseCell(TraversalObject* traversal_object, LayoutProperties* table_lprops, const TableRowBox* row);

	/** Traverse z children. */

	virtual void	TraverseZChildren(TraversalObject* traversal_object, LayoutProperties* layout_props, BOOL after, BOOL handle_overflow_clip) {}

	/** Paint background and border of this box on screen. */

	virtual void	PaintBgAndBorder(LayoutProperties* cascade, FramesDocument* doc, VisualDevice* visual_device) const;

	/** Calculate width of the containing block established by this box. */

	virtual void	CalculateContainingBlockWidth() {}

	/** Calculate height of the containing block established by this box.

		Only to be called after the table in which this box lives has been reflowed. */

	virtual void	CalculateContainingBlockHeight() { OP_ASSERT(IsPositionedBox()); }

	/** Return TRUE if the containing block size changed in the last reflow pass. */

	virtual BOOL	HasContainingBlockSizeChanged() { OP_ASSERT(!"Missing re-implementation of this method"); return FALSE; }

	/** Return the space manager of the object, if it has any. */

	virtual SpaceManager*
					GetLocalSpaceManager() { return &space_manager; }

#ifdef PAGED_MEDIA_SUPPORT

	/** Insert a page break. */

	virtual BREAKST	InsertPageBreak(LayoutInfo& info, int strength);

#endif // PAGED_MEDIA_SUPPORT

	/** Find table row box */

	TableRowBox*	GetTableRowBox() const;

	/** Calculate used vertical CSS properties (height and margins).

		Some box types need to override the regular height (and margin)
		calculation algorithms. This method will allow such boxes to do so.

		@return TRUE if height and vertical margins have been calculated. This
		means that the caller should refrain from calculating them in the
		default way. If FALSE is returned, heights and vertical margins should
		be calculated in the default way. */

	virtual BOOL	ResolveHeightAndVerticalMargins(LayoutProperties* cascade, LayoutCoord& content_height, LayoutCoord& margin_top, LayoutCoord& margin_bottom, LayoutInfo& info) const;

	/** Paint background and border. */

	void			PaintBg(LayoutProperties* cascade, FramesDocument* doc, VisualDevice* visual_device, const Border* border, const Border* collapsed_border) const;

	/** Paint background on columns or columngroups in strict mode. */

	void			PaintColumnBg(LayoutProperties* cascade, LayoutProperties* table_lprops, FramesDocument* doc, VisualDevice* visual_device, TableColGroupBox* colgroupbox, TableColGroupBox* child_colbox, BOOL border_collapsed) const;

	/** Paint background on rows or rowgroups in strict mode. */

	void			PaintRowBg(LayoutProperties* cascade, LayoutProperties* table_lprops, FramesDocument* doc, VisualDevice* visual_device, LayoutProperties* rowgroup_cascade, LayoutProperties* row_cascade, BOOL border_collapsed) const;

	/** Signal that content may have changed. */

	virtual void	SignalChange(FramesDocument* doc, BoxSignalReason reason = BOX_SIGNAL_REASON_UNKNOWN);

	/** Recalculate containing block size and schedule the specified descendant box for reflow if needed. */

	virtual void	RecalculateContainingBlock(FramesDocument* doc, AbsolutePositionedBox* box);

	/** Does cell have content with percentage height? */

	BOOL			HasContentWithPercentageHeight() const { return packed.has_content_with_percentage_height; }

	/** Set that cell has content with percentage height. */

	void			SetHasContentWithPercentageHeight() { packed.has_content_with_percentage_height = 1; }

	/** Invalidate the screen area that the box uses. */

	virtual void	Invalidate(LayoutProperties* parent_cascade, LayoutInfo& info) const;

	/** Get bounding box relative to left border edge of this cell and top edge of the row. */

	virtual void	GetBoundingBox(AbsoluteBoundingBox& box, BOOL include_overflow = TRUE, BOOL adjust_for_multipane = FALSE, BOOL apply_clip = TRUE) const;

	virtual void	SetIsHidden() {}
};

/** Relatively positioned table-cell box. */

class PositionedTableCellBox
	: public TableCellBox
{
protected:

	/** Positioned element data and Z entry for stacking context. */

	PositionedElement
					pos_elm;

public:

					PositionedTableCellBox(HTML_Element* element, Content* content)
						: TableCellBox(element, content),
						  pos_elm(element) {}

	/** Is this box a positioned box? */

	virtual BOOL	IsPositionedBox() const { return TRUE; }

	/** Return the z element of the object, if it has any. */

	virtual ZElement*
					GetLocalZElement() { return &pos_elm; }

	/** Get offset caused by relative positioning. */

	virtual void	GetPositionedOffset(LayoutCoord& x, LayoutCoord& y) const { pos_elm.GetPositionedOffset(x, y); }

	/** Traverse box with children.

		This method will traverse the contents of this box and its children, recursively. */

	virtual void	TraverseCell(TraversalObject* traversal_object, LayoutProperties* table_lprops, const TableRowBox* row);

	/** Calculate width of the containing block established by this box. */

	virtual void	CalculateContainingBlockWidth() { pos_elm.SetNewContainingBlockWidth(GetWidth()); }

	/** Calculate height of the containing block established by this box.

		Only to be called after the table in which this box lives has been reflowed. */

	virtual void	CalculateContainingBlockHeight() { if (!pos_elm.IsContainingBlockHeightCalculated()) pos_elm.SetNewContainingBlockHeight(GetHeight()); }

	/** Return TRUE if the containing block size changed in the last reflow pass. */

	virtual BOOL	HasContainingBlockSizeChanged() { return pos_elm.HasContainingBlockSizeChanged(); }

	/** Get the value to use when calculating the containing block width for
		absolutely positioned children. The value will include border and
		padding. */

	virtual LayoutCoord
					GetContainingBlockWidth() { return pos_elm.GetContainingBlockWidth(); }

	/** Get the value to use when calculating the containing block height for
		absolutely positioned children. The value will include border and
		padding. */

	virtual LayoutCoord
					GetContainingBlockHeight() { return pos_elm.GetContainingBlockHeight(); }
};

/** Relatively positioned table-cell box that establishes a local stacking context. */

class PositionedZRootTableCellBox
	: public PositionedTableCellBox
{
private:

	/** Ordered list of elements with z-index different from 0 and 'auto' */

	StackingContext	stacking_context;

protected:

	/** Restart z element list. */

	virtual void	RestartStackingContext() { stacking_context.Restart(); }

public:

					PositionedZRootTableCellBox(HTML_Element* element, Content* content)
						: PositionedTableCellBox(element, content) {}

	/** Return the z element list of the object, if it has any. */

	virtual StackingContext*
					GetLocalStackingContext() { return &stacking_context; }

	/** Reflow box and children. */

	virtual LAYST
					Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Traverse z children. */

	virtual void	TraverseZChildren(TraversalObject* traversal_object, LayoutProperties* layout_props, BOOL after, BOOL handle_overflow_clip);
};

/** Table cell box with special borders. */

class TableBorderCellBox
  : public TableCellBox
{
protected:

	/** Border. */

	Border			border;

	/** Modify properties for collapsing cell box. */

	virtual void	ModifyBorderProps(LayoutInfo& info, LayoutProperties* cascade, TableRowBox* table_row);

public:

					TableBorderCellBox(HTML_Element* element, Content* content)
						: TableCellBox(element, content) { border.Reset(); }

	/** Get border. */

	const Border&	GetBorder() const { return border; }

	/** Set bottom border. Return TRUE if size is changed. */

	BOOL			SetBottomBorder(const BorderEdge& bottom);

	/** Paint background and border of this box on screen. */

	virtual void	PaintBgAndBorder(LayoutProperties* cascade, FramesDocument* doc, VisualDevice* visual_device) const;
};

/** Relatively positioned table-cell box with special borders. */

class PositionedTableBorderCellBox
	: public TableBorderCellBox
{
protected:

	/** Positioned element data and Z entry for stacking context. */

	PositionedElement
					pos_elm;

public:

					PositionedTableBorderCellBox(HTML_Element* element, Content* content)
						: TableBorderCellBox(element, content),
						  pos_elm(element) {}

	/** Is this box a positioned box? */

	virtual BOOL	IsPositionedBox() const { return TRUE; }

	/** Return the z element of the object, if it has any. */

	virtual ZElement*
					GetLocalZElement() { return &pos_elm; }

	/** Get offset caused by relative positioning. */

	virtual void	GetPositionedOffset(LayoutCoord& x, LayoutCoord& y) const { pos_elm.GetPositionedOffset(x, y); }

	/** Traverse box with children.

		This method will traverse the contents of this box and its children, recursively. */

	virtual void	TraverseCell(TraversalObject* traversal_object, LayoutProperties* table_lprops, const TableRowBox* row);

	/** Calculate width of the containing block established by this box. */

	virtual void	CalculateContainingBlockWidth() { pos_elm.SetNewContainingBlockWidth(GetWidth()); }

	/** Calculate height of the containing block established by this box.

		Only to be called after the table in which this box lives has been reflowed. */

	virtual void	CalculateContainingBlockHeight() { if (!pos_elm.IsContainingBlockHeightCalculated()) pos_elm.SetNewContainingBlockHeight(GetHeight()); }

	/** Return TRUE if the containing block size changed in the last reflow pass. */

	virtual BOOL	HasContainingBlockSizeChanged() { return pos_elm.HasContainingBlockSizeChanged(); }

	/** Get the value to use when calculating the containing block width for
		absolutely positioned children. The value will include border and
		padding. */

	virtual LayoutCoord
					GetContainingBlockWidth() { return pos_elm.GetContainingBlockWidth(); }

	/** Get the value to use when calculating the containing block height for
		absolutely positioned children. The value will include border and
		padding. */

	virtual LayoutCoord
					GetContainingBlockHeight() { return pos_elm.GetContainingBlockHeight(); }
};

/** Relatively positioned table-cell box with special borders that establishes a local stacking context. */

class PositionedZRootTableBorderCellBox
	: public PositionedTableBorderCellBox
{
private:

	/** Ordered list of elements with z-index different from 0 and 'auto' */

	StackingContext	stacking_context;

protected:

	/** Restart z element list. */

	virtual void	RestartStackingContext() { stacking_context.Restart(); }

public:

					PositionedZRootTableBorderCellBox(HTML_Element* element, Content* content)
						: PositionedTableBorderCellBox(element, content) {}

	/** Return the z element list of the object, if it has any. */

	virtual StackingContext*
					GetLocalStackingContext() { return &stacking_context; }

	/** Reflow box and children. */

	virtual LAYST
					Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Traverse z children. */

	virtual void	TraverseZChildren(TraversalObject* traversal_object, LayoutProperties* layout_props, BOOL after, BOOL handle_overflow_clip);
};

enum TableCellBorderCornerStacking
{
	BORDER_CORNER_STACKING_NOT_SET,
	TOP_LEFT_ON_TOP,
	TOP_RIGHT_ON_TOP,
	BOTTOM_LEFT_ON_TOP,
	BOTTOM_RIGHT_ON_TOP
};

/** Table cell box with collapsing borders. */

class TableCollapsingBorderCellBox
  : public TableBorderCellBox
{
private:

	union
	{
		struct
		{
			unsigned int
				 	top_left_corner:3;

			unsigned int
				 	top_right_corner:3;

			unsigned int
				 	bottom_left_corner:3;

			unsigned int
				 	bottom_right_corner:3;

			/** Is the left border valid? */

			unsigned int
					left_border_collapsed:1;

			/** Is the right border valid? */

			unsigned int
					right_border_collapsed:1;

			/** Is the bottom border valid? */

			unsigned int
					bottom_border_collapsed:1;

			/** Hidden visibility */

			unsigned int
					hidden:1;

			/** Collapsed bottom border width. */

			signed short	collapsed_bottom_border_width:16;
		} packed2;
		unsigned int
					packed2_init;
	};

	/** Collapsed left border width. */

	short			collapsed_left_border_width;

	/** Collapsed right border width. */

	short			collapsed_right_border_width;

	/** Modify properties for collapsing cell box. */

	virtual void	ModifyBorderProps(LayoutInfo& info, LayoutProperties* cascade, TableRowBox* table_row);

public:

					TableCollapsingBorderCellBox(HTML_Element* element, Content* content);

	/** Set left border. */

	short			CollapseLeftBorder(LayoutInfo& info, BorderEdge& left_cell_right_edge);

	/** Set right border. */

	short			CollapseRightBorder(LayoutInfo& info, BorderEdge& right, BOOL collapse = TRUE, BOOL right_is_cell = TRUE);

	/** Set bottom border. */

	short			CollapseBottomBorder(LayoutInfo& info, TableRowGroupBox* row_group, TableRowBox* row, LayoutCoord row_height, const BorderEdge& bottom, BOOL collapse = TRUE);

	/** Paint background and border of this box on screen. */

	virtual void	PaintBgAndBorder(LayoutProperties* cascade, FramesDocument* doc, VisualDevice* visual_device) const;

	/** Get bounding box relative to x,y position of this box */

	virtual void	GetBoundingBox(AbsoluteBoundingBox& box, BOOL include_overflow = TRUE, BOOL adjust_for_multipane = FALSE, BOOL apply_clip = TRUE) const;

	TableCellBorderCornerStacking
					GetTopLeftCorner() const { return (TableCellBorderCornerStacking) packed2.top_left_corner; }
	TableCellBorderCornerStacking
					GetTopRightCorner() const { return (TableCellBorderCornerStacking) packed2.top_right_corner; }
	TableCellBorderCornerStacking
					GetBottomLeftCorner() const { return (TableCellBorderCornerStacking) packed2.bottom_left_corner; }
	TableCellBorderCornerStacking
					GetBottomRightCorner() const { return (TableCellBorderCornerStacking) packed2.bottom_right_corner; }

	void			SetTopLeftCorner(TableCellBorderCornerStacking value) { packed2.top_left_corner = value; }
	void			SetTopRightCorner(TableCellBorderCornerStacking value) { packed2.top_right_corner = value; }
	void			SetBottomLeftCorner(TableCellBorderCornerStacking value) { packed2.bottom_left_corner = value; }
	void			SetBottomRightCorner(TableCellBorderCornerStacking value) { packed2.bottom_right_corner = value; }

	void			PaintCollapsedBorders(PaintObject* paint_object,
										  LayoutCoord width,
										  LayoutCoord height,
										  TableCollapsingBorderCellBox* top_left_cell,
										  TableCollapsingBorderCellBox* left_cell,
										  TableCollapsingBorderCellBox* bottom_left_cell,
										  TableCollapsingBorderCellBox* top_cell,
										  TableCollapsingBorderCellBox* bottom_cell,
										  TableCollapsingBorderCellBox* top_right_cell,
										  TableCollapsingBorderCellBox* right_cell,
										  TableCollapsingBorderCellBox* bottom_right_cell);


	/** Get used border widths, including collapsing table borders */

	virtual void	GetBorderWidths(const HTMLayoutProperties& props, short& top, short& left, short& right, short& bottom, BOOL inner) const;

	virtual void	SetIsHidden() { packed2.hidden = 1; }
};

/** Relatively positioned table-cell box with collapsing borders. */

class PositionedTableCollapsingBorderCellBox
	: public TableCollapsingBorderCellBox
{
protected:

	/** Positioned element data and Z entry for stacking context. */

	PositionedElement
					pos_elm;

public:

					PositionedTableCollapsingBorderCellBox(HTML_Element* element, Content* content)
						: TableCollapsingBorderCellBox(element, content),
						  pos_elm(element) {}

	/** Is this box a positioned box? */

	virtual BOOL	IsPositionedBox() const { return TRUE; }

	/** Return the z element of the object, if it has any. */

	virtual ZElement*
					GetLocalZElement() { return &pos_elm; }

	/** Get offset caused by relative positioning. */

	virtual void	GetPositionedOffset(LayoutCoord& x, LayoutCoord& y) const { pos_elm.GetPositionedOffset(x, y); }

	/** Traverse box with children.

		This method will traverse the contents of this box and its children, recursively. */

	virtual void	TraverseCell(TraversalObject* traversal_object, LayoutProperties* table_lprops, const TableRowBox* row);

	/** Calculate width of the containing block established by this box. */

	virtual void	CalculateContainingBlockWidth() { pos_elm.SetNewContainingBlockWidth(GetWidth()); }

	/** Calculate height of the containing block established by this box.

		Only to be called after the table in which this box lives has been reflowed. */

	virtual void	CalculateContainingBlockHeight() { if (!pos_elm.IsContainingBlockHeightCalculated()) pos_elm.SetNewContainingBlockHeight(GetHeight()); }

	/** Return TRUE if the containing block size changed in the last reflow pass. */

	virtual BOOL	HasContainingBlockSizeChanged() { return pos_elm.HasContainingBlockSizeChanged(); }

	/** Get the value to use when calculating the containing block width for
		absolutely positioned children. The value will include border and
		padding. */

	virtual LayoutCoord
					GetContainingBlockWidth() { return pos_elm.GetContainingBlockWidth(); }

	/** Get the value to use when calculating the containing block height for
		absolutely positioned children. The value will include border and
		padding. */

	virtual LayoutCoord
					GetContainingBlockHeight() { return pos_elm.GetContainingBlockHeight(); }
};

/** Relatively positioned table-cell box with collapsing borders that establishes a local stacking context. */

class PositionedZRootTableCollapsingBorderCellBox
	: public PositionedTableCollapsingBorderCellBox
{
private:

	/** Ordered list of elements with z-index different from 0 and 'auto' */

	StackingContext	stacking_context;

protected:

	/** Restart z element list. */

	virtual void	RestartStackingContext() { stacking_context.Restart(); }

public:

					PositionedZRootTableCollapsingBorderCellBox(HTML_Element* element, Content* content)
						: PositionedTableCollapsingBorderCellBox(element, content) {}

	/** Return the z element list of the object, if it has any. */

	virtual StackingContext*
					GetLocalStackingContext() { return &stacking_context; }

	/** Reflow box and children. */

	virtual LAYST
					Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Traverse z children. */

	virtual void	TraverseZChildren(TraversalObject* traversal_object, LayoutProperties* layout_props, BOOL after, BOOL handle_overflow_clip);
};

/** Table row box. */

class TableRowBox
  : public Box,
    public Link
{
private:

	/** Height of row. */

	LayoutCoord		height;

	/** Y position of box. */

	LayoutCoord		y_pos;

	/** Baseline. */

	LayoutCoord baseline;

	union
	{
		struct
		{
			unsigned int
					spec_height_pixels:1;

			/** Does the row have a specified height? */

			unsigned int
					has_spec_height:1;

			/** Row is rowspanned from by own or preceding cell.

				If this is TRUE, it means that we cannot break page after this row. */

			unsigned int
					rowspanned_from:1;

			/** Row has a trailing page break. */

			unsigned int
					has_trailing_page_break:1;

			/** Row has a pending page break. */

			unsigned int
					pending_page_break:1;

			/** Paged media causes vertical align to be . */

			unsigned int
					override_vertical_align:1;

			/** TRUE if this row group lives in a paged or multicol container. */

			unsigned int
					in_multipane:1;

			/** Is this a wrapped row? (AMSR) */

			unsigned int
					is_wrapped:1;

			/** Specified percentage height in layout fixed point format.
				The maximum actual percent is 2^24 / LAYOUT_FIXED_POINT_BASE. */

			unsigned int
					spec_height_percentage:24;

		} packed;
		UINT32		packed_init;
	};

	union
	{
		struct
		{
			/** visibility set to collapse? */

			unsigned int
					is_visibility_collapsed:1;

			/** Does this row have overflowed cells? */

			unsigned int
					overflowed:1;

			/** Can/must we break page before this block? See type BREAK_POLICY. */

			unsigned int
					page_break_before:3;

			/** Can/must we break page after this block? See type BREAK_POLICY. */

			unsigned int
					page_break_after:3;

			/** Can/must we break column before this block? See type BREAK_POLICY. */

			unsigned int
					column_break_before:2;

			/** Can/must we break column after this block? See type BREAK_POLICY. */

			unsigned int
					column_break_after:2;
		} packed2;
		UINT32		packed2_init;
	};

	/** List of cells. */

	Head			cells;

	union
	{
		/** State of reflow in box.

			When LSB is clear, it points to the html element of this box.
			Otherwise it points at the reflow state.  This is deleted
			when reflow is complete. */

		UINTPTR
					data;

		HTML_Element*
					html_element;
	} un;

	/** Initialise reflow state. */

	TableRowBoxReflowState*
					InitialiseReflowState();

	/** Delete reflow state.

		This is called on FinishLayout(). */

	virtual void	DeleteReflowState();

protected:

	/** Get reflow state. */

	TableRowBoxReflowState*
					GetReflowState() const { if (un.data & 1) return (TableRowBoxReflowState*) (un.data & ~1); else return NULL; }

public:

					TableRowBox(HTML_Element* element);
	virtual			~TableRowBox() { Out(); cells.RemoveAll(); DeleteReflowState(); }

#ifdef LAYOUT_TRAVERSE_DIRTY_TREE

	/** Signal that the HTML_Element associated with this layout box about to be deleted. */

	virtual void	SignalHtmlElementDeleted(FramesDocument* doc);

#endif // LAYOUT_TRAVERSE_DIRTY_TREE

	virtual const HTMLayoutProperties*
					GetHTMLayoutProperties() const { OP_ASSERT(un.data & 1); return ((TableRowBoxReflowState*) (un.data & ~1))->cascade->GetProps(); }

	/** Get HTML element this box belongs to. */

	virtual HTML_Element*
					GetHtmlElement() const { if (un.data & 1) return ((TableRowBoxReflowState*) (un.data & ~1))->html_element; else return un.html_element; }

	/** Get X position of row. */

	LayoutCoord		GetPositionedX() const { LayoutCoord rel_x, rel_y; GetPositionedOffset(rel_x, rel_y); return rel_x; }

	/** Get the table row group box to which this table row belongs. */

	TableRowGroupBox*
					GetTableRowGroupBox() const { return GetReflowState()->row_group_box; }

	/** Get Y position of row. */

	LayoutCoord		GetPositionedY() const { return y_pos; }

	/** Get Y position, ignoring any offset caused by relative positioning. */

	LayoutCoord		GetStaticPositionedY() const { LayoutCoord rel_x, rel_y; GetPositionedOffset(rel_x, rel_y); return y_pos - rel_y; }

	/** Set Y position of row. */

	void			SetPositionedY(LayoutCoord new_y) { y_pos = new_y; }

	/** Set Y position of row, ignoring any offset caused by relative positioning. */

	void			SetStaticPositionedY(LayoutCoord new_y) { LayoutCoord rel_x, rel_y; GetPositionedOffset(rel_x, rel_y); y_pos = new_y + rel_y; }

	/** Increase y position (or decrease, if using negative values). */

	void			IncreaseY(LayoutInfo& info, LayoutCoord increase);

	/** Is row wrapped? (AMSR) */

	BOOL			IsWrapped() const { return packed.is_wrapped; }

	/** Is table wrapped? (AMSR) */

	BOOL			IsTableWrapped() const;

	/** Get offset caused by relative positioning. */

	virtual void	GetPositionedOffset(LayoutCoord& x, LayoutCoord& y) const { x = LayoutCoord(0); y = LayoutCoord(0); }

	/** Get first cell in this row. */

	TableCellBox*	GetFirstCell() const { return (TableCellBox*) cells.First(); }

	/** Get last cell in this row. */

	TableCellBox*	GetLastCell() const { return (TableCellBox*) cells.Last(); }

	/** Get cell spanning 'column'. */

	TableCellBox*	GetCell(unsigned short column) const;

	/** Get next column to be laid out. */

	unsigned short	GetNextColumn() const { return GetReflowState()->next_column; }

	/** Is this box one of the table boxes or a box with table content? */

	virtual BOOL	IsTableBox() const { return TRUE; }

	/** Is this box a table row? */

	virtual BOOL	IsTableRow() const { return TRUE; }

	/** Is this box (and potentially also its children) columnizable?

		A box may only be columnizable if it lives in a paged or multicol container.

		A descendant of a box that isn't columnizable can never be columnizable (unless
		there's a new paged or multicol container between that box and its descendant).

		Table-rows, table-captions, table-row-groups and block-level descendant boxes of
		a paged or multicol container are typically columnizable. Being columnizable
		means that they need to be taken into account by the ancestor paged or multicol
		container somehow. They may become a start or stop element of a column or
		page. It may also be possible to split them over multiple columns or pages, but
		that depends on details about the box type, and that is where the
		"require_children_columnizable" parameter comes into play.

		@param require_children_columnizable If TRUE, only return TRUE if this box is of
		such a type that not only the box itself is columnizable, but also that it
		doesn't prevent its children from becoming columnizable. A table-row, block with
		scrollbars, absolutely-positioned box or float prevents children from being
		columnizable (FALSE will be returned it this parameter is TRUE), but the box
		itself may be very well be columnizable. Example: an absolutely positioned box
		may live inside of a paged or multicol container (so that its Y position is
		stored as a virtual Y position within the paged or multicol container), but its
		descendants do not have to worry about the multi-columnness or multi-pagedness.

		@return TRUE if this box (and possibly its children too) is columnizable. Return
		FALSE if this box isn't columnizable, or if require_children_columnizable is set
		and the children aren't columnizable. */

	virtual BOOL	IsColumnizable(BOOL require_children_columnizable = FALSE) const;

	/** Recalculate containing block size and schedule the specified descendant box for reflow if needed. */

	virtual void	RecalculateContainingBlock(FramesDocument* doc, AbsolutePositionedBox* box);

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Finish reflowing box. */

	virtual LAYST	FinishLayout(LayoutInfo& info);

	/** Update screen. */

	virtual void	UpdateScreen(LayoutInfo& info);

	/** Close any rowspanned cells that might not be expired. */

	void			CloseRowspannedCell(LayoutInfo& info, unsigned short column, TableContent* table);

	/** Invalidate the screen area that the box uses. */

	virtual void	Invalidate(LayoutProperties* parent_cascade, LayoutInfo& info) const;

	/** Add new table cell. Return FALSE on OOM, TRUE otherwise. */

	BOOL			AddCell(LayoutCoord* x, LayoutInfo& info, TableContent* table, TableCellBox* cell, int colspan, int rowspan, LayoutCoord desired_width, LayoutCoord* cell_width);

	/** Propagate minimum and maximum table cell widths. */

	void			PropagateCellWidths(const LayoutInfo& info, int column, int colspan, LayoutCoord desired_width, LayoutCoord min_width, LayoutCoord normal_min_width, LayoutCoord max_width);

	/** Propagate the bounding box of cell's absolutely positioned descendant to the table.

		@param info Layout information structure
		@param child_bounding_box Bounding-box of the propagating descendant, joined with the
		bounding-boxes of the elements up the parent chain towards this element. Relative to the
		top border-edge of this box.
		@param opts Bounding box propagation options */

	void			PropagateCellBottom(const LayoutInfo& info, const AbsoluteBoundingBox& child_bounding_box, PropagationOptions opts);

	/** Get height. */

	virtual LayoutCoord
					GetHeight() const { return height; }

	/** Grow height.

		Use with care. */
	void			GrowHeight(LayoutCoord increment) { height += increment; }

	/** Grow row to height.

		Propagates height to table.  Should only be called for the
	    last row in a table. */

	void			GrowRow(LayoutInfo& info, LayoutCoord min_height, LayoutCoord baseline, BOOL set_specifyed_height = FALSE);

	/** Get cell spanning 'column' with rowspan at least as big as 'rowspan'. */

	static TableCellBox*
					GetRowSpannedCell(TableRowBox* row, unsigned int column, int rowspan, BOOL table_is_wrapped, int* relative_row_number = NULL, TableRowBox** foreign_row = NULL);

	/** The row, or one of its cells, has non-auto height. */

	void			SetHasSpecifiedHeight() { packed.has_spec_height = 1; }

	/** Does the row, or any of its cells, have non-auto height? */

	BOOL			HasSpecifiedHeight() const { return packed.has_spec_height; }

	LayoutFixed		GetSpecifiedHeightPercentage() const { return LayoutFixed(packed.spec_height_percentage); }
	BOOL			GetSpecifiedHeightPixels() const { return packed.spec_height_pixels && height; }

	void			ClearSpecifiedHeightPercentage() { packed.spec_height_percentage = 0; }
	void			SetSpecifiedHeightPixels(BOOL pixels) { packed.spec_height_pixels = pixels; }

	/** Get baseline of row. */

	LayoutCoord		GetBaseline() const { return baseline; }

	/** Calculate baseline of row. */

	LayoutCoord		CalculateBaseline() const;

	/** Update baseline */

	void			UpdateBaseline() { baseline = CalculateBaseline(); }

	/** Traverse box with children.

		This method will traverse the contents of this box and its children, recursively. */

	virtual void	TraverseRow(TraversalObject* traversal_object, LayoutProperties* table_lprops, TableContent* table);

	/** Traverse z children. */

	virtual void	TraverseZChildren(TraversalObject* traversal_object, LayoutProperties* table_lprops, BOOL after, BOOL handle_overflow_clip) {}

	/** Traverse the content of this box, i.e. its children. */

	virtual void	TraverseContent(TraversalObject* traversal_object, LayoutProperties* layout_props);

	/** Calculate width of the containing block established by this box. */

	virtual void	CalculateContainingBlockWidth() {}

	/** Calculate height of the containing block established by this box.

		Only to be called after the table in which this box lives has been reflowed. */

	virtual void	CalculateContainingBlockHeight() { OP_ASSERT(IsPositionedBox()); }

	/** Return TRUE if the containing block size changed in the last reflow pass. */

	virtual BOOL	HasContainingBlockSizeChanged() { OP_ASSERT(!"Missing re-implementation of this method"); return FALSE; }

#ifdef PAGED_MEDIA_SUPPORT

	/** Attempt to break page. */

	BREAKST			AttemptPageBreak(LayoutInfo& info, int strength, TableRowBox*& potential_break);

	/** Insert a page break. */

	BREAKST			InsertPageBreak(LayoutInfo& info, int strength);

	/** Paged media causes vertical align to be overridden. */

	BOOL			OverrideVerticalAlign() const { return packed.override_vertical_align; }

	/** Return IS_TRUE if this table-row crosses page boundaries. */

	OP_BOOLEAN		SpansMultiplePages(const LayoutInfo& info) const;

#endif // PAGED_MEDIA_SUPPORT

	/** Get page break policy before this layout element. */

	BREAK_POLICY	GetPageBreakPolicyBefore() const { return (BREAK_POLICY) packed2.page_break_before; }

	/** Get page break policy after this layout element. */

	BREAK_POLICY	GetPageBreakPolicyAfter() const { return (BREAK_POLICY) packed2.page_break_after; }

	/** Get column break policy before this layout element. */

	BREAK_POLICY	GetColumnBreakPolicyBefore() const { return (BREAK_POLICY) packed2.column_break_before; }

	/** Get column break policy after this layout element. */

	BREAK_POLICY	GetColumnBreakPolicyAfter() const { return (BREAK_POLICY) packed2.column_break_after; }

	/** Collapse top border (in collapsing border model). */

	virtual void	CollapseTopBorder() {}

	/** Collapse bottom border (in collapsing border model). */

	virtual void	CollapseBottomBorder(const BorderEdge& bottom_border) {}

	/** Finish right border (in collapsing border model). In a rtl table this will instead be the left border in the leftmost column. */

	virtual void	FinishLastBorder(LayoutInfo& info, int number_of_columns, const BorderEdge& table_last_border) {}

	/** Finish bottom border (in collapsing border model). */

	virtual void	FinishBottomBorder(LayoutInfo& info, const BorderEdge& bottom_border, TableRowGroupBox* row_group, TableContent* table) {}

	/** Get old row height. */

	LayoutCoord		GetOldHeight() const { TableRowBoxReflowState* state = GetReflowState(); return state ? state->old_height : LayoutCoord(0); }

	/** Get old row baseline. */

	LayoutCoord		GetOldBaseline() const { TableRowBoxReflowState* state = GetReflowState(); return state ? state->old_baseline : LayoutCoord(0); }

	/** Is row rowspanned from by own or preceding cell?

		This means that we cannot break page after this row. */

	BOOL			RowspannedFrom() const { return packed.rowspanned_from; }

	/** Propagate minimum height from table cell. */

	void			PropagateMinHeight(LayoutCoord minimum_height) { TableRowBoxReflowState* state = GetReflowState(); if (state->minimum_height < minimum_height) state->minimum_height = minimum_height; }

	/** Is there a page break after this row? */

	BOOL			HasTrailingPagebreak() const { return packed.has_trailing_page_break; }

	/** Is there a pending page break after this row? */

	BOOL			HasPendingPagebreak() { BOOL pending_page_break = packed.pending_page_break; packed.pending_page_break = FALSE; return pending_page_break; }

	/** Remove pending page break info. */

	virtual void	RemovePendingPagebreak() { packed.pending_page_break = FALSE; packed.has_trailing_page_break = FALSE; }

	/** Is this a border collapsing row box? */

	virtual BOOL	IsTableCollapsingBorderRowBox() const { return FALSE; }

	/** Use old row height? */

	BOOL			UseOldRowHeight() const { OP_ASSERT(GetReflowState()); return GetReflowState()->use_old_height; }

	/** Get bounding box values. */

	virtual short	GetBoundingBoxTop() const { return 0; }
	virtual short	GetBoundingBoxBottom() const { return 0; }

	/** Notify the table row that a child cell has been overflowed. */

	void			SetHasOverflowedCell() { packed2.overflowed = 1; }

	/** Does child content overflow this row? */

	BOOL			IsOverflowed() const { return !!packed2.overflowed; }

	/** @return TRUE if visibility == collapse. */

	BOOL			IsVisibilityCollapsed() const { return packed2.is_visibility_collapsed == 1; }

	/** Get used border widths, including collapsing table borders */

	virtual void	GetBorderWidths(const HTMLayoutProperties& props, short& top, short& left, short& right, short& bottom, BOOL inner) const;

	/** Return TRUE if this row is inside of a paged or multicol container. */

	BOOL			IsInMultiPaneContainer() const { return packed.in_multipane == 1; }
};

/** Relatively positioned table row box. */

class PositionedTableRowBox
	: public TableRowBox
{
protected:

	/** Positioned element data and Z entry for stacking context. */

	PositionedElement
					pos_elm;

public:

					PositionedTableRowBox(HTML_Element* element)
						: TableRowBox(element),
						  pos_elm(element) {}

	/** Is this box a positioned box? */

	virtual BOOL	IsPositionedBox() const { return TRUE; }

	/** Return the z element of the object, if it has any. */

	virtual ZElement*
					GetLocalZElement() { return &pos_elm; }

	/** Get offset caused by relative positioning. */

	virtual void	GetPositionedOffset(LayoutCoord& x, LayoutCoord& y) const { pos_elm.GetPositionedOffset(x, y); }

	/** Traverse box with children.

		This method will traverse the contents of this box and its children, recursively. */

	virtual void	TraverseRow(TraversalObject* traversal_object, LayoutProperties* table_lprops, TableContent* table);

	/** Calculate width of the containing block established by this box. */

	virtual void	CalculateContainingBlockWidth() { pos_elm.SetNewContainingBlockWidth(GetReflowState()->cascade->table->GetRowWidth()); }

	/** Calculate height of the containing block established by this box.

		Only to be called after the table in which this box lives has been reflowed. */

	virtual void	CalculateContainingBlockHeight() { if (!pos_elm.IsContainingBlockHeightCalculated()) pos_elm.SetNewContainingBlockHeight(GetHeight()); }

	/** Return TRUE if the containing block size changed in the last reflow pass. */

	virtual BOOL	HasContainingBlockSizeChanged() { return pos_elm.HasContainingBlockSizeChanged(); }

	/** Get the value to use when calculating the containing block width for
		absolutely positioned children. The value will include border and
		padding. */

	virtual LayoutCoord
					GetContainingBlockWidth() { return pos_elm.GetContainingBlockWidth(); }

	/** Get the value to use when calculating the containing block height for
		absolutely positioned children. The value will include border and
		padding. */

	virtual LayoutCoord
					GetContainingBlockHeight() { return pos_elm.GetContainingBlockHeight(); }
};

/** Relatively positioned table row box that establishes a local stacking context. */

class PositionedZRootTableRowBox
	: public PositionedTableRowBox
{
private:

	/** Ordered list of elements with z-index different from 0 and 'auto' */

	StackingContext	stacking_context;

protected:

	/** Restart z element list. */

	void			RestartStackingContext() { stacking_context.Restart(); }

public:

					PositionedZRootTableRowBox(HTML_Element* element)
						: PositionedTableRowBox(element) {}

	/** Return the z element list of the object, if it has any. */

	virtual StackingContext*
					GetLocalStackingContext() { return &stacking_context; }

	/** Reflow box and children. */

	virtual LAYST
					Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Traverse z children. */

	virtual void	TraverseZChildren(TraversalObject* traversal_object, LayoutProperties* table_lprops, BOOL after, BOOL handle_overflow_clip);
};

/** Table row box with collapsing borders. */

class TableCollapsingBorderRowBox
  : public TableRowBox
{
private:

	/** Border. */

	Border			border;

	/** Top edge of bounding box */

	short			bounding_box_top;

	/** Bottom edge of bounding box */

	short			bounding_box_bottom;

public:

					TableCollapsingBorderRowBox(HTML_Element* element);

	/** Get border. */

	const Border&	GetBorder() const { return border; }

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Collapse top border (in collapsing border model). */

	virtual void	CollapseTopBorder();

	/** Collapse bottom border (in collapsing border model). */

	virtual void	CollapseBottomBorder(const BorderEdge& bottom_border);

	/** Finish right border (in collapsing border model). In a rtl table this will instead be the left border in the leftmost column.*/

	virtual void	FinishLastBorder(LayoutInfo& info, int number_of_columns, const BorderEdge& table_last_border);

	/** Finish bottom border (in collapsing border model). */

	virtual void	FinishBottomBorder(LayoutInfo& info, const BorderEdge& bottom_border, TableRowGroupBox* row_group, TableContent* table);

	/** Is this a border collapsing row box? */

	virtual BOOL	IsTableCollapsingBorderRowBox() const { return TRUE; }

	/** Update bounding box. */

	void			UpdateBoundingBox(short top, short bottom);

	/** Get bounding box values. */

	virtual short	GetBoundingBoxTop() const { return bounding_box_top; }
	virtual short	GetBoundingBoxBottom() const { return bounding_box_bottom; }

	/** Get previous. */

	TableCollapsingBorderRowBox*
					GetPrevious() const;

	/** Get used border widths, including collapsing table borders */

	virtual void	GetBorderWidths(const HTMLayoutProperties& props, short& top, short& left, short& right, short& bottom, BOOL inner) const;
};

/** Relatively positioned table row box with collapsing borders. */

class PositionedTableCollapsingBorderRowBox
	: public TableCollapsingBorderRowBox
{
protected:

	/** Positioned element data and Z entry for stacking context. */

	PositionedElement
					pos_elm;

public:

					PositionedTableCollapsingBorderRowBox(HTML_Element* element)
						: TableCollapsingBorderRowBox(element),
						  pos_elm(element) {}

	/** Is this box a positioned box? */

	virtual BOOL	IsPositionedBox() const { return TRUE; }

	/** Return the z element of the object, if it has any. */

	virtual ZElement*
					GetLocalZElement() { return &pos_elm; }

	/** Get offset caused by relative positioning. */

	virtual void	GetPositionedOffset(LayoutCoord& x, LayoutCoord& y) const { pos_elm.GetPositionedOffset(x, y); }

	/** Traverse box with children.

		This method will traverse the contents of this box and its children, recursively. */

	virtual void	TraverseRow(TraversalObject* traversal_object, LayoutProperties* table_lprops, TableContent* table);

	/** Calculate width of the containing block established by this box. */

	virtual void	CalculateContainingBlockWidth() { pos_elm.SetNewContainingBlockWidth(GetReflowState()->cascade->table->GetRowWidth()); }

	/** Calculate height of the containing block established by this box.

		Only to be called after the table in which this box lives has been reflowed. */

	virtual void	CalculateContainingBlockHeight() { if (!pos_elm.IsContainingBlockHeightCalculated()) pos_elm.SetNewContainingBlockHeight(GetHeight()); }

	/** Return TRUE if the containing block size changed in the last reflow pass. */

	virtual BOOL	HasContainingBlockSizeChanged() { return pos_elm.HasContainingBlockSizeChanged(); }

	/** Get the value to use when calculating the containing block width for
		absolutely positioned children. The value will include border and
		padding. */

	virtual LayoutCoord
					GetContainingBlockWidth() { return pos_elm.GetContainingBlockWidth(); }

	/** Get the value to use when calculating the containing block height for
		absolutely positioned children. The value will include border and
		padding. */

	virtual LayoutCoord
					GetContainingBlockHeight() { return pos_elm.GetContainingBlockHeight(); }
};

/** Relatively positioned table row box with collapsing borders that establishes a local stacking context. */

class PositionedZRootTableCollapsingBorderRowBox
	: public PositionedTableCollapsingBorderRowBox
{
private:

	/** Ordered list of elements with z-index different from 0 and 'auto' */

	StackingContext	stacking_context;

protected:

	/** Restart z element list. */

	void			RestartStackingContext() { stacking_context.Restart(); }

public:

					PositionedZRootTableCollapsingBorderRowBox(HTML_Element* element)
						: PositionedTableCollapsingBorderRowBox(element) {}

	/** Return the z element list of the object, if it has any. */

	virtual StackingContext*
					GetLocalStackingContext() { return &stacking_context; }

	/** Reflow box and children. */

	virtual LAYST
					Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Traverse z children. */

	virtual void	TraverseZChildren(TraversalObject* traversal_object, LayoutProperties* table_lprops, BOOL after, BOOL handle_overflow_clip);
};

/** Table content reflow state. */

class TableRowGroupBoxReflowState
  : public ReflowState
{
public:

	/** border of row group. */

	Border			border;

	BOOL			border_collapsing;

	LayoutCoord		border_spacing_vertical;

	BOOL			rules_groups;

	/** Bottom of reflow. */

	LayoutCoord		reflow_position;

	/** Old y position of group. */

	LayoutCoord		old_y;

	/** Old height of group. */

	LayoutCoord		old_height;

	/** Height of row group if the table's maximum_width is satisfied. */

	LayoutCoord		minimum_height;

	/** Previous y root established by positioned box. */

	LayoutCoord		previous_root_y;

	/** Old x position of group. */

	LayoutCoord		old_x;

	/** Previous x root established by positioned box. */

	LayoutCoord		previous_root_x;

	/** Bounding box. */

	AbsoluteBoundingBox
					bounding_box;

					TableRowGroupBoxReflowState() { border.Reset(); }

	void*			operator new(size_t nbytes) OP_NOTHROW { return g_tablerowgroup_reflow_state_pool->New(sizeof(TableRowGroupBoxReflowState)); }
	void			operator delete(void* p, size_t nbytes) { g_tablerowgroup_reflow_state_pool->Delete(p); }
};

enum TableRowGroupType
{
	TABLE_ROW_GROUP = 0,
	TABLE_HEADER_GROUP,
	TABLE_FOOTER_GROUP
};

/** Table row group box. */

class TableRowGroupBox
  : public Box,
    public TableListElement
{
private:

	union
	{
		struct
		{
			/** Y position of row group. */

			int	 	y_pos:30;

			/** Type of group. */

			unsigned int
					type:2;
		} packed;
		unsigned long
					packed_init;
	};


	union
	{
		struct
		{
			/** visibility set to collapse? */

			unsigned int
					is_visibility_collapsed:1;

			/** Does this row-group have overflowed rows or cells? */

			unsigned int
					overflowed:1;

			/** Avoid page break inside table row group? */

			unsigned int
					avoid_page_break_inside:1;

			/** Avoid column break inside table row group? */

			unsigned int
					avoid_column_break_inside:1;

			/** Can/must we break page before this row group? See type BREAK_POLICY. */

			unsigned int
					page_break_before:3;

			/** Can/must we break page after this row group? See type BREAK_POLICY. */

			unsigned int
					page_break_after:3;

			/** Can/must we break column before this row group? See type BREAK_POLICY. */

			unsigned int
					column_break_before:2;

			/** Can/must we break column after this row group? See type BREAK_POLICY. */

			unsigned int
					column_break_after:2;

			/** TRUE if this row group lives in a paged or multicol container. */

			unsigned int
					in_multipane:1;
		} packed2;
		UINT32		packed2_init;
	};

	/** Stack of rows. */

	Head			rows;

	union
	{
		/** State of reflow in box.

			When LSB is clear, it points to the html element of this box.
			Otherwise it points at the reflow state.  This is deleted
			when reflow is complete. */

		UINTPTR
					data;

		/** Html element that this box belongs to. */

		HTML_Element*
					html_element;
	} un;

	/** Initialise reflow state. */

	TableRowGroupBoxReflowState*
					InitialiseReflowState();

public:

					TableRowGroupBox(HTML_Element* element) { packed_init = 0; packed2_init = 0; un.html_element = element; }
	virtual			~TableRowGroupBox() { Out(); rows.RemoveAll(); DeleteReflowState(); }

#ifdef LAYOUT_TRAVERSE_DIRTY_TREE

	/** Signal that the HTML_Element associated with this layout box about to be deleted. */

	virtual void	SignalHtmlElementDeleted(FramesDocument* doc);

#endif // LAYOUT_TRAVERSE_DIRTY_TREE

	/** Traverse box. */

	virtual void	Traverse(TraversalObject* traversal_object, LayoutProperties* table_lprops, TableContent* table);

	/** Traverse z children. */

	virtual void	TraverseZChildren(TraversalObject* traversal_object, LayoutProperties* layout_props, BOOL after, BOOL handle_overflow_clip) {}

	/** Traverse the content of this box, i.e. its children. */

	virtual void	TraverseContent(TraversalObject* traversal_object, LayoutProperties* layout_props);

	/** Traverse all rows in the table-row-group. */

	void			TraverseAllRows(TraversalObject* traversal_object, LayoutProperties* table_lprops, TableContent* table);

	/** Calculate width of the containing block established by this box. */

	virtual void	CalculateContainingBlockWidth() {}

	/** Calculate height of the containing block established by this box.

		Only to be called after the table in which this box lives has been reflowed. */

	virtual void	CalculateContainingBlockHeight() { OP_ASSERT(IsPositionedBox()); }

	/** Return TRUE if the containing block size changed in the last reflow pass. */

	virtual BOOL	HasContainingBlockSizeChanged() { OP_ASSERT(!"Missing re-implementation of this method"); return FALSE; }

#ifdef PAGED_MEDIA_SUPPORT

	/** Attempt to break page. */

	virtual BREAKST	AttemptPageBreak(LayoutInfo& info, int strength);

	/** Insert a page break. */

	BREAKST			InsertPageBreak(LayoutInfo& info, int strength);

#endif // PAGED_MEDIA_SUPPORT

	/** Get page break policy before this layout element. */

	virtual BREAK_POLICY
					GetPageBreakPolicyBefore() const { return (BREAK_POLICY) packed2.page_break_before; }

	/** Get page break policy after this layout element. */

	virtual BREAK_POLICY
					GetPageBreakPolicyAfter() const { return (BREAK_POLICY) packed2.page_break_after; }

	/** Get column break policy before this layout element. */

	virtual BREAK_POLICY
					GetColumnBreakPolicyBefore() const { return (BREAK_POLICY) packed2.column_break_before; }

	/** Get column break policy after this layout element. */

	virtual BREAK_POLICY
					GetColumnBreakPolicyAfter() const { return (BREAK_POLICY) packed2.column_break_after; }

	/** Get HTML element this box belongs to. */

	virtual HTML_Element*
					GetHtmlElement() const { if (un.data & 1) return ((TableRowGroupBoxReflowState*) (un.data & ~1))->html_element; else return un.html_element; }

	virtual const HTMLayoutProperties*
					GetHTMLayoutProperties() const { OP_ASSERT(un.data & 1); return ((ReflowState*) (un.data & ~1))->cascade->GetProps(); }

	/** Get border. */

	const Border*	GetBorder() const { if (un.data & 1) return &((TableRowGroupBoxReflowState*) (un.data & ~1))->border; else return NULL; }

	/** Is this a row group? */

	virtual BOOL	IsRowGroup() const { return TRUE; }

	/** Is this box one of the table boxes or a box with table content? */

	virtual BOOL	IsTableBox() const { return TRUE; }

	/** Is this box a table row group? */

	virtual BOOL	IsTableRowGroup() const { return TRUE; }

	/** Is this box (and potentially also its children) columnizable?

		A box may only be columnizable if it lives in a paged or multicol container.

		A descendant of a box that isn't columnizable can never be columnizable (unless
		there's a new paged or multicol container between that box and its descendant).

		Table-rows, table-captions, table-row-groups and block-level descendant boxes of
		a paged or multicol container are typically columnizable. Being columnizable
		means that they need to be taken into account by the ancestor paged or multicol
		container somehow. They may become a start or stop element of a column or
		page. It may also be possible to split them over multiple columns or pages, but
		that depends on details about the box type, and that is where the
		"require_children_columnizable" parameter comes into play.

		@param require_children_columnizable If TRUE, only return TRUE if this box is of
		such a type that not only the box itself is columnizable, but also that it
		doesn't prevent its children from becoming columnizable. A table-row, block with
		scrollbars, absolutely-positioned box or float prevents children from being
		columnizable (FALSE will be returned it this parameter is TRUE), but the box
		itself may be very well be columnizable. Example: an absolutely positioned box
		may live inside of a paged or multicol container (so that its Y position is
		stored as a virtual Y position within the paged or multicol container), but its
		descendants do not have to worry about the multi-columnness or multi-pagedness.

		@return TRUE if this box (and possibly its children too) is columnizable. Return
		FALSE if this box isn't columnizable, or if require_children_columnizable is set
		and the children aren't columnizable. */

	virtual BOOL	IsColumnizable(BOOL require_children_columnizable = FALSE) const;

	/** Get first row. */

	TableRowBox*	GetFirstRow() { return (TableRowBox*) rows.First(); }

	/** Get last row. */

	TableRowBox*	GetLastRow() { return (TableRowBox*) rows.Last(); }

	TableRowGroupType
		GetType() const { return (TableRowGroupType) packed.type; }

	/** Get x position. */

	virtual LayoutCoord
					GetPositionedX() const { LayoutCoord x_offs; LayoutCoord y_offs; GetPositionedOffset(x_offs, y_offs); return x_offs + GetStaticPositionedX(); }

	/** Get x position, ignoring offset caused by relative positioning. */

	LayoutCoord		GetStaticPositionedX() const { return LayoutCoord(GetHtmlElement()->Parent()->GetLayoutBox()->GetTableContent()->GetRowLeftOffset()); }

	/** Get Y position of row group. */

	virtual LayoutCoord
					GetPositionedY() const { return LayoutCoord(packed.y_pos); }

	/** Set y position, ignoring offset caused by relative positioning. */

	void			SetStaticPositionedY(LayoutCoord new_y) { LayoutCoord x_offs, y_offs; GetPositionedOffset(x_offs, y_offs); packed.y_pos = LayoutCoord(new_y + y_offs); }

	/** Get y position, ignoring offset caused by relative positioning. */

	virtual LayoutCoord
					GetStaticPositionedY() const { LayoutCoord x_offs; LayoutCoord y_offs; GetPositionedOffset(x_offs, y_offs); return LayoutCoord(packed.y_pos) - y_offs; }

	/** Increase y position (or decrease, if using negative values). */

	virtual void	IncreaseY(LayoutInfo& info, TableContent* table, LayoutCoord increase);

	/** Get height. */

	virtual LayoutCoord
					GetHeight() const;

	/** Get offset caused by relative positioning. */

	virtual void	GetPositionedOffset(LayoutCoord& x, LayoutCoord& y) const { x = LayoutCoord(0); y = LayoutCoord(0); }

	/** Recalculate containing block size and schedule the specified descendant box for reflow if needed. */

	virtual void	RecalculateContainingBlock(FramesDocument* doc, AbsolutePositionedBox* box);

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Finish reflowing box. */

	virtual LAYST	FinishLayout(LayoutInfo& info);

	/** Update screen. */

	virtual void	UpdateScreen(LayoutInfo& info);

	/** Invalidate the screen area that the box uses. */

	virtual void	Invalidate(LayoutProperties* parent_cascade, LayoutInfo& info) const;

	/** Add row to table. */

	BOOL			GetNewRow(LayoutInfo& info, TableRowBox* row_box);

	/** Close rows. */

	void			CloseRow(TableRowGroupBoxReflowState* reflow_state, LayoutInfo& info, BOOL group_finished);

	/** Finish right border on every cell in rightmost column. In a rtl table this will instead be the left border in the leftmost column.*/

	void			FinishLastBorder(TableCollapsingBorderContent* table, LayoutInfo& info);

	/** Get height of spanned rows including vertical cell spacing */

	LayoutCoord		GetRowSpannedHeight(const TableRowBox* row_box, int rowspan);

	/** Grow height. */

	void			GrowRowGroup(LayoutInfo& info, LayoutCoord increment);

	/** Force increase of row height. */

	void			ForceRowHeightIncrease(LayoutInfo& info, TableRowBox* row, LayoutCoord increment, BOOL clear_use_old_row_heights = FALSE);

	/** Get reflow state. */

	TableRowGroupBoxReflowState*
					GetReflowState() const { if (un.data & 1) return (TableRowGroupBoxReflowState*) (un.data & ~1); else return NULL; }

	/** Delete reflow state.

		This is called on FinishLayout(). */

	virtual void	DeleteReflowState();

	/** Add minimum height from table row. */

	void			AddRowMinHeight(LayoutCoord minimum_height) { GetReflowState()->minimum_height += minimum_height; }

	/** Add the bounding box of a child to our bounding box. */

	void			UpdateBoundingBox(const AbsoluteBoundingBox& child_bounding_box) { GetReflowState()->bounding_box.UnionWith(child_bounding_box); }

	/** Get bounding box relative to top/left border edge of this box */

	void			GetBoundingBox(AbsoluteBoundingBox& box) const { box = GetReflowState()->bounding_box; }

	/** Does child content overflow this row-group? */

	BOOL			IsOverflowed() const { return !!packed2.overflowed; }

	/** Specify whether this row-group has overflowing child content. */

	void			SetIsOverflowed(BOOL overflowed) { packed2.overflowed = !!overflowed; }

	/** @return TRUE if visibility == collapse. */

	BOOL			IsVisibilityCollapsed() const { return packed2.is_visibility_collapsed == 1; }

	/** Get used border widths, including collapsing table borders */

	virtual void	GetBorderWidths(const HTMLayoutProperties& props, short& top, short& left, short& right, short& bottom, BOOL inner) const;

	/** Distribute content into columns.

		@return FALSE on OOM, TRUE otherwise. */

	virtual BOOL	Columnize(Columnizer& columnizer, Container* container);

	/** Figure out to which column(s) or spanned element a box belongs. */

	virtual void	FindColumn(ColumnFinder& cf);

	/** Return TRUE if this row group is inside of a paged or multicol container. */

	BOOL			IsInMultiPaneContainer() const { return packed2.in_multipane == 1; }
};

/** Relatively positioned table row group box. */

class PositionedTableRowGroupBox
	: public TableRowGroupBox
{
protected:

	/** Positioned element data and Z entry for stacking context. */

	PositionedElement
					pos_elm;

public:

					PositionedTableRowGroupBox(HTML_Element* element)
						: TableRowGroupBox(element),
						  pos_elm(element) {}

	/** Is this box a positioned box? */

	virtual BOOL	IsPositionedBox() const { return TRUE; }

	/** Return the z element of the object, if it has any. */

	virtual ZElement*
					GetLocalZElement() { return &pos_elm; }

	/** Get offset caused by relative positioning. */

	virtual void	GetPositionedOffset(LayoutCoord& x, LayoutCoord& y) const { pos_elm.GetPositionedOffset(x, y); }

	/** Traverse box. */

	virtual void	Traverse(TraversalObject* traversal_object, LayoutProperties* table_lprops, TableContent* table);

	/** Calculate width of the containing block established by this box. */

	virtual void	CalculateContainingBlockWidth() { pos_elm.SetNewContainingBlockWidth(GetReflowState()->cascade->table->GetRowWidth()); }

	/** Calculate height of the containing block established by this box.

		Only to be called after the table in which this box lives has been reflowed. */

	virtual void	CalculateContainingBlockHeight() { if (!pos_elm.IsContainingBlockHeightCalculated()) pos_elm.SetNewContainingBlockHeight(GetHeight()); }

	/** Return TRUE if the containing block size changed in the last reflow pass. */

	virtual BOOL	HasContainingBlockSizeChanged() { return pos_elm.HasContainingBlockSizeChanged(); }

	/** Get the value to use when calculating the containing block width for
		absolutely positioned children. The value will include border and
		padding. */

	virtual LayoutCoord
					GetContainingBlockWidth() { return pos_elm.GetContainingBlockWidth(); }

	/** Get the value to use when calculating the containing block height for
		absolutely positioned children. The value will include border and
		padding. */

	virtual LayoutCoord
					GetContainingBlockHeight() { return pos_elm.GetContainingBlockHeight(); }
};

/** Relatively positioned table row group box that establishes a local stacking context. */

class PositionedZRootTableRowGroupBox
	: public PositionedTableRowGroupBox
{
private:

	/** Ordered list of elements with z-index different from 0 and 'auto' */

	StackingContext	stacking_context;

protected:

	/** Restart z element list. */

	void			RestartStackingContext() { stacking_context.Restart(); }

public:

					PositionedZRootTableRowGroupBox(HTML_Element* element)
						: PositionedTableRowGroupBox(element) {}

	/** Return the z element list of the object, if it has any. */

	virtual StackingContext*
					GetLocalStackingContext() { return &stacking_context; }

	/** Reflow box and children. */

	virtual LAYST
					Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Traverse z children. */

	virtual void	TraverseZChildren(TraversalObject* traversal_object, LayoutProperties* layout_props, BOOL after, BOOL handle_overflow_clip);
};

/** Table column/column-group box. */

class TableColGroupBox
  : public Box
{
private:

	HTML_Element*	html_element;

	Border*			border;

	COLORREF		bg_color;
	COLORREF		font_color;
	BgImages		bg_images;
	Shadows			box_shadows;

	/** Desired width from CSS. Negative if percentage.
		@see HTMLayoutProperties::content_width. */

	LayoutCoord		desired_width;

	/** Left edge of the (leftmost) column. */

	LayoutCoord		x;

	/** Width of column (or total width of all columns, if column group). */

	LayoutCoord		width;

	union
	{
		struct
		{
			/** Has a descendant box added columns to the table? */

			unsigned int
					has_col_child:1;

			/** Is this a table-column-group or table-column? Due to lack of
				reflow state for this box, this information needs to be
				cached. */

			unsigned int
					is_group:1;

			/** visibility set to collapse? */

			unsigned int
					is_visibility_collapsed:1;
		} col_packed;
		UINT32		col_packed_init;
	};

public:

					TableColGroupBox(HTML_Element* element)
						: html_element(element),
						  border(NULL),
						  desired_width(0),
						  x(0),
						  width(0)
					{
						col_packed_init = 0;
						bg_images.Reset(); box_shadows.Reset();
					}

	virtual			~TableColGroupBox() { OP_DELETE(border); }

#ifdef LAYOUT_TRAVERSE_DIRTY_TREE

	/** Signal that the HTML_Element associated with this layout box about to be deleted. */

	virtual void	SignalHtmlElementDeleted(FramesDocument* doc);

#endif // LAYOUT_TRAVERSE_DIRTY_TREE

	/** Get HTML element this box belongs to. */

	virtual HTML_Element*
					GetHtmlElement() const { return html_element; }

	/** Get border. */

	const Border*	GetBorder() const { return border; }

	/** Get background images. */

	const BgImages& GetBackgroundImages() const { return bg_images; }

	/** Get box shadows. */

	const Shadows*	GetBoxShadows() const { return &box_shadows; }

	/** Get background color. */

	COLORREF		GetBackgroundColor() const { return bg_color; }

	/** Get font color. */

	COLORREF		GetFontColor() const { return font_color; }

	/** Get column desired width. */

	LayoutCoord		GetDesiredWidth() const { return desired_width; }

	/** Is this box one of the table boxes or a box with table content? */

	virtual BOOL	IsTableBox() const { return TRUE; }

	/** Is this box a table column group? */

	virtual BOOL	IsTableColGroup() const { return TRUE; }

	/** Finish reflowing box. */

	virtual LAYST	FinishLayout(LayoutInfo& info);

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Invalidate the screen area that the box uses. */

	virtual void	Invalidate(LayoutProperties* parent_cascade, LayoutInfo& info) const;

	/** Collapse left border with some adjacent border. */

	void			CollapseLeftBorder(const BorderEdge& edge) { if (border) border->left.Collapse(edge); }

	/** Collapse right border with some adjacent border. */

	void			CollapseRightBorder(const BorderEdge& edge) { if (border) border->right.Collapse(edge); }

	/** Set left edge position (of leftmost column). */

	void			SetX(LayoutCoord x) { this->x = x; }

	/** Get left edge position (of leftmost column). */

	LayoutCoord		GetX() const { return x; }

	/** Set width of column (or total width of all columns that a column group spans). */

	void			SetWidth(LayoutCoord width) { this->width = width; }

	/** Get width of column (or total width of all columns that a column group spans). */

	LayoutCoord		GetWidth() const { return width; }

	/** Is this a table-column-group (as opposed to table-column)? */

	BOOL			IsGroup() { return !!col_packed.is_group; }

	/** A descendant of this box has added columns to the table (which means that this box shouldn't add any). */

	void			SetHasColChild() { col_packed.has_col_child = 1; }

	/** Is it a child of this box that has added columns for the table? */

	BOOL			HasColChild() { return col_packed.has_col_child; }

	/* Is this the first table-column in the table-column-group
	   (assuming it is a table-column) */

	BOOL			IsFirstColInGroup();

	/** @return TRUE if visibility of the table-column or table-column-group box defining this column is collapsed. */

	BOOL			IsVisibilityCollapsed() const { return col_packed.is_visibility_collapsed; }

	/** Update properties cached by this class. */

	void			UpdateProps(const HTMLayoutProperties& props);
};

/** Table caption box. */

class TableCaptionBox
  : public VerticalBox,
    public TableListElement
{
protected:

	union
	{
		struct
		{
			/** Can/must we break page before this caption? See type BREAK_POLICY. */

			unsigned int
					page_break_before:3;

			/** Can/must we break page after this caption? See type BREAK_POLICY. */

			unsigned int
					page_break_after:3;

			/** Can/must we break column before this caption? See type BREAK_POLICY. */

			unsigned int
					column_break_before:2;

			/** Can/must we break column after this caption? See type BREAK_POLICY. */

			unsigned int
					column_break_after:2;

			/** TRUE if this caption lives in a paged or multicol container. */

			unsigned int
					in_multipane:1;

			/** Top caption? */

			unsigned int
					is_top_caption:1;

		} packed;
		UINT32		packed_init;
	};

	/** Width of box. */

	LayoutCoord		content_width;

	/** X position of box. */

	LayoutCoord		x_pos;

	/** Y position of box. */

	LayoutCoord		y_pos;

	/** Space manager for allocating space. */

	SpaceManager	space_manager;

public:

					TableCaptionBox(HTML_Element* element, Content* content);
	virtual			~TableCaptionBox() { Out(); }

	/** Recalculate the top margin after a new block box has been added to a container's layout
		stack. Collapse the margin with preceding adjacent margins if appropriate. If the top
		margin of this block is adjacent to an ancestor's top margin, it may cause the ancestor's
		Y position to change. If the top margin of this block is adjacent to a preceding sibling's
		bottom margin, this block may change its Y position.

		@return TRUE if the Y position of any element was changed. */

	virtual BOOL	RecalculateTopMargins(LayoutInfo& info, const VerticalMargin* top_margin, BOOL has_bottom_margin = FALSE);

	/** Layout of this box is finished (or skipped). Propagate changes (bottom margins,
		bounding-box) to parents. This may grow the box, which in turn may cause its parents to be
		grown. Bottom margins may participate in margin collapsing with successive content, but
		only if this box is part of the normal flow. In that case, propagate the bottom margin to
		the reflow state of the container of this box. */

	virtual void	PropagateBottomMargins(LayoutInfo& info, const VerticalMargin* bottom_margin = 0, BOOL has_inflow_content = TRUE);

	/** Expand relevant parent containers and their bounding-boxes to contain floating and
		absolutely positioned boxes. A call to this method is only to be initiated by floating or
		absolutely positioned boxes.

		@param info Layout information structure
		@param bottom Only relevant for floats: The bottom margin edge of the float, relative to
		top border-edge of this box.
		@param min_bottom Only relevant for floats: The bottom margin edge of the float if
		maximum width of its container is satisfied
		@param child_bounding_box Bounding-box of the propagating descendant, joined with the
		bounding-boxes of the elements up the parent chain towards this element. Relative to the
		top border-edge of this box.
		@param opts Bounding box propagation options */

	virtual void	PropagateBottom(const LayoutInfo& info, LayoutCoord bottom, LayoutCoord min_bottom, const AbsoluteBoundingBox& child_bounding_box, PropagationOptions opts);

	/** Is this box one of the table boxes or a box with table content? */

	virtual BOOL	IsTableBox() const { return TRUE; }

	/** Is this a caption? */

	virtual BOOL	IsCaption() const { return TRUE; }

	/** Is this box a table caption? */

	virtual BOOL	IsTableCaption() const { return TRUE; }

	/** Is this a top caption? */

	virtual BOOL	IsTopCaption() const { return packed.is_top_caption; }

	/** Is this box (and potentially also its children) columnizable?

		A box may only be columnizable if it lives in a paged or multicol container.

		A descendant of a box that isn't columnizable can never be columnizable (unless
		there's a new paged or multicol container between that box and its descendant).

		Table-rows, table-captions, table-row-groups and block-level descendant boxes of
		a paged or multicol container are typically columnizable. Being columnizable
		means that they need to be taken into account by the ancestor paged or multicol
		container somehow. They may become a start or stop element of a column or
		page. It may also be possible to split them over multiple columns or pages, but
		that depends on details about the box type, and that is where the
		"require_children_columnizable" parameter comes into play.

		@param require_children_columnizable If TRUE, only return TRUE if this box is of
		such a type that not only the box itself is columnizable, but also that it
		doesn't prevent its children from becoming columnizable. A table-row, block with
		scrollbars, absolutely-positioned box or float prevents children from being
		columnizable (FALSE will be returned it this parameter is TRUE), but the box
		itself may be very well be columnizable. Example: an absolutely positioned box
		may live inside of a paged or multicol container (so that its Y position is
		stored as a virtual Y position within the paged or multicol container), but its
		descendants do not have to worry about the multi-columnness or multi-pagedness.

		@return TRUE if this box (and possibly its children too) is columnizable. Return
		FALSE if this box isn't columnizable, or if require_children_columnizable is set
		and the children aren't columnizable. */

	virtual BOOL	IsColumnizable(BOOL require_children_columnizable = FALSE) const;

	/** Return the HTML_Element. */

	virtual HTML_Element*
					GetHtmlElement() const { return VerticalBox::GetHtmlElement(); }

	/** Lay out box. */

	virtual LAYST	Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child = NULL, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Finish reflowing box. */

	virtual LAYST	FinishLayout(LayoutInfo& info);

	/** Update screen. */

	virtual void	UpdateScreen(LayoutInfo& info);

	/** Invalidate the screen area that the box uses. */

	virtual void	Invalidate(LayoutProperties* parent_cascade, LayoutInfo& info) const;

	/** Get height. */

	virtual LayoutCoord
					GetHeight() const { return content->GetHeight(); }

	/** Get width. */

	virtual LayoutCoord
					GetWidth() const { return content_width; }

	/** Get X position of caption. */

	virtual LayoutCoord
					GetPositionedX() const { return x_pos; }

	/** Get Y position of caption. */

	virtual LayoutCoord
					GetPositionedY() const { return y_pos; }

	/** Set x position, ignoring offset caused by relative positioning. */

	void			SetStaticPositionedX(LayoutCoord new_x) { LayoutCoord x_offs; LayoutCoord y_offs; GetPositionedOffset(x_offs, y_offs); x_pos = new_x + x_offs; }

	/** Set y position, ignoring offset caused by relative positioning. */

	void			SetStaticPositionedY(LayoutCoord new_y) { LayoutCoord x_offs, y_offs; GetPositionedOffset(x_offs, y_offs); y_pos = new_y + y_offs; }

	/** Get y position, ignoring offset caused by relative positioning. */

	virtual LayoutCoord
					GetStaticPositionedY() const { LayoutCoord x_offs; LayoutCoord y_offs; GetPositionedOffset(x_offs, y_offs); return y_pos - y_offs; }

	/** Increase y position (or decrease, if using negative values). */

	virtual void	IncreaseY(LayoutInfo& info, TableContent* table, LayoutCoord increase);

	/** Get offset caused by relative positioning. */

	virtual void	GetPositionedOffset(LayoutCoord& x, LayoutCoord& y) const { x = LayoutCoord(0); y = LayoutCoord(0); }

	/** Traverse box with children.

		This method will traverse the contents of this box and its children, recursively. */

	virtual void	Traverse(TraversalObject* traversal_object, LayoutProperties* table_lprops, TableContent* table);

	/** Traverse z children. */

	virtual void	TraverseZChildren(TraversalObject* traversal_object, LayoutProperties* layout_props, BOOL after, BOOL handle_overflow_clip) {}

	/** Do we need to calculate min/max widths of this box's content? */

	virtual BOOL	NeedMinMaxWidthCalculation(LayoutProperties* cascade) const;

	/** Propagate widths to container. */

	virtual void	PropagateWidths(const LayoutInfo& info, LayoutCoord min_width, LayoutCoord normal_min_width, LayoutCoord max_width);

#ifdef PAGED_MEDIA_SUPPORT

	/** Attempt to break page. */

	virtual BREAKST	AttemptPageBreak(LayoutInfo& info, int strength);

	/** Insert a page break. */

	virtual BREAKST	InsertPageBreak(LayoutInfo& info, int strength);

#endif // PAGED_MEDIA_SUPPORT

	/** Get page break policy before this layout element. */

	virtual BREAK_POLICY
					GetPageBreakPolicyBefore() const { return (BREAK_POLICY) packed.page_break_before; }

	/** Get page break policy after this layout element. */

	virtual BREAK_POLICY
					GetPageBreakPolicyAfter() const { return (BREAK_POLICY) packed.page_break_after; }

	/** Get column break policy before this layout element. */

	virtual BREAK_POLICY
					GetColumnBreakPolicyBefore() const { return (BREAK_POLICY) packed.column_break_before; }

	/** Get column break policy after this layout element. */

	virtual BREAK_POLICY
					GetColumnBreakPolicyAfter() const { return (BREAK_POLICY) packed.column_break_after; }

	/** Return the space manager of the object, if it has any. */

	virtual SpaceManager*
					GetLocalSpaceManager() { return &space_manager; }

	/** Calculate bottom margins of layout element, by collapsing with adjoining child margins.
	 * This is only done when skipping layout of a box.
	 * @param parent_cascade parent properties
	 * @param info Layout information structure
	 * @return OpBoolean::IS_FALSE if top and bottom margins of this element are adjoining,
	 * OpBoolean::IS_TRUE otherwise, unless an error occurred.
	 */

	virtual OP_BOOLEAN
					CalculateBottomMargins(LayoutProperties* parent_cascade, LayoutInfo& info, VerticalMargin* bottom_margin) const;

	/** Calculate top margins of layout element, by collapsing with adjoining child margins.
	 * This is only done when skipping layout of a box.
	 * @param parent_cascade parent properties
	 * @param info Layout information structure
	 * @param max_top_positive_margin (IN/OUT) Becomes the resulting positive top margin
	 * @param max_top_negative_margin (IN/OUT) Becomes the resulting negative top margin
	 * @return OpBoolean::IS_FALSE if top and bottom margins of this element are adjoining,
	 * OpBoolean::IS_TRUE otherwise, unless an error occurred.
	 */

	virtual OP_BOOLEAN
					CalculateTopMargins(LayoutProperties* parent_cascade, LayoutInfo& info, VerticalMargin* top_margin) const;

	/** Distribute content into columns.

		@return FALSE on OOM, TRUE otherwise. */

	virtual BOOL	Columnize(Columnizer& columnizer, Container* container);

	/** Figure out to which column(s) or spanned element a box belongs. */

	virtual void	FindColumn(ColumnFinder& cf);

	/** Return TRUE if this caption is inside of a paged or multicol container. */

	BOOL			IsInMultiPaneContainer() const { return packed.in_multipane == 1; }
};

/** Relatively positioned table caption box. */

class PositionedTableCaptionBox
	: public TableCaptionBox
{
protected:

	/** Z element for stacking context. */

	ZElement		z_element;

	/** Y offset (as specified by top/bottom CSS properties) */

	LayoutCoord		y_offset;

	/** X offset (as specified by left/right CSS properties) */

	LayoutCoord		x_offset;

public:

					PositionedTableCaptionBox(HTML_Element* element, Content* content)
						: TableCaptionBox(element, content),
						  z_element(element),
						  y_offset(0),
						  x_offset(0) {}

	/** Is this box a positioned box? */

	virtual BOOL	IsPositionedBox() const { return TRUE; }

	/** Return the z element of the object, if it has any. */

	virtual ZElement*
					GetLocalZElement() { return &z_element; }

	/** Get offset caused by relative positioning. */

	virtual void	GetPositionedOffset(LayoutCoord& x, LayoutCoord& y) const { x = x_offset; y = y_offset; }

	/** Reflow box and children. */

	virtual LAYST
					Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Traverse box with children.

		This method will traverse the contents of this box and its children, recursively. */

	virtual void	Traverse(TraversalObject* traversal_object, LayoutProperties* table_lprops, TableContent* table);
};

/** Relatively positioned table caption box that establishes a local stacking context. */

class PositionedZRootTableCaptionBox
	: public PositionedTableCaptionBox
{
private:

	/** Ordered list of elements with z-index different from 0 and 'auto' */

	StackingContext	stacking_context;

protected:

	/** Restart z element list. */

	virtual void	RestartStackingContext() { stacking_context.Restart(); }

public:

					PositionedZRootTableCaptionBox(HTML_Element* element, Content* content)
						: PositionedTableCaptionBox(element, content) {}

	/** Return the z element list of the object, if it has any. */

	virtual StackingContext*
					GetLocalStackingContext() { return &stacking_context; }

	/** Reflow box and children. */

	virtual LAYST
					Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position = LAYOUT_COORD_MIN);

	/** Traverse z children. */

	virtual void	TraverseZChildren(TraversalObject* traversal_object, LayoutProperties* layout_props, BOOL after, BOOL handle_overflow_clip);
};

#endif // _TABLES_H_
