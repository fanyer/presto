/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MULTICOL_H
#define MULTICOL_H

#include "modules/layout/layout.h"
#include "modules/layout/layout_coord.h"
#include "modules/util/simset.h"

class Container;
class MultiColumnContainer;
class BlockBox;
class Box;
class FloatedPaneBox;
class TableRowBox;
class TableListElement;
class VerticalLayout;
class LayoutProperties;
class TraversalObject;
class ColumnRow;
class FlexItemBox;

struct LayoutInfo;

/** Start or end element of a column / page.

	This may for example be a line, block, line break, table-row-group,
	table-caption, table-row, or a column row in an inner multi-pane
	container. */

class ColumnBoundaryElement
{
private:

	union
	{
		VerticalLayout*
					vertical_layout;

		TableListElement*
					table_list_element;

		TableRowBox*
					table_row_box;

		FlexItemBox*
					flex_item_box;

		ColumnRow*	column_row;

		void*		data;
	};

	/** Container of the boundary element. */

	Container*		container;

	enum Type
	{
		NOT_SET,
		VERTICAL_LAYOUT,
		TABLE_LIST_ELEMENT,
		TABLE_ROW_BOX,
		FLEX_ITEM_BOX,
		COLUMN_ROW
	};

	Type			type;

public:

					ColumnBoundaryElement() { Reset(); }
					ColumnBoundaryElement(VerticalLayout* vertical_layout, Container* container) { Set(vertical_layout, container); }
					ColumnBoundaryElement(BlockBox* block_box) { Set(block_box); }
					ColumnBoundaryElement(TableListElement* table_list_element) { Set(table_list_element); }
					ColumnBoundaryElement(TableRowBox* table_row_box) { Set(table_row_box); }
					ColumnBoundaryElement(FlexItemBox* flex_item_box) { Set(flex_item_box); }
					ColumnBoundaryElement(ColumnRow* column_row, MultiColumnContainer* mc) { Set(column_row, mc); }

	/** Reset element. It now represents nothing. */

	void			Reset() { container = NULL; type = NOT_SET; }

	/** Set to be vertical layout. */

	void			Set(VerticalLayout* vertical_layout, Container* c);

	/** Set to be vertical layout. */

	void			Set(BlockBox* block_box);

	/** Set to be table list element. */

	void			Set(TableListElement* table_list_element);

	/** Set to be table row. */

	void			Set(TableRowBox* table_row_box);

	/** Set to be flex item. */

	void			Set(FlexItemBox* flex_item_box);

	/** Set to be column row. */

	void			Set(ColumnRow* column_row, MultiColumnContainer* mc);

	/** Return TRUE if this object represents something. */

	BOOL			IsSet() const { return type != NOT_SET; }

	/** Return TRUE if the two boundary elements are equal. */

	BOOL			Equals(const ColumnBoundaryElement& other) const { return !IsSet() && !other.IsSet() || type == other.type && data == other.data; }

	/** Cast to VerticalLayout, if it is one. */

	VerticalLayout*	GetVerticalLayout() const { return type == VERTICAL_LAYOUT ? vertical_layout : NULL; }

	/** Cast to TableListElement, if it is one. */

	TableListElement*
					GetTableListElement() const { return type == TABLE_LIST_ELEMENT ? table_list_element : NULL; }

	/** Cast to TableRowBox, if it is one. */

	TableRowBox*	GetTableRowBox() const { return type == TABLE_ROW_BOX ? table_row_box : NULL; }

	/** Cast to FlexboxItemBox, if it is one. */

	FlexItemBox*	GetFlexItemBox() const { return type == FLEX_ITEM_BOX ? flex_item_box : NULL; }

	/** Cast to ColumnRow, if it is one.

		This will be a ColumnRow of an inner multi-pane container. */

	ColumnRow*		GetColumnRow() const { return type == COLUMN_ROW ? column_row : NULL; }

	/** Return the HTML_Element associated with this start/end element, if there is one.

		If the boundary element is Line, return the HTML_Element for the
		container in which the line lives.

		If the boundary element is ColumnRow, return the HTML_Element for the
		inner multi-pane container that has the column row.

		If no boundary element has been specified (see @ref Set()), this method
		will return NULL. */

	HTML_Element*	GetHtmlElement() const;
};

/** Float entry that is attached to a column or page. */

class PaneFloatEntry
	: public Link
{
private:
	FloatedPaneBox*	box;

public:
	PaneFloatEntry(FloatedPaneBox* box) : box(box) {}

	FloatedPaneBox*	GetBox() { return box; }

	PaneFloatEntry*	Suc() { return (PaneFloatEntry*) Link::Suc(); }
	PaneFloatEntry*	Pred() { return (PaneFloatEntry*) Link::Pred(); }
};

/** List of floats attached to a column or page. */

class PaneFloatList
	: public Head
{
public:
					~PaneFloatList() { while (Link* l = First()) l->Out(); }

	PaneFloatEntry*	First() { return (PaneFloatEntry*) Head::First(); }
	PaneFloatEntry*	Last() { return (PaneFloatEntry*) Head::Last(); }
};

/** A column in a row in a multi-column container, or a page in a paged container. */

class Column : public Link
{
private:

	/** Element that this column starts with. */

	ColumnBoundaryElement
					start_element;

	/** Element that this column ends with. */

	ColumnBoundaryElement
					stop_element;

	/** List of floats that are attached to this column or page.

		The list is sorted in visual order, from top to bottom. */

	PaneFloatList	floats;

	/** Get virtual Y position where this column starts. */

	LayoutCoord		virtual_y;

	/** Height of this column. */

	LayoutCoord		height;

	/** Vertical offset into start_element where this column starts. */

	LayoutCoord		start_offset;

	/** Total height of pane-attached top-floats that affect the current pane. */

	LayoutCoord		top_floats_height;

