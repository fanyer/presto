/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Cihat Imamoglu (cihati)
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/QuickPagingLayout.h"
#include "adjunct/quick/managers/AnimationManager.h"


namespace
{

const unsigned ANIMATION_INTERVAL = 250;

} // unnamed namespace

class QuickPagingLayout::PageChangeAnimation : public QuickAnimation
{
public:
	PageChangeAnimation(QuickPagingLayout& paging_layout) : m_paging_layout(paging_layout)
	{
		m_animation_curve = ANIM_CURVE_LINEAR;
	}

	// QuickAnimation
	virtual void OnAnimationStart() {}
	virtual void OnAnimationComplete() {}

	virtual void OnAnimationUpdate(double position)
	{
		m_paging_layout.SetCurrentFactor(position);
	}

private:
	QuickPagingLayout& m_paging_layout;
};


QuickPagingLayout::QuickPagingLayout()
	: m_active_pos(-1)
	, m_previous_pos(-1)
	, m_parent_op_widget(0)
	, m_visible(TRUE)
	, m_valid(FALSE)
	, m_common_height_for_tabs(true)
	, m_animation(NULL)
	, m_current_factor(0.0)
{
}

QuickPagingLayout::~QuickPagingLayout()
{
	if (m_animation && m_animation->IsAnimating())
	{
		g_animation_manager->abortAnimation(m_animation);
	}
	OP_DELETE(m_animation);
}

OP_STATUS QuickPagingLayout::InsertPage(QuickWidget* content, int pos)
{
	OpAutoPtr<QuickWidget> content_holder(content);

	if (pos == -1)
		pos = m_pages.GetCount();

	RETURN_IF_ERROR(m_pages.Insert(pos, content));
	content_holder.release();

	content->SetContainer(this);

	if (m_active_pos < 0)
	{
		GoToPage(0);
	}
	else
	{
		content->SetParentOpWidget(0);
		if (pos <= m_active_pos)
			++m_active_pos;
	}
	
	Invalidate();
	return OpStatus::OK;
}

void QuickPagingLayout::RemovePage(int pos)
{
	if (0 > pos || UINT32(pos) >= m_pages.GetCount())
	{
		OP_ASSERT(!"Index out of bounds");
		return;
	}

	m_pages.Remove(pos);

	if (pos == m_active_pos)
	{
		// If active page is removed, then make the first page active.
		m_active_pos = -1;
		if (m_pages.GetCount() > 0)
			GoToPage(0);
	}
	else if (pos < m_active_pos)
	{
		--m_active_pos;
	}

	Invalidate();
}

void QuickPagingLayout::RemovePage(QuickWidget* content)
{
	const INT32 pos = m_pages.Find(content);
	if (pos >= 0)
	{
		RemovePage(pos);
	}
}

OP_STATUS QuickPagingLayout::GoToPage(const QuickWidget* content)
{
	const INT32 pos = m_pages.Find(const_cast<QuickWidget*>(content));
	return pos >= 0 ? GoToPage(pos) : OpStatus::ERR;
}

OP_STATUS QuickPagingLayout::GoToPage(int pos)
{
	if (0 > pos || UINT32(pos) >= m_pages.GetCount())
	{
		OP_ASSERT(!"Index out of bounds");
		return OpStatus::ERR_OUT_OF_RANGE;
	}

	if (pos == m_active_pos)
		return OpStatus::OK;

	if (m_active_pos >= 0)
		m_pages[m_active_pos]->SetParentOpWidget(0);

	m_previous_pos = m_active_pos;
	m_active_pos = pos;

	if (!m_visible)
		return OpStatus::OK;

	m_pages[m_active_pos]->SetParentOpWidget(m_parent_op_widget);

	if (m_previous_pos >= 0)
		StartAnimation();

	if (m_rect.IsEmpty())
		return OpStatus::OK;

	return Layout(m_rect);
}

