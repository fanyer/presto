/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/bindings/QuickBinder.h"

#include "adjunct/desktop_util/adt/typedobjectcollection.h"
#include "adjunct/quick/widgets/DesktopFileChooserEdit.h"
#include "adjunct/quick_toolkit/bindings/QuickBinder_AddressDropDown.h"
#include "adjunct/quick_toolkit/bindings/QuickBinder_DropDown.h"
#include "adjunct/quick_toolkit/bindings/QuickBinder_Enabled.h"
#include "adjunct/quick_toolkit/bindings/QuickBinder_NumberEdit.h"
#include "adjunct/quick_toolkit/bindings/QuickBinder_Selectable.h"
#include "adjunct/quick_toolkit/bindings/QuickBinder_Text.h"
#include "adjunct/quick_toolkit/bindings/QuickBinder_Tree.h"
#include "adjunct/quick_toolkit/widgets/QuickAddressDropDown.h"
#include "adjunct/quick_toolkit/widgets/QuickButton.h"
#include "adjunct/quick_toolkit/widgets/QuickSelectable.h"
#include "adjunct/quick_toolkit/widgets/QuickDropDown.h"
#include "adjunct/quick_toolkit/widgets/QuickEdit.h"
#include "adjunct/quick_toolkit/widgets/QuickGroupBox.h"
#include "adjunct/quick_toolkit/widgets/QuickLabel.h"
#include "adjunct/quick_toolkit/widgets/QuickMultilineEdit.h"
#include "adjunct/quick_toolkit/widgets/QuickNumberEdit.h"
#include "adjunct/quick_toolkit/widgets/QuickRadioButton.h"
#include "adjunct/quick_toolkit/widgets/QuickTreeView.h"


QuickBinder::QuickBinder()
	: m_widgets(NULL)
{
}

QuickWidget* QuickBinder::GetWidgetByName(const char* name) const
{
	QuickWidget* const widget =
			m_widgets != NULL ? m_widgets->Get<QuickWidget>(name) : NULL;
	OP_ASSERT(widget != NULL || !"Failed to look up a widget by name");
	return widget;
}

OP_STATUS QuickBinder::Connect(const char* widget_name, PrefsCollectionFiles::filepref preference)
{
	QuickWidget* const widget = GetWidgetByName(widget_name);
	if (widget == NULL)
		return OpStatus::ERR;

	PrefUtils::StringAccessor* accessor = OP_NEW(PrefUtils::FilePathAccessor, (preference));
	RETURN_OOM_IF_NULL(accessor);
	const OP_STATUS status = Connect(*widget, accessor);
	OP_ASSERT(status != OpStatus::ERR_NOT_SUPPORTED
			|| !"Unsupported widget <-> preference type combination");
	return status;
}

template <> OP_STATUS QuickBinder::Connect<OpString>(QuickWidget& widget,
		OpProperty<OpString>& property)
{
	if (widget.IsOfType<QuickLabel>())
		return Connect(*widget.GetTypedObject<QuickLabel>(), property);
	else if (widget.IsOfType<QuickEdit>())
		return Connect(*widget.GetTypedObject<QuickEdit>(), property);
	else if (widget.IsOfType<QuickDropDown>())
		return Connect(*widget.GetTypedObject<QuickDropDown>(), property);
	else if (widget.IsOfType<QuickAddressDropDown>())
		return Connect(*widget.GetTypedObject<QuickDropDown>(), property);
	else if (widget.IsOfType<QuickButton>())
		return Connect(*widget.GetTypedObject<QuickButton>(), property);
	else if (widget.IsOfType<QuickSelectable>())
		return Connect(*widget.GetTypedObject<QuickSelectable>(), property);
	else if (widget.IsOfType<QuickGroupBox>())
		return Connect(*widget.GetTypedObject<QuickGroupBox>(), property);
	else if (widget.IsOfType<QuickMultilineEdit>())
		return Connect(*widget.GetTypedObject<QuickMultilineEdit>(), property);
	else if (widget.IsOfType<QuickDesktopFileChooserEdit>())
		return Connect(*widget.GetTypedObject<QuickDesktopFileChooserEdit>(), property);

	OP_ASSERT(!"Don't know how to connect widget to OpProperty<OpString>");
	return OpStatus::ERR_NOT_SUPPORTED;
}

template <> OP_STATUS QuickBinder::Connect<bool>(QuickWidget& widget,
		OpProperty<bool>& property)
{
	if (widget.IsOfType<QuickSelectable>())
		return Connect(*widget.GetTypedObject<QuickSelectable>(), property);

	OP_ASSERT(!"Don't know how to connect widget to OpProperty<bool>");
	return OpStatus::ERR_NOT_SUPPORTED;
}