	/** Total height of pane-attached bottom-floats that affect the current pane. */

	LayoutCoord		bottom_floats_height;

	/** X position - distance from left edge of the row. */

	LayoutCoord		x;

	/** Y position - distance from top edge of the row. */

	LayoutCoord		y;

	/** Distance between actual Y position and Y position in the virtual coordinate system. */

	LayoutCoord		translate_y;

	/** TRUE if this column stops right before its stop element, instead of right after. */

	BOOL			exclude_stop_element;

public:

					Column(const ColumnBoundaryElement& start_element, LayoutCoord virtual_y, LayoutCoord start_offset, LayoutCoord x, LayoutCoord y, LayoutCoord translate_y)
						: start_element(start_element),
						  stop_element(start_element),
						  virtual_y(virtual_y),
						  height(0),
						  start_offset(start_offset),
						  top_floats_height(0),
						  bottom_floats_height(0),
						  x(x),
						  y(y),
						  translate_y(translate_y),
						  exclude_stop_element(FALSE) {}

	Column*			Suc() const { return (Column*) Link::Suc(); }
	Column*			Pred() const { return (Column*) Link::Pred(); }

	/** Set height of column. */

	void			SetHeight(LayoutCoord new_height) { height = new_height; }

	/** Clear the column's start and stop element, to mark it as content-less.

		Happens when there were too many floats around to fit any actual
		content into the column. */

	void			SetContentLess() { start_element.Reset(); stop_element.Reset(); }

	/** Set the element after which this column stops. */

	void			SetStopAfterElement(const ColumnBoundaryElement& elm) { exclude_stop_element = FALSE; stop_element = elm; }

	/** Set the element before which this column stops. */

	void			SetStopBeforeElement(const ColumnBoundaryElement& elm) { exclude_stop_element = TRUE; stop_element = elm; }

	/** Move the column down by the specified amount. */

	void			MoveDown(LayoutCoord amount) { y += amount; }

	/** Increase Y translation. */

	void			TranslateY(LayoutCoord y) { translate_y += y; }

	/** Attach a float to this page or column. */

	void			AddFloat(FloatedPaneBox* box);

	/** Position top-aligned floats. */

	void			PositionTopFloats();

	/** Position bottom-aligned floats. */

	void			PositionBottomFloats(LayoutCoord row_height);

	/** Return TRUE if the specified float lives in this column. */

	BOOL			HasFloat(FloatedPaneBox* box);

	/** Return the first GCPM float entry in this column. */

	PaneFloatEntry*	GetFirstFloatEntry() { return floats.First(); }

	/** Set total height of pane-attached top-floats that affect the current pane. */

	void			SetTopFloatsHeight(LayoutCoord height) { top_floats_height = height; }

	/** Get total height of pane-attached top-floats that affect the current pane. */

	LayoutCoord		GetTopFloatsHeight() const { return top_floats_height; }

	/** Set total height of pane-attached bottom-floats that affect the current pane. */

	void			SetBottomFloatsHeight(LayoutCoord height) { bottom_floats_height = height; }

	/** Get total height of pane-attached bottom-floats that affect the current pane. */

	LayoutCoord		GetBottomFloatsHeight() const { return bottom_floats_height; }

	/** Get the start element of this column.

		The start element is the innermost first element of the column.

		The start offset into this element can be obtained via @ref
		GetStartOffset(). */

	const ColumnBoundaryElement&
					GetStartElement() const { return start_element; }

	/** Get vertical offset into the start element where this column starts.

		Some elements (typically regular blocks) may be split over multiple
		columns, and then their start offset will be different from 0 in all
		but the first column. */

	LayoutCoord		GetStartOffset() const { return start_offset; }

	/** Get the element with which this column ends.

		The stop element is the innermost last element of the column. */

	const ColumnBoundaryElement&
					GetStopElement() const { return stop_element; }

	/** Return TRUE if this column stops right BEFORE its stop element.

		Normally a column stops right AFTER its stop element, but in some
		situations we need to make the stop element exclusive. */

	BOOL			ExcludeStopElement() const { return exclude_stop_element; }

	/** Get virtual Y position. */

	LayoutCoord		GetVirtualY() const { return virtual_y; }

	/** Get the height of this column. */

	LayoutCoord		GetHeight() const { return height; }

	/** Get X position - distance from left edge of the row. */

	LayoutCoord		GetX() const { return x; }

	/** Get Y position - distance from top edge of the row. */

	LayoutCoord		GetY() const { return y; }

	/** Get the distance between actual Y position and Y position in the virtual coordinate system. */

	LayoutCoord		GetTranslationY() const { return translate_y; }
};

/** A row of columns in a multi-column container.

	A multi-column container consists of a list of rows. A new row is created
	each time we need to put content into columns, and for each spanned
	element. In paged media layout, a row is created for each page.

	If a row represents a spanned element, it may have non-zero vertical
	margins, which collapse with adjacent rows. It never collapses through
	rows, or with margins on content inside any row. */

class ColumnRow : public Link
{
private:

	/** List of columns in this row. */

	AutoDeleteHead	columns;

	/** Y position, relative to the top content edge of the multi-column container. */

	LayoutCoord		y;

public:

					ColumnRow(LayoutCoord y) : y(y) {}

	ColumnRow*		Suc() const { return (ColumnRow*) Link::Suc(); }
	ColumnRow*		Pred() const { return (ColumnRow*) Link::Pred(); }

	/** Add a column to the row. */

	void			AddColumn(Column* column) { column->Into(&columns); }

	/** Remove all columns from the row. */

	void			Clear() { columns.Clear(); }

	/** Get the number of columns in the row. */

	int				GetColumnCount() const { return columns.Cardinal(); }

	/** Get the first column in the row. */

	Column*			GetFirstColumn() const { return (Column*) columns.First(); }

