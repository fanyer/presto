/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef QUICK_BINDER_H
#define QUICK_BINDER_H

#include "adjunct/desktop_util/adt/opproperty.h"
#include "adjunct/desktop_util/prefs/PrefAccessors.h"
#include "adjunct/quick_toolkit/widgets/QuickWidgetDecls.h"
#include "modules/util/adt/opvector.h"

class OpTreeModel;
class TypedObjectCollection;


/**
 * Connects widgets with properties and preferences by means of
 * auto-synchronizing bindings.
 *
 * The bindings are in place as long as the QuickBinder object that created them
 * exists.
 *
 * The idea is that a UiContext/Controller object will own a QuickBinder object
 * and use it to connect the UI elements with data/model and probably with the
 * UiContext/Controller object itself, too.
 *
 * QuickBinder is an abstract base class.  Concrete subclasses may have
 * different strategies for making the effects of bindings permanent.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 * @see OpProperty
 */
class QuickBinder
{
public:
	QuickBinder();
	virtual ~QuickBinder() {}

	/**
	 * Associates the binder with a collection of named widgets.
	 *
	 * @param widgets the named widget collection, or NULL to reset
	 */
	void SetWidgetCollection(const TypedObjectCollection* widgets) { m_widgets = widgets; }

	/**
	 * Connects a named QuickWidget to a property.  From now on, the "widget
	 * value" shall be automatically synchronized with the property value.
	 *
	 * The meaning of "widget value" depends on both widget type and property
	 * type.  E.g., connecting a check box to an OpProperty<bool> binds the
	 * "checked" state of the widget, while connecting the same check box to an
	 * OpProperty<OpString> binds the text of the widget.
	 *
	 * For this to work, the binder must be given a named widget collection with
	 * SetWidgetCollection() first.
	 *
	 * @param T property type
	 * @param widget_name identifies the widget
	 * @param property the property the widget is connected to
	 * @return status
	 */
	template <typename T>
	OP_STATUS Connect(const char* widget_name, OpProperty<T>& property);

	/**
	 * Connects a QuickWidget instance to a property.  From now on, the "widget
	 * value" shall be automatically synchronized with the property value.
	 *
	 * The meaning of "widget value" depends on both widget type and property
	 * type.  E.g., connecting a check box to an OpProperty<bool> binds the
	 * "checked" state of the widget, while connecting the same check box to an
	 * OpProperty<OpString> binds the text of the widget.
	 *
	 * @param widget the widget instance
	 * @param property the property the widget is connected to
	 * @return status
	 */
	OP_STATUS Connect(QuickLabel& widget, OpProperty<OpString>& property);
	OP_STATUS Connect(QuickEdit& widget, OpProperty<OpString>& property);
	OP_STATUS Connect(QuickNumberEdit& widget, OpProperty<UINT32>& property);
	OP_STATUS Connect(QuickDropDown& widget, OpProperty<OpString>& property);
	OP_STATUS Connect(QuickDropDown& widget, OpProperty<INT32>& property);
	OP_STATUS Connect(QuickAddressDropDown& widget, OpProperty<OpString>& property);
	OP_STATUS Connect(QuickButton& widget, OpProperty<OpString>& property);
	OP_STATUS Connect(QuickSelectable& widget, OpProperty<OpString>& property);
	OP_STATUS Connect(QuickSelectable& widget, OpProperty<bool>& property);
	OP_STATUS Connect(QuickGroupBox& widget, OpProperty<OpString>& property);
	OP_STATUS Connect(QuickTreeView& widget, OpProperty<OpTreeModel*>& property);
	OP_STATUS Connect(QuickTreeView& widget, OpProperty<INT32>& property);
	OP_STATUS Connect(QuickTreeView& widget, OpProperty<OpINT32Vector>& property);
	OP_STATUS Connect(QuickMultilineEdit& widget, OpProperty<OpString>& property);
	OP_STATUS Connect(QuickDesktopFileChooserEdit& widget, OpProperty<OpString>& property);

	/**
	 * Connects a named QuickWidget to a preference.  This is very similar to
	 * connecting to a property, only the "widget value" is copied into a
	 * preference rather than a property.
	 *
	 * @param C preference collection type
	 * @param widget_name identifies the widget
	 * @param collection the preference collection containing the preference
	 * @param preference the preference the widget is connected to
	 * @param selected (QuickSelectable only) the preference value to be mapped to
	 * 		the "selected" state
	 * @param deselected (QuickSelectable only) the preference value to be mapped to
	 * 		the "deselected" state
	 * @return status
	 * @see Connect(const char*, OpProperty<T>&)
	 */
	template <typename C>
	OP_STATUS Connect(const char* widget_name, C& collection, typename C::integerpref preference,
			int selected = 1, int deselected = 0);
	template <typename C>
	OP_STATUS Connect(const char* widget_name, C& collection, typename C::stringpref preference);
	OP_STATUS Connect(const char* widget_name, PrefsCollectionFiles::filepref preference);

