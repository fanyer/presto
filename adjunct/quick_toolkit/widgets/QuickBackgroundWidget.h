/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef QUICK_BACKGROUND_WIDGET_H
#define QUICK_BACKGROUND_WIDGET_H

#include "adjunct/quick_toolkit/widgets/QuickOpWidgetWrapper.h"
#include "adjunct/quick_toolkit/widgets/QuickWidgetContainer.h"
#include "adjunct/quick_toolkit/widgets/QuickWidgetContent.h"


/** A QuickWidgetContainer that places some OpWidget underneath its content.
  *
  * Optionally, it pads the content using skin information for the background
  * OpWidget.
  *
  * @author Arjan van Leeuwen (arjanl)
  * @author Wojciech Dzierzanowski (wdzierzanowski)
  */
class GenericQuickBackgroundWidget
		: public GenericQuickOpWidgetWrapper
		, public QuickWidgetContainer
{
	IMPLEMENT_TYPEDOBJECT(GenericQuickOpWidgetWrapper);
public:
	OP_STATUS Init();

	/** Set contents to display in this widget
	  * @param content
	  */
	void SetContent(QuickWidget* content);

	/** Only apply padding if the content doesn't prefer to be bigger
	  * @param dynamic_padding
	  */
	void SetDynamicPadding(bool dynamic_padding) { m_dynamic_padding = dynamic_padding; }

	/** @return Contents of this widget
	  */
	QuickWidget* GetContent() const { return m_content.GetContent(); }

	// Override QuickWidget
	virtual BOOL HeightDependsOnWidth() { return m_content->HeightDependsOnWidth(); }
	virtual OP_STATUS Layout(const OpRect& rect);

	// Override QuickWidgetContainer
	virtual void OnContentsChanged() { BroadcastContentsChanged(); }

protected:
	/** @param add_padding whether to pad content using skin information
	  */
	explicit GenericQuickBackgroundWidget(bool add_padding) : m_add_padding(add_padding), m_dynamic_padding(false) {}

	/** @return the OpWidget to wrap using this widget.  Called from Init().
	  */
	virtual OpWidget* CreateOpWidget() = 0;

	// Override QuickWidget
	virtual unsigned GetDefaultMinimumWidth() { return m_content->GetMinimumWidth() + GetHorizontalPadding(); }
	virtual unsigned GetDefaultMinimumHeight(unsigned width) { return m_content->GetMinimumHeight(GetContentWidth(width)) + GetVerticalPadding(); }
	virtual unsigned GetDefaultNominalWidth() { return m_content->GetNominalWidth() + GetHorizontalPadding(); }
	virtual unsigned GetDefaultNominalHeight(unsigned width) { return m_content->GetNominalHeight(GetContentWidth(width)) + GetVerticalPadding(); }
	virtual unsigned GetDefaultPreferredWidth();
	virtual unsigned GetDefaultPreferredHeight(unsigned width);

private:
	unsigned GetHorizontalPadding() { return m_add_padding ? GetOpWidget()->GetPaddingLeft() + GetOpWidget()->GetPaddingRight() : 0; }
	unsigned GetVerticalPadding() { return m_add_padding ? GetOpWidget()->GetPaddingTop() + GetOpWidget()->GetPaddingBottom() : 0; }
	unsigned GetContentWidth(unsigned width) { return m_add_padding && width < WidgetSizes::UseDefault ? max(int(width - GetHorizontalPadding()), 0) : width; }

	QuickWidgetContent	m_content;
	const bool			m_add_padding;
	bool				m_dynamic_padding;
};


/** Generates a concrete GenericQuickBackgroundWidget subclass for some
  * OpWidgetType.
  */
template <typename OpWidgetType>
class QuickBackgroundWidget : public GenericQuickBackgroundWidget
{
	IMPLEMENT_TYPEDOBJECT(GenericQuickBackgroundWidget);
public:
	/** @param add_padding whether to pad content using skin information for
	  *		OpWidgetType
	  */
	explicit QuickBackgroundWidget(bool add_padding = false) : GenericQuickBackgroundWidget(add_padding) {}

	/** @return the underlying OpWidget for this QuickWidget
	  */
	OpWidgetType* GetOpWidget() const { return static_cast<OpWidgetType*>(GenericQuickBackgroundWidget::GetOpWidget()); }

protected:
	virtual OpWidget* CreateOpWidget()
	{
		OpWidgetType* widget = 0;
		RETURN_VALUE_IF_ERROR(QuickOpWidgetBase::Construct<OpWidgetType>(&widget), 0);
		return widget;
	}
};

#endif // QUICK_BACKGROUND_WIDGET_H