	/** Get the last column in the row. */

	Column*			GetLastColumn() const { return (Column*) columns.Last(); }

	/** Get the top coordinate of this row.

		This is relative to the top content edge of the multi-column
		container, below the row's top margin (if any). */

	LayoutCoord		GetPosition() const { return y; }

	/** Move the row down by the specified amount. */

	void			MoveDown(LayoutCoord amount);

	/** Get the height of this row, excluding row margins (if any). */

	LayoutCoord		GetHeight() const;

	/** Traverse the row. */

	void			Traverse(MultiColumnContainer* multicol_container, TraversalObject* traversal_object, LayoutProperties* layout_props);
};

/** List of ColumnRow elements. */

class ColumnRowStack : public AutoDeleteHead
{
public:
	ColumnRow*		First() const { return (ColumnRow*) AutoDeleteHead::First(); }
	ColumnRow*		Last() const { return (ColumnRow*) AutoDeleteHead::Last(); }
};

/** A column or row break inside a multicol container.

	These are created and inserted during layout. We need to know the
	virtual Y position of the breakpoints, in order to balance the
	columns correctly.

	Breaks are either caused by explicit column/page breaks (break-before and
	break-after CSS properties), or by spanned elements. */

class MultiColBreakpoint : public Link
{
private:

	/** Virtual Y position of the break. */

	LayoutCoord		virtual_y;

	/** Total virtual height of all pane-attached floats between this break and the next. */

	LayoutCoord		floats_virtual_height;

	/** Number of assumed implicit breaks before this explicit break.

		This member is only set and used by the code that calculates an initial
		row height. */

	int				implicit_breaks_assumed;

	/** Type of break; column break, row (page) break, or spanned element. */

	BREAK_TYPE		break_type;

public:

					MultiColBreakpoint(LayoutCoord virtual_y, BREAK_TYPE break_type)
						: virtual_y(virtual_y),
						  floats_virtual_height(0),
						  implicit_breaks_assumed(0),
						  break_type(break_type) {}

	MultiColBreakpoint*
					Suc() { return (MultiColBreakpoint*) Link::Suc(); }
	MultiColBreakpoint*
					Pred() { return (MultiColBreakpoint*) Link::Pred(); }

	/** Get virtual Y position of the break. */

	LayoutCoord		GetVirtualY() const { return virtual_y; }

	/** Return TRUE if this is a row break, FALSE if it's just a column break.

		A page break is also a row break, so we will return TRUE for page
		breaks as well. */

	BOOL			IsRowBreak() const { return break_type != COLUMN_BREAK; }

	/** Return TRUE if this is a break caused by a spanned element. */

	BOOL			IsSpannedElement() const { return break_type == SPANNED_ELEMENT; }

	/** Propagate height of pane-attached float to this breakpoint.

		This means that the pane-attached float is assumed to end up after this
		break, but before the next (if any). */

	void			PropagatePaneFloatHeight(LayoutCoord float_height) { floats_virtual_height += float_height; }

	/** Get the total height occupied by the pane-attached floats between this break and the next. */

	LayoutCoord		GetPaneFloatsVirtualHeight() const { return floats_virtual_height; }

	/** Assume another implicit column break before this explicit break. */

	void			AssumeImplicitBreakBefore() { implicit_breaks_assumed ++; }

	/** Get the number of assumed implicit breaks before this explicit break. */

	int				GetAssumedImplicitBreaksBefore() const { return implicit_breaks_assumed; }

	/** Reset number of assumed implicit breaks. */

	void			ResetAssumedImplicitBreaks() { implicit_breaks_assumed = 0; }
};

/** Entry for a spanned box inside of a multi-column container.

	Objects of this type are created and added to a list during layout, and
	consumed in the following columnization phase. */

class SpannedElm : public Link
{
private:

	/** The HTML_Element of the spanned box. */

	HTML_Element*	html_element;

	/** Link to the break point caused by this spanned element.

		Will be set to NULL after layout. */

	MultiColBreakpoint*
					breakpoint;

	/** Top margin of spanned box. */

	LayoutCoord		margin_top;

	/** Bottom margin of spanned box. */

	LayoutCoord		margin_bottom;

public:

					SpannedElm(HTML_Element* html_element, MultiColBreakpoint* breakpoint, LayoutCoord margin_top, LayoutCoord margin_bottom)
						: html_element(html_element),
						  breakpoint(breakpoint),
						  margin_top(margin_top),
						  margin_bottom(margin_bottom) {}

	SpannedElm*		Suc() const { return (SpannedElm*) Link::Suc(); }
	SpannedElm*		Pred() const { return (SpannedElm*) Link::Pred(); }

	/** Cancel the break caused by this spanned element. */

	void			CancelBreak();

	/** Get the computed value of the top margin of the spanned box. */

	LayoutCoord		GetMarginTop() const { return margin_top; }

	/** Get the computed value of the bottom margin of the spanned box. */

	LayoutCoord		GetMarginBottom() const { return margin_bottom; }

	/** Get the block box established by the spanned element.

		This method may return NULL if what was initial thought to be a spanned
		element didn't turn out to become one. This may happen when a run-in is
		converted from block to inline. */

	BlockBox*		GetSpannedBox() const;
};

