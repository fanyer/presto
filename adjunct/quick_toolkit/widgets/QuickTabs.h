/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Cihat Imamoglu (cihati)
 */

#ifndef QUICK_TABS_H
#define QUICK_TABS_H

#include "adjunct/quick_toolkit/widgets/QuickWidget.h"
#include "adjunct/quick_toolkit/widgets/QuickWidgetContainer.h"
#include "modules/widgets/OpWidget.h"

class OpTabs;
class QuickPagingLayout;
class QuickSkinElement;

/** @brief 
 * */
class QuickTabs : public QuickWidget, public QuickWidgetContainer, protected OpWidgetListener
{
	IMPLEMENT_TYPEDOBJECT(QuickWidget);
public:
	QuickTabs();
	virtual ~QuickTabs();
	
	OP_STATUS Init();
	
	// Override QuickWidget
	virtual BOOL HeightDependsOnWidth()  { return FALSE; }
	virtual OP_STATUS Layout(const OpRect& rect);
	virtual void SetParentOpWidget(class OpWidget* parent);
	virtual void Show();
	virtual void Hide();
	virtual BOOL IsVisible();
	virtual void SetEnabled(BOOL enabled);
	
	// Override QuickWidgetContainer
	virtual void OnContentsChanged();

	/**
	 * Inserts a widget
	 * @param content Pointer to the content to be added (takes ownership)
	 * @param title Title of the tab(that is to be shown on tab buttons)
	 * @param name tab name (for Watir)
	 * @param pos The position to add. Default position is the end of the stack
	 */
	OP_STATUS InsertTab(QuickWidget* content, const OpStringC& title, const OpStringC8& name = NULL, int pos = -1);
	
	/** Remove and deallocate a tab at a certain position 
	  * @param pos Position of tab to remove
	  */
	void RemoveTab(int pos);

	/** Show a tab at a given position, and hide the others
	  * @param pos Position of the tab
	  */
	void GoToTab(int pos);
	
	/** @return the active tab content widget
	  */
	QuickWidget* GetActiveTab() const;

protected:
	
	virtual void OnChange(OpWidget *widget, BOOL changed_by_mouse);
	
	// Override QuickWidget
	virtual unsigned GetDefaultMinimumWidth();
	virtual unsigned GetDefaultMinimumHeight(unsigned width);
	virtual unsigned GetDefaultNominalWidth() { return GetMinimumWidth(); }
	virtual unsigned GetDefaultNominalHeight(unsigned width) { return GetMinimumHeight(width); }
	virtual unsigned GetDefaultPreferredWidth();
	virtual unsigned GetDefaultPreferredHeight(unsigned width);
	virtual void GetDefaultMargins(WidgetSizes::Margins& margins);

private:
	OpTabs*				m_tabs;
	QuickPagingLayout*	m_pages;
	QuickSkinElement*	m_skinned_pages;
};

#endif //QUICK_TABS_H