template <> OP_STATUS QuickBinder::Connect<OpTreeModel*>(QuickWidget& widget,
		OpProperty<OpTreeModel*>& property)
{
	if (widget.IsOfType<QuickTreeView>())
		return Connect(*widget.GetTypedObject<QuickTreeView>(), property);

	OP_ASSERT(!"Don't know how to connect widget to OpProperty<OpTreeModel>");
	return OpStatus::ERR_NOT_SUPPORTED;
}

template <> OP_STATUS QuickBinder::Connect<UINT32>(QuickWidget& widget,
		OpProperty<UINT32>& property)
{
	if (widget.IsOfType<QuickNumberEdit>())
		return Connect(*widget.GetTypedObject<QuickNumberEdit>(), property);

	return OpStatus::ERR_NOT_SUPPORTED;
}

template <> OP_STATUS QuickBinder::Connect<INT32>(QuickWidget& widget,
		OpProperty<INT32>& property)
{
	if (widget.IsOfType<QuickDropDown>())
		return Connect(*widget.GetTypedObject<QuickDropDown>(), property);
	else if (widget.IsOfType<QuickTreeView>())
		return Connect(*widget.GetTypedObject<QuickTreeView>(), property);

	OP_ASSERT(!"Don't know how to connect widget to OpProperty<INT32>");
	return OpStatus::ERR_NOT_SUPPORTED;
}

template <> OP_STATUS QuickBinder::Connect<OpINT32Vector>(QuickWidget& widget,
		OpProperty<OpINT32Vector>& property)
{
	if (widget.IsOfType<QuickTreeView>())
		return Connect(*widget.GetTypedObject<QuickTreeView>(), property);

	OP_ASSERT(!"Don't know how to connect widget to OpProperty<OpINT32Vector>");
	return OpStatus::ERR_NOT_SUPPORTED;
}


OP_STATUS QuickBinder::Connect(QuickWidget& widget, PrefUtils::IntegerAccessor* accessor, int selected, int deselected)
{
	OpAutoPtr<PrefUtils::IntegerAccessor> accessor_holder(accessor);
	if (widget.IsOfType<QuickNumberEdit>())
		return Connect(*widget.GetTypedObject<QuickNumberEdit>(), accessor_holder.release());
	else if (widget.IsOfType<QuickSelectable>())
		return Connect(*widget.GetTypedObject<QuickSelectable>(), accessor_holder.release(), selected, deselected);
	else if (widget.IsOfType<QuickDropDown>())
		return Connect(*widget.GetTypedObject<QuickDropDown>(), accessor_holder.release());

	OP_ASSERT(!"Don't know how to connect widget to integer preference");
	return OpStatus::ERR_NOT_SUPPORTED;
}

OP_STATUS QuickBinder::Connect(QuickWidget& widget, PrefUtils::StringAccessor* accessor)
{
	OpAutoPtr<PrefUtils::StringAccessor> accessor_holder(accessor);
	if (widget.IsOfType<QuickEdit>())
		return Connect(*widget.GetTypedObject<QuickEdit>(), accessor_holder.release());
	else if (widget.IsOfType<QuickDropDown>())
		return Connect(*widget.GetTypedObject<QuickDropDown>(), accessor_holder.release());
	else if (widget.IsOfType<QuickDesktopFileChooserEdit>())
		return Connect(*widget.GetTypedObject<QuickDesktopFileChooserEdit>(), accessor_holder.release());

	OP_ASSERT(!"Don't know how to connect widget to string preference");
	return OpStatus::ERR_NOT_SUPPORTED;
}


OP_STATUS QuickBinder::Connect(QuickLabel& label,
		OpProperty<OpString>& property)
{
	return AddBinding<TextPropertyBinding>(label, property);
}

OP_STATUS QuickBinder::Connect(QuickEdit& edit, OpProperty<OpString>& property)
{
	return AddBinding<EditableTextPropertyBinding>(edit, property);
}

OP_STATUS QuickBinder::Connect(QuickMultilineEdit& edit, OpProperty<OpString>& property)
{
	return AddBinding<EditableTextPropertyBinding>(edit, property);
}

OP_STATUS QuickBinder::Connect(QuickNumberEdit& number_edit,
		OpProperty<UINT32>& property)
{
	return AddBinding<QuickNumberEditPropertyBinding>(number_edit, property);
}

OP_STATUS QuickBinder::Connect(QuickDropDown& drop_down,
		OpProperty<OpString>& property)
{
	drop_down.GetOpWidget()->SetEditableText();
	return AddBinding<EditableTextPropertyBinding>(drop_down, property);
}

OP_STATUS QuickBinder::Connect(QuickDropDown& drop_down,
		OpProperty<INT32>& property)
{
	return AddBinding<QuickDropDownSelectionPropertyBinding>(drop_down, property);
}

OP_STATUS QuickBinder::Connect(QuickAddressDropDown& drop_down,
		OpProperty<OpString>& property)
{
	drop_down.GetOpWidget()->SetEditableText(TRUE);
	return AddBinding<QuickAddressDropDownTextBinding>(drop_down, property);
}