/** State of column distribution; a columnizer.

	At the end of layout of a multi-column container, the content will be
	distributed into rows of columns, and this class is used to keep track of
	the state.

	The term "virtual Y position" is used a lot in this class. It means the Y
	position it would have had if there were only one column. A multi-column
	container is laid out like that; as if it only had one column. Lines,
	blocks and line breaks are simply put in a regular vertical layout stack.

	Virtual Y positions are relative to the content edge of the multicol
	container (while vertical layout elements, for instance, are relative to
	the border edge). It's relative to the content edge in order to make it
	easier to keep track of the total vertical space used in a row.

	An instance of this class may be created at the end of layout of a
	multi-column container, and will be passed to that multi-column container's
	Container::Columnize(), which will recurse into descendants and feed this
	columnizer with input, and based on that input, the columnizer will
	distribute the content into columns, rows and pages accordingly. The single
	column originally created by MultiColumnContainer will remain more or less
	intact. The multiple columns are represented by one Column object each. The
	Column object holds a translation, start element and stop element, and
	that's what causes the multi-column effect.

	AllocateContent() is called when we reach the top (typically border) edge
	of some content, to record it as part of the uncommitted/pending content,
	and to keep track of a possible virtual start position of the next column
	(in case it turns out that more columns are needed), and to keep track of
	the last element we process.

	AdvanceHead() is called each time we reach the bottom (typically border)
	edge of some content, to update the uncommitted virtual Y position, that
	is, the bottom of the stack of uncommitted content so far.

	CommitContent() is called at each possible break point (and at the end) to
	put all the uncommitted content into _one_ column; into the current column
	if there's enough space, or otherwise create a new column and put it there.

	EnterChild() is called when entering a child container, and LeaveChild() is
	called when leaving one. These are required to ensure that callers of
	various Columnizer methods don't have to worry about the multicol
	container's coordinate system, only their own. */

class Columnizer
{
protected:

	/** Layout info. */

	const LayoutInfo&
					layout_info;

	/** Desired number of columns, as calculated from CSS properties and containing block width. */

	const int		column_count;

	/** Width of one column. */

	const LayoutCoord
					column_width;

	/** Content box width of multicol container. */

	const LayoutCoord
					content_width;

	/** Column gap. */

	const LayoutCoord
					column_gap;

	/** Vertical distance from one column to the next.

		If CONTENT_HEIGHT_AUTO initially, it will be increased as we paginate. */

	LayoutCoord		column_y_stride;

	/** Virtual content height.

		This is the virtual content height of the entire multicol container,
		i.e. its content box height if there were only one column. */

	const LayoutCoord
					virtual_height;

	/** Virtual height of all pane-attached floats that have not yet been added to a column. */

	LayoutCoord		remaining_floats_virtual_height;

	/** Maximum height of multicol container content box. */

	const LayoutCoord
					max_height;

	/** Set to TRUE if column balancing is required on every row. */

	const BOOL		always_balance;

	/** Columnizer of ancestor multicol container, if any.

		When columnizing an inner multicol container, it needs to interact with
		its ancestor multicol container(s). */

	Columnizer*		ancestor_columnizer;

	/** TRUE if ancestor_columnizer is not NULL and distributes the same content as this.

		Typically this happens when an element has more than one content
		distribution method, e.g. has both multiple columns and paged overflow. */

	BOOL			ancestor_columnizer_in_same_element;

	/** Descendant multicol container that we're currently processing. */

	MultiColumnContainer*
					descendant_multicol;

	/** List of ColumnRow elements.

		Initially it will be empty. The columnizer will populate it as it
		creates new rows for the content (and fill each row with columns as
		needed). */

	ColumnRowStack&	rows;

	/** Next explicit break.

		Initially this will be the first explicit break in the multicol
		container, and we will advance when we walk past (and handle) an
		explicit break.

		Explicit breaks may be inserted between columns, and they may either be
		a column break or a page break. In addition we consider an explicit
		break to exist between a column and a spanned element.

		The breaks' virtual Y position is used to calculate suitable column row
		heights. */

	MultiColBreakpoint*
					next_break;

	/** Next pending spanned element.

		Initially this will be the first spanned element in the multicol
		container, and we will advance when we walk past (and handle) a spanned
		element. */

	SpannedElm*		next_spanned_elm;

	/** First uncommitted element.

		This is a start element candidate for the next column; if the next call
		to CommitContent() finds that there is not enough space in the current
		column for the uncommitted content, this element will become the start
		element in the next column. */

	ColumnBoundaryElement
					first_uncommitted_element;

	/** Last element allocated.

		Each time we commit content to a column, this element will become the
		tentative end element of that column. */

	ColumnBoundaryElement
					last_allocated_element;

	/** List of floats that are looking for a column to fit in.

		Sorted in logical order. */

	PaneFloatList	float_queue;

	/** Vertical offset into first_uncommitted_element where the next column may start.

		Some elements (typically regular blocks) may be split over multiple
		columns, and then their start offset will be different from 0 in all
		but the first column. */

	LayoutCoord		pending_start_offset;

	/** Uncommitted head; the current uncommitted virtual bottom.

		This is the last position we have recorded so far (typically the border
		bottom of the last element visited by the columnizer). It will be
		copied to committed_virtual_y as soon as CommitContent() is called. It
		is moved forwards by AdvanceHead(). */

	LayoutCoord		virtual_y;

	/** Last committed virtual Y position.

		This is typically the bottom border edge of the last committed
		element. It is less than or equal to virtual_y and
		pending_virtual_y_start. It is moved forwards by CommitContent(). */

	LayoutCoord		committed_virtual_y;

	/** Virtual Y start position for the next column.

		This is typically the top border edge of the first uncommitted element.
		It is moved forwards by AllocateContent(). */

	LayoutCoord		pending_virtual_y_start;

	/** Virtual Y position where the current column starts. */

	LayoutCoord		col_virtual_y_start;

	/** Earliest possible break position.

		Committing at positions before this one will have no effect. */

	LayoutCoord		earliest_break_virtual_y;

	/** Virtual Y translation from the multicol container's coordinate system.

		This is relative to the multicol container's border edge, and it will
		be updated as we enter and leave child containers. */

	LayoutCoord		stack_offset;

	/** Initial row height of current row.

		If column balancing is enabled, this is just a guess, as we need to
		analyze and walk past possible break points before we can tell for
		sure. */