	/**
	 * Connects a QuickWidget instance to a preference.  This is very similar
	 * to connecting to a property, only the "widget value" is copied into a
	 * preference rather than a property.
	 *
	 * @param widget the widget instance
	 * @param accessor represents the preference the widget is connected to.
	 * 		Ownership is transferred.
	 * @param selected (QuickSelectable only) the preference value to be mapped to
	 * 		the "selected" state
	 * @param deselected (QuickSelectable only) the preference value to be mapped to
	 * 		the "deselected" state
	 * @return status
	 * @see Connect(const char*, OpProperty<T>&)
	 */
	OP_STATUS Connect(QuickSelectable& widget, PrefUtils::IntegerAccessor* accessor, int selected = 1, int deselected = 0);
	OP_STATUS Connect(QuickNumberEdit& widget, PrefUtils::IntegerAccessor* accessor);
	OP_STATUS Connect(QuickDropDown& widget, PrefUtils::IntegerAccessor* accessor);
	OP_STATUS Connect(QuickDropDown& widget, PrefUtils::StringAccessor* accessor);
	OP_STATUS Connect(QuickEdit& widget, PrefUtils::StringAccessor* accessor);
	OP_STATUS Connect(QuickDesktopFileChooserEdit& widget, PrefUtils::StringAccessor* accessor);

	/**
	 * Connects a named QuickWidget's "enabled" state to a property.  From now
	 * on, the widget shall automatically be enabled if and only if the property
	 * is @c true.
	 *
	 * For this to work, the binder must be given a named widget collection with
	 * SetWidgetCollection() first.
	 *
	 * @param widget_name identifies the widget
	 * @param property the property the "enabled" state of the widget is
	 * 		connected to
	 * @return status
	 */
	OP_STATUS EnableIf(const char* widget_name, OpProperty<bool>& property);

	/**
	 * Connects the "enabled" state of a QuickWidget instance to a property.
	 * From now on, the widget shall automatically be enabled if and only if the
	 * property is @c true.
	 *
	 * @param widget the widget instance
	 * @param property the property the "enabled" state of the widget is
	 * 		connected to
	 * @return status
	 */
	OP_STATUS EnableIf(QuickWidget& widget, OpProperty<bool>& property);

	/**
	 * Disconnect associations for a specific widget
	 * @param widget_name Name of the widget to disconnect (needs a named
	 * 		widgets collection via SetWidgetCollection)
	 * @param widget Widget to disconnect
	 */
	void Disconnect(const char* name);
	void Disconnect(QuickWidget& widget);

protected:
	/**
	 * Called upon adding a property binding.
	 *
	 * @param property the property a widget is being connected to
	 * @return status.  Returning an error will prevent the binding from being
	 * 		created.
	 */
	virtual OP_STATUS OnAddBinding(OpProperty<bool>& property) = 0;
	virtual OP_STATUS OnAddBinding(OpProperty<OpString>& property) = 0;
	virtual OP_STATUS OnAddBinding(OpProperty<INT32>& property) = 0;
	virtual OP_STATUS OnAddBinding(OpProperty<UINT32>& property) = 0;
	virtual OP_STATUS OnAddBinding(OpProperty<OpTreeModel*>& property) = 0;
	virtual OP_STATUS OnAddBinding(OpProperty<OpINT32Vector>& property) = 0;

	/**
	 * Called upon adding a preference binding.
	 *
	 * @param accessor the preference accessor representing the preference
	 * 		a widget is being connected to
	 * @return status.  Returning an error will prevent the binding from being
	 * 		created.
	 */
	virtual OP_STATUS OnAddBinding(PrefUtils::IntegerAccessor& accessor) = 0;
	virtual OP_STATUS OnAddBinding(PrefUtils::StringAccessor& accessor) = 0;

private:
	/**
	 * A tagging interface that makes it possible to store the different kinds
	 * of Binding in one container.
	 */
	class Binding
	{
	public:
		virtual ~Binding()  {}
		virtual const QuickWidget* GetBoundWidget() const = 0;
	};

	class TextBinding;
	class TextPropertyBinding;
	class EditableTextPropertyBinding;
	class EditableTextPreferenceBinding;
	class QuickAddressDropDownTextBinding;
	class QuickSelectableBinding;
	class QuickSelectablePropertyBinding;
	class QuickSelectablePreferenceBinding;
	class QuickNumberEditBinding;
	class QuickNumberEditPropertyBinding;
	class QuickNumberEditPreferenceBinding;
	class QuickDropDownSelectionBinding;
	class QuickDropDownSelectionPropertyBinding;
	class QuickDropDownSelectionPreferenceBinding;
	class QuickTreeViewModelBinding;
	class QuickTreeViewSelectionBinding;
	class QuickTreeViewMultiSelectionBinding;
	class EnabledBinding;

