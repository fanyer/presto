/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef OP_BUFFERING_PROPERTY_H
#define OP_BUFFERING_PROPERTY_H

#include "adjunct/desktop_util/adt/opproperty.h"


/**
 * An OpProperty<T> acting as a buffer for another OpProperty<T>.
 *
 * An OpBufferingProperty wrapping an OpProperty is first initialized with the
 * wrapped property's value.  Changes applied to the OpBufferingProperty object
 * only affect the wrapped property when the commit trigger is fired.  If the
 * trigger is never fired, the wrapped property is left intact.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
template <typename T>
class OpBufferingProperty : public OpProperty<T>
{
public:
	/**
	 * Constructs a non-functional OpBufferingProperty.  Use Init() to make it
	 * functional.
	 */
	OpBufferingProperty() : m_target(NULL)  {}

	~OpBufferingProperty()
	{
		if (m_commit_trigger != NULL)
		{
			m_commit_trigger->Unsubscribe(this);
		}
	}

	/**
	 * Initializes the OpBufferingProperty.  Must be called to make it functional.
	 *
	 * @param target the target property wrapped by this OpBufferingProperty
	 * @param commit_trigger a @c false -> @c true transition will commit the
	 * 		buffered value into @a target
	 * @return status
	 */
	OP_STATUS Init(OpProperty<T>& target, OpProperty<bool>& commit_trigger)
	{
		RETURN_IF_ERROR(commit_trigger.Subscribe(
					MAKE_DELEGATE(*this, &OpBufferingProperty::OnTrigger)));
		this->Set(target.Get());

		m_commit_trigger = &commit_trigger;
		m_target = &target;

		return OpStatus::OK;
	}

	void OnTrigger(bool trigger)
	{
		if (trigger)
		{
			m_target->Set(OpProperty<T>::Get());
		}
	}

private:
	OpProperty<T>* m_target;
	OpProperty<bool>* m_commit_trigger;
};


#endif // OP_BUFFERING_PROPERTY_H