	LayoutCoord		initial_row_height;

	/** Currently calculated height of current row.

		If column balancing is enabled, this may be increased during row
		columnization, but not beyond the value of max_row_height. */

	LayoutCoord		current_row_height;

	/** Maximum row height. */

	LayoutCoord		max_row_height;

	/** Total height of pane-attached top-floats in the current pane. */

	LayoutCoord		top_floats_height;

	/** Total height of pane-attached bottom-floats in the current pane. */

	LayoutCoord		bottom_floats_height;

	/** Bottom margin of last row added.

		Rows that are established by spanned elements may have a margin, and it
		is supposed to be collapsed with sibling rows established by spanned
		elements. */

	LayoutCoord		pending_margin;

	/** Column number that we're currently at in the current row.

		If this one reaches 'column_count' in paged media layout, we
		will jump to the next page and create a new row.

		If this one reaches 'column_count' and the multicol container
		worked by this columnizer is to be columnized into an ancestor
		container, we will switch to a new column in the ancestor
		multicol container, and create a new row here. */

	int				current_column_num;

	/** X position of current column.

		This is the distance from the row's left edge to the current
		column's. */

	LayoutCoord		column_x;

	/** Y position of current column.

		This is the distance from the row's top edge to the current
		column's. */

	LayoutCoord		column_y;

	/** Y translation of current column.

		This is the distance from the current column's actual Y start position
		(relative to the multicol container's top border edge) to the virtual Y
		start position to the current column. */

	LayoutCoord		y_translation;

	/** TRUE if inside column-span:all */

	BOOL			span_all;

#ifdef SUPPORT_TEXT_DIRECTION

	/** TRUE if text direction is right-to-left.

		Only set for multicol containers; NOT for paged containers. */

	BOOL			is_rtl;

#endif // SUPPORT_TEXT_DIRECTION

	/** Set to TRUE if column balancing is required the current row.

		It generally follows the value of always_balance, but if there are
		spanned elements, it may be forced to TRUE. */

	BOOL			balance_current_row;

	/** TRUE if the we are allowed to attempt to stretch the current column (further). */

	BOOL			allow_column_stretch;

	/** TRUE when a column allows more content, FALSE if not.

		If it FALSE and more content is to be columnized, a new column must
		first be started. */

	BOOL			column_open;

	/** TRUE when a row allows more content, FALSE if not.

		If it FALSE and more content is to be columnized, a new row must first
		be started. */

	BOOL			row_open;

	/** TRUE if height is restricted by properties or ancestor multi-pane container. */

	BOOL			height_restricted;

	/** Set to TRUE if we reach max-height at some point during columnization. */

	BOOL			reached_max_height;

	/** TRUE if the current row has floats that span more than one column. */

	BOOL			row_has_spanned_floats;

	/** TRUE if this is an inner multicol container and we're just
		trying to guess its height. */

	BOOL			trial;

#ifdef PAGED_MEDIA_SUPPORT

	/** The last row after which page breaking is allowed. */

	ColumnRow*		last_allowed_page_break_after;

	/** Page breaking policy after the element that was just added. */

	BREAK_POLICY	page_break_policy_after_previous;

	/** Page breaking policy before the element that is about to be added. */

	BREAK_POLICY	page_break_policy_before_current;

	/** TRUE when the current page allows more content, FALSE if not.

		If it FALSE and more content is to be columnized, a new page must first
		be started. */

	BOOL			page_open;

#endif // PAGED_MEDIA_SUPPORT

	/** Update row and/or container height. */

	virtual void	UpdateHeight(LayoutCoord height);

public:

	/** Column creation policy when adding queued pane-attached floats. */

	enum ColumnCreationPolicy
	{
		/** Don't allow new columns. */

		COLUMNS_NONE,

		/** Allow one new column, but only of there is no current one. */

		COLUMNS_ONE,

		/** Allow new columns, as long as it doesn't require a new row to be created. */

		COLUMNS_ONE_ROW,

		/** Allow as many columns as necessary to add all floats. */

		COLUMNS_ANY
	};

					Columnizer(Columnizer* ancestor,
							   BOOL ancestor_columnizer_in_same_element,
							   const LayoutInfo& layout_info,
							   ColumnRowStack& rows,
							   MultiColBreakpoint* first_break,
							   SpannedElm* first_spanned_elm,
							   LayoutCoord max_height,
							   int column_count,
							   LayoutCoord column_width,
							   LayoutCoord content_width,
							   LayoutCoord column_gap,
							   LayoutCoord column_y_stride,
							   LayoutCoord top_offset,
							   LayoutCoord virtual_height,
							   LayoutCoord floats_virtual_height,
							   BOOL height_restricted,
#ifdef SUPPORT_TEXT_DIRECTION
							   BOOL is_rtl,
#endif // SUPPORT_TEXT_DIRECTION
							   BOOL balance);

	/** Specify that this is an inner multicol container and we're
		just trying to guess its height. */

	void			SetTrial() { trial = TRUE; }

	/** Get layout info. */

	const LayoutInfo&
					GetLayoutInfo() const { return layout_info; }

	/** Enter child block.

		@param offset Child's static Y offset, relative to the top border edge
		of its container.
		@param desc_mc The descendant multicol container entered, if any. */

	void			EnterChild(LayoutCoord offset, MultiColumnContainer* desc_mc = NULL) { stack_offset += offset; descendant_multicol = desc_mc; }

	/** Leave child block.

		@param offset Child's static Y offset, relative to the top border edge
		of its container. */

	void			LeaveChild(LayoutCoord offset) { stack_offset -= offset; descendant_multicol = NULL; }

	/** Get the amount of vertical space used in the current column so far. */

	LayoutCoord		GetSpaceUsed() const { return committed_virtual_y - col_virtual_y_start + top_floats_height + bottom_floats_height; }

