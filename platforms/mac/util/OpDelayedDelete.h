/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_DELAYED_DELETE_H
#define OP_DELAYED_DELETE_H

#include "adjunct/desktop_util/actions/delayed_action.h"

template<class T>
class OpDelayedDelete : private OpDelayedAction
{
public:
	OpDelayedDelete(T* ptr) : OpDelayedAction(0), m_ptr(ptr) {}
	void DoAction() {delete m_ptr;}
private:
	T* m_ptr;
};

#endif // OP_DELAYED_DELETE_H
