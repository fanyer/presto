/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Chris Pine (chrispi)
 */

#ifndef QUICK_ANIMATED_WIDGET_H
#define QUICK_ANIMATED_WIDGET_H

#include "adjunct/quick_toolkit/widgets/QuickOpWidgetWrapper.h"


/** @brief A QuickWidget, based on an OpWidget, that will animate
  * @param OpWidgetType OpWidget to use as base
  * eg.
  *   class MyQuickWidget : public QuickAnimatedWidget<MyOpWidget>
  * or
  *   QuickWidget* widget = OP_NEW(QuickAnimatedWidget<MyOpWidget>, ());
  *
  * @note Please use predefined wrappers from QuickWidgetDecls.h, and feel free
  * to add new ones there
  *
  * This is a simple wrapper for OpWidgets that provides all the information
  * needed in the Quick layout system, as well as handle animations.
  */
class QuickAnimationListener;

OP_STATUS QuickAnimatedWidget_Layout(const OpRect& new_rect, QuickAnimationListener* listener, OpWidget* opwidget);
void QuickAnimatedWidget_SetZ(OpWidget::Z z, OpWidget* opwidget);

template <class OpWidgetType> class QuickAnimatedWidget : public QuickOpWidgetWrapper<OpWidgetType>
{
public:
	QuickAnimatedWidget() : m_listener(NULL) {}

	virtual OP_STATUS Layout(const OpRect& rect) { return QuickAnimatedWidget_Layout(rect, m_listener, this->GetOpWidget()); }

	virtual void SetZ(OpWidget::Z z) { QuickAnimatedWidget_SetZ(z, this->GetOpWidget()); }

	void SetListener(QuickAnimationListener* listener) { m_listener = listener; }

private:
	QuickAnimationListener* m_listener;
};







#endif // QUICK_ANIMATED_WIDGET_H