	/** Get space left in the current column. */

	LayoutCoord		GetSpaceLeft() const { return current_row_height - GetSpaceUsed(); }

	/** Return the position of the current row, if any.

		If there is no current row, return the hypothetical position of the
		next row to create. */

	LayoutCoord		GetRowPosition() const;

	/** Get (non-virtual) bottom, relative to the first row.

		Convert a given virtual position to a non-virtual bottom. A non-virtual
		bottom in this multi-pane container happens to be a virtual position in
		an ancestor container. */

	LayoutCoord		GetBottom(LayoutCoord some_virtual_y) const;

	/** Set virtual position of the earliest break, if later than already set.

		Committing at positions earlier than this position will have no effect. */

	void			SetEarliestBreakPosition(LayoutCoord local_virtual_y);

	/** Get accumulated height occupied by floats in the current column. */

	LayoutCoord		GetFloatsHeight();

	/** Prepare content for addition.

		This will add to the stack of uncommitted content, which later will be
		committed by calling CommitContent().

		This call informs the columnizer about the new element and its
		whereabouts. The element cannot be added to any column(s) just yet,
		because first we need to find the next possible break point. That is,
		we need to know what comes after 'elm', and if breaking columns between
		'elm' and what comes after is allowed. When a possible break point has
		been found, CommitContent() is called.

		If there was no uncommitted content prior to this call, this element
		will be a start candidate for the next column (if it later turns out
		that the uncommited content cannot fit in the current column).

		@param local_virtual_y Y position of 'elm', relative to the container
		we're currently inside.
		@param elm The element we're about to add.
		@param start_offset Start offset into 'elm'. Some elements (typically
		regular blocks) may be split over multiple columns, and then their
		start offset will be different from 0 in all but the first column. */

	void			AllocateContent(LayoutCoord local_virtual_y, const ColumnBoundaryElement& elm, LayoutCoord start_offset = LayoutCoord(0));

	/** Set new uncommitted head, the bottom of uncommitted content.

		@param local_y New virtual bottom, relative to the container we're
		currently inside. */

	void			AdvanceHead(LayoutCoord local_y) { virtual_y = local_y + stack_offset; }

	/** Return TRUE if we can create taller columns by advancing to a next row or a next page. */

	BOOL			MoreSpaceAhead() const;

	/** Return the bottom margin of the last row that was added.

		Regular rows don't have margins, but rows caused by spanned elements
		may. */

	LayoutCoord		GetPendingMargin() const { return pending_margin; }

	/** Get number of columns still unused in the current row. */

	int				GetColumnsLeft() const { return (span_all ? 1 : column_count) - current_column_num; }

	/** Calculate X position of new column to be added. */

	LayoutCoord		CalculateColumnX() const;

#ifdef PAGED_MEDIA_SUPPORT

	/** Create a new page.

		@param y_offset (out) Y offset caused by the page break
		@return FALSE on OOM, TRUE otherwise. */

	BOOL			GetNewPage(LayoutCoord& y_offset);

#endif // PAGED_MEDIA_SUPPORT

	/** Create a new column.

		This will also create a new row to put the column in, if necessary, and
		also advance to the next page, when the current page has already been
		closed.

		Creating a new column in one multicol container may cause a new column
		to be created in the ancestor multicol container (if any) as well. When
		the maximum number of columns ('column_count') has been reached in one
		multicol container, another column should be created in the ancestor
		multicol container, and a new row should be created in the child
		multicol container.

		There must be a pending start element (see SetPendingStartElement())
		prior to calling this method, since empty columns are not allowed.

		@param start_element The start element of the new column
		@param minimum_height Minimum height requirement of the column. To
		satisfy the requirements, we may move to the next page, or to the next
		column in the ancestor multicol container (if any).
		@param add_queued_floats If TRUE, this method may attempt to add queued
		pane floats, and if there are any pane floats added to the new column,
		we may actually run out of space just because of that. If that happens,
		we will create another new column right away.

		@return FALSE on OOM, TRUE otherwise. */

	BOOL			GetNewColumn(const ColumnBoundaryElement& start_element, LayoutCoord minimum_height, BOOL add_queued_floats = TRUE);

	/** Enter a spanned box before columnizing it.

		Finalizes any previous column and row, and collapses the previous row's
		bottom margin with the top margin of this spanned element. A row that
		is established by another spanned element has a bottom margin, and it
		should collapse with the top margin of this spanned element.

		This method prepares the columnizer for the spanned element. Spanned
		elements should of course not be split into multiple columns, but we
		still use the columnizer on them, in order to handle page breaking.

		@param box The spanned box we're entering.
		@param container Container of the spanned box we're entering.

		@return FALSE on OOM, TRUE otherwise. */

	BOOL			EnterSpannedBox(BlockBox* box, Container* container);

	/** Leave a spanned box after having columnized it.

		This will finalize "columnization" of the spanned element, and record
		its bottom margin, so that it is applied before the next row (and
		possibly collapsed with the top margin of that row).

		@param box The spanned box we're leaving.
		@param container Container of the spanned box we're leaving.

		@return FALSE on OOM, TRUE otherwise. */

	BOOL			LeaveSpannedBox(BlockBox* box, Container* container);

	/** Skip a spanned box.

		Called when a spanned box was rejected (because there wasn't enough
		vertical space for it). */

	void			SkipSpannedBox(BlockBox* box);

	/** Add pane-attached (GCPM) floating box. */

	void			AddPaneFloat(FloatedPaneBox* box);

	/** Find space for pending content.

		May attempt to stretch the column if allowed, if that would help
		satisfy the space requirement (as communicated by new_virtual_y).

		@param new_virtual_y Virtual bottom position of the element for which
		we're trying to make room

		@return FALSE if there is not enough space available in the current
		column, not even after having attempted to stretch it. Returns TRUE if
		there is no current column, if there is no pending content, or if there
		is (or was made) enough space in the current column. */

