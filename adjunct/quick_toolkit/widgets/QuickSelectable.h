/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Cihat Imamoglu (cihati)
 */

#ifndef QUICK_SELECTABLE_H
#define QUICK_SELECTABLE_H

#include "adjunct/quick_toolkit/widgets/QuickTextWidget.h"
#include "adjunct/quick_toolkit/widgets/QuickWidgetContent.h"
#include "modules/widgets/OpButton.h"


/** @brief A base class for checkboxes and radio buttons ('selectables')
  * Offers the control itself and correctly placed content (text or other content)
  */
class QuickSelectable : public QuickTextWidgetWrapper<OpCheckBox>, protected OpWidgetListener
{
	typedef QuickTextWidgetWrapper<OpCheckBox> Base;
	IMPLEMENT_TYPEDOBJECT(Base);
public:
	QuickSelectable() : m_image_width(0) {}
	virtual ~QuickSelectable();

	virtual OP_STATUS Init();

	/** Similar to inline child, but has no effect on changing state, i.e toggling/checking/unchecking
	 **/
	void SetChild(QuickWidget* new_child);
	
	// Overriding QuickWidget
	virtual OP_STATUS Layout(const OpRect& rect);
	virtual void SetParentOpWidget(OpWidget* widget);
	
protected:
	virtual unsigned GetDefaultMinimumWidth();
	virtual unsigned GetDefaultMinimumHeight(unsigned width);
	virtual unsigned GetDefaultNominalWidth();
	virtual unsigned GetDefaultNominalHeight(unsigned width);
	virtual unsigned GetDefaultPreferredWidth();
	virtual unsigned GetDefaultPreferredHeight(unsigned width);
	virtual void GetDefaultMargins(WidgetSizes::Margins& margins);
	
	virtual void OnChange(OpWidget *widget, BOOL changed_by_mouse);
	
	/** @brief This function is run by the child class(QuickRadioButton or QuickCheckBox) to call necessary functions for skinning and for OpButton
	 **/	
	virtual OP_STATUS ConstructSelectableQuickWidget() = 0;
	
	/** @brief Given widget's skin name, find its image's sizes and save in image_width and image_height
	 **/
	OP_STATUS FindImageSizes(const char* widget_type);
	
private:
	unsigned GetHorizontalSpacing() const { return m_image_width; }
	unsigned GetVerticalSpacing() { return m_child->GetMargins().top; }

	QuickWidgetContent m_child;

	INT32 m_image_width;
};

#endif // QUICK_SELECTABLE_H
