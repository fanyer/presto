/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/QuickComposite.h"

#include "modules/scope/src/scope_manager.h"
#include "adjunct/desktop_scope/src/scope_desktop_window_manager.h"
#include "adjunct/desktop_scope/src/scope_widget_info.h"

#include "modules/widgets/OpButton.h"


class OpComposite::CompositeWidgetInfo : public OpScopeWidgetInfo
{
public:
	CompositeWidgetInfo(OpComposite& composite) : m_composite(composite) {}
	virtual OP_STATUS AddQuickWidgetInfoItems(OpScopeDesktopWindowManager_SI::QuickWidgetInfoList &list, BOOL include_nonhoverable, BOOL include_invisible);
private:
	OpComposite& m_composite;
};

OpScopeWidgetInfo* OpComposite::CreateScopeWidgetInfo()
{
	return OP_NEW(CompositeWidgetInfo, (*this));
}

void OpComposite::SetActionButton(OpButton* button)
{
	m_button = button;
	if (m_button)
		m_button->SetTabStop(TRUE);
}

void OpComposite::AddChild(OpWidget* child, BOOL is_internal_child, BOOL first)
{
	OpWidget::AddChild(child, TRUE, first); // force children to be internal, so that the OpComposite remains to be their listener
	child->SetIgnoresMouse(TRUE);
	child->SetListener(this);
}

void OpComposite::OnMouseMove(const OpPoint& point)
{
	if (!m_hovered)
	{
		m_hovered = true;
		GenerateSetSkinState(this, SKINSTATE_HOVER, true);
	}
}

void OpComposite::OnMouseLeave()
{
	if (m_hovered)
	{
		m_hovered = false;
		GenerateSetSkinState(this, SKINSTATE_HOVER, false);
	}
}

void OpComposite::OnMouseUp(const OpPoint& point, MouseButton button, UINT8 nclicks)
{
	if (nclicks)
	{
		OpWidget *widget  = m_button;
		if (!widget)
			widget = GetWidget(point, TRUE, TRUE);

		if (widget && widget != this)
			widget->OnMouseUp(point, button, nclicks);
	}
}

void OpComposite::OnMouseDown(const OpPoint& point, MouseButton button, UINT8 nclicks)
{
	if (nclicks)
	{
		OpWidget *widget  = m_button;
		if (!widget)
			widget = GetWidget(point, TRUE, TRUE);

		if (widget && widget != this)
			widget->OnMouseDown(point, button, nclicks);
	}
}

void OpComposite::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	if (listener)
	{
		listener->OnMouseEvent(widget, pos, x, y, button, down, nclicks);
	}
}

void OpComposite::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (widget == m_button)
	{
		GenerateSetSkinState(this, SKINSTATE_SELECTED, m_button->GetValue() ? true : false);
	}
}

void OpComposite::SetFocus(FOCUS_REASON reason)
{
	if (m_button)
		m_button->SetFocus(reason);
	else
		OpWidget::SetFocus(reason);
}

void OpComposite::GenerateSetSkinState(OpWidget* widget, SkinState mask, bool state)
{
	widget->GetBorderSkin()->SetState(state ? mask : 0, mask, state ? 100 : 0);
	widget->GetForegroundSkin()->SetState(state ? mask : 0, mask, state ? 100 : 0);

	for (OpWidget* child = widget->GetFirstChild(); child; child = child->GetNextSibling())
		GenerateSetSkinState(child, mask, state);
}


OP_STATUS OpComposite::GetText(OpString& text)
{
	OpPoint pt = g_input_manager->GetMousePosition();
	for (OpWidget* child = GetFirstChild(); child; child = child->GetNextSibling())
	{
		if (child->GetScreenRect().Contains(pt))
			return child->GetText(text);
	}

	return OpStatus::ERR;
}

/**
 * This is needed for the hover tooltip because the widgets inside the Composite don't take mouse input and 
 * therefore cannot be 'hovered' with the UI property examiner.
 */
OP_STATUS OpComposite::CompositeWidgetInfo::AddQuickWidgetInfoItems(OpScopeDesktopWindowManager_SI::QuickWidgetInfoList &list, BOOL include_nonhoverable, BOOL include_invisible /* = TRUE*/)
{
	if (include_nonhoverable)
	{
		for (OpWidget* child = m_composite.GetFirstChild(); child; child = child->GetNextSibling())
		{
			OpAutoPtr<OpScopeDesktopWindowManager_SI::QuickWidgetInfo> info(OP_NEW(OpScopeDesktopWindowManager_SI::QuickWidgetInfo, ()));
			RETURN_OOM_IF_NULL(info.get());

			OpString wname;
			RETURN_IF_ERROR(wname.Set(child->GetName()));
			RETURN_IF_ERROR(info.get()->SetName(wname.CStr()));

			OpScopeDesktopWindowManager_SI::QuickWidgetInfo::QuickWidgetType type =  g_scope_manager->desktop_window_manager->GetWidgetType(child);
			// Set the info
			info.get()->SetType(type);

			OpString text_value;
			RETURN_IF_ERROR(m_composite.GetText(text_value));
			RETURN_IF_ERROR(info.get()->SetText(text_value.CStr()));

			// Should only get here for the visible items
			info.get()->SetVisible(child->IsVisible());

			// Set the enabled flag based on the main control
			info.get()->SetEnabled(child->IsEnabled());

			// Set the rect
			OpRect rect = child->GetScreenRect();
			info.get()->GetRectRef().SetX(rect.x );
			info.get()->GetRectRef().SetY(rect.y );
			info.get()->GetRectRef().SetWidth(rect.width);
			info.get()->GetRectRef().SetHeight(rect.height);

			OpString name;
			OpWidget* parent = m_composite.GetParent();
			if (parent && parent->GetName().HasContent())
			{
				RETURN_IF_ERROR(name.Set(parent->GetName()));
				info.get()->SetParent(name);
			}
			RETURN_IF_ERROR(list.GetQuickwidgetListRef().Add(info.get()));
			info.release();
		}
	}

	return OpStatus::OK;
}