	BOOL			FindSpace(LayoutCoord new_virtual_y);

	/** Commit pending content to one or more columns.

		This is the method that essentially drives the columnizer machinery,
		and makes it go forward; it advances to new columns / rows / pages when
		the pending content doesn't fit in the current column.

		Content is first added to the stack of uncommitted content by calling
		AllocateContent() and AdvanceHead(). Later, when a possible break point
		is found, this method is called to commit it to one or more columns.

		This method is called each time a column break is allowed, and also
		when finalizing.

		When this method has returned, the committed virtual Y position will
		have caught up with the uncommitted virtual Y position.

		@param check_available_space TRUE if we should check for available
		space, and advance to the next column if there isn't enough. In most
		cases, it should be TRUE, however, in nested multicol container
		scenarios we need to control space allocation from the inner-most
		multicol container, and it needs to be FALSE when called on outer
		multicol containers.

		@return FALSE on OOM, TRUE otherwise. */

	BOOL			CommitContent(BOOL check_available_space = TRUE);

	/** Flush pane floats in ancestors.

		Pane floats are not committed in the multi-pane container where they
		live, but ancestors need to regard them as regular content. */

	void			FlushFloats(LayoutCoord virtual_bottom);

	/** Fill up with any queued floats that might fit in the current column. */

	BOOL			AddQueuedFloats(ColumnCreationPolicy policy);

	/** Add and the empty parts of a block, and split it into multiple columns, if necessary.

		@param block A block box to add empty content of.
		@param container The Container in which the block lies.

		@return FALSE on OOM, TRUE otherwise. */

	BOOL			AddEmptyBlockContent(BlockBox* block, Container* container);

	/** Attempt to stretch the column to make some element fit.

		Stretching is not allowed if column balancing is disabled.

		If balancing is enabled, stretching at most once per column is the
		rule, except for the last column, which may be stretched as many times
		as needed to fit the content, as long as it doesn't exceed maximum row
		height allowed.

		For nested multicol containers, each column in the outer multicol
		container may be stretched once by each column in the inner multicol
		container.

		@param element_height Height of the element that wants to fit.

		@return TRUE if the column was stretched, FALSE otherwise. */

	BOOL			StretchColumn(LayoutCoord element_height);

	/** Increase current row height. */

	void			IncreaseRowHeight(LayoutCoord increase);

	/** Explicitly break (end) the current page (and therefore column and row as well).

		@return FALSE on OOM, TRUE otherwise. */

	virtual BOOL	ExplicitlyBreakPage(const ColumnBoundaryElement& stop_before, BOOL closed_by_pane_float = FALSE, BOOL has_child_columnizer = FALSE);

	/** Explicitly break (end) the current column.

		@return FALSE on OOM, TRUE otherwise. */

	virtual BOOL	ExplicitlyBreakColumn(const ColumnBoundaryElement& stop_before, BOOL closed_by_pane_float = FALSE);

	/** Get the row we're currently at, or NULL if none. */

	ColumnRow*		GetCurrentRow() const;

	/** Get the column we're currently at, or NULL if none. */

	Column*			GetCurrentColumn() const;

	/** Finish and close current column.

		The column won't accept any more content now, and any attempt at adding
		more content after this will result in a new column being created
		(which may cause a new page + row to be created as well). */

	void			CloseColumn();

	/** Finish and close current row.

		This will also finish and close the current column.

		The row won't accept any more content now, and any attempt at adding
		more content after this will result in a new column and row being
		created (which may cause a new page to be created as well). */

	void			CloseRow(BOOL stretch_to_ancestor = FALSE);

#ifdef PAGED_MEDIA_SUPPORT

	/** Finish and close current page.

		This will also finish and close the current row and column.

		The page won't accept more content now, and any attempt at adding more
		content after this will result in a new page, row and column being
		created. */

	virtual void	ClosePage();

	/** Set page breaking policy after the element that was just added. */

	void			SetPageBreakPolicyAfterPrevious(BREAK_POLICY page_break_policy);

	/** Set page breaking policy before the element that is about to be added. */

	void			SetPageBreakPolicyBeforeCurrent(BREAK_POLICY page_break_policy);

#endif // PAGED_MEDIA_SUPPORT

	/** Finalize column. Commit pending content. */

	BOOL			FinalizeColumn();

	/** Finalize columnization. Commit pending content and floats.

		@param stretch_to_ancestor If TRUE, stretch to ancestor (assumedly
		paginator) height, so that bottom floats end up at the very bottom of
		the page. */

	BOOL			Finalize(BOOL stretch_to_ancestor = FALSE);

	/** Set the element before which the last column added shall stop. */

	void			SetStopBefore(const ColumnBoundaryElement& elm);

	/** Return TRUE if we reached max-height at some point during columnization. */

	BOOL			ReachedMaxHeight() const { return reached_max_height; }
};

#ifdef PAGED_MEDIA_SUPPORT

/** List of all pages in a paged container. */

class PageStack : public ColumnRow
{
public:
					PageStack()
						: ColumnRow(LayoutCoord(0)) {}
};

/** State of pagination.

	Invoked at the end of layout of a paged container, to split content into
	multiple pages. */

class Paginator : public Columnizer
{
protected:

	/** Needed by the columnizer to create an hold a list of column rows.

		In our case there will only be at most one entry (ColumnRow) in this
		list, which holds the list of all pages created. */

	ColumnRowStack	page_list_holder;

	/** Sum of top and bottom padding on the paged container. */

	LayoutCoord		vertical_padding;

	/** Bottom border+padding of the paged container. */

	BOOL			page_height_auto;

	/** Update row and/or container height. */

