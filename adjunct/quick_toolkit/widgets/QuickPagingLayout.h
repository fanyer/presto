/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Cihat Imamoglu (cihati)
 */

#ifndef QUICK_PAGING_LAYOUT_H
#define QUICK_PAGING_LAYOUT_H

#include "adjunct/desktop_util/adt/optightvector.h"
#include "adjunct/quick_toolkit/widgets/QuickWidget.h"
#include "adjunct/quick_toolkit/widgets/QuickWidgetContainer.h"


class QuickAnimation;


/** @brief A widget that shows one child widget at a time. Useful for implementing wizards, setting dialogs etc.
 * */
class QuickPagingLayout : public QuickWidget, public QuickWidgetContainer
{
	IMPLEMENT_TYPEDOBJECT(QuickWidget);
public:
	QuickPagingLayout();
	~QuickPagingLayout();
	
	OP_STATUS Init() { return OpStatus::OK; }

	// Override QuickWidget
	virtual BOOL HeightDependsOnWidth() { CalculateSizes(); return m_height_depends_on_width; }
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
	 * @param pos The position to add. Default position is the end
	 */
	OP_STATUS InsertPage(QuickWidget* content, int pos = -1);
	
	/** Remove and deallocate a page at a certain position
	  * @param pos Position of page to remove
	  */
	void RemovePage(int pos);
	
	/** Remove and deallocate a page
	  * @param content Pointer to the content to be removed
	  */
	void RemovePage(QuickWidget* content);
	
	/** Show a page at a given position, and hide the others
	  * @param pos Position of the widget
	  */
	OP_STATUS GoToPage(int pos);
	
	/** Show a page and hide the others
	  * @param content Pointer to the content to be shown
	  */
	OP_STATUS GoToPage(const QuickWidget* content);

	bool HasPreviousPage() const { return m_active_pos > 0; }
	bool HasNextPage() const { return m_active_pos < int(GetPageCount()) - 1; }
	OP_STATUS GoToPreviousPage();
	OP_STATUS GoToNextPage();


	/** @return the number of pages
	  */
	unsigned GetPageCount() const { return m_pages.GetCount(); }

	/** @return the active page content widget
	  */
	QuickWidget* GetActivePage() const { return m_active_pos >= 0 ? m_pages[m_active_pos] : NULL; }

	/** @return the active page number
	  */
	int GetActivePagePos() const { return m_active_pos; }
	
	/**
	 * Set flag responsible for method of calculating dialog's height.
	 * @param common_height if set to true all dialog's pages will use common height (maximum height of all pages)
	 *                      if set to false every page will use its own optimal height
	 */
	void SetCommonHeightForTabs(bool common_height) { m_common_height_for_tabs = common_height; }

protected:
	virtual unsigned GetDefaultMinimumWidth();
	virtual unsigned GetDefaultMinimumHeight(unsigned width);
	virtual unsigned GetDefaultNominalWidth();
	virtual unsigned GetDefaultNominalHeight(unsigned width);
	virtual unsigned GetDefaultPreferredWidth();
	virtual unsigned GetDefaultPreferredHeight(unsigned width);
	virtual void GetDefaultMargins(WidgetSizes::Margins& margins) { CalculateSizes(); margins = m_default_margins; }
	
private:
	class PageChangeAnimation;

	void Invalidate() { m_valid = FALSE; BroadcastContentsChanged(); }

	// Override QuickWidget
	void	 CalculateSizes();

	/*
	 * Start animation of dialog (change of dialog's width and height from the height of the previous page to
	 * height of the current page). It is done automatically after switching pages.
	 */
	void StartAnimation();

	/** 
	 * Set current factor of dialog's dimensions. Its value should vary between 0 and 1. If it is set to 0 dialog's
	 * width and height will correspond to dimensions of the previous page; if it is set to 1 dialog's width and height
	 * correspond to width and height of the current page. It is used during dialog's resizing after page is changed.
	 * @param current_factor current factor (see full description of the method)
	 */
	void SetCurrentFactor(double current_factor);
	
	OpAutoTightVector<QuickWidget*> m_pages;
	int m_active_pos;
	int m_previous_pos;

	OpWidget* 	m_parent_op_widget;
	BOOL		m_visible;
	BOOL		m_valid;
	bool		m_common_height_for_tabs;
	OpRect		m_rect;
	QuickAnimation* m_animation;

	bool		m_height_depends_on_width;
	unsigned	m_default_minimum_width;
	unsigned	m_default_minimum_height;
	unsigned	m_default_nominal_width;
	unsigned	m_default_nominal_height;
	unsigned	m_default_preferred_width;
	unsigned	m_default_preferred_height;
	double		m_current_factor;
	WidgetSizes::Margins m_default_margins;
};

#endif //QUICK_PAGING_LAYOUT_H
