/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/bindings/QuickBinder_Text.h"

#include "adjunct/quick_toolkit/widgets/QuickTextWidget.h"


QuickBinder::TextBinding::TextBinding()
  : m_widget(NULL)
  , m_text_widget(NULL)
{
}

OP_STATUS QuickBinder::TextBinding::Init(QuickWidget& widget, QuickTextWidget& text_widget)
{
	m_widget = &widget;
	m_text_widget = &text_widget;
	return OpStatus::OK;
}


QuickBinder::TextPropertyBinding::TextPropertyBinding()
  : m_property(NULL)
{
}

QuickBinder::TextPropertyBinding::~TextPropertyBinding()
{
	if (m_property != NULL)
		m_property->Unsubscribe(this);
}

OP_STATUS QuickBinder::TextPropertyBinding::Init(QuickWidget& widget, QuickTextWidget& text_widget,
		OpProperty<OpString>& property)
{
	RETURN_IF_ERROR(TextBinding::Init(widget, text_widget));

	RETURN_IF_ERROR(property.Subscribe(
				MAKE_DELEGATE(*this, &TextPropertyBinding::OnPropertyChanged)));
	m_property = &property;

	// Initial sync.
	OnPropertyChanged(m_property->Get());

	return OpStatus::OK;
}

void QuickBinder::TextPropertyBinding::OnPropertyChanged(const OpStringC& new_value)
{
	m_text_widget->SetText(new_value);
}

QuickBinder::EditableTextPropertyBinding::~EditableTextPropertyBinding()
{
	if (m_widget != NULL)
		static_cast<QuickEditableTextWidget*>(m_text_widget)->RemoveChangeDelegates(this);
}

OP_STATUS QuickBinder::EditableTextPropertyBinding::Init(QuickWidget& widget, QuickEditableTextWidget& edit,
		OpProperty<OpString>& property)
{
	RETURN_IF_ERROR(TextPropertyBinding::Init(widget, edit, property));
	return edit.AddChangeDelegate(MAKE_DELEGATE(*this, &EditableTextPropertyBinding::OnTextChanged));

}

void QuickBinder::EditableTextPropertyBinding::OnTextChanged(OpWidget& widget)
{
	OpString text;
	if (OpStatus::IsSuccess(widget.GetText(text)))
		m_property->Set(text);
}



QuickBinder::EditableTextPreferenceBinding::EditableTextPreferenceBinding()
	: m_accessor(NULL)
{
}

QuickBinder::EditableTextPreferenceBinding::~EditableTextPreferenceBinding()
{
	if (m_widget != NULL)
		static_cast<QuickEditableTextWidget*>(m_text_widget)->RemoveChangeDelegates(this);
	OP_DELETE(m_accessor);
}

OP_STATUS QuickBinder::EditableTextPreferenceBinding::Init(QuickWidget& widget,
		QuickEditableTextWidget& edit, PrefUtils::StringAccessor* accessor)
{
	OpAutoPtr<PrefUtils::StringAccessor> accessor_holder(accessor);

	RETURN_IF_ERROR(TextBinding::Init(widget, edit));

	m_accessor = accessor_holder.release();
	RETURN_IF_ERROR(edit.AddChangeDelegate(MAKE_DELEGATE(*this, &EditableTextPreferenceBinding::OnTextChanged)));

	// Initial sync
	OpString value;
	RETURN_IF_ERROR(m_accessor->GetPref(value));
	RETURN_IF_ERROR(edit.SetText(value));

	return OpStatus::OK;
}

void QuickBinder::EditableTextPreferenceBinding::OnTextChanged(OpWidget& widget)
{
	OpString text;
	if (OpStatus::IsSuccess(widget.GetText(text)))
		OpStatus::Ignore(m_accessor->SetPref(text));
}