	virtual void	UpdateHeight(LayoutCoord height);

public:
					Paginator(const LayoutInfo& layout_info,
							  LayoutCoord max_height,
							  LayoutCoord column_x_stride,
							  LayoutCoord column_y_stride,
							  LayoutCoord top_offset,
							  LayoutCoord vertical_padding,
							  BOOL height_restricted);

	/** Explicitly break (end) the current page (and therefore column and row as well).

		@return FALSE on OOM, TRUE otherwise. */

	virtual BOOL	ExplicitlyBreakPage(const ColumnBoundaryElement& stop_before, BOOL closed_by_pane_float = FALSE, BOOL has_child_columnizer = FALSE);

	/** Explicitly break (end) the current column.

		@return FALSE on OOM, TRUE otherwise. */

	virtual BOOL	ExplicitlyBreakColumn(const ColumnBoundaryElement& stop_before, BOOL closed_by_pane_float = FALSE);

	/** Finish and close current page.

		This will also finish and close the current row and column.

		The page won't accept more content now, and any attempt at adding more
		content after this will result in a new page, row and column being
		created. */

	virtual void	ClosePage();

	/** Get page height.

		To be called after pagination if page height was auto. */

	LayoutCoord		GetPageHeight() const;

	/** Update bottom floats when final paged container height is known. */

	void			UpdateBottomFloats(LayoutCoord content_height);

	/** Move all pages created during pagination into 'pages'. */

	void			GetAllPages(PageStack& pages);
};

#endif // PAGED_MEDIA_SUPPORT

/** State class while looking for a box inside of a multicol container or paged container.

	This class makes it possible to find the whereabouts of some element inside
	of a multicol container, without having to traverse.

	Input:
	- the box

	Output:
	- the column and row in which this box starts
	- the column and row in which this box ends
	- the offset into any descendant multicol container
	- (union rectangle of) the border area(s) of the box, relative to the
      top/left border edge of the box. */

class ColumnFinder
{
private:

	/** The box we're looking for. */

	const Box*		box;

	/** Ancestor column finder, if any. */

	ColumnFinder*	ancestor_cf;

	/** The ColumnRow we're currently examining. */

	const ColumnRow*
					cur_row;

	/** The Column we're currently examining, if any. */

	const Column*	cur_column;

	/** The ColumnRow where the box starts. */

	const ColumnRow*
					start_row;

	/** The Column where the box starts, if any. */

	const Column*	start_column;

	/** X translation from the multi-pane container's content edge to the first column in which the box lives. */

	LayoutCoord		box_translation_x;

	/** Y translation from the multi-pane container's content edge to the first column in which the box lives. */

	LayoutCoord		box_translation_y;

	/** Distance from box start to column start. */

	LayoutCoord		start_offset_from_top;

	/** The ColumnRow where the box starts. */

	const ColumnRow*
					end_row;

	/** The Column where the box starts, if any. */

	const Column*	end_column;

	/** Current virtual Y position.

		Updated as we travel through the contents of the multicol container. */

	LayoutCoord		virtual_y;

	/** Distance to the multi-column container's virtual Y coordinate system. */

	LayoutCoord		stack_offset;

	/** Y translation into descendant multicol container. */

	LayoutCoord		descendant_translation_y;

	/** X translation into descendant multicol container. */

	LayoutCoord		descendant_translation_x;

	/** Union of the border area in each column in which the target box lives.

		The values are relative to the top/left border edge of the target box
		(in the first column in which the box lives). Note that these values
		are presented as if the box width is 0 (so to get the correct border
		box width, do border_rect.right - border_rect.left +
		box->GetWidth()). */

	RECT			border_rect;

	/** Expand the border rectangle, based on the current position. */

	void			UpdateBorderRect(LayoutCoord virtual_y);

public:

					ColumnFinder(const Box* box, ColumnRow* first_row, LayoutCoord top_border_padding, ColumnFinder* ancestor_cf);

	/** Return the box we're looking for. */

	const Box*		GetTarget() const { return box; }

	/** Enter a child block.

		@param offset Child's static Y offset, relative to the top border edge
		of its container. */

	void			EnterChild(LayoutCoord offset) { stack_offset += offset; }

	/** Leave a child block.

		@param offset Child's static Y offset, relative to the top border edge
		of its container. */

	void			LeaveChild(LayoutCoord offset) { stack_offset -= offset; }

	/** Set position of the current element we're examining.

		Advance to next column/spanned element if appropriate.

		@param local_virtual_y New virtual Y position, relative to the
		container we're currently inside. */

	void			SetPosition(LayoutCoord local_virtual_y);

	/** Found the start of the box.

		This will record the current column + row as the start column + row. */

	void			SetBoxStartFound();

	/** Found the end of the box. We're done.

		This will record the current column + row as the end column + row. */

	void			SetBoxEndFound();

	/** Return TRUE if we're done searching. */

	BOOL			IsBoxEndFound() const { return end_row != NULL; }

	/** Get X translation from the multi-pane container's content edge to the first column in which the box lives. */

	LayoutCoord		GetBoxTranslationX() const { return box_translation_x; }

	/** Get Y translation from the multi-pane container's content edge to the first column in which the box lives. */

	LayoutCoord		GetBoxTranslationY() const { return box_translation_y; }

	/** Set translation into descendant multicol container. */

	void			SetDescendantTranslation(LayoutCoord x, LayoutCoord y) { descendant_translation_x = x; descendant_translation_y = y; }

	/** Get X translation into descendant multicol container. */

	LayoutCoord		GetDescendantTranslationX() const { return descendant_translation_x; }

	/** Get Y translation into descendant multicol container. */

	LayoutCoord		GetDescendantTranslationY() const { return descendant_translation_y; }

	/** Get the union of the border area in each column in which the target box lives. */

	RECT& 			GetBorderRect() { return border_rect; }
};

#endif // MULTICOL_H