	template <typename T>
	OP_STATUS Connect(QuickWidget& widget, OpProperty<T>& property);

	OP_STATUS Connect(QuickWidget& widget, PrefUtils::IntegerAccessor* accessor, int selected, int deselected);
	OP_STATUS Connect(QuickWidget& widget, PrefUtils::StringAccessor* accessor);

	template <typename B, typename W, typename T>
	OP_STATUS AddBinding(W& widget, OpProperty<T>& property);

	template <typename B, typename W>
	OP_STATUS AddBinding(W& widget, PrefUtils::IntegerAccessor* accessor);
	OP_STATUS AddBinding(Binding* binding, PrefUtils::IntegerAccessor& accessor);

	template <typename B, typename W>
	OP_STATUS AddBinding(W& widget, PrefUtils::StringAccessor* accessor);

	QuickWidget* GetWidgetByName(const char* name) const;

	const TypedObjectCollection* m_widgets;
	OpAutoVector<Binding> m_bindings;
};



template <typename T>
OP_STATUS QuickBinder::Connect(const char* widget_name, OpProperty<T>& property)
{
	QuickWidget* const widget = GetWidgetByName(widget_name);
	if (widget == NULL)
		return OpStatus::ERR;

	const OP_STATUS status = Connect(*widget, property);
	OP_ASSERT(status != OpStatus::ERR_NOT_SUPPORTED
			|| !"Unsupported widget <-> property type combination");
	return status;
}

template <typename C>
OP_STATUS QuickBinder::Connect(const char* widget_name, C& collection,
		typename C::integerpref preference, int selected, int deselected)
{
	QuickWidget* const widget = GetWidgetByName(widget_name);
	if (widget == NULL)
		return OpStatus::ERR;

	PrefUtils::IntegerAccessor* accessor =
			OP_NEW(PrefUtils::IntegerAccessorImpl<C>, (collection, preference));
	RETURN_OOM_IF_NULL(accessor);
	const OP_STATUS status = Connect(*widget, accessor, selected, deselected);
	OP_ASSERT(status != OpStatus::ERR_NOT_SUPPORTED
			|| !"Unsupported widget <-> preference type combination");
	return status;
}

template <typename C>
OP_STATUS QuickBinder::Connect(const char* widget_name, C& collection, typename C::stringpref preference)
{
	QuickWidget* const widget = GetWidgetByName(widget_name);
	if (widget == NULL)
		return OpStatus::ERR;

	PrefUtils::StringAccessor* accessor =
			OP_NEW(PrefUtils::StringAccessorImpl<C>, (collection, preference));
	RETURN_OOM_IF_NULL(accessor);
	const OP_STATUS status = Connect(*widget, accessor);
	OP_ASSERT(status != OpStatus::ERR_NOT_SUPPORTED
			|| !"Unsupported widget <-> preference type combination");
	return status;
}

template <typename B, typename W, typename T>
OP_STATUS QuickBinder::AddBinding(W& widget, OpProperty<T>& property)
{
	OpAutoPtr<B> binding(OP_NEW(B, ()));
	RETURN_OOM_IF_NULL(binding.get());
	RETURN_IF_ERROR(binding->Init(widget, property));
	RETURN_IF_ERROR(m_bindings.Add(binding.get()));
	binding.release();

	const OP_STATUS result = OnAddBinding(property);
	if (OpStatus::IsError(result))
		m_bindings.Delete(m_bindings.GetCount() - 1);

	return result;
}

template <typename B, typename W>
OP_STATUS QuickBinder::AddBinding(W& widget, PrefUtils::IntegerAccessor* accessor)
{
	OpAutoPtr<PrefUtils::IntegerAccessor> accessor_holder(accessor);

	OpAutoPtr<B> binding(OP_NEW(B, ()));
	RETURN_OOM_IF_NULL(binding.get());
	RETURN_IF_ERROR(binding->Init(widget, accessor_holder.release()));
	return AddBinding(binding.release(), *accessor);
}

template <typename B, typename W>
OP_STATUS QuickBinder::AddBinding(W& widget, PrefUtils::StringAccessor* accessor)
{
	OpAutoPtr<PrefUtils::StringAccessor> accessor_holder(accessor);

	OpAutoPtr<B> binding(OP_NEW(B, ()));
	RETURN_OOM_IF_NULL(binding.get());
	RETURN_IF_ERROR(binding->Init(widget, accessor_holder.release()));
	RETURN_IF_ERROR(m_bindings.Add(binding.get()));
	binding.release();

	const OP_STATUS result = OnAddBinding(*accessor);
	if (OpStatus::IsError(result))
		m_bindings.Delete(m_bindings.GetCount() - 1);

	return result;
}

#endif // QUICK_BINDER_H