OP_STATUS QuickBinder::Connect(QuickButton& button,
		OpProperty<OpString>& property)
{
	return AddBinding<TextPropertyBinding>(button, property);
}

OP_STATUS QuickBinder::Connect(QuickSelectable& selectable,
		OpProperty<bool>& property)
{
	return AddBinding<QuickSelectablePropertyBinding>(selectable, property);
}

OP_STATUS QuickBinder::Connect(QuickSelectable& selectable,
		OpProperty<OpString>& property)
{
	return AddBinding<TextPropertyBinding>(selectable, property);
}

OP_STATUS QuickBinder::Connect(QuickGroupBox& group_box,
		OpProperty<OpString>& property)
{
	return AddBinding<TextPropertyBinding>(group_box, property);
}

OP_STATUS QuickBinder::Connect(QuickTreeView& tree_view,
		OpProperty<OpTreeModel*>& property)
{
	return AddBinding<QuickTreeViewModelBinding>(tree_view, property);
}

OP_STATUS QuickBinder::Connect(QuickTreeView& tree_view,
		OpProperty<INT32>& property)
{
	return AddBinding<QuickTreeViewSelectionBinding>(tree_view, property);
}

OP_STATUS QuickBinder::Connect(QuickTreeView& tree_view,
		OpProperty<OpINT32Vector>& property)
{
	return AddBinding<QuickTreeViewMultiSelectionBinding>(tree_view, property);
}

OP_STATUS QuickBinder::Connect(QuickDesktopFileChooserEdit& edit, OpProperty<OpString>& property)
{
	return AddBinding<EditableTextPropertyBinding>(edit, property);
}


OP_STATUS QuickBinder::Connect(QuickSelectable& widget, PrefUtils::IntegerAccessor* accessor,
		int selected, int deselected)
{
	OpAutoPtr<PrefUtils::IntegerAccessor> accessor_holder(accessor);

	OpAutoPtr<QuickSelectablePreferenceBinding> binding(OP_NEW(QuickSelectablePreferenceBinding, ()));
	RETURN_OOM_IF_NULL(binding.get());
	RETURN_IF_ERROR(binding->Init(widget, accessor_holder.release(), selected, deselected));
	return AddBinding(binding.release(), *accessor);
}

OP_STATUS QuickBinder::Connect(QuickNumberEdit& widget, PrefUtils::IntegerAccessor* accessor)
{
	return AddBinding<QuickNumberEditPreferenceBinding>(widget, accessor);
}

OP_STATUS QuickBinder::Connect(QuickDropDown& widget, PrefUtils::IntegerAccessor* accessor)
{
	return AddBinding<QuickDropDownSelectionPreferenceBinding>(widget, accessor);
}

OP_STATUS QuickBinder::Connect(QuickEdit& widget, PrefUtils::StringAccessor* accessor)
{
	return AddBinding<EditableTextPreferenceBinding>(widget, accessor);
}

OP_STATUS QuickBinder::Connect(QuickDropDown& widget, PrefUtils::StringAccessor* accessor)
{
	widget.GetOpWidget()->SetEditableText();
	return AddBinding<EditableTextPreferenceBinding>(widget, accessor);
}

OP_STATUS QuickBinder::Connect(QuickDesktopFileChooserEdit& widget, PrefUtils::StringAccessor* accessor)
{
	return AddBinding<EditableTextPreferenceBinding>(widget, accessor);
}

OP_STATUS QuickBinder::EnableIf(const char* widget_name,
		OpProperty<bool>& property)
{
	QuickWidget* const widget = GetWidgetByName(widget_name);
	if (widget == NULL)
	{
		return OpStatus::ERR;
	}
	return EnableIf(*widget, property);
}

OP_STATUS QuickBinder::EnableIf(QuickWidget& widget, OpProperty<bool>& property)
{
	return AddBinding<EnabledBinding>(widget, property);
}

OP_STATUS QuickBinder::AddBinding(Binding* binding, PrefUtils::IntegerAccessor& accessor)
{
	OpAutoPtr<Binding> binding_holder(binding);
	RETURN_IF_ERROR(m_bindings.Add(binding));
	binding_holder.release();

	const OP_STATUS result = OnAddBinding(accessor);
	if (OpStatus::IsError(result))
		m_bindings.Delete(m_bindings.GetCount() - 1);

	return result;
}

void QuickBinder::Disconnect(const char* widget_name)
{
	QuickWidget* const widget = GetWidgetByName(widget_name);
	if (widget != NULL)
	{
		Disconnect(*widget);
	}
}

void QuickBinder::Disconnect(QuickWidget& widget)
{
	for (int i = (int)m_bindings.GetCount() - 1; i >= 0; i--)
	{
		if (m_bindings.Get(i)->GetBoundWidget() == &widget)
			m_bindings.Delete(i);
	}
}
