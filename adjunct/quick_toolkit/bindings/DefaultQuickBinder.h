/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef DEFAULT_QUICK_BINDER_H
#define DEFAULT_QUICK_BINDER_H

#include "adjunct/quick_toolkit/bindings/QuickBinder.h"

/**
 * A QuickBinder that makes the effects of bindings permanent immediately.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class DefaultQuickBinder : public QuickBinder
{
protected:
	virtual OP_STATUS OnAddBinding(OpProperty<bool>& property) { return OpStatus::OK; }
	virtual OP_STATUS OnAddBinding(OpProperty<OpString>& property) { return OpStatus::OK; }
	virtual OP_STATUS OnAddBinding(OpProperty<INT32>& property) { return OpStatus::OK; }
	virtual OP_STATUS OnAddBinding(OpProperty<UINT32>& property) { return OpStatus::OK; }
	virtual OP_STATUS OnAddBinding(OpProperty<OpTreeModel*>& property) { return OpStatus::OK; }
	virtual OP_STATUS OnAddBinding(OpProperty<OpINT32Vector>& property) { return OpStatus::OK; }
	virtual OP_STATUS OnAddBinding(PrefUtils::IntegerAccessor& accessor) { return OpStatus::OK; }
	virtual OP_STATUS OnAddBinding(PrefUtils::StringAccessor& accessor) { return OpStatus::OK; }
};

#endif // DEFAULT_QUICK_BINDER_H
