/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Alexander Remen (alexr@opera.com)
 */

#ifndef QUICK_WRAP_LAYOUT_H
#define QUICK_WRAP_LAYOUT_H

#include "adjunct/quick_toolkit/widgets/QuickLayoutBase.h"

/** @brief A layout where widgets always get their nominal height and widths and wraps to a new row at the given width
  */
class QuickWrapLayout : public QuickLayoutBase
{
	IMPLEMENT_TYPEDOBJECT(QuickLayoutBase);
public:
	QuickWrapLayout();

	OP_STATUS Init() { return OpStatus::OK; }
	
	virtual OP_STATUS	Layout(const OpRect& rect);
	virtual BOOL		HeightDependsOnWidth() { return TRUE; }
 
	virtual unsigned	GetDefaultMinimumWidth() { return GetDefaultWidth(MINIMUM); }
	virtual unsigned	GetDefaultMinimumHeight(unsigned width) { return GetDefaultHeight(MINIMUM, width); }
	virtual unsigned	GetDefaultNominalWidth() { return GetDefaultWidth(NOMINAL); }
	virtual unsigned	GetDefaultNominalHeight(unsigned width) { return GetDefaultHeight(NOMINAL, width); }
	virtual unsigned	GetDefaultPreferredWidth() { return GetDefaultWidth(PREFERRED); }
	virtual unsigned	GetDefaultPreferredHeight(unsigned width) { return GetDefaultHeight(PREFERRED, width); }
	virtual void		GetDefaultMargins(WidgetSizes::Margins& margins);
	
						// Override QuickWidgetContainer
	virtual void		OnContentsChanged();

	void				SetMaxVisibleLines(unsigned max_lines, bool broadcast_contents_changed = true);
	bool				HasHiddenItems() const { return m_hidden_items; }

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

	void CalculateDefaults();
	void CalculateMargins();
	unsigned GetDefaultWidth(SizeType type) { if (!m_valid_defaults) CalculateDefaults(); return m_default_sizes.width[type]; }
	unsigned GetDefaultHeight(SizeType type, unsigned width);
	unsigned GetRowHeight(SizeType type, unsigned start, unsigned count, unsigned width);
	OP_STATUS LayoutRow(const OpRect& row_rect, unsigned start, unsigned count);
	static unsigned GetWidgetHeight(QuickWidget* widget, SizeType type, unsigned width);

	bool			m_valid_defaults;
	SizeInfo		m_default_sizes;
	unsigned		m_horizontal_margins;
	unsigned		m_vertical_margins;

	unsigned		m_max_visible_lines;	///< if we should only show a limited amount of lines (0 == all items are visible)
	bool			m_hidden_items;			///< whether there are items hidden
};

#endif // QUICK_WRAP_LAYOUT_H
