/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "OpGroup.h"
#include "adjunct/quick_toolkit/widgets/OpToolbar.h"
#include "adjunct/quick/Application.h"

#include "modules/widgets/OpButton.h"
#include "modules/widgets/OpButtonGroup.h"

#include "modules/locale/oplanguagemanager.h"

#include "modules/display/vis_dev.h"

/***********************************************************************************
**
**	OpGroup
**
***********************************************************************************/

OpGroup::OpGroup()
{
	m_button_group = NULL;
	m_tab = NULL;
	m_fail_button_group = FALSE;
}

OpGroup::~OpGroup()
{
	OP_DELETE(m_button_group);
}

OP_STATUS OpGroup::SetText(const uni_char* text)
{
	OP_STATUS status = m_string.Set(text, this);
	Invalidate(GetBounds());
	return status;
}

OP_STATUS OpGroup::GetText(OpString& text)
{
	return text.Set(m_string.Get());
}

void OpGroup::OnDeleted()
{
	OP_DELETE(m_button_group);
	m_button_group = NULL;

	OpWidget::OnDeleted();
}

void OpGroup::AddChild(OpWidget* child, BOOL is_internal_child, BOOL first)
{
	OpWidget::AddChild(child, is_internal_child, first);
	if (child->GetType() == WIDGET_TYPE_RADIOBUTTON)
	{
		if (!m_button_group)
			m_button_group = OP_NEW(OpButtonGroup, (this));

		if (!((OpRadioButton*)child)->RegisterToButtonGroup(m_button_group))
			m_fail_button_group = TRUE;
	}
}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

#ifdef PROPERTY_PAGE_CHILDREN_OF_TABS
OpAccessibleItem* OpGroup::GetAccessibleParent()
{
	if (m_tab)
	{
		if (m_tab->AccessibilitySkips())
			return m_tab->GetAccessibleParent();
		return m_tab;
	}

	return OpWidget::GetAccessibleParent();
}
#endif //PROPERTY_PAGE_CHILDREN_OF_TABS

int	OpGroup::GetAccessibleChildrenCount()
{
	int count = OpWidget::GetAccessibleChildrenCount();
	if (m_button_group && !m_fail_button_group)
		count+= 1;

	return count;
}

OpAccessibleItem* OpGroup::GetAccessibleChild(int child_nr)
{
	if (m_button_group && !m_fail_button_group && child_nr-- == 0)
		return m_button_group;

	return OpWidget::GetAccessibleChild(child_nr);
}

int OpGroup::GetAccessibleChildIndex(OpAccessibleItem* child)
{
	if (m_button_group && !m_fail_button_group && child == m_button_group)
		return 0;
	int index = OpWidget::GetAccessibleChildIndex(child);
	if (index != Accessibility::NoSuchChild && m_button_group && !m_fail_button_group)
		return index +1;
	return index;
}

OpAccessibleItem* OpGroup::GetAccessibleChildOrSelfAt(int x, int y)
{
	if (m_button_group && !m_fail_button_group)
		if (m_button_group->GetAccessibleChildOrSelfAt(x, y))
			return m_button_group;
	
	return OpWidget::GetAccessibleChildOrSelfAt(x, y);
}

OpAccessibleItem* OpGroup::GetAccessibleFocusedChildOrSelf()
{
	if (m_button_group && !m_fail_button_group)
		if (m_button_group->GetAccessibleFocusedChildOrSelf())
			return m_button_group;
	
	return OpWidget::GetAccessibleFocusedChildOrSelf();
}
#endif // ACCESSIBILITY_EXTENSION_SUPPORT

///////////////////////////////////////////////////////////////////////////////////////////

void OpGroup::OnShow(BOOL show)
{
	// If a group is being shown call a re-layout.
	// This is to make sure that controls that are hidden and shown like in
	// tabbed dialogs actually get a relayout and show themselves since
	// invisible controls are normally skipped
	if (show)
		Relayout();
}

void OpGroup::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	if (listener)
	{
		listener->OnMouseEvent(widget, pos, x, y, button, down, nclicks);
	}
}

BOOL OpGroup::OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked)
{
	if (listener)
	{
		return listener->OnContextMenu(widget, child_index, menu_point, avoid_rect, keyboard_invoked);
	}

	return FALSE;
}

void OpGroup::OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	if (listener && GetBounds().Contains(point) && !IsDead())
		listener->OnClick(this); // Invoke!
}