OP_STATUS QuickPagingLayout::GoToPreviousPage()
{
	if (HasPreviousPage())
		return GoToPage(m_active_pos - 1);

	return OpStatus::ERR;
}

OP_STATUS QuickPagingLayout::GoToNextPage()
{
	if (HasNextPage())
		return GoToPage(m_active_pos + 1);

	return OpStatus::ERR;
}

void QuickPagingLayout::SetCurrentFactor(double current_factor)
{
	m_current_factor = current_factor;
	Invalidate();
}

void QuickPagingLayout::StartAnimation()
{
	if (m_common_height_for_tabs)
	{
		return;
	}

	if (m_animation && m_animation->IsAnimating())
	{
		g_animation_manager->abortAnimation(m_animation);
	}

	OP_DELETE(m_animation);
	m_animation = OP_NEW(PageChangeAnimation, (*this));
	if (m_animation)
	{
		g_animation_manager->startAnimation(m_animation, m_animation->m_animation_curve, ANIMATION_INTERVAL);
	}
}

OP_STATUS QuickPagingLayout::Layout(const OpRect& rect)
{
	m_rect = rect;

	if (m_active_pos >= 0)
		RETURN_IF_ERROR(m_pages[m_active_pos]->Layout(rect));

	return OpStatus::OK;
}

void QuickPagingLayout::OnContentsChanged()
{
	Invalidate();
}

void QuickPagingLayout::SetEnabled(BOOL enabled)
{
	for (unsigned i = 0; i < m_pages.GetCount(); ++i)
		m_pages[i]->SetEnabled(enabled);
}

void QuickPagingLayout::Show()
{
	m_visible = TRUE;

	if (m_active_pos >= 0)
		m_pages[m_active_pos]->SetParentOpWidget(m_parent_op_widget);
}

void QuickPagingLayout::Hide()
{
	m_visible = FALSE;

	if (m_active_pos >= 0)
		m_pages[m_active_pos]->SetParentOpWidget(0);
}

void QuickPagingLayout::SetParentOpWidget(OpWidget* parent)
{
	m_parent_op_widget = parent;

	if (m_active_pos >= 0 && m_visible)
		m_pages[m_active_pos]->SetParentOpWidget(parent);
}

BOOL QuickPagingLayout::IsVisible()
{
	return m_visible;
}

unsigned QuickPagingLayout::GetDefaultMinimumWidth()
{
	unsigned width = 0;
	if (m_common_height_for_tabs)
	{
		CalculateSizes();
		width = m_default_minimum_width;
	}
	else if (m_previous_pos < 0)
	{
		width = m_pages[m_active_pos]->GetMinimumWidth();
	}
	else
	{
		width = m_pages[m_previous_pos]->GetMinimumWidth();
		int delta = m_pages[m_active_pos]->GetMinimumWidth() - width;
		delta *= m_current_factor;
		width += delta;
	}
	return width;
}

unsigned QuickPagingLayout::GetDefaultNominalWidth()
{
	unsigned width = 0;
	if (m_common_height_for_tabs)
	{
		CalculateSizes();
		width = m_default_nominal_width;
	}
	else if (m_previous_pos < 0)
	{
		width = m_pages[m_active_pos]->GetNominalWidth();
	}
	else
	{
		width = m_pages[m_previous_pos]->GetNominalWidth();
		int delta = m_pages[m_active_pos]->GetNominalWidth() - width;
		delta *= m_current_factor;
		width += delta;
	}
	return width;
}

unsigned QuickPagingLayout::GetDefaultPreferredWidth()
{
	unsigned width = 0;
	if (m_common_height_for_tabs)
	{
		CalculateSizes();
		width = m_default_preferred_width;
	}
	else
	{
		width = m_pages[m_active_pos]->GetPreferredWidth();
	}
	return width;
}

