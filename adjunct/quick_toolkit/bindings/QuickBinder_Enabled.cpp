/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/bindings/QuickBinder_Enabled.h"
#include "adjunct/quick_toolkit/widgets/QuickWidget.h"


QuickBinder::EnabledBinding::EnabledBinding()
	: m_widget(NULL), m_property(NULL)
{
}

QuickBinder::EnabledBinding::~EnabledBinding()
{
	if (m_property != NULL)
	{
		m_property->Unsubscribe(this);
	}
}

OP_STATUS QuickBinder::EnabledBinding::Init(QuickWidget& widget,
		OpProperty<bool>& property)
{
	RETURN_IF_ERROR(property.Subscribe(MAKE_DELEGATE(
				*this, &EnabledBinding::OnPropertyChanged)));
	m_property = &property;
	m_widget = &widget;

	// Initial sync.
	OnPropertyChanged(m_property->Get());

	return OpStatus::OK;
}

void QuickBinder::EnabledBinding::OnPropertyChanged(bool new_value)
{
	m_widget->SetEnabled(new_value);
}
