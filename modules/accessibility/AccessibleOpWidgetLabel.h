/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef ACCESSIBILE_OPWIDGET_LABEL_H
#define ACCESSIBILE_OPWIDGET_LABEL_H

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

class OpWidget;
class OpAccessibleItem;

/** This class is used to join an abstract text label with an OpWidget.
  * This is primarilly used to join up OpLabels with OpDropdowns and OpEdits in the UI,
  * and to join <label> and <input> tags in the web page.
  * 
  * Subclasses (labels) would normally also inherit from OpAccessibilityExtensionListener, and call
  * GetControlExtension from GetAccessibleControlForLabel.
  */

class AccessibleOpWidgetLabel
{
public:
	AccessibleOpWidgetLabel();
	virtual ~AccessibleOpWidgetLabel();

/** Link this label up with an OpWidget control. The OpWidget MUST call DissociateLabelFromOpWidget when it stops being valid.
  */
	void AssociateLabelWithOpWidget(OpWidget* widget);

/** Break the link between the label and control. For instance when the label is destroyed.
  */
	void DissociateLabelFromOpWidget(OpWidget* widget);

/** Get the OpAccessibilityExtension from the label. Used by OpWidget::GetAccessibleLabelForControl
  */
	virtual OpAccessibleItem* GetLabelExtension() = 0;

/** Getter for the OpWidget. m_op_widget should only be modified from within AssociateLabelWithOpWidget.
  */
	const OpWidget* GetOpWidget() const { return m_op_widget; }

protected:
/** Get the AccessibilityExtension for the OpWidget.
  */
	OpAccessibleItem* GetControlExtension();

private:
	OpWidget* m_op_widget;
};

#endif // ACCESSIBILITY_EXTENSION_SUPPORT

#endif // ACCESSIBILITY_TREE_LABEL_NODE_H
