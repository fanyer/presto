/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef TEST_WIDGET_H
#define TEST_WIDGET_H

#ifdef SELFTEST

#include "adjunct/quick_toolkit/widgets/QuickWidget.h"

/** @brief A QuickWidget that does nothing and returns predictable values
  */
class ST_TestWidget : public QuickWidget
{
	IMPLEMENT_TYPEDOBJECT(QuickWidget);
public:
	static const unsigned MinimumWidth = 100;
	static const unsigned NominalWidth = MinimumWidth * 2;
	static const unsigned PreferredWidth = MinimumWidth * 4;
	static const unsigned MinimumHeight = 150;
	static const unsigned NominalHeight = MinimumHeight * 2;
	static const unsigned PreferredHeight = MinimumHeight * 4;
	static const unsigned Margin = 5;
	static const unsigned FixedPadding = 10;

	ST_TestWidget() : m_parent_op_widget(0), m_visible(TRUE), m_enabled(FALSE) {}

	virtual BOOL HeightDependsOnWidth() { return FALSE; }
	virtual OP_STATUS Layout(const OpRect& rect) { m_layout_rect = rect; return OpStatus::OK; }
	virtual void SetParentOpWidget(class OpWidget* parent) { m_parent_op_widget = parent; }
	virtual void Show() { m_visible = TRUE; BroadcastContentsChanged(); }
	virtual void Hide() { m_visible = FALSE; BroadcastContentsChanged(); }
	virtual BOOL IsVisible() { return m_visible; }
	virtual void SetEnabled(BOOL enabled) { m_enabled = enabled; }
	
	virtual unsigned GetDefaultMinimumWidth() { return MinimumWidth; }
	virtual unsigned GetDefaultMinimumHeight(unsigned width) { return MinimumHeight; }
	virtual unsigned GetDefaultNominalWidth() { return NominalWidth; }
	virtual unsigned GetDefaultNominalHeight(unsigned width) { return NominalHeight; }
	virtual unsigned GetDefaultPreferredWidth() { return PreferredWidth; }
	virtual unsigned GetDefaultPreferredHeight(unsigned width) { return PreferredHeight; }
	virtual void GetDefaultMargins(WidgetSizes::Margins& margins) { margins = WidgetSizes::Margins(Margin); }

	OpRect m_layout_rect;
	OpWidget* m_parent_op_widget;
	BOOL m_visible;
	BOOL m_enabled;
};

#endif // SELFTEST

#endif // TEST_WIDGET_H
