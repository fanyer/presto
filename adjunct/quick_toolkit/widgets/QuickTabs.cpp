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

#include "adjunct/quick_toolkit/widgets/QuickTabs.h"

#include "adjunct/desktop_util/widget_utils/WidgetUtils.h"
#include "adjunct/quick_toolkit/widgets/OpGroup.h"
#include "adjunct/quick_toolkit/widgets/OpTabs.h"
#include "adjunct/quick_toolkit/widgets/QuickBackgroundWidget.h"
#include "adjunct/quick_toolkit/widgets/QuickPagingLayout.h"
#include "adjunct/quick_toolkit/widgets/QuickSkinElement.h"
#include "modules/skin/OpSkinManager.h"

namespace
{
	typedef QuickBackgroundWidget<OpGroup> TabPage;
}


QuickTabs::QuickTabs()
	: m_tabs(0)
	, m_pages(0)
	, m_skinned_pages(0)
{
}

QuickTabs::~QuickTabs()
{
	if (m_tabs && !m_tabs->IsDeleted())
	{
		m_tabs->Delete();
	}
	OP_DELETE(m_skinned_pages);
}

OP_STATUS QuickTabs::Init()
{
	m_skinned_pages = OP_NEW(QuickSkinElement, ());
	RETURN_OOM_IF_NULL(m_skinned_pages);
	RETURN_IF_ERROR(m_skinned_pages->Init());
	m_skinned_pages->SetContainer(this);

	// If a skin contains "Dialog Page Skin" but not "Dialog Tab Page Skin"
	// use "Dialog Page Skin" as fallback instead of "Dialog Tab Page Skin"
	// from standard skin, this is the conventional behavior and some skins
	// depend on this to work properly. See DSK-289395
	const char* skin = "Dialog Tab Page Skin";
	OpSkin* current = g_skin_manager->GetCurrentSkin();
	if (current && !current->GetSkinElement("Dialog Tab Page Skin") && current->GetSkinElement("Dialog Page Skin"))
		skin = "Dialog Page Skin";

	m_skinned_pages->SetSkin(skin);

	m_pages = OP_NEW(QuickPagingLayout,());
	RETURN_OOM_IF_NULL(m_pages);
	m_skinned_pages->SetContent(m_pages);
		
	RETURN_IF_ERROR(OpTabs::Construct(&m_tabs));
	m_tabs->SetListener(this);

	return OpStatus::OK;
}

OP_STATUS QuickTabs::InsertTab(QuickWidget* content, const OpStringC& title, const OpStringC8& name, int pos)
{
	// Make each page a child of an OpGroup.  This is a Watir and Accessibility
	// requirement.
	OpAutoPtr<TabPage> page(OP_NEW(TabPage, ()));
	RETURN_OOM_IF_NULL(page.get());
	RETURN_IF_ERROR(page->Init());
	page->SetContent(content);

	OpButton* tab_button = m_tabs->AddTab(title.CStr());
	RETURN_VALUE_IF_NULL(tab_button, OpStatus::ERR_NULL_POINTER);

	tab_button->SetPropertyPage(page->GetOpWidget());
	page->GetOpWidget()->SetTab(tab_button);
	page->GetOpWidget()->SetName(name);
	tab_button->SetName(name);

	const OP_STATUS result = m_pages->InsertPage(page.release(), pos);
	if (OpStatus::IsError(result))
	{
		m_tabs->RemoveTab(pos);
		return result;
	}

	if (m_tabs->GetSelected() == -1)
		GoToTab(0);

	BroadcastContentsChanged();
	return OpStatus::OK;
}

void QuickTabs::GoToTab(int pos)
{
	m_tabs->SetSelected(pos, TRUE);
}

void QuickTabs::RemoveTab(int pos)
{
	m_tabs->RemoveTab(pos);
	m_pages->RemovePage(pos);
	BroadcastContentsChanged();
}