unsigned QuickPagingLayout::GetDefaultMinimumHeight(unsigned width)
{
	unsigned height = 0;

	if (m_common_height_for_tabs)
	{
		CalculateSizes();
		if (!m_height_depends_on_width)
			return m_default_minimum_height;

		for (unsigned i = 0; i < m_pages.GetCount(); ++i)
		{
			QuickWidget* widget = m_pages[i];
			height = max(height, widget->GetMinimumHeight(width));
		}
	}
	else if (m_previous_pos < 0)
	{
		height = m_pages[m_active_pos]->GetMinimumHeight(width);
	}
	else
	{
		height = m_pages[m_previous_pos]->GetMinimumHeight(width);
		int delta = m_pages[m_active_pos]->GetMinimumHeight(width) - height;
		delta *= m_current_factor;
		height += delta;
	}
	return height;
}

unsigned QuickPagingLayout::GetDefaultNominalHeight(unsigned width)
{
	unsigned height = 0;

	if (m_common_height_for_tabs)
	{
		CalculateSizes();
		if (!m_height_depends_on_width)
			return m_default_nominal_height;

		for (unsigned i = 0; i < m_pages.GetCount(); ++i)
		{
			QuickWidget* widget = m_pages[i];
			height = max(height, widget->GetNominalHeight(width));
		}
	}
	else if (m_previous_pos < 0)
	{
		height = m_pages[m_active_pos]->GetNominalHeight(width);
	}
	else
	{
		height = m_pages[m_previous_pos]->GetNominalHeight(width);
		int delta = m_pages[m_active_pos]->GetNominalHeight(width) - height;
		delta *= m_current_factor;
		height += delta;
	}
	return height;
}

unsigned QuickPagingLayout::GetDefaultPreferredHeight(unsigned width)
{
	unsigned height = 0;

	if (m_common_height_for_tabs)
	{
		CalculateSizes();
		if (!m_height_depends_on_width)
			return m_default_preferred_height;

		for (unsigned i = 0; i < m_pages.GetCount(); ++i)
		{
			QuickWidget* widget = m_pages[i];
			height = max(height, widget->GetPreferredHeight(width));
		}
	}
	else
	{
		height = m_pages[m_active_pos]->GetPreferredHeight(width);
	}
	return height;
}

void QuickPagingLayout::CalculateSizes()
{
	if (m_valid)
		return;

	m_height_depends_on_width = false;
	m_default_preferred_width = 0;
	m_default_preferred_height = 0;
	m_default_minimum_width = 0;
	m_default_minimum_height = 0;
	m_default_nominal_width = 0;
	m_default_nominal_height = 0;
	m_default_margins = WidgetSizes::Margins(0);

	for (unsigned i = 0; i < m_pages.GetCount(); ++i)
	{
		QuickWidget* widget = m_pages[i];
		if (widget->HeightDependsOnWidth())
		{
			m_height_depends_on_width = true;
			break;
		}
	}

	for (unsigned i = 0; i < m_pages.GetCount(); ++i)
	{
		QuickWidget* widget = m_pages[i];
		m_default_margins.bottom 	= max(m_default_margins.bottom, widget->GetMargins().bottom);
		m_default_margins.right 	= max(m_default_margins.right, widget->GetMargins().right);
		m_default_margins.left	 	= max(m_default_margins.left, widget->GetMargins().left);
		m_default_margins.top	 	= max(m_default_margins.top, widget->GetMargins().top);

		m_default_preferred_width	= max(m_default_preferred_width, widget->GetPreferredWidth());
		m_default_minimum_width		= max(m_default_minimum_width, widget->GetMinimumWidth());
		m_default_nominal_width		= max(m_default_nominal_width, widget->GetNominalWidth());

		if (!m_height_depends_on_width)
		{
			m_default_preferred_height	= max(m_default_preferred_height, widget->GetPreferredHeight());
			m_default_minimum_height 	= max(m_default_minimum_height, widget->GetMinimumHeight());
			m_default_nominal_height 	= max(m_default_nominal_height, widget->GetNominalHeight());
		}
	}

	m_valid = TRUE;
}
