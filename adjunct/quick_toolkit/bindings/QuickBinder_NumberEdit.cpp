/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/bindings/QuickBinder_NumberEdit.h"

#include "adjunct/desktop_util/prefs/PrefAccessors.h"
#include "adjunct/quick_toolkit/widgets/QuickNumberEdit.h"


QuickBinder::QuickNumberEditBinding::QuickNumberEditBinding()
	: m_number_edit(NULL)
{
}

QuickBinder::QuickNumberEditBinding::~QuickNumberEditBinding()
{
	if (m_number_edit != NULL)
	{
		m_number_edit->RemoveOpWidgetListener(*this);
	}
}

OP_STATUS QuickBinder::QuickNumberEditBinding::Init(QuickNumberEdit& number_edit)
{
	RETURN_IF_ERROR(number_edit.AddOpWidgetListener(*this));
	m_number_edit = &number_edit;

	m_number_edit->GetOpWidget()->SetMinValue(0);
	m_number_edit->GetOpWidget()->SetMaxValue(UINT32_MAX);

	return OpStatus::OK;
}

const QuickWidget* QuickBinder::QuickNumberEditBinding::GetBoundWidget() const
{
	return m_number_edit;
}

void QuickBinder::QuickNumberEditBinding::OnChange(OpWidget* widget, BOOL by_mouse)
{
	int value = 0;
	if (m_number_edit->GetOpWidget()->GetIntValue(value))
		OpStatus::Ignore(FromWidget(value));
}

OP_STATUS QuickBinder::QuickNumberEditBinding::ToWidget(UINT32 new_value)
{
	OpString string;
	RETURN_IF_ERROR(string.AppendFormat(UNI_L("%d"), new_value));
	return m_number_edit->GetOpWidget()->SetText(string.CStr());
}


QuickBinder::QuickNumberEditPropertyBinding::QuickNumberEditPropertyBinding()
	: m_property(NULL)
{
}

QuickBinder::QuickNumberEditPropertyBinding::~QuickNumberEditPropertyBinding()
{
	if (m_property != NULL)
	{
		m_property->Unsubscribe(this);
	}
}

OP_STATUS QuickBinder::QuickNumberEditPropertyBinding::Init(
		QuickNumberEdit& number_edit, OpProperty<UINT32>& property)
{
	RETURN_IF_ERROR(QuickNumberEditBinding::Init(number_edit));

	RETURN_IF_ERROR(property.Subscribe(MAKE_DELEGATE(
				*this, &QuickNumberEditPropertyBinding::OnPropertyChanged)));
	m_property = &property;

	// Initial sync.
	OnPropertyChanged(m_property->Get());

	return OpStatus::OK;
}

void QuickBinder::QuickNumberEditPropertyBinding::OnPropertyChanged(UINT32 new_value)
{
	OpStatus::Ignore(ToWidget(new_value));
}

OP_STATUS QuickBinder::QuickNumberEditPropertyBinding::FromWidget(UINT32 new_value)
{
	m_property->Set(new_value);
	return OpStatus::OK;
}


QuickBinder::QuickNumberEditPreferenceBinding::QuickNumberEditPreferenceBinding()
	: m_accessor(NULL)
{
}

QuickBinder::QuickNumberEditPreferenceBinding::~QuickNumberEditPreferenceBinding()
{
	OP_DELETE(m_accessor);
}

OP_STATUS QuickBinder::QuickNumberEditPreferenceBinding::Init(
		QuickNumberEdit& number_edit, PrefUtils::IntegerAccessor* accessor)
{
	OpAutoPtr<PrefUtils::IntegerAccessor> accessor_holder(accessor);

	RETURN_IF_ERROR(QuickNumberEditBinding::Init(number_edit));

	m_accessor = accessor_holder.release();

	// Initial sync
	OpStatus::Ignore(ToWidget(accessor->GetPref()));
	return OpStatus::OK;
}

OP_STATUS QuickBinder::QuickNumberEditPreferenceBinding::FromWidget(UINT32 new_value)
{
	return m_accessor->SetPref(new_value);
}