QuickWidget* QuickTabs::GetActiveTab() const
{
	return m_pages->GetActivePage()->GetTypedObject<TabPage>()->GetContent();
}

void QuickTabs::SetParentOpWidget(OpWidget* parent)
{
	if (parent)
		parent->AddChild(m_tabs);
	else
		m_tabs->Remove();

	m_skinned_pages->SetParentOpWidget(parent);
}

void QuickTabs::Show()
{
	m_tabs->SetVisibility(TRUE);
	m_pages->Show();
}

void QuickTabs::Hide()
{
	m_tabs->SetVisibility(FALSE);
	m_pages->Hide();
}

BOOL QuickTabs::IsVisible()
{
	return m_pages->IsVisible();
}

void QuickTabs::SetEnabled(BOOL enabled)
{
	m_pages->SetEnabled(enabled);
	m_tabs->SetEnabled(enabled);
}

void QuickTabs::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	OP_ASSERT(widget == m_tabs);
	m_pages->GoToPage(m_tabs->GetActiveTab());
	WidgetUtils::SetFocusInDialogPage(m_skinned_pages->GetOpWidget());
}

OP_STATUS QuickTabs::Layout(const OpRect& rect)
{
	INT32 tabs_width = 0, tabs_height = 0;
	m_tabs->GetRequiredSize(tabs_width, tabs_height);
	OpRect tabs_rect = rect;
	tabs_rect.height = tabs_height;
	m_tabs->SetRect(tabs_rect);
	
	OpRect pages_rect = rect;
	pages_rect.y += tabs_rect.height;
	pages_rect.height -= tabs_rect.height + m_skinned_pages->GetMargins().top;
	m_skinned_pages->Layout(pages_rect);
	
	return OpStatus::OK;
}

void QuickTabs::OnContentsChanged()
{
	BroadcastContentsChanged();
}

unsigned QuickTabs::GetDefaultMinimumWidth()
{
	INT32 tabs_width = 0, tabs_height = 0;
	m_tabs->GetRequiredSize(tabs_width, tabs_height);
	
	return max(unsigned(tabs_width), m_skinned_pages->GetMinimumWidth());
}

unsigned QuickTabs::GetDefaultMinimumHeight(unsigned width)
{
	INT32 tabs_width = 0, tabs_height = 0;
	m_tabs->GetRequiredSize(tabs_width, tabs_height);

	return tabs_height + m_skinned_pages->GetMinimumHeight() + m_skinned_pages->GetMargins().top;
}

unsigned QuickTabs::GetDefaultPreferredWidth()
{
	INT32 tabs_width = 0, tabs_height = 0;
	m_tabs->GetPreferedSize(&tabs_width, &tabs_height, 0, 0);
	return max(m_skinned_pages->GetPreferredWidth(), unsigned(tabs_width));
}

unsigned QuickTabs::GetDefaultPreferredHeight(unsigned width)
{
	INT32 tabs_width = 0, tabs_height = 0;
	m_tabs->GetPreferedSize(&tabs_width, &tabs_height, 0, 0);
	if (unsigned(m_skinned_pages->GetPreferredHeight()) > WidgetSizes::UseDefault
			|| unsigned(tabs_height) > WidgetSizes::UseDefault)
		return m_skinned_pages->GetPreferredHeight();
	
	return tabs_height + m_skinned_pages->GetPreferredHeight() + m_skinned_pages->GetMargins().top;
}

void QuickTabs::GetDefaultMargins(WidgetSizes::Margins& margins)
{
	INT32 left_margin, top_margin, right_margin, bottom_margin;
	m_tabs->GetBorderSkin()->GetMargin(&left_margin, &top_margin, &right_margin, &bottom_margin);
	margins = m_skinned_pages->GetMargins();
	margins.left = max(margins.left, unsigned(left_margin));
	margins.right = max(margins.right, unsigned(right_margin));
	margins.top = top_margin;
}
