/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/bindings/QuickBinder_DropDown.h"

#include "adjunct/desktop_util/prefs/PrefAccessors.h"
#include "adjunct/quick_toolkit/widgets/QuickDropDown.h"


QuickBinder::QuickDropDownSelectionBinding::QuickDropDownSelectionBinding()
	: m_drop_down(NULL)
{
}

QuickBinder::QuickDropDownSelectionBinding::~QuickDropDownSelectionBinding()
{
	if (m_drop_down != NULL)
	{
		m_drop_down->RemoveOpWidgetListener(*this);
	}
}

OP_STATUS QuickBinder::QuickDropDownSelectionBinding::Init(QuickDropDown& drop_down)
{
	RETURN_IF_ERROR(drop_down.AddOpWidgetListener(*this));
	m_drop_down = &drop_down;

	return OpStatus::OK;
}

const QuickWidget* QuickBinder::QuickDropDownSelectionBinding::GetBoundWidget() const
{
	return m_drop_down;
}

void QuickBinder::QuickDropDownSelectionBinding::OnChange(OpWidget* widget, BOOL by_mouse)
{
	const INT32 selected_index = m_drop_down->GetOpWidget()->GetSelectedItem();
	if (selected_index >= 0)
	{
		const INT32 widget_value = m_drop_down->GetOpWidget()->GetItemUserData(selected_index);
		FromWidget(widget_value);
	}
}

void QuickBinder::QuickDropDownSelectionBinding::ToWidget(INT32 new_value)
{
	DesktopOpDropDown* drop_down = m_drop_down->GetOpWidget();
	for (INT32 i = 0; i < drop_down->CountItems(); i++)
	{
		if (drop_down->GetItemUserData(i) == new_value)
		{
			drop_down->SelectItem(i, TRUE);
			return;
		}
	}

	drop_down->SelectItem(-1, TRUE);
}


QuickBinder::QuickDropDownSelectionPropertyBinding::QuickDropDownSelectionPropertyBinding()
	: m_property(NULL)
{
}

QuickBinder::QuickDropDownSelectionPropertyBinding::~QuickDropDownSelectionPropertyBinding()
{
	if (m_property != NULL)
	{
		m_property->Unsubscribe(this);
	}
}

OP_STATUS QuickBinder::QuickDropDownSelectionPropertyBinding::Init(
		QuickDropDown& drop_down, OpProperty<INT32>& property)
{
	RETURN_IF_ERROR(QuickDropDownSelectionBinding::Init(drop_down));

	RETURN_IF_ERROR(property.Subscribe(MAKE_DELEGATE(
				*this, &QuickDropDownSelectionPropertyBinding::OnPropertyChanged)));
	m_property = &property;

	// Initial sync.
	OnPropertyChanged(m_property->Get());

	return OpStatus::OK;
}

OP_STATUS QuickBinder::QuickDropDownSelectionPropertyBinding::FromWidget(INT32 new_value)
{
	m_property->Set(new_value);
	return OpStatus::OK;
}


QuickBinder::QuickDropDownSelectionPreferenceBinding::QuickDropDownSelectionPreferenceBinding()
	: m_accessor(NULL)
{
}

QuickBinder::QuickDropDownSelectionPreferenceBinding::~QuickDropDownSelectionPreferenceBinding()
{
	OP_DELETE(m_accessor);
}

OP_STATUS QuickBinder::QuickDropDownSelectionPreferenceBinding::Init(
		QuickDropDown& drop_down, PrefUtils::IntegerAccessor* accessor)
{
	OpAutoPtr<PrefUtils::IntegerAccessor> accessor_holder(accessor);

	RETURN_IF_ERROR(QuickDropDownSelectionBinding::Init(drop_down));

	m_accessor = accessor_holder.release();

	// Initial sync
	ToWidget(accessor->GetPref());

	return OpStatus::OK;
}

OP_STATUS QuickBinder::QuickDropDownSelectionPreferenceBinding::FromWidget(INT32 new_value)
{
	return m_accessor->SetPref(new_value);
}
