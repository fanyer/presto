/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef REVERSIBLE_QUICK_BINDER_H
#define REVERSIBLE_QUICK_BINDER_H

#include "adjunct/desktop_util/adt/opproperty.h"
#include "adjunct/desktop_util/prefs/PrefAccessors.h"
#include "adjunct/quick_toolkit/bindings/QuickBinder.h"


/**
 * A QuickBinder that can revert the bound properties and preferences to their
 * original values.
 *
 * The binder will do the reverting when it's destroyed, unless specifically
 * asked not to.
 *
 * OK/Cancel-type dialogs are the typical use case: you want changes entered in
 * the dialog to be removed from the model if the user discards them.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class ReversibleQuickBinder : public QuickBinder
{
public:
	/**
	 * Constructs a binder that will do the reverting upon destruction.
	 *
	 * Call Commit() to tell the binder not to.
	 */
	ReversibleQuickBinder();
	virtual ~ReversibleQuickBinder();

	/**
	 * Tells the binder not to revert the bound properties.
	 */
	void Commit();

protected:
	virtual OP_STATUS OnAddBinding(OpProperty<bool>& property)
		{ return AddReverter(property); }

	virtual OP_STATUS OnAddBinding(OpProperty<OpString>& property)
		{ return AddReverter(property); }

	virtual OP_STATUS OnAddBinding(OpProperty<INT32>& property)
		{ return AddReverter(property); }

	virtual OP_STATUS OnAddBinding(OpProperty<UINT32>& property)
		{ return AddReverter(property); }

	virtual OP_STATUS OnAddBinding(OpProperty<OpTreeModel*>& property)
		{ return AddReverter(property); }

	virtual OP_STATUS OnAddBinding(OpProperty<OpINT32Vector>& property)
		{ return AddReverter(property); }

	virtual OP_STATUS OnAddBinding(PrefUtils::IntegerAccessor& accessor)
		{ return AddReverter(accessor); }

	virtual OP_STATUS OnAddBinding(PrefUtils::StringAccessor& accessor)
		{ return AddReverter(accessor); }

private:
	class Reverter
	{
	public:
		virtual ~Reverter()  {}
		virtual void Revert() = 0;
	};

	template <typename T>
	class PropertyReverter : public Reverter
	{
	public:
		explicit PropertyReverter(OpProperty<T>& property)
			: m_property(&property), m_property_copy(OP_NEW(OpProperty<T>, ()))
		{}

		virtual ~PropertyReverter()
			{ OP_DELETE(m_property_copy); }

		OP_STATUS Init()
		{
			RETURN_OOM_IF_NULL(m_property_copy);
			m_property_copy->Set(m_property->Get());
			return OpStatus::OK;
		}

		virtual void Revert()
			{ if (m_property_copy != NULL) m_property->Set(m_property_copy->Get()); }

	private:
		OpProperty<T>* const m_property;
		OpProperty<T>* m_property_copy;
	};

	class IntegerPrefReverter : public Reverter
	{
	public:
		explicit IntegerPrefReverter(PrefUtils::IntegerAccessor& accessor)
			: m_accessor(&accessor), m_original_value(accessor.GetPref())
		{}

		virtual void Revert() { OpStatus::Ignore(m_accessor->SetPref(m_original_value)); }

	private:
		PrefUtils::IntegerAccessor* const m_accessor;
		int m_original_value;
	};

	class StringPrefReverter : public Reverter
	{
	public:
		explicit StringPrefReverter(PrefUtils::StringAccessor& accessor)
			: m_accessor(&accessor)
		{}

		OP_STATUS Init() { return m_accessor->GetPref(m_original_value); }
		virtual void Revert() { OpStatus::Ignore(m_accessor->SetPref(m_original_value)); }

	private:
		PrefUtils::StringAccessor* const m_accessor;
		OpString m_original_value;
	};

	template <typename T>
	OP_STATUS AddReverter(OpProperty<T>& property);

	OP_STATUS AddReverter(PrefUtils::IntegerAccessor& accessor);
	OP_STATUS AddReverter(PrefUtils::StringAccessor& accessor);

	void RevertAll();

	bool m_committed;
	OpAutoVector<Reverter> m_reverters;
};



template <typename T>
OP_STATUS ReversibleQuickBinder::AddReverter(OpProperty<T>& property)
{
	OpAutoPtr< PropertyReverter<T> > reverter(OP_NEW(PropertyReverter<T>, (property)));
	RETURN_OOM_IF_NULL(reverter.get());
	RETURN_IF_ERROR(reverter->Init());
	RETURN_IF_ERROR(m_reverters.Add(reverter.get()));
	reverter.release();
	return OpStatus::OK;
}

#endif // REVERSIBLE_QUICK_BINDER_H