/***********************************************************************************
**
**	OpSpacer
**
***********************************************************************************/

OpSpacer::OpSpacer(SpacerType spacer_type) :
	m_spacer_type(spacer_type),
	m_descr_label(NULL),
	m_last_customize_state(FALSE),
	m_fixed_size(16)
{
	OpLabel::Construct(&m_descr_label);

	SetEnabled(FALSE);	//makes sure the mouse events (clicks) are sent to the toolbar
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	AccessibilitySkipsMe(TRUE);
#endif
	
	OpString label;
	switch (m_spacer_type)
	{
		case SPACER_FIXED:
			g_languageManager->GetString(Str::S_SPACER_FIXED, label);
			m_descr_label->SetLabel(label.CStr(), TRUE);
			SetSkinned(FALSE);
			m_descr_label->SetSkinned(FALSE);
			break;

		case SPACER_DYNAMIC:
			g_languageManager->GetString(Str::S_SPACER_DYNAMIC, label);
			m_descr_label->SetLabel(label.CStr(), TRUE);
			SetSkinned(FALSE);
			m_descr_label->SetSkinned(FALSE);
			SetGrowValue(2);
			break;

		case SPACER_SEPARATOR:
			break;

		case SPACER_WRAPPER:
			g_languageManager->GetString(Str::S_SPACER_WRAPPER, label);
			m_descr_label->SetLabel(label.CStr(), TRUE);
			SetSkinned(FALSE);
			m_descr_label->SetSkinned(FALSE);
			break;
	}
	m_descr_label->SetIgnoresMouse(TRUE);
	AddChild(m_descr_label);
}

void OpSpacer::Update()
{
	m_last_customize_state = g_application->IsCustomizingToolbars();
	GetPreferedSize(&m_required_width, &m_required_height, 0, 0);
}

void OpSpacer::GetRequiredSize(INT32 &width, INT32 &height)
{
	OP_ASSERT(width != -1 || !"The caller probably meant to call Update() first");
	if (m_required_width == -1 || m_last_customize_state != g_application->IsCustomizingToolbars())
		Update();
	width = m_required_width;
	height = m_required_height;
}

void OpSpacer::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	if (g_application->IsCustomizingToolbars() && m_spacer_type != SPACER_SEPARATOR)
	{
		m_descr_label->OnLayout();		//Making sure the size gets calculated properly
		m_descr_label->GetPreferedSize(w, h, cols, rows);
		return;
	}

	BOOL is_vertical = GetParent() && GetParent()->GetType() == WIDGET_TYPE_TOOLBAR && ((OpToolbar*)GetParent())->IsVertical();

	switch (m_spacer_type)
	{
		case SPACER_FIXED:
			*w = is_vertical ? 2 : m_fixed_size;
			*h = is_vertical ? m_fixed_size : 2;
			break;

		case SPACER_DYNAMIC:
			*w = 2;
			*h = 2;
			break;

		case SPACER_SEPARATOR:
			*w = is_vertical ? 16 : 6;
			*h = is_vertical ? 6 : 16;
			break;

		case SPACER_WRAPPER:
			*w = 1;
			*h = 1;
			break;
	}
}


/***********************************************************************************
**
**	OnLayout
**
***********************************************************************************/

void OpSpacer::OnLayout()
{
	m_descr_label->SetRect(GetBounds());
	m_descr_label->SetVisibility(g_application->IsCustomizingToolbars());
}

void OpSpacer::OnAdded()
{
	if (m_spacer_type == SPACER_SEPARATOR && GetParent() && GetParent()->GetType() == WIDGET_TYPE_TOOLBAR)
	{
		SetSkinned(TRUE);
		GetBorderSkin()->SetImage(((OpToolbar*)GetParent())->IsVertical() ? "Horizontal Separator" : "Vertical Separator");
	}
}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

int OpSpacer::GetAccessibleChildrenCount()
{
	return 0;
}


OpAccessibleItem* OpSpacer::GetAccessibleChild(int)
{
	return NULL;
}


OpAccessibleItem* OpSpacer::GetAccessibleChildOrSelfAt(int x, int y)
{
	return NULL;
}


OpAccessibleItem* OpSpacer::GetAccessibleFocusedChildOrSelf()
{
	return NULL;
}

#endif // ACCESSIBILITY_EXTENSION_SUPPORT
