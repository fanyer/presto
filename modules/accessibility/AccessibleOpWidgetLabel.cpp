/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

#include "modules/accessibility/AccessibleOpWidgetLabel.h"
#include "modules/widgets/OpWidget.h"

AccessibleOpWidgetLabel::AccessibleOpWidgetLabel()
	: m_op_widget(NULL)
{
}

AccessibleOpWidgetLabel::~AccessibleOpWidgetLabel()
{
	if (m_op_widget)
		m_op_widget->SetAccessibilityLabel(NULL);
}

void AccessibleOpWidgetLabel::AssociateLabelWithOpWidget(OpWidget* widget)
{
	if (m_op_widget)
		m_op_widget->SetAccessibilityLabel(NULL);
	m_op_widget = widget;
	if (m_op_widget)
		m_op_widget->SetAccessibilityLabel(this);
}

void AccessibleOpWidgetLabel::DissociateLabelFromOpWidget(OpWidget* widget)
{
	if (m_op_widget == widget)
		m_op_widget->SetAccessibilityLabel(NULL);
	m_op_widget = NULL;
}

OpAccessibleItem* AccessibleOpWidgetLabel::GetControlExtension()
{
	return m_op_widget;
}

#endif // ACCESSIBILITY_TREE_LABEL_NODE_H
