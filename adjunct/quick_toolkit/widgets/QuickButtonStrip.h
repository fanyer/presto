/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Cihat Imamoglu (cihati)
 */
 
#ifndef QUICK_BUTTON_STRIP_H
#define QUICK_BUTTON_STRIP_H
 
#include "adjunct/quick_toolkit/widgets/QuickWidget.h"
#include "modules/widgets/OpWidget.h"

class QuickStackLayout;

/** @brief Represents the group of buttons that normally appear at the bottom of a dialog
  * A button strip consists of two content areas - a primary group, where buttons that
  * close the dialog are usually displayed (OK, Cancel, etc) and a secondary group
  * where other controls that refer to the whole dialog can be placed
  */
class QuickButtonStrip : public QuickWidget
{
	IMPLEMENT_TYPEDOBJECT(QuickWidget);
public:
	enum Order
	{
		SYSTEM, ///< order of widgets in primary group decided by system we're running on
		NORMAL,	///< widgets in primary group are displayed in the order they were inserted
		REVERSE ///< widgets in primary group are displayed in reverse order
	};

	/** Constructor
	  * @param order Order of widgets in the primary group, use this to reverse buttons
	  * 	where necessary (eg. Mac, sometimes Unix GTK)
	  */
	QuickButtonStrip(Order order = SYSTEM);

	virtual ~QuickButtonStrip();

	OP_STATUS Init();

	/** Inserts a widget into the primary area of the button strip (usually displayed on the right)
	  * @param widget Widget to insert (takes ownership)
	  */
	OP_STATUS InsertIntoPrimaryContent(QuickWidget* widget);

	/** Content to use in the secondary area of the button strip (usually displayed on the left)
	  * @param widget Content to use (takes ownership)
	  */
	OP_STATUS SetSecondaryContent(QuickWidget* widget);

	// Overriding QuickWidget
	virtual BOOL HeightDependsOnWidth()  { return FALSE; }
	virtual OP_STATUS Layout(const OpRect& rect);
	virtual void SetParentOpWidget(class OpWidget* parent);
	virtual void Show();
	virtual void Hide();
	virtual BOOL IsVisible();
	virtual void SetEnabled(BOOL enabled);	
	virtual void SetContainer(QuickWidgetContainer* container);

protected:
	virtual unsigned GetDefaultMinimumWidth();
	virtual unsigned GetDefaultMinimumHeight(unsigned width);
	virtual unsigned GetDefaultNominalWidth() { return GetMinimumWidth(); }
	virtual unsigned GetDefaultNominalHeight(unsigned width) { return GetMinimumHeight(width); }
	virtual unsigned GetDefaultPreferredWidth();
	virtual unsigned GetDefaultPreferredHeight(unsigned width);
	virtual void GetDefaultMargins(WidgetSizes::Margins& margins);

private:
	const Order m_order;
	QuickStackLayout* m_content;
	QuickStackLayout* m_primary;
};

#endif //QUICK_BUTTON_STRIP_H
