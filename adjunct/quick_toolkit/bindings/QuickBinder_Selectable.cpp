/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/bindings/QuickBinder_Selectable.h"
#include "adjunct/quick_toolkit/widgets/QuickSelectable.h"


QuickBinder::QuickSelectableBinding::QuickSelectableBinding()
	: m_selectable(NULL)
{
}

QuickBinder::QuickSelectableBinding::~QuickSelectableBinding()
{
	if (m_selectable != NULL)
	{
		m_selectable->RemoveOpWidgetListener(*this);
	}
}

OP_STATUS QuickBinder::QuickSelectableBinding::Init(QuickSelectable& selectable)
{
	RETURN_IF_ERROR(selectable.AddOpWidgetListener(*this));
	m_selectable = &selectable;

	return OpStatus::OK;
}

const QuickWidget* QuickBinder::QuickSelectableBinding::GetBoundWidget() const
{
	return m_selectable;
}

void QuickBinder::QuickSelectableBinding::OnChange(OpWidget* widget,
		BOOL by_mouse)
{
	OpStatus::Ignore(FromWidget(widget->GetValue() != 0));
}

OP_STATUS QuickBinder::QuickSelectableBinding::ToWidget(bool new_value)
{
	m_selectable->GetOpWidget()->SetValue(new_value);
	return OpStatus::OK;
}


QuickBinder::QuickSelectablePropertyBinding::QuickSelectablePropertyBinding()
	: m_property(NULL)
{
}

QuickBinder::QuickSelectablePropertyBinding::~QuickSelectablePropertyBinding()
{
	if (m_property != NULL)
	{
		m_property->Unsubscribe(this);
	}
}

OP_STATUS QuickBinder::QuickSelectablePropertyBinding::Init(
		QuickSelectable& selectable, OpProperty<bool>& property)
{
	RETURN_IF_ERROR(QuickSelectableBinding::Init(selectable));

	RETURN_IF_ERROR(property.Subscribe(MAKE_DELEGATE(
				*this, &QuickSelectablePropertyBinding::OnPropertyChanged)));
	m_property = &property;

	// Initial sync.
	OnPropertyChanged(m_property->Get());

	return OpStatus::OK;
}

void QuickBinder::QuickSelectablePropertyBinding::OnPropertyChanged(bool new_value)
{
	OpStatus::Ignore(ToWidget(new_value));
}

OP_STATUS QuickBinder::QuickSelectablePropertyBinding::FromWidget(bool new_value)
{
	m_property->Set(new_value);
	return OpStatus::OK;
}


QuickBinder::QuickSelectablePreferenceBinding::QuickSelectablePreferenceBinding()
	: m_accessor(NULL), m_selected(1), m_deselected(0)
{
}

QuickBinder::QuickSelectablePreferenceBinding::~QuickSelectablePreferenceBinding()
{
	OP_DELETE(m_accessor);
}

OP_STATUS QuickBinder::QuickSelectablePreferenceBinding::Init(
		QuickSelectable& selectable, PrefUtils::IntegerAccessor* accessor, int selected, int deselected)
{
	OpAutoPtr<PrefUtils::IntegerAccessor> accessor_holder(accessor);

	RETURN_IF_ERROR(QuickSelectableBinding::Init(selectable));
	m_accessor = accessor_holder.release();

	m_selected = selected;
	m_deselected = deselected;

	// Initial sync
	return ToWidget(m_accessor->GetPref() == m_selected);
}

OP_STATUS QuickBinder::QuickSelectablePreferenceBinding::FromWidget(bool new_value)
{
	OP_STATUS status = OpStatus::OK;
	if (new_value != (m_accessor->GetPref() == m_selected))
		status = m_accessor->SetPref(new_value ? m_selected : m_deselected);
	return status;
}
